import { Hono } from 'hono';
import { getTimeZoneInfo } from './libs/timezone.mjs';
import { createMiddleware } from 'hono/factory';
import { encode } from 'cbor2';
import { basicAuth } from 'hono/basic-auth';

// Create versioned endpoint
const v0 = new Hono().basePath('/api/v0');

// Create the cbor middleware
v0.use(createMiddleware(async (c, next) => {
    c.header('Content-Type', 'application/cbor');
    c.setRenderer((content) => {
        return c.body(encode(content));
    });
    await next();
}));

// Create an endpoint for getting timezone data
v0.get('/timezone/:timezone?', async (c) => {
    let _cbor = c.req.query('cbor');
    try {
        return c[_cbor ? 'render' : 'json'](getTimeZoneInfo(
            c.req.param('timezone')
            ?? c.req.query('timezone')
            ?? c.req.query('zone')
            ?? c.req.query('tz')
            ?? c.req.header('x-timezone')
            ?? c.req.header('x-tz')
            ?? c.req?.raw?.cf?.timezone
            ?? undefined
        ));
    } catch (e) {
        return c.json({ error: e.message });
    }
});

// Reusable auth middleware, always requires Basic Auth, ignores SKIP_AUTH
const authMiddleware = async (c, next) => {
    let users = [];
    try {
        users = Object.entries(JSON.parse(String(c.env.USERS).trim().replace(/\\"/g, '"')));
    } catch (e) {
        return c.text("Users not found/configured.", 500);
    }
    return basicAuth(...users.map(([username, password]) => ({ username, password })))(c, next);
};

// Helper to extract username from Basic Auth header
const getUser = (c) => {
    try {
        return atob((c.req.header('Authorization'))?.split(' ')?.[1])?.split?.(':')?.[0] ?? 'unknown';
    } catch {
        return 'unknown';
    }
};

// Helper to parse and normalize image tags
const parseTags = (tags) => {
    try {
        // Parse tags
        let _tags = String(tags ?? '').split(',').map(t => t.trim()).filter(t => t.length > 0);
        if (!Array.isArray(_tags))
            _tags = [_tags];

        // Normalize tags
        return _tags.map(t => t.toLowerCase().replace(/\s+/g, '-')
            .replace(/[^a-z0-9-]/g, '_').trim())
            .filter((t, i, a) => a.indexOf(t) === i)
            .filter(t => t.length > 0)
            .sort();
    } catch {
        return [];
    }
}

// Apply Auth to Upload and Gallery
v0.use('/images/upload', authMiddleware);
v0.use('/images/list', authMiddleware);

// Get a list of supported image tags
v0.get('/images/tags', async (c) => {
    try {
        // Ensure table exists just in case
        await c.env.DB.prepare(
            `CREATE TABLE IF NOT EXISTS tags (tag TEXT PRIMARY KEY)`
        ).run();

        // Get all tags and return
        return c.json({
            tags: (await c.env.DB.prepare(
                `SELECT tag FROM tags ORDER BY tag ASC`
            ).all()).results.map(r => r.tag)
        });
    } catch (e) {
        return c.json({ tags: [] });
    }
});

// Upload endpoint
v0.post('/images/upload', async (c) => {
    try {
        // { all: true } ensures we get an array if multiple files share the key 'image'
        const body = await c.req.parseBody({ all: true });
        let files = body['image'],
            tagsInput = body['tags'];

        // Normalize to array
        if (!files)
            return c.json({ error: "No images provided" }, 400);
        if (!Array.isArray(files))
            files = [files];

        // Parse tags
        const tags = parseTags(tagsInput);

        // Set up variables
        const user = getUser(c),
            timestamp = Date.now(),
            uploadedIds = [],
            dbStatements = [];

        // Ensure tables exist
        await c.env.DB.prepare(`
            CREATE TABLE IF NOT EXISTS images (
                id TEXT PRIMARY KEY,
                created INTEGER,
                width INTEGER,
                height INTEGER,
                deleted INTEGER DEFAULT 0,
                uploaded_by TEXT,
                deleted_by TEXT,
                tags TEXT
            )
        `).run();
        await c.env.DB.prepare(
            `CREATE TABLE IF NOT EXISTS tags (tag TEXT PRIMARY KEY)`
        ).run();

        // Add new tags to the master 'tags' list
        for (const tag of tags)
            dbStatements.push(c.env.DB.prepare("INSERT OR IGNORE INTO tags (tag) VALUES (?)").bind(tag));

        // Process each file
        for (const file of files) {
            if (!(file instanceof File))
                continue;

            // Get image data + generate ID
            const arrayBuffer = await file.arrayBuffer(),
                id = crypto.randomUUID();

            // Upload to R2
            await c.env.INKY_IMAGES.put(id, arrayBuffer, {
                httpMetadata: { contentType: file.type }
            });

            // Prepare D1 Insert
            dbStatements.push(
                c.env.DB.prepare(
                    `INSERT INTO images (id, created, uploaded_by, tags) VALUES (?, ?, ?, ?)`
                ).bind(id, timestamp, user, JSON.stringify(tags))
            );

            uploadedIds.push(id);
        }

        // Execute Batch Insert in D1
        if (dbStatements.length > 0)
            await c.env.DB.batch(dbStatements);

        // Return the uploaded IDs
        return c.json({ success: true, ids: uploadedIds });
    } catch (e) {
        return c.json({ error: e.message }, 500);
    }
});

// Soft delete image endpoint
v0.delete('/images/:id', async (c) => {
    const id = c.req.param('id'),
        user = getUser(c);
    try {
        const result = await c.env.DB.prepare(
            "UPDATE images SET deleted = 1, deleted_by = ? WHERE id = ?"
        ).bind(user, id).run();
        if (result.meta.changes > 0)
            return c.json({ success: true });
        else
            return c.json({ error: "Image not found" }, 404);
    } catch (e) {
        return c.json({ error: e.message }, 500);
    }
});

v0.get('/images/random/:random_key?', async (c) => {
    try {
        const tags = parseTags(c.req.query('tags') || c.req.query('tag'));

        // Build query
        let query = "SELECT id FROM images WHERE deleted = 0",
            params = [];

        // Filter by tags if provided
        if (tags.length > 0) {
            query = `
                SELECT DISTINCT images.id FROM images, json_each(images.tags)
                WHERE images.deleted = 0 AND json_each.value IN (${tags.map(() => '?').join(',')})
            `;
            params = tags;
        }

        // Add random ordering
        query += " ORDER BY RANDOM() LIMIT 1";

        // Get a random image
        const result = await c.env.DB.prepare(query).bind(...params).first();
        if (!result)
            return c.json({ error: "No images found" }, 404);

        // Redirect to the image
        return c.redirect(`/api/v0/images/${result.id}`);
    } catch (e) {
        return c.json({ error: e.message }, 500);
    }
});

// List endpoint
v0.get('/images/list', async (c) => {
    try {
        const limit = 20,
            offset = parseInt(c.req.query('offset') ?? 0),
            tags = parseTags(c.req.query('tags') || c.req.query('tag'));
        let query, params;

        // Filter by deleted = 0 and order by created, then filter by tags
        if (tags.length > 0) {
            query = `
                SELECT DISTINCT images.* FROM images, json_each(images.tags) WHERE (deleted = 0 OR deleted IS NULL)
                AND json_each.value IN (${tags.map(() => '?').join(',')}) ORDER BY created DESC LIMIT ? OFFSET ?
            `;
            params = [...tags, limit, offset];
        } else {
            query = `SELECT * FROM images WHERE (deleted = 0 OR deleted IS NULL)  ORDER BY created DESC LIMIT ? OFFSET ?`;
            params = [limit, offset];
        }

        // Return the images and whether or not there are more
        const res = await c.env.DB.prepare(query).bind(...params).all();
        return c.json({
            images: res.results,
            hasMore: res.results.length === limit
        });
    } catch {
        return c.json({ images: [], hasMore: false });
    }
});

// Route to pull the image with support for Edge Caching
v0.get('/images/:id', async (c) => {
    const id = c.req.param('id'),
        width = c.req.query('w'),
        height = c.req.query('h');

    // If resizing is requested, we trigger a sub-request
    if (width || height) {
        // Construct the sub-request
        const requestUrl = new URL(c.req.url);
        requestUrl.search = '';

        // Make the sub-request
        const imageRes = await fetch(requestUrl.toString(), {
            headers: c.req.headers,
            cf: {
                image: {
                    width: width ? parseInt(width) : undefined,
                    height: height ? parseInt(height) : undefined,
                    fit: 'scale-down'
                }
            }
        });

        // Return the response from the sub-request
        return new Response(imageRes.body, imageRes);
    }

    // We construct a unique cache key based on the ID
    const cache = caches.default,
        cacheUrl = new URL(c.req.url);
    cacheUrl.pathname = `/api/v0/images/${id}`;
    cacheUrl.search = ''; // Ensure clean URL
    const cacheKey = new Request(cacheUrl.toString());

    // Check Cache
    let response = await cache.match(cacheKey);
    if (response)
        return response;

    // Cache Miss - Fetch from Storage (R2)
    const object = await c.env.INKY_IMAGES.get(id);
    if (!object)
        return c.json({ error: "Image not found" }, 404);

    // Create Response
    const headers = new Headers();
    object.writeHttpMetadata(headers);
    headers.set('etag', object.httpEtag);
    headers.set('Cache-Control', 'public, max-age=3600'); // 1 hour

    // Create Response
    response = new Response(object.body, {
        headers
    });

    // Save to Edge Cache
    c.executionCtx.waitUntil(cache.put(cacheKey, response.clone()));

    // Return
    return response;
});

export { v0 as default, v0 };