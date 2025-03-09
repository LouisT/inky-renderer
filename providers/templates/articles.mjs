import { html, raw } from 'hono/html'
import QRCode from 'qrcode-svg';

export default function (articles, mode, footer = false) {
    return html`
        <html lang="en">
            <head>
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <style>
                    body {
                        margin: 0;
                        padding: 0;
                        display: flex;
                        justify-content: center;
                        align-items: center;
                    }
                    .inky-content {
                        color: #333;
                        width: ${mode.w}px;
                        max-width: 100vw;
                        margin: 0 auto;
                        padding: 8px;
                        height: 100vh;
                        max-height: ${mode.h}px;
                        overflow: hidden;
                        box-sizing: border-box;
                        border: 1px solid #333;
                    }
                    .content-cell {
                        display: flex;
                        flex-direction: row;
                        justify-content: space-between;
                    }
                    .title {
                        font-weight: 800;
                        font-size: 1.3em;
                    }
                    .byline {
                        font-weight: 600;
                        font-size: 1em;
                    }
                    .article-cell {
                        display: flex;
                        justify-content: space-between;
                    }
                    .image-cell {
                        margin-right: 16px;
                    }
                    .image-cell img {
                        width: 110px;
                        height: 110px;
                    }
                    .qr-cell {
                        margin: 0 5px;
                    }
                    .qr-cell svg {
                        width: 100px;
                        height: 100px;
                    }
                    .article-abstract {
                        font-weight: 700;
                        font-size: 1.1em;
                    }
                    .article-snippet {
                        font-size: 1em;
                        font-weight: 600;
                    }
                    .footer-cell {
                        position: sticky;
                        top: 100%;
                    }
                </style>
            </head>
            <body>
                <div class="inky-content">
                    ${articles.map((article) => html`
                        <div class="article-cell">
                            <div class="content-cell">
                                <div class="image-cell">
                                    <img src="${article.multimedia?.find?.(m => ["Large Thumbnail", "Standard Thumbnail", "Small Thumbnail"].includes(m.format))?.url}" alt="${article.title}">
                                </div>
                                <div>
                                    <div class="title-cell">
                                        <div class="title">${article.title}</div>
                                        <div class="byline">${article.byline}</div>
                                    </div>
                                    <div class="article-abstract">${article.abstract}</div>
                                    <span class="article-snippet">${article.snippet}</span>
                                </div>
                            </div>
                            <div class="qr-cell">
                                ${html(qr(article.url))}
                            </div>
                        </div>
                        <hr />
                    `)}
                    ${footer && html`
                        <div class="footer-cell">
                            <img src="${footer}">
                        </div>
                    `}
                </div>
            </body>
        </html>
    `;
};

function qr(content) {
    return new QRCode({ content, padding: 0, width: 100, height: 100, color: "#000000", background: "#ffffff", ecl: "M" }).svg();
}