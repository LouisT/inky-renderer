import { fallback, imgix, wsrv, RAWGScreenshots, pickOne } from "./utils.mjs";
import RAWG from "./libs/RAWG.mjs";

const providers = {
    "nasa": {
        description: "NASA Astronomy Picture of the Day",
        mbhOffset: 2,
        api: async (mode, { env }, headers) => {
            // Parse the API endpoint
            let url = new URL("https://api.nasa.gov/planetary/apod");

            // Set the search params
            url.searchParams.set("api_key", env.NASA_API_KEY)

            // Return the JSON data from the API
            return await (await fetch(url, { headers })).json();
        },
        apiHeaders: async () => [
            ["Accept", "application/json"],
        ],
        headers: async (data, mode) => (data?.url ? [
            ["X-Inky-Message-0", `"${data?.title ?? '???'}"`],
            ["X-Inky-Message-2", `APOD by NASA (${data?.date ?? '???'})`],
        ] : [
            ["X-Inky-Message-0", "Please check your renderer settings!"],
            ["X-Inky-Message-2", "Invalid response from NASA; using Lorem Picsum."],
        ]),
        image: async (data, mode) => {
            return new URL(data?.hdurl ?? data?.url ?? fallback(mode));
        },
    },
    "xkcd": {
        description: "xkcd Comics",
        mbhOffset: 2,
        api: async (mode, { env }, headers) => {
            // A "hack" to get a random image from xkcd
            let max = 3062;
            try {
                max = (await (await fetch("https://xkcd.com/info.0.json")).json()).num ?? 3000;
            } catch { }

            // Build the API endpoint
            let url = new URL(`https://xkcd.com/${Math.floor(Math.random() * max) + 1}/info.0.json`);

            // Return the JSON data from the API
            return await (await fetch(url, { headers })).json();
        },
        apiHeaders: async () => [
            ["Accept", "application/json"],
        ],
        headers: async (data, mode) => (data?.img ? [
            ["X-Inky-Message-0", `"${data?.title ?? '???'}" (#${data?.num ?? '???'})`],
            ["X-Inky-Message-2", data?.alt ?? '???'],
        ] : [
            ["X-Inky-Message-0", "Please check your renderer settings!"],
            ["X-Inky-Message-2", "Invalid response from xkcd; using Lorem Picsum."],
        ]),
        image: async (data, mode) => {
            return new URL(data?.img ?? fallback(mode));
        },
    },
    "unsplash": {
        description: "Unsplash random images",
        mbhOffset: 2,
        api: async (mode, { env }, headers) => {
            // Parse the API endpoint
            let url = new URL("https://api.unsplash.com/photos/random");

            // Set the search params
            url.searchParams.set("client_id", env.UNSPLASH_CLIENT_ID)
            url.searchParams.set("orientation", mode.w > mode.h ? 'landscape' : 'portrait');

            // Return the JSON data from the API
            return await (await fetch(url, { headers })).json();
        },
        apiHeaders: async () => [
            ["Accept", "application/json"],
        ],
        headers: async (data, mode) => (data?.urls?.raw ? [
            ["X-Inky-Message-0", `"${data?.alt_description ?? '???'}"`],
            ["X-Inky-Message-2", `by ${data?.user?.name ?? '???'} (@${data?.user?.username ?? '???'}) on Unsplash (${data?.likes ?? 0} likes)`],
        ] : [
            ["X-Inky-Message-0", "Please check your renderer settings!"],
            ["X-Inky-Message-2", "Invalid response from Unsplash; using Lorem Picsum."],
        ]),
        image: async (data, mode) => {
            return imgix(new URL(data?.urls?.raw ?? fallback(mode)), mode);
        }
    },
    "wallhaven": {
        description: "Wallhaven random images",
        mbhOffset: 1,
        api: async (mode, { env, req }, headers) => {
            // Parse the API endpoint
            let url = new URL("https://wallhaven.cc/api/v1/search");

            // Set the search params
            url.searchParams.set("apikey", env.WALLHAVEN_API_KEY); // Wallhaven API key
            url.searchParams.set("sorting", "random"); // Get a random image
            url.searchParams.set("categories", req.query('categories') ?? "101"); // Image categories (general, anime, people)
            url.searchParams.set("purity", req.query('purity') ?? "100"); // Image purity (sfw, sketchy, nsfw)
            url.searchParams.set("ratios", `${(mode.w / mode.h).toFixed(2)}x1`);

            // Return the JSON data from the API
            return await (await fetch(url, { headers })).json();
        },
        apiHeaders: async () => [
            ["Accept", "application/json"],
        ],
        headers: async ({ data = [] }, mode) => (data?.[0]?.path ? [
            ["X-Inky-Message-2", `${data?.[0]?.id ?? '???'} on Wallhaven (${data?.[0]?.views ?? 0} views, ${data?.[0]?.favorites ?? 0} favorites)`],
        ] : [
            ["X-Inky-Message-2", "Invalid response from Wallhaven; using Lorem Picsum."],
        ]),
        image: async ({ data = [] }, mode, { env }) => {
            return new URL(data?.[0]?.path ?? fallback(mode));
        }
    },
    "rawg": {
        description: "RAWG Video Game Screenshots",
        mbhOffset: 2,
        api: async (mode, { env, req }, headers) => {
            return await RAWG({
                apiKey: env.RAWG_API_KEY,
                gameId: req.query('gameId') ?? null, // If a specific game ID is provided, use it
                gameSlug: req.query('gameSlug') ?? null, // If a specific game slug is provided, use it
                gameSearch: req.query('gameSearch') ?? null, // If a specific game search term is provided, use it
            });
        },
        apiHeaders: async () => [
            ["Accept", "application/json"],
        ],
        headers: async (data, mode) => (data?.artworks?.length ? [
            ["X-Inky-Message-0", `"${data?.game?.name ?? '???'}" released on ${data?.game?.released ?? '???'}`],
            ["X-Inky-Message-2", `${data?.source ?? '???'} - Game #${data?.game?.id ?? '???'}`],
        ] : [
            ["X-Inky-Message-0", "Please check your renderer settings!"],
            ["X-Inky-Message-2", "Invalid response from RAWG; using Lorem Picsum."],
        ]),
        image: async (data, mode, { req }) => {
            return new URL(data?.artworks?.length ? wsrv(pickOne(data.artworks), mode) : fallback(mode));
        }
    },
    "uploaded": {
        hidden: true,
        description: "Uploaded image",
        image: async (data, mode, c) => {
            // Generate a random key for the URL to prevent caching, with the same origin
            return new URL(`/api/v0/images/${ Math.random().toString(36).substring(7) }`, new URL(c.req.url).origin);
        },
    },
}

export {
    providers as default,
    providers
}