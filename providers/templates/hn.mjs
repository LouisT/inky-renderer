import { html, raw } from 'hono/html';
import images from './_images.mjs';
import { qr, epochToDateTime } from './_utils.mjs';

export default async function (data, mode, c) {
    let timezone = c?.req?.raw?.cf?.timezone ?? "America/Los_Angeles";

    return html`
<html lang="en">
    <head>
        <meta charset="UTF-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Hacker News</title>
        <link rel="stylesheet" href="/styles/retro.css" />
        <link rel="stylesheet" href="/styles/queries.css" />
    </head>
    <body>
        <a href="/" class="home-btn">Home</a>
        <div class="container">
            <div class="content">
                <div class="articles" style="--gap:0.6rem;">
                ${data?.hits?.map((post) => html`
                    <span class="underline">
                        <a href="${post.url}" target="_blank">
                            <h3 class="fs-xs" style="margin-bottom:0.2rem;">
                                ${post.title}
                            </h3>
                        </a>
                        <div class="fs-xxs" style="margin-bottom:0.2rem;">
                            ${epochToDateTime(post.created_at_i, timezone)} - ${post.author} - ${post.points} points - ${post.num_comments} comments
                            - <a href="https://news.ycombinator.com/item?id=${post.objectID}" target="_blank">#${post.objectID}</a>
                        </div>
                    </span>
                `)}
                </div>
            </div>
            <footer class="sp:fs-xxs">
                Hacker News
            </footer>
        </div>
    </body>
</html>
    `;
};