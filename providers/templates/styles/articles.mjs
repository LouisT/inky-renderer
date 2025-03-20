export default function (mode) {
    return (`
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
`);
}