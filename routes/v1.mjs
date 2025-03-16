import { Hono } from 'hono';
import puppeteer from "@cloudflare/puppeteer";
import { basicAuth } from 'hono/basic-auth';
import { transform, getFallbackResponse } from '../providers/utils.mjs';
import allProviders from '../providers/index.mjs';

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

// Content rendering endpoint
v1.get('/render/:providers?/:raw?', async (c) => {
    let _providers = c.req.param('providers'),
        _mode = {
            w: parseInt(c.req.query('w') ?? 1200),
            h: parseInt(c.req.query('h') ?? 825),
            mbh: parseInt(c.req.query('mbh') ?? 0),
        },
        _raw = c.req.param('raw') == "raw",
        _provider;

    if (_providers) {
        _provider = (_providers.split(/[\|,\s-]/gi)).sort(() => .5 - Math.random())[0];  // Get a random provider from a supplied list
    } else {
        _provider = Object.keys(allProviders).sort(() => .5 - Math.random())[0];  // Get a random provider
    }

    // If the provider doesn't exist, use Lorem Picsum
    if (!allProviders[_provider]) {
        return getFallbackResponse(_mode, _provider);
    }

    // Get the provider
    let provider = allProviders[_provider],
        headers = new Headers([
            ... (await provider.apiHeaders?.() ?? []),
            ['User-Agent', c.req.header('User-Agent') ?? "Inky Renderer/v0.0.1-dev.1"],
        ]);

    // Adjust the height based on mbh + offset
    if (_mode.mbh > 0 && provider.mbhOffset > 0)
        _mode.h = _mode.h - (_mode.mbh * provider.mbhOffset);

    //  Fetch the json data from the API endpoint
    let data;
    try {
        data = await (await fetch(await provider.api(_mode, c), { headers })).json();
    } catch (e) {
        return c.json({
            error: {
                message: e.message ?? "Unknown error",
            }
        }, 500);
    }

    if (_raw)
        return provider.type == "render" ?
            c.html(await provider.source(data, _mode, c))
            : c.json(data);

    // Check the provider type
    switch (String(provider._type).toLowerCase()) {
        case "image":
            // Build the image URL
            let img = await provider.image(data, _mode, c);

            // Get headers
            let _headers = (await provider.headers?.(data, _mode, c) ?? []);

            // Fetch the image + return to the client
            return new Response((await fetch(img, {
                headers,
                ...transform(_mode, _headers),
            })).body, {
                headers: new Headers([
                    ["Content-Type", "image/jpeg"],
                    ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                    ["X-Image-Source", img.toLocaleString()],
                    ["X-Image-Provider", _provider],
                    ..._headers,
                ]),
            });

        case "render":
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

        default:
            return getFallbackResponse(_mode, _provider);
    }
});

// Export v1
export { v1 as default, v1 };