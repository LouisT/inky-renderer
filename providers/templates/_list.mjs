import { html, raw } from 'hono/html';

export default async function (endpoints, mode, c) {
    return html`
<html lang="en">
    <head>
        <meta charset="UTF-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>Endpoint Listing</title>
        <link rel="stylesheet" href="/styles/retro.css" />
        <link rel="stylesheet" href="/styles/queries.css" />
    </head>
    <body>
        <div class="container">
            <div class="content">
                <div class="articles" style="--gap:0.6rem;">
                ${endpoints?.filter?.((ep) => !ep.hidden)?.map((ep) => html`
                    <span class="underline">
                        <a href="/api/v1/render/${ep.name}/raw">
                            <h3 class="fs-xs" style="margin-bottom:0.2rem;">
                                ${ep.name} ${ep.description ? ` - ${ep.description}` : ""}
                            </h3>
                        </a>
                    </span>
                `)}
                </div>
            </div>
            <footer class="sp:fs-xxs">
                Endpoint Listing
            </footer>
        </div>
    </body>
</html>
    `;
};