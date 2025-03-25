import { html, raw } from 'hono/html';
import QRCode from 'qrcode-svg';
import images from './_images.mjs';

export default async function (articles, mode, provider = false) {
    return html`
<html lang="en">
    <head>
        <meta charset="UTF-8" />
        <meta name="viewport" content="width=device-width, initial-scale=1.0" />
        <title>News Listing</title>
        <link rel="stylesheet" href="/styles/retro.css" />
        <link rel="stylesheet" href="/styles/queries.css" />
    </head>
    <body>
        <div class="container">
            <div class="content">
                <div class="articles">
                ${articles.map((article) => html`
                    <article>
                        <div class="article-content">
                            <h2 class="fs-xs">
                                ${article.title}
                            </h2>
                            <div class="article-snippet for-1200-825 for-825-1200" style="--display-mode:flex;">
                                <img
                                    src="${article.multimedia?.find?.(m => ["Large Thumbnail", "Standard Thumbnail", "Small Thumbnail"].includes(m.format))?.url}"
                                    class="for-1200-825 for-825-1200 pixelated"
                                />
                                <blockquote cite="${article.byline}" class="fs-xxs">
                                    ${article.snippet || article.abstract}
                                </blockquote>
                                <div class="for-1200-825 for-825-1200">
                                    ${html(qr(article.url))}
                                </div>
                            </div>
                        </div>
                    </article>
                `)}
                </div>
                ${provider && html`
                    <div class="footer-cell" style="position: sticky; top: 100%; float: right;">
                        <img src="${images[provider]}" alt="${provider}">
                    </div>
                `}
            </div>
            <footer class="sp:fs-xxs">
                ${new Date().toDateString()}
            </footer>
        </div>
    </body>
</html>
    `;
};

function qr(content) {
    return new QRCode({ content, padding: 0, width: 100, height: 100, color: "#000000", background: "#ffffff", ecl: "M" }).svg();
}