// Maps relative URLs to absolute URLs
class URLHandler {
    constructor(base) {
        this.base = base.replace(/\/+$/, "");
        this.attrs = new Set([
            "href",
            "src",
            "action",
        ]);
    }

    element(element) {
        // Iterate over the element's attributes
        for (const attr of element.attributes) {
            // Some attributes may not have a `name` property
            if (!this.attrs.has(attr?.[0]) || !attr?.[1]?.startsWith?.('/'))
                continue;

            // Replace the attribute value with the absolute URL
            element.setAttribute(attr[0], new URL(attr[1], this.base).href);
        }
    }
}

// Patch injected HTML content with the base URL
export async function patch(html, base) {
    return await (new HTMLRewriter().on('*', new URLHandler(base)).transform(new Response(html, {
        headers: { "Content-Type": "text/html" },
    }))).text();
}