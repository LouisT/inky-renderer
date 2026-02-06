# Inky Renderer
Firmware + remote rendering service for Inkplate devices via Cloudflare Workers.

### This is a WIP!

### Gallery & Uploads (/api/v0)
A private image hosting system backed by Cloudflare R2 (storage) and D1 (metadata).
1) Web Gallery
    * Upload images via a web interface. (/uploader)
    * View images via a web interface. (/gallery)
    * Requires Basic Auth.
2) API Endpoints (See ./routes/v0.mjs for more information)
    * Random Image: `/api/v0/images/random` (Redirects to a specific image ID).
    * Specific Image: `/api/v0/images/:id`
        * Supports resizing params: `?w=600&h=448` (Uses Cloudflare Image resizing).

### Image Services (/api/v1/render)
1) [Unsplash](https://unsplash.com/developers)  (/unsplash) - Inkplates: 10, 6COLOR
2) [Wallhaven](https://wallhaven.cc/help/api) (/wallhaven) - Inkplates: 10, 6COLOR
    * Can sometimes contain NSFW content.
3) [NASA APOD](https://api.nasa.gov/) (/nasa) - Inkplates: 10, 6COLOR
4) [xkcd](https://xkcd.com/) (/xkcd) - Inkplates: 10, 6COLOR (can be hard to read)
5) [AI Slop](https://en.wikipedia.org/wiki/AI_slop) (/ai-slop) - Inkplates: 10, 6COLOR
    * Uses [Cloudflare AI](https://developers.cloudflare.com/workers-ai/) to generate a random prompt + resulting image.
6) [RAWG.io](https://rawg.io/) (/rawg) - Inkplates: 10, 6COLOR
    * Pull game screenshots/info using `?gameId=123`, `?gameSlug=game-slug`, `?gameSearch=Game Name` or defaults back to a random game.

### Render Services (/api/v1/render)
1) [NY Times](https://developer.nytimes.com/) (/news, /nytimes) - Inkplates: 10, 6COLLOR
    * Supports custom section query: `/api/v1/render/news?section=us` (default: world)
    * See [docs](https://developer.nytimes.com/docs/top-stories-product/1/routes/%7Bsection%7D.json/get) for more information.
    * 6COLOR only displays headlines (for now!?)
2) Weather from [Visual Crossing](https://www.visualcrossing.com/) (/weather) - Inkplates: 10, 6COLOR
    * 6COLOR only displays the forecast for the next 3 days while in landscape.
3) [Hacker News](https://news.ycombinator.com/) (/hn) - Inkplates: 10, 6COLOR
    * Uses the [Algolia Search API](https://hn.algolia.com/api).
    * Currently only supports the `/api/v1/search` endpoint. Example: `/render/hn?tags=story`
4) [Google Calendar](https://calendar.google.com/) (/google-calendar) - Inkplates: 10, 6COLOR
    * Must be a public calendar. See [help](https://support.google.com/calendar/answer/41207?hl=en) for more information.
    * Pass multiple `calendarId`'s to the endpoint using commas. Example: `/render/google-calendar?calendarId=123asd,456fgh`

### Supported Devices
1) [Inkplate 10](https://soldered.com/product/inkplate-10-9-7-e-paper-board-copy/)
2) [Inkplate 6COLOR](https://soldered.com/product/inkplate-6color-e-paper-display/)
    * I don't currently own any others for testing/development; feel free to PR!

### Setup
1) Clone the repo.
2) Copy config to data directory + edit.
    * Inkplate 10: Copy `config.example.json` to `./data/config.json`.
    * Inkplate 6COLOR: Copy `config_6color.example.json` to `./data/config_6color.json`.
3) Create Cloudflare Resources:
    * R2 Bucket: `npx wrangler r2 bucket create inky-images`
    * D1 Database: `npx wrangler d1 create inky-images`
4) Modify `wrangler.jsonc`:
    * Update `d1_databases` with your new Database ID from step 3.
5) Configure Secrets:
    * Copy `.secrets.example.json` to `.secrets.json`.
    * Run `npm run secrets`.
6) `npm run deploy`
7) Hang on wall.