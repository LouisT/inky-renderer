import articles from "./templates/articles.mjs";

const providers = {
    "nytimes": {
        api: (mode, env) => {
            // Parse the API endpoint
            // TODO: Make "world" configurable
            let url = new URL("https://api.nytimes.com/svc/news/v3/content/nyt/world.json");

            // Set the search params
            url.searchParams.set("api-key", env.NYTIMES_API_KEY); // Set the API key

            // Return the API endpoint
            return url;
        },
        apiHeaders: () => [
            ["Accept", "application/json"],
        ],
        source: async ({ results = [] }, mode, env) => {
            return articles(
                results.slice(0, mode.orientation === "landscape" ? 6 : 8),
                mode,
                'https://developer.nytimes.com/files/poweredby_nytimes_200a.png', // Base64 encode image?
            );
        }
    },
}

// Alias for nytimes
providers.news = providers.nytimes;

export {
    providers as default,
    providers
}