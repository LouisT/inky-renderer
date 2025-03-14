import articles from "./templates/articles.mjs";

const providers = {
    "nytimes": {
        api: (mode, c) => {
            // Get the section
            let section = c.req.query('section') || 'world';

            // Parse the API endpoint
            let url = new URL(`https://api.nytimes.com/svc/news/v3/content/nyt/${section}.json`);

            // Set the search params
            url.searchParams.set("api-key", c.env.NYTIMES_API_KEY); // Set the API key

            // Return the API endpoint
            return url;
        },
        apiHeaders: () => [
            ["Accept", "application/json"],
        ],
        source: async ({ results = [] }, mode) => {
            return articles(
                results.slice(0, mode.w > mode.h ? 6 : 8),
                mode,
                'nytimes'
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