// Fallback image
export function fallback(mode) {
    return `https://picsum.photos/${mode.w}/${mode.h}/?blur=5&grayscale`;
}

// Get a fallback response
export async function getFallbackResponse(_mode, _provider) {
    let src = new URL(fallback(_mode));
    return new Response((await fetch(src)).body, {
        ...transform(_mode),
        headers: new Headers([
            ["Content-Type", "image/jpeg"],
            ["X-Image-Size", `${_mode.w}x${_mode.h}`],
            ["X-Image-Source", src],
            ["X-Image-Provider", "Lorem Picsum"],
            ['X-Invalid-Provider', _provider],
        ])
    });
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
export function transform(mode, _headers = [], fit = "pad") {
    return {
        cf: {
            image: {
                format: "baseline-jpeg",
                fit,
                background: "#FFF", // Default to white for cleaner inkplate messages
                width: mode.w,
                height: mode.h,
                ...(mode.mbh > 0 ? {
                    border: {
                        color: "#FFF", // Default to white for cleaner inkplate messages
                        top: _headers.some((h) => h.includes("X-Inky-Message-0")) ? mode.mbh : 0,
                        bottom: _headers.some((h) => h.includes("X-Inky-Message-2")) ? mode.mbh : 0
                    }
                } : {})
            }
        }
    }
}

// Convert base65 string to PNG
export function b64png(b64) {
    return new Response(Uint8Array.from(atob(b64.replace(/^data:image\/png;base64,/, '')), char => char.charCodeAt(0)), {
        headers: { 'Content-Type': 'image/png' }
    });
}


// Convert a Response to a ReadableStream
export function responseToReadableStream(response) {
    // If there's already a ReadableStream, you can simply return `response.body`:
    if (!response.body) {
        throw new Error("The response has no body or the body was already consumed.");
    }

    return new ReadableStream({
        async start(controller) {
            const reader = response.body.getReader();

            // Continuously pump from the reader until there is no more data
            async function pump() {
                const { done, value } = await reader.read();
                if (done) {
                    // Signal that we're done reading
                    controller.close();
                    return;
                }
                // Enqueue the current chunk and read the next
                controller.enqueue(value);
                return pump();
            }

            // Start reading
            return pump();
        }
    });
}

// A basic function to pick a random element from an array
export function pickOne(arr = []) {
    return arr[Math.floor(Math.random() * arr.length)];
}
