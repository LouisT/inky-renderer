# Inky Renderer
Firmware + remote rendering service for Inkplate devices via Cloudflare Workers.

### This is a WIP!

### Image Services (/api/v1/image)
1) [Unsplash](https://unsplash.com/developers)  (/unsplash) - Inkplates: 10, 6COLOR
2) [Wallhaven](https://wallhaven.cc/help/api) (/wallhaven) - Inkplates: 10, 6COLOR
    * Can sometimes contain NSFW content.
    * TODO: Add `purity`, `categories` and `q` (possibly others?) as query params.
3) [NASA APOD](https://api.nasa.gov/) (/nasa) - Inkplates: 10, 6COLOR
4) [xkcd](https://xkcd.com/) (/xkcd) - Inkplates: 10, 6COLOR (can be hard to read)

### Render Services (/api/v1/render)
1) [NY Times](https://developer.nytimes.com/) (/news, /nytimes) - Inkplates: 10
    * Supports custom section query: `/api/v1/render/news?section=us` (default: world)
    * See [docs](https://developer.nytimes.com/docs/top-stories-product/1/routes/%7Bsection%7D.json/get) for more information.

### Supported Devices
1) [Inkplate 10](https://soldered.com/product/inkplate-10-9-7-e-paper-board-copy/)
2) [Inkplate 6COLOR](https://soldered.com/product/inkplate-6color-e-paper-display/)
    * API needs reworked to further support 6COLOR screen size.

### Setup
1) Clone the repo.
2) Copy config to data directory.
    * Inkplate 10: Copy `config.example.json` to `./data/config.json`.
    * Inkplate 6COLOR: Copy `config_color.example.json` to `./data/config_color.json`.
3) Coming Soon!?
4) ....
5) Hang on wall.



![NY Times example](https://cdn.lou.ist/Inky/nytimes-resized.jpeg)