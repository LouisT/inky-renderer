// TODO: Misc utils for providers

// Fallback image
export function fallback(mode) {
    return `https://picsum.photos/${mode.w}/${mode.h}/?blur=5&grayscale`;
}

// Apply imgix args to the image
export function imgix(img, mode) {
    img.searchParams.set("w", mode.w);
    img.searchParams.set("h", mode.h);
    img.searchParams.set("fit", "fillmax");
    img.searchParams.set("fill", "blur");
    img.searchParams.set("format", "jpg");
    img.searchParams.set("jpeg-progressive", "false");

    // Return the image URL
    return img;
}

// Apply Cloudflare args to the image
export function transform(mode) {
    return {
        cf: {
            image: {
                format: "baseline-jpeg",
                fit: "pad",
                background: "#808080",
                width: mode.w,
                height: mode.h,
            }
        }
    }
}

