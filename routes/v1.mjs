import { Hono } from 'hono';
import puppeteer from "@cloudflare/puppeteer";
import { basicAuth } from 'hono/basic-auth';
import imageProviders from '../providers/images.mjs';
import renderProviders from '../providers/renders.mjs';
import { transform, fallback } from '../providers/utils.mjs';

// Create versioned endpoint
const v1 = new Hono().basePath('/api/v1');

// Authentication middleware
v1.use("/*", async (c, next) => {
    if (c.env.DEVELOPMENT == "true" || c.env.SKIP_AUTH == "true")
        return await next();

    let users = [];
    try {
        users = Object.entries(JSON.parse(String(c.env.USERS).trim().replace(/\\"/g, '"')));
    } catch (e) {
        return c.text("Users not found.");
    }
    return basicAuth(...users.map(([username, password]) => ({ username, password })))(c, next)
});

// Get images from a remote providers
v1.get('/image/:providers?/:raw?', async (c) => {
    let _providers = c.req.param('providers'),
        _mode = {
            w: parseInt(c.req.query('w') ?? 1200),
            h: parseInt(c.req.query('h') ?? 825)
        },
        _raw = c.req.param('raw') == "raw",
        _provider;

    if (_providers) {
        _provider = (_providers.split(/[\|,\s-]/gi)).sort(() => .5 - Math.random())[0];  // Get a random provider from a supplied list
    } else {
        _provider = Object.keys(imageProviders).sort(() => .5 - Math.random())[0];  // Get a random provider
    }

    // If the provider doesn't exist, use Lorem Picsum
    if (!imageProviders[_provider]) {
        let src = new URL(fallback(_mode));
        return new Response((await fetch(src)).body, {
            ...transform(_mode),
            headers: new Headers([
                ["Content-Type", "image/jpeg"],
                ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                ["X-Image-Source", src],
                ["X-Image-Provider", "Lorem Picsum"],
                ['X-Invalid-Provider', _provider],
            ])
        });
    }

    let provider = imageProviders[_provider],
        headers = new Headers([
            ...(await provider.apiHeaders?.() ?? []),
            ['User-Agent', c.req.header('User-Agent') ?? "Inky Renderer/v0.0.1-dev.1"],
        ]);

    //  Fetch the json data from the API endpoint
    // TODO: Validate the response
    let data = await (await fetch(await provider.api(_mode, c), { headers })).json();

    if (_raw)
        return c.json(data);

    // Build the image URL
    let img = await provider.image(data, _mode, c);

    // Fetch the image + return to the client
    return new Response((await fetch(img, {
        headers,
        ...(provider.transform ? transform(_mode) : {}),
    })).body, {
        headers: new Headers([
            ["Content-Type", "image/jpeg"],
            ["X-Image-Size", `${_mode.w}x${_mode.h}`],
            ["X-Image-Source", img.toLocaleString()],
            ["X-Image-Provider", _provider],
            ...(await provider.headers?.(data, _mode, c) ?? []),
        ]),
    });
});

// Browser renderer
v1.get('/render/:providers?/:raw?', async (c) => {
    let _providers = c.req.param('providers'),
        _mode = {
            w: parseInt(c.req.query('w') ?? 1200),
            h: parseInt(c.req.query('h') ?? 825)
        },
        _raw = c.req.param('raw') == "raw",
        _provider;

    if (_providers) {
        _provider = (_providers.split(/[\|,\s-]/gi)).sort(() => .5 - Math.random())[0];  // Get a random provider from a supplied list
    } else {
        _provider = Object.keys(renderProviders).sort(() => .5 - Math.random())[0];  // Get a random provider
    }

    // If the provider doesn't exist, use Lorem Picsum
    if (!renderProviders[_provider]) {
        let src = new URL(fallback(_mode));
        return new Response((await fetch(src)).body, {
            ...transform(_mode),
            headers: new Headers([
                ["Content-Type", "image/jpeg"],
                ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                ["X-Image-Source", src],
                ["X-Image-Provider", "Lorem Picsum"],
                ['X-Invalid-Provider', _provider],
            ])
        });
    }

    // Get the provider
    let provider = renderProviders[_provider],
        headers = new Headers([
            ... (await provider.apiHeaders?.() ?? []),
            ['User-Agent', c.req.header('User-Agent') ?? "Inky Renderer/v0.0.1-dev.1"],
        ]);

    //  Fetch the json data from the API endpoint
    // TODO: Validate the response
    let data = await (await fetch(provider.api(_mode, c), { headers })).json();

    if (_raw)
        return c.html(await provider.source(data, _mode, c));

    // Create the browser instance
    const browser = await puppeteer.launch(c.env.BROWSER);
    const page = await browser.newPage();
    await page.setViewport({ width: _mode.w, height: _mode.h });

    // Set the page content
    await page.setContent(await provider.source(data, _mode, c));

    // Get the image element
    let $target = await page.$('.inky-content');

    // Take the screenshot + return to the client
    return new Response((await $target.screenshot({
        type: "jpeg",
        fromSurface: true,
        omitBackground: true,
        optimizeForSpeed: true
    })), {
        headers: new Headers([
            ["Content-Type", "image/jpeg"],
            ["X-Image-Size", `${_mode.w}x${_mode.h}`],
            ["X-Image-Provider", _provider],
            ...(await provider.headers?.(data, _mode, c.env) ?? []),
        ]),
    });
});

// Export v1
export { v1 as default, v1 };