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

// Force basic auth for uploads
v0.use('/images/upload', async (c, next) => {
    let users = [];
    try {
        users = Object.entries(JSON.parse(String(c.env.USERS).trim().replace(/\\"/g, '"')));
    } catch (e) {
        return c.text("Users not found/configured.", 500);
    }
    return basicAuth(...users.map(([username, password]) => ({ username, password })))(c, next);
});

// Upload endpoint
v0.post('/images/upload', async (c) => {
    try {
        const body = await c.req.parseBody(),
            file = body['image'];
        if (!file || !(file instanceof File))
            return c.json({ error: "No image provided" }, 400);
        const arrayBuffer = await file.arrayBuffer();
        await c.env.INKY_IMAGES.put('latest-upload', arrayBuffer, {
            metadata: { contentType: file.type }
        });
        return c.json({ success: true });
    } catch (e) {
        return c.json({ error: e.message }, 500);
    }
});

// Route to pull the image. The :random_key param prevents caching,
// but we always pull 'latest-upload' from KV
v0.get('/images/:random_key', async (c) => {
    const storageKey = 'latest-upload',
        { value, metadata } = await c.env.INKY_IMAGES.getWithMetadata(storageKey, { type: 'arrayBuffer' });
    if (!value)
        return c.json({ error: "Image not found" }, 404);
    return new Response(value, {
        headers: {
            'Content-Type': metadata?.contentType || 'image/jpeg',
            'Cache-Control': 'no-cache, no-store, must-revalidate' // Ensure no caching
        }
    });
});

// Export v0
export { v0 as default, v0 };