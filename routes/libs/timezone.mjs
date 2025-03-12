// Get the timezone offset in seconds
export function getOffsetInSeconds(tz, date = new Date()) {
    const utcd = new Date(date.toLocaleString('en-US', { timeZone: 'UTC', hour12: false }));
    const tzd = new Date(date.toLocaleString('en-US', { timeZone: tz, hour12: false }));
    return (tzd - utcd) / 1000;
}

// Get the timezone info
export function getTimeZoneInfo(tz) {
    let result = {
        tz: 'UTC',
        gmtOffset: 0,
        dst: 0
    };

    if (!tz) // Default to UTC, no timezone specified, mark as defaulted
        return { ...result, defaulted: true };

    let gmtOffset = 0, year, januaryOffset, julyOffset, error
    try {
        gmtOffset = getOffsetInSeconds(tz) || 0;
        year = new Date().getFullYear();
        januaryOffset = getOffsetInSeconds(tz, new Date(year, 0, 1));  // Jan 1
        julyOffset = getOffsetInSeconds(tz, new Date(year, 6, 1));  // Jul 1
    } catch (e) {
        result.error = e.message || "Invalid timezone";
    }

    return {
        ...result,
        tz,
        gmtOffset,
        dst: (gmtOffset !== (
            januaryOffset === julyOffset ? gmtOffset : (
                (januaryOffset < julyOffset) ? januaryOffset : julyOffset
            )
        )) ? 1 : 0
    };
}
