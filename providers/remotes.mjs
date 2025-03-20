import articles from "./templates/articles.mjs";

const providers = {
    "example": {
        link: async (mode, c) => {
            // Return the website url
            return new URL(`https://example.com/`);
        },
    },
    "example-target": {
        link: async (mode, c) => {
            // Return the website url
            return new URL(`https://example.com/`);
        },
        // Selector for the target element
        target: 'body div'
    },
}

export {
    providers as default,
    providers
}