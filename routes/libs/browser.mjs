import puppeteer from "@cloudflare/puppeteer";

// Get a random Browser Rendering session
async function getBrowserSession(endpoint) {
    return ((await puppeteer.sessions(endpoint))
        ?.filter?.((v) => !v.connectionId)
        ?.map?.((v) => v.sessionId) ?? [])
        ?.sort?.(() => .5 - Math.random())?.[0] ?? null;
}

export {
    getBrowserSession as default,
    getBrowserSession
}