import { fallback, imgix } from "./utils.mjs";

const providers = {
    "nasa": {
        api: (mode, { env }) => {
            // Parse the API endpoint
            let url = new URL("https://api.nasa.gov/planetary/apod");

            // Set the search params
            url.searchParams.set("api_key", env.NASA_API_KEY)

            // Return the API endpoint
            return url;
        },
        apiHeaders: () => [
            ["Accept", "application/json"],
        ],
        headers: (data, mode) => (data?.url ? [
            ["X-Inky-Message-0", `"${data?.title ?? '???'}"`],
            ["X-Inky-Message-2", `APOD by NASA (${data?.date ?? '???'})`],
        ] : [
            ["X-Inky-Message-2", "Invalid response from NASA; using Lorem Picsum."],
        ]),
        image: (data, mode) => {
            return new URL(data?.hdurl ?? data?.url ?? fallback(mode));
        },
        transform: true,
    },
    "unsplash": {
        api: (mode, { env }) => {
            // Parse the API endpoint
            let url = new URL("https://api.unsplash.com/photos/random");

            // Set the search params
            url.searchParams.set("client_id", env.UNSPLASH_CLIENT_ID)
            url.searchParams.set("orientation", mode.orientation);

            // Return the API endpoint
            return url;
        },
        apiHeaders: () => [
            ["Accept", "application/json"],
        ],
        headers: (data, mode) => (data?.urls?.raw ? [
            ["X-Inky-Message-0", `"${data?.alt_description ?? '???'}"`],
            ["X-Inky-Message-2", `by ${data?.user?.name ?? '???'} (@${data?.user?.username ?? '???'}) on Unsplash (${data?.likes ?? 0} likes)`],
        ] : [
            ["X-Inky-Message-2", "Invalid response from Unsplash; using Lorem Picsum."],
        ]),
        image: (data, mode) => {
            return imgix(new URL(data?.urls?.raw ?? fallback(mode)), mode);
        }
    },
    "wallhaven": {
        api: (mode, { env }) => {
            // Parse the API endpoint
            let url = new URL("https://wallhaven.cc/api/v1/search");

            // Set the search params
            url.searchParams.set("apikey", env.WALLHAVEN_API_KEY); // Wallhaven API key
            url.searchParams.set("sorting", "random"); // Get a random image
            url.searchParams.set("categories", "101"); // Image categories (general, anime, people)
            url.searchParams.set("purity", "111"); // Image purity (sfw, sketchy, nsfw)
            url.searchParams.set("ratios", mode.orientation == 'landscape' ? '1.45x1' : '0.69x1');

            // Return the API endpoint
            return url;
        },
        apiHeaders: () => [
            ["Accept", "application/json"],
        ],
        headers: ({ data = [] }, mode) => (data?.[0]?.path ? [
            ["X-Inky-Message-2", `${data?.[0]?.id ?? '???'} on Wallhaven (${data?.[0]?.views ?? 0} views, ${data?.[0]?.favorites ?? 0} favorites)`],
        ] : [
            ["X-Inky-Message-2", "Invalid response from Wallhaven; using Lorem Picsum."],
        ]),
        image: ({ data = [] }, mode, { env }) => {
            let imgurl = data?.[0]?.path ?? fallback(mode);
            if (env.WALLHAVEN_IMGIX_URL) {
                imgurl = imgurl.replace("https://w.wallhaven.cc/full", env.WALLHAVEN_IMGIX_URL);
            }
            return imgix(new URL(imgurl), mode);
        }
    },
}

export {
    providers as default,
    providers
}