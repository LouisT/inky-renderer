import { Hono } from 'hono';
import puppeteer from "@cloudflare/puppeteer";
import { basicAuth } from 'hono/basic-auth';
import allProviders from '../providers/index.mjs';
import getBrowserSession from './libs/browser.mjs';
import { patch } from './libs/patches.mjs';
import {
    transform,
    getFallbackResponse,
    pickOne,
    b64png,
    responseToReadableStream
} from '../providers/utils.mjs';

// The AI slop system prompt
let _systemPrompt = (`
    You are a vivid visual storyteller.
    In less than 100 words, craft imaginative, richly detailed scenes suitable for all ages.
    Avoid nudity, profanity, graphic violence, or adult themes.
    Focus on wholesome creativity, vibrant imagery, and a sense of wonder.
`).trim().replace(/\n/g, " ").replace(/\s+/g, " ");

// The AI slop prompt
let _prompt = (`
    Describe a completely random - yet strictly safe for work - scene as if it were an image.
    Envision any subject matter that is appropriate for all audiences (no nudity, explicit sexuality, graphic violence, gore, or profane/hateful language), in any art style, color palette, or composition.
    Provide a vivid, open-ended textual description that embodies pure spontaneity and unpredictability, focusing on fine details while keeping the content wholesome and suitable for a workplace setting.
`).trim().replace(/\n/g, " ").replace(/\s+/g, " ");

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

// Create an AI slop endpoint
v1.get('/_internal/ai-slop/:token?', async (c) => {
    if (c.env.SLOP_ACCESS_TOKEN !== c.req.param('token')) {
        console.error("Missing SLOP_ACCESS_TOKEN");
        return getFallbackResponse({ w: 1200, h: 825 }, "ai-slop");
    }

    if (!c?.env?.AI || !c?.env?.SLOP_IMAGE_MODEL || !c?.env?.SLOP_PROMPT_MODEL) {
        console.error("Missing AI, SLOP_IMAGE_MODEL or SLOP_PROMPT_MODEL");
        return getFallbackResponse({ w: 1200, h: 825 }, "ai-slop");
    }

    return b64png((await c.env.AI.run(c.env.SLOP_IMAGE_MODEL, {
        prompt: (await c.env.AI.run(c.env.SLOP_PROMPT_MODEL, {
            max_tokens: 256,
            messages: [
                { role: "system", content: _systemPrompt },
                { role: "user", content: _prompt },
            ],
            seed: ~~(Math.random() * 100000000),
            temperature: 1,
        })).response
    })).image);
});

// Content rendering endpoint
v1.get('/render/:providers?/:raw?', async (c) => {
    let _providers = c.req.param('providers'),
        _mode = {
            w: parseInt(c.req.query('w') ?? 1200),
            h: parseInt(c.req.query('h') ?? 825),
            mbh: parseInt(c.req.query('mbh') ?? 0),
            fit: c.req.query('fit') ?? undefined
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

    // If the provider is "ai-slop", return the AI slop endpoint
    if (_provider == "ai-slop") {
        if (!c?.env?.AI || !c?.env?.SLOP_IMAGE_MODEL || !c?.env?.SLOP_PROMPT_MODEL)
            return getFallbackResponse(_mode, "ai-slop");

        // Build the endpoint
        let _host = new URL(c.req.raw.url),
            cacheEverything = c.req.query('cache') == "false" ? false : true,
            endpoint = [
                _host.origin,
                "/api/v1/_internal/ai-slop/",
                c.env.SLOP_ACCESS_TOKEN,
                `?${new URLSearchParams(_mode).toString()}`
            ].join("");

        // To property transform we must make an API call to the internal AI slop endpoint
        return new Response((
            (await fetch(endpoint, {
                cf: {
                    cacheTtlByStatus: { "200-299": 1800, 404: 30, "500-599": 30 },
                    cacheEverything,
                    ...(transform(_mode, ["X-Inky-Message-2"], 'cover')?.cf || {}),
                }
            })).body
        ), {
            headers: new Headers([
                ["Content-Type", "image/jpeg"],
                ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                ["X-Image-Source", "AI Slop"],
                ['X-Inky-Message-2', "AI Generated Image"],
            ])
        });
    }

    // If the provider doesn't exist, use Lorem Picsum
    if (!allProviders[_provider])
        return getFallbackResponse(_mode, _provider);

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

                // Determine fit mode
                let _modeFit = provider.fit ?? _mode.fit ?? 'pad';

                // Fetch the image + return to the client
                return new Response((await fetch(img, {
                    headers,
                    ...transform(_mode, _headers, _modeFit),
                })).body, {
                    headers: new Headers([
                        ["Content-Type", _raw ? "text/plain" : "image/jpeg"],
                        ["X-Image-Size", `${_mode.w}x${_mode.h}`],
                        ["X-Image-Source", img.toLocaleString()],
                        ["X-Image-Provider", _provider],
                        ..._headers,
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

                // Disconnect or close the browser to free up resources
                await (c?.env?.USE_BROWSER_SESSIONS === "true"
                    ? page.disconnect()
                    : page.close()
                );

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
        return getFallbackResponse(_mode, _provider);
    }
});

// Export v1
export { v1 as default, v1 };