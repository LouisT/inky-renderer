# Inky Renderer
Firmware + remote rendering service for Inkplate devices via Cloudflare Workers.

### This is a WIP!

### Image Services (/api/v1/render)
1) [Unsplash](https://unsplash.com/developers)  (/unsplash) - Inkplates: 10, 6COLOR
2) [Wallhaven](https://wallhaven.cc/help/api) (/wallhaven) - Inkplates: 10, 6COLOR
    * Can sometimes contain NSFW content.
3) [NASA APOD](https://api.nasa.gov/) (/nasa) - Inkplates: 10, 6COLOR
4) [xkcd](https://xkcd.com/) (/xkcd) - Inkplates: 10, 6COLOR (can be hard to read)
5) [AI Slop](https://en.wikipedia.org/wiki/AI_slop) (/ai-slop) - Inkplates: 10, 6COLOR
    * Uses [Cloudflare AI](https://developers.cloudflare.com/workers-ai/) to generate a random prompt + resulting image.

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

### Supported Devices
1) [Inkplate 10](https://soldered.com/product/inkplate-10-9-7-e-paper-board-copy/)
2) [Inkplate 6COLOR](https://soldered.com/product/inkplate-6color-e-paper-display/)
    * I don't currently own any others for testing/development; feel free to PR!

### Setup
1) Clone the repo.
2) Copy config to data directory.
    * Inkplate 10: Copy `config.example.json` to `./data/config.json`.
    * Inkplate 6COLOR: Copy `config_color.example.json` to `./data/config_color.json`.
4) ....
5) Hang on wall.

***This example is outdated; will update eventually!***
![NY Times example](https://cdn.lou.ist/Inky/nytimes-resized.jpeg)