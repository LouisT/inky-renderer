import { html, raw } from 'hono/html';
import QRCode from 'qrcode-svg';
import images from './_images.mjs';
import styles from './styles/articles.mjs';

export default async function (articles, mode, provider = false) {
    return html`
        <html lang="en">
            <head>
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <style>${raw(styles(mode))}</style>
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
                    ${provider && html`
                        <div class="footer-cell">
                            <img src="${images[provider]}" alt="${provider}">
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