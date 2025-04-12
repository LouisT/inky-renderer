import { Hono } from 'hono';
import puppeteer from "@cloudflare/puppeteer";
import { basicAuth } from 'hono/basic-auth';
import { transform, getFallbackResponse } from '../providers/utils.mjs';
import allProviders from '../providers/index.mjs';
import getBrowserSession from './libs/browser.mjs';
import { patch } from './libs/patches.mjs';
import { pickOne, b64png } from './libs/utils.mjs';

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
        _json = c.req.query('json') == "true",
        _isDev = c.env.DEVELOPMENT == "true",
        _base = new URL(c.req.raw.url).origin,
        _provider = pickOne(
            _providers
                ? _providers.split(/[\|,\s]/gi) // Split by pipe, comma, or space
                : Object.keys(allProviders) // Otherwise, use all providers available
        );

    // If the provider doesn't exist, use Lorem Picsum
    if (!allProviders[_provider]) {
        return getFallbackResponse(_mode, _provider);
    }

    // Get the provider
    let provider = allProviders[_provider];
    if (provider.alias) {
        provider = allProviders[provider.alias];
        if (!provider) {
            return getFallbackResponse(_mode, provider.alias);
        }
    }

    // Get headers
    let headers = new Headers([
        ... (await provider.apiHeaders?.() ?? []),
        ['User-Agent', c.req.header('User-Agent') ?? "Inky Renderer/v0.0.1-dev.1"],
    ]);

    // Adjust the height based on mbh + offset
    if (_mode.mbh > 0 && provider.mbhOffset > 0)
        _mode.h = _mode.h - (_mode.mbh * provider.mbhOffset);

    //  Fetch the json data from the API endpoint
    let data;
    if ('api' in provider) {
        try {
            data = await provider.api(_mode, c, headers);
        } catch (e) {
            console.trace(e);
            return c.json({
                error: {
                    message: e.message ?? "Unknown error",
                }
            }, 500);
        }
    }

    try {
        // Get the provider type
        let _type = String(provider._type).toLowerCase();

        // If raw, return the data
        if (_raw && data)
            return (_type == "render" && !_json ?
                c.html(await provider.source(data, _mode, c))
                : c.json(data));

        // Check the provider type
        switch (_type) {
            // Feetch remote images
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
                        ["Content-Type", _raw ? "text/plain" : "image/jpeg"],
                        ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                        ["X-Image-Source", img.toLocaleString()],
                        ["X-Image-Provider", _provider],
                        ..._headers,
                    ]),
                });

            // Handle AI calls for images
            case "image:ai":
                // Fetch the image + return to the client
                return new Response((await provider.request(_mode, c)).body, {
                    headers: new Headers([
                        ["Content-Type", "image/jpeg"],
                        ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                        ["X-Image-Provider", _provider],
                        ["X-Inky-Message-2", "AI Generated Image"],
                    ]),
                });

            // Handle Browser Rendering calls
            case "render":
            case "remote":
                // Create a browser session
                let _browser;
                try {
                    _browser = await puppeteer.connect(c.env.BROWSER, (await getBrowserSession(c.env.BROWSER)));
                } catch {
                    _browser = undefined;
                    /* Ignore */
                }
                _browser = _browser ?? await puppeteer.launch(c.env.BROWSER);

                // Create a new page
                let page = await _browser.newPage();

                // Set the viewport, with a 5px buffer
                await page.setViewport({ width: _mode.w + 5, height: _mode.h + 5 });

                // If render, set the page content
                let $target;
                if (_type == "render") {
                    await page.setContent(await patch(await provider.source(data, _mode, c), _base), { waitUntil: 'networkidle2' });
                } else {
                    await page.goto(await provider.link(_mode, c), { waitUntil: 'networkidle2' });
                }

                // Select the target element
                let target = page;
                try {
                    if (provider.selectorrs instanceof Function) {
                        $target = await provider.selectorrs(_mode, c, page);
                    } else if (("target" in provider) && typeof provider.target == "string") {
                        $target = await page.$(provider.target);
                    } else {
                        $target = await page.$('.container');
                    }
                } catch (e) {
                    console.trace(e);
                    /* Falling back to page */
                }

                // Take a screenshot
                let screenshot = (await $target.screenshot(Object.assign({
                    type: "jpeg", // Always use jpeg
                    quality: 100,
                    omitBackground: true,
                    optimizeForSpeed: true,
                }, (await provider?.options?.(_mode, c) ?? {}))));

                // Disconnect and free up resources
                await _browser.disconnect();

                // Take the screenshot + return to the client
                return new Response(screenshot, {
                    headers: new Headers([
                        ["Content-Type", "image/jpeg"],
                        ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                        ["X-Image-Provider", _provider],
                        ...(await provider.headers?.(data, _mode, c.env) ?? []),
                    ]),
                });

            // Fallback to Lorem Picsum
            default:
                return getFallbackResponse(_mode, _provider);
        }
    } catch (e) {
        console.log(e);
        return getFallbackResponse(_mode, _provider);
    }
});

// AI slop; use AI to generate a random image.
// This is because you can't send Authorization headers with Image Transform.
// XXX: Replace errors with fallback image?
v1.get('/_ai/slop/:auth?', async (c) => {
    // Check auth
    if (c.env.SLOP_ACCESS_KEY && c.req.param('auth') != c.env.SLOP_ACCESS_KEY)
        return new Response("Unauthorized", { status: 401 });

    // Check if AI exists
    if (!c?.env?.AI)
        return new Response("AI not enabled", { status: 404 });

    // Generate the image
    return b64png((await c.env.AI.run(c.env.SLOP_IMAGE_MODEL, {
            prompt: (await c.env.AI.run(c.env.SLOP_PROMPT_MODEL, {
                max_tokens: 256,
                messages: [
                    { role: "system", content: "You are an artist. You respond in no more than 100 words." },
                    { role: "user", content: "Describe a completely random scene as if it were an image. Envision any subject matter, in any art style, color palette, or composition. Provide a vivid, open-ended textual description that embodies pure spontaneity and unpredictability, focusing on the details." },
                ],
                seed: ~~(Math.random() * 100000000),
                temperature: 1,
            })).response
        })).image);
});

// Export v1
export { v1 as default, v1 };