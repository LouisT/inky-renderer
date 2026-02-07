# Inky Renderer
Firmware + remote rendering service for Inkplate devices via Cloudflare Workers.

### Gallery & Uploads (/api/v0)
A private image hosting system backed by **Cloudflare R2** (storage) and **D1** (metadata/tags).

**Web Gallery** (`/gallery`)
* A secure web interface to upload, view, and manage images.
* **Features**:
    * **Batch Upload**: Drag & drop multiple images at once.
    * **Tagging**: Add tags to images during upload for easy organizing.
    * **Filtering**: Filter the gallery view by specific tags.
    * **Soft Delete**: Remove images from the view (soft delete).
* **Authentication**: Protected by Basic Auth (configured via `USERS` secret).

**Image Uploader** (`/uploader`)
* A dedicated tool for processing and uploading images.
* **Features**:
    * **Client-Side Resizing**: Automatically crops and resizes images to Inkplate-native resolutions (Landscape 1200x825 or Portrait 825x1200) *in the browser* before uploading.
    * **Batch Support**: Drag & drop multiple images to queue them for processing.
    * **Tagging System**: Add tags to uploads with autocomplete support for existing tags.
    * **Session Preview**: See the converted high-contrast images immediately before upload.

**API Endpoints**
* **Random Image**: `GET /api/v0/images/random`
    * Returns a random image ID (redirects to the image).
    * Filter by tag: `?tag=nature` or `?tags=nature,space`.
* **List Images**: `GET /api/v0/images/list`
    * Returns a JSON list of recent uploads.
    * Params: `offset` (paging), `tag` (filter).
* **Get Tags**: `GET /api/v0/images/tags`
    * Returns a list of all unique tags in the system.
* **Get Image**: `GET /api/v0/images/:id`
    * Supports Cloudflare Image Resizing params: `?w=600&h=448`.

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

## Device Management & Setup

The Inky Renderer firmware includes built-in tools for managing WiFi credentials and performing OTA (Over-The-Air) updates without needing to remove the SD card or re-flash via USB.

### 1. WiFi Setup (Captive Portal)
Allows you to configure WiFi remotely without the need of flashing your `config.json`.

1.  **Trigger:** Automatic on boot if WiFi fails.
2.  **Display:** The screen will show:
    * **QR Code 1:** Scan to connect to the device's Hotspot (SSID: `Inky-Renderer`, No Password).
    * **QR Code 2:** Scan to open the configuration page (`http://192.168.4.1`).
3.  **Action:** Connect to the network, open the page, and enter your new WiFi credentials.
4.  **Result:** The device will save the settings and reboot.

### 2. Maintenance Mode (OTA Updates)
Maintenance Mode allows you to upload new firmware (`firmware.bin`) or filesystem images (`littlefs.bin`) wirelessly.

* **How to Enter:**
    1.  Press the **RESET** button.
    2.  Immediately hold down the **WAKE** button (GPIO 36).
    3.  **Watch the screen:**
        * Hold for **2 seconds**: Screen shows "Release for WiFi Setup". See `Wifi Setup` above.
        * Hold for **>5 seconds**: Screen shows "Release for Maintenance Mode".
    4.  Release the button when "Maintenance Mode" appears.
* **How to Use:**
    1.  The screen will display the device's IP address and a QR code.
    2.  Scan the QR code or visit the IP address in your browser.
    3.  Select the update type:
        * **Firmware:** For code updates (`firmware.bin`).
        * **Filesystem:** For config/asset updates (`littlefs.bin`).
    4.  Click **Upload & Flash**.

### Button Controls Reference

| Action | Duration | Description |
| :--- | :--- | :--- |
| **Normal Boot** | Click | Simply press Reset (or Wake from sleep) to run normally. |
| **WiFi Setup** | Hold ~2-5s | Forces the device into the Captive Portal to re-configure WiFi. |
| **Maintenance** | Hold >5s | Enters OTA mode for wireless updates. |

### Supported Devices
1) [Inkplate 10](https://soldered.com/product/inkplate-10-9-7-e-paper-board-copy/)
2) [Inkplate 6COLOR](https://soldered.com/product/inkplate-6color-e-paper-display/)

*Note: I don't currently own any others for testing/development; feel free to PR!*

### Setup
1) Clone the repo.
2) Copy config to data directory + edit.
    * Inkplate 10: Copy `config.example.json` to `config.json`.
    * Inkplate 6COLOR: Copy `config_6color.example.json` to `config_6color.json`.
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