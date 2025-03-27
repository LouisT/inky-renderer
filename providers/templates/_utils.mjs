import QRCode from 'qrcode-svg';

export function epochToDate(epoch) {
    return new Date(epoch * 1000);
}

export function epochToTime(epoch = Date.now(), timeZone = "America/Los_Angeles") {
    return epochToDate(epoch).toLocaleTimeString('en-US', {
        timeZone,
    });
}

export function epochToDateTime(epoch = Date.now(), timeZone = "America/Los_Angeles") {
    return epochToDate(epoch).toLocaleString('en-US', {
        timeZone,
    });
}

export function getDay(ds) {
    return ["Sun", "Mon", "Tue", "Wen", "Thu", "Fri", "Sat"][new Date(ds).getDay()];
}

export function qr(content) {
    return new QRCode({ content, padding: 0, width: 100, height: 100, color: "#000000", background: "#ffffff", ecl: "M" }).svg();
}