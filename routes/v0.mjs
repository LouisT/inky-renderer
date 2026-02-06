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

// Reusable Auth Middleware
const authMiddleware = async (c, next) => {
    let users = [];
    try {
        users = Object.entries(JSON.parse(String(c.env.USERS).trim().replace(/\\"/g, '"')));
    } catch (e) {
        return c.text("Users not found/configured.", 500);
    }
    return basicAuth(...users.map(([username, password]) => ({ username, password })))(c, next);
};

// Apply Auth to Upload and Gallery
v0.use('/images/upload', authMiddleware);
v0.use('/images/list', authMiddleware);

// Upload endpoint
v0.post('/images/upload', async (c) => {
    try {
        const body = await c.req.parseBody(),
            file = body['image'];

        // Validate the request
        if (!file || !(file instanceof File))
            return c.json({ error: "No image provided" }, 400);

        // Get the image details + generate an ID
        const arrayBuffer = await file.arrayBuffer(),
            id = crypto.randomUUID(),
            timestamp = Date.now();

        // Store the image in the R2 bucket
        await c.env.INKY_IMAGES.put(id, arrayBuffer, {
            httpMetadata: { contentType: file.type }
        });

        // Make sure the images table exists
        await c.env.DB.prepare(`
            CREATE TABLE IF NOT EXISTS images (
                id TEXT PRIMARY KEY,
                created INTEGER,
                width INTEGER,
                height INTEGER
            )
        `).run();

        // Insert the image into the database
        await c.env.DB.prepare(
            "INSERT INTO images (id, created) VALUES (?, ?)"
        ).bind(id, timestamp).run();

        // Return the ID
        return c.json({ success: true, id: id });
    } catch (e) {
        return c.json({ error: e.message }, 500);
    }
});

// Get a random image, allow for a random key for cache busting
v0.get('/images/random/:random_key?', async (c) => {
    try {
        // Get a random image
        const result = await c.env.DB.prepare("SELECT id FROM images ORDER BY RANDOM() LIMIT 1").first();
        if (!result)
            return c.json({ error: "No images found" }, 404);

        // Redirect to the image
        return c.redirect(`/api/v0/images/${result.id}`);
    } catch (e) {
        return c.json({ error: e.message }, 500);
    }
});

// List images (Gallery API)
v0.get('/images/list', async (c) => {
    const limit = 20,
        offset = parseInt(c.req.query('offset') ?? 0);
    try {
        // Get the images
        const results = await c.env.DB.prepare(
            "SELECT * FROM images ORDER BY created DESC LIMIT ? OFFSET ?"
        ).bind(limit, offset).all();

        // Return the images and whether or not there are more
        return c.json({
            images: results.results,
            hasMore: results.results.length === limit
        });
    } catch (e) {
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