import { Hono } from 'hono';
import puppeteer from "@cloudflare/puppeteer";
import { basicAuth } from 'hono/basic-auth';
import imageProviders from '../providers/images.mjs';
import renderProviders from '../providers/renders.mjs';
import { transform, fallback } from '../providers/utils.mjs';

let modes = {
    0: {
        "orientation": "landscape",
        "w": 1200,
        "h": 825,
    },
    1: {
        "orientation": "portrait",
        "w": 825,
        "h": 1200,
    },
};
modes[2] = modes[0];
modes[3] = modes[1];

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
    let mode = modes[c.req.query('rotation') ?? 0] || modes[0],
        _providers = c.req.param('providers'),
        _raw = c.req.param('raw') == "raw",
        _provider;

    if (_providers) {
        _provider = (_providers.split(/[\|,\s-]/gi)).sort(() => .5 - Math.random())[0];  // Get a random provider from a supplied list
    } else {
        _provider = Object.keys(imageProviders).sort(() => .5 - Math.random())[0];  // Get a random provider
    }

    // If the provider doesn't exist, use Lorem Picsum
    if (!imageProviders[_provider]) {
        let src = new URL(fallback(mode));
        return new Response((await fetch(src)).body, {
            ...transform(mode),
            headers: new Headers([
                ["Content-Type", "image/jpeg"],
                ["X-Image-Orientation", mode.orientation],
                ["X-Image-Size", `${mode.w}x${mode.h}`],
                ["X-Image-Source", src],
                ["X-Image-Provider", "Lorem Picsum"],
                ['X-Invalid-Provider', _provider],
            ])
        });
    }

    let provider = imageProviders[_provider],
        headers = new Headers([
            ... (provider.apiHeaders?.() ?? []),
            ['User-Agent', c.req.header('User-Agent') ?? "Inky Renderer/v0.0.1-dev.1"],
        ]);

    //  Fetch the json data from the API endpoint
    // TODO: Validate the response
    let data = await (await fetch(provider.api(mode, c), { headers })).json();

    if (_raw)
        return c.json(data);

    // Build the image URL
    let img = provider.image(data, mode, c);

    // Fetch the image + return to the client
    return new Response((await fetch(img, {
        headers,
        ...(provider.transform ? transform(mode) : {}),
    })).body, {
        headers: new Headers([
            ["Content-Type", "image/jpeg"],
            ["X-Image-Orientation", mode.orientation],
            ["X-Image-Size", `${mode.w}x${mode.h}`],
            ["X-Image-Source", img.toLocaleString()],
            ["X-Image-Provider", _provider],
            ...(provider.headers?.(data, mode, c) ?? []),
        ]),
    });
});

// Browser renderer
v1.get('/render/:providers?/:raw?', async (c) => {
    let mode = modes[c.req.query('rotation') ?? 0] || modes[0],
        _providers = c.req.param('providers'),
        _raw = c.req.param('raw') == "raw",
        _provider;

    if (_providers) {
        _provider = (_providers.split(/[\|,\s-]/gi)).sort(() => .5 - Math.random())[0];  // Get a random provider from a supplied list
    } else {
        _provider = Object.keys(renderProviders).sort(() => .5 - Math.random())[0];  // Get a random provider
    }

    // If the provider doesn't exist, use Lorem Picsum
    if (!renderProviders[_provider]) {
        let src = new URL(fallback(mode));
        return new Response((await fetch(src)).body, {
            ...transform(mode),
            headers: new Headers([
                ["Content-Type", "image/jpeg"],
                ["X-Image-Orientation", mode.orientation],
                ["X-Image-Size", `${mode.w}x${mode.h}`],
                ["X-Image-Source", src],
                ["X-Image-Provider", "Lorem Picsum"],
                ['X-Invalid-Provider', _provider],
            ])
        });
    }

    // Get the provider
    let provider = renderProviders[_provider],
        headers = new Headers([
            ... (provider.apiHeaders?.() ?? []),
            ['User-Agent', c.req.header('User-Agent') ?? "Inky Renderer/v0.0.1-dev.1"],
        ]);

    //  Fetch the json data from the API endpoint
    // TODO: Validate the response
    let data = await (await fetch(provider.api(mode, c), { headers })).json();

    if (_raw)
        return c.html(await provider.source(data, mode, c));

    // Create the browser instance
    const browser = await puppeteer.launch(c.env.BROWSER);
    const page = await browser.newPage();
    await page.setViewport({ width: mode.w, height: mode.h });

    // Set the page content
    await page.setContent(await provider.source(data, mode, c));

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
            ["X-Image-Orientation", mode.orientation],
            ["X-Image-Size", `${mode.w}x${mode.h}`],
            ["X-Image-Provider", _provider],
            ...(provider.headers?.(data, mode, c.env) ?? []),
        ]),
    });
});

// Export v1
export { v1 as default, v1 };