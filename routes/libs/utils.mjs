// Basic non-specific utility functions
export function pickOne(arr) {
    return arr[Math.floor(Math.random() * arr.length)];
}

// Convert base65 string to PNG
export function b64png(b64) {
    return new Response(Uint8Array.from(atob(b64.replace(/^data:image\/png;base64,/, '')), char => char.charCodeAt(0)), {
        headers: { 'Content-Type': 'image/png' }
    });
}