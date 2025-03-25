import articles from "./templates/articles.mjs";
import weather from "./templates/weather.mjs";

const providers = {
    "nytimes": {
        api: async (mode, c, headers) => {
            // Get the section
            let section = c.req.query('section') || 'world';

            // Parse the API endpoint
            let url = new URL(`https://api.nytimes.com/svc/news/v3/content/nyt/${section}.json`);

            // Set the search params
            url.searchParams.set("api-key", c.env.NYTIMES_API_KEY); // Set the API key

            // Return the JSON data from the API
            return await (await fetch(url, { headers })).json();
        },
        apiHeaders: async () => [
            ["Accept", "application/json"],
        ],
        source: async ({ results = [] }, mode) => {
            return await articles(
                results.filter(v => v.title && v.abstract && v.url && !!v.multimedia?.length).slice(0, 5),
                mode,
                'nytimes'
            );
        },
    },
    "weather": {
        async api(mode, c, headers) {
            // Parse the API endpoint
            let url = new URL(`https://weather.visualcrossing.com/VisualCrossingWebServices/rest/services/timeline/`);

            // TODO: Set the search params
            url.searchParams.set("key", c.env.WEATHER_API_KEY); // Set the API key
            url.searchParams.set("contentType", "json");
            url.searchParams.set("unitGroup", "us");
            url.searchParams.set("include", "days,alerts,current");

            // Apply all args to the URL, allows for custom args
            for (let arg of Object.entries(c.req.query())) {
                url.searchParams.set(arg[0], arg[1]);
            }

            // Append the query param "location" to the URL path, url encoded
            url.searchParams.set("location", c.req.query('location') ?? c.env.DEFAULT_WEATHER_LOCATION ?? "San Francisco, CA");

            // Return the JSON data from the API
            return await (await fetch(url, { headers })).json();
        },

        headers: async () => [
            ["X-No-Dithering", "true"], // Disable dithering for e-ink display
        ],
        source: async (data, mode, c) => {
            return await weather(
                data,
                mode,
                c
            );
        },
    },
}

// Alias for nytimes
providers.news = providers.nytimes;

export {
    providers as default,
    providers
}