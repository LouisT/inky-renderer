#include <Inkplate.h>
#include <esp_err.h>
#include <ArduinoJson.h>
#include <map>

#include "time_utils.h"
#include "logger.h"
#include "time.h"
#include "sys/time.h"
#include "urlparser.h"

// Global variable to track if NTP sync was successful
bool TimeSet = false;

// Function to get the local time as a string (e.g., "2025-01-01 12:00:00 AM")
String getLocalTimestamp(time_t epochFallback)
{
    time_t now = time(NULL);
    return fmtEpoch(now > 0 ? now : epochFallback);
}

// Function to format a Unix epoch timestamp as a string
String fmtEpoch(time_t epoch)
{
    struct tm timeInfo;
    localtime_r(&epoch, &timeInfo);

    char buffer[30];
    if (!strftime(buffer, sizeof(buffer), "%Y-%m-%d %I:%M:%S %p", &timeInfo))
    {
        return "";
    }
    return String(buffer);
}

// Helper function to parse time strings (10:30pm, 7:30am, 22:00, etc.) into
// hours and minutes
ParsedTime parseTime(const String &timeStrOriginal)
{
    ParsedTime result = {0, 0, false};

    // Work on a local copy we can modify.
    String timeStr = timeStrOriginal;
    timeStr.trim();
    timeStr.toLowerCase(); // now case insensitive

    // Check if the string contains "am" or "pm".
    int amIndex = timeStr.indexOf("am");
    int pmIndex = timeStr.indexOf("pm");

    // Determine if input is in 12-hour format.
    bool is12HourFormat = (amIndex != -1 || pmIndex != -1);
    bool isAM = false;

    // If it's 12-hour format, remove the period indicator.
    if (is12HourFormat)
    {
        if (amIndex != -1)
        {
            isAM = true;
            timeStr.remove(amIndex, 2); // remove "am"
        }
        else
        {
            isAM = false;
            timeStr.remove(pmIndex, 2); // remove "pm"
        }
        timeStr.trim(); // remove any extra spaces after removal
    }

    // Find the colon to split hour and minute.
    int colonIndex = timeStr.indexOf(':');
    if (colonIndex == -1)
    {
        // Invalid format if ':' is missing.
        return result;
    }

    // Extract hour and minute substrings.
    String hourStr = timeStr.substring(0, colonIndex);
    String minuteStr = timeStr.substring(colonIndex + 1);

    // Convert to integers.
    int parsedHour = hourStr.toInt();
    int parsedMinute = minuteStr.toInt();

    if (is12HourFormat)
    {
        // Validate hour in [1..12] and minute in [0..59] for 12-hour format.
        if (parsedHour < 1 || parsedHour > 12 || parsedMinute < 0 || parsedMinute > 59)
        {
            return result;
        }

        // Convert to 24-hour format.
        // 12:xx am -> 00:xx; 1-11 am remain unchanged.
        // 12:xx pm -> 12:xx; 1-11 pm -> add 12.
        if (isAM)
        {
            if (parsedHour == 12)
            {
                parsedHour = 0;
            }
        }
        else
        {
            if (parsedHour != 12)
            {
                parsedHour += 12;
            }
        }
    }
    else
    {
        // Validate for 24-hour clock: hour in [0..23] and minute in [0..59].
        if (parsedHour < 0 || parsedHour > 23 || parsedMinute < 0 || parsedMinute > 59)
        {
            return result;
        }
    }

    // Populate the result structure.
    result.hour = parsedHour;
    result.minute = parsedMinute;
    result.valid = true;

    return result;
}

// Returns the epoch time of the next top of the hour
time_t getNextTopOfHour(time_t now)
{
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    // If it's 10:00 exactly, we want 11:00, not 10:00 again
    timeinfo.tm_min = 0;
    timeinfo.tm_sec = 0;
    time_t nextHour = mktime(&timeinfo);
    if (nextHour <= now)
    {
        // Bump by one hour
        nextHour += 3600;
    }
    return nextHour;
}

// Retrieves the epoch time of the earliest scheduled wake that is strictlyafter 'now'. Returns (time_t)(-1) if none found.
WakeEntry getNextScheduledWake(time_t now, const std::map<String, String> &wakes)
{
    // Initialize "best" as an invalid entry:
    //   - epoch = -1 indicates "no valid schedule found"
    //   - endpoint = "" is empty
    WakeEntry best;
    best.epoch = (time_t)(-1);
    best.endpoint = "";

    // Break down “now” to local time
    struct tm currentTm;
    localtime_r(&now, &currentTm);

    for (auto &entry : wakes)
    {
        // Parse the time
        ParsedTime pt = parseTime(entry.first);
        if (!pt.valid)
        {
            // Skip invalid
            continue;
        }

        // Create a candidate time "today at pt.hour:pt.minute:00"
        struct tm schedTm = currentTm;
        schedTm.tm_hour = pt.hour;
        schedTm.tm_min = pt.minute;
        schedTm.tm_sec = 0;
        time_t candidate = mktime(&schedTm);

        // If not strictly in the future, add 24h
        if (candidate <= now)
        {
            candidate += 24 * 3600;
        }

        // Compare with best so far
        if (best.epoch == (time_t)(-1) || candidate < best.epoch)
        {
            best.epoch = candidate;
            best.endpoint = entry.second;
        }
    }

    return best;
}

// Calculates the next wake time based on the sleep window and wake schedule
WakeEntry calculateNextWake(
    time_t currentEpoch,
    const String &sleepStartStr,
    const String &sleepStopStr,
    const std::map<String, String> &wakes,
    const String &defaultEndpoint)
{
    WakeEntry result;

    // Parse sleep window boundaries.
    ParsedTime sleepStart = parseTime(sleepStartStr);
    ParsedTime sleepStop = parseTime(sleepStopStr);
    if (!sleepStart.valid || !sleepStop.valid)
    {
        // If sleep window times are invalid, fall back to next top of hour.
        result.epoch = getNextTopOfHour(currentEpoch);
        result.endpoint = defaultEndpoint;
        return result;
    }

    // Convert sleep boundaries to minutes since midnight.
    int startMinutes = sleepStart.hour * 60 + sleepStart.minute;
    int stopMinutes = sleepStop.hour * 60 + sleepStop.minute;
    bool crossesMidnight = (startMinutes > stopMinutes);

    // Convert current time to local time.
    struct tm currentTm;
    localtime_r(&currentEpoch, &currentTm);
    int currentMinutes = currentTm.tm_hour * 60 + currentTm.tm_min;

    // If we are already in the sleep window, schedule wake-up at sleepStop.
    bool currentlySleeping = false;
    if (!crossesMidnight)
    {
        currentlySleeping = (currentMinutes >= startMinutes && currentMinutes < stopMinutes);
    }
    else
    {
        // For windows crossing midnight (e.g. 10:30pm to 7:30am)
        currentlySleeping = (currentMinutes >= startMinutes || currentMinutes < stopMinutes);
    }
    if (currentlySleeping)
    {
        // Build a time structure for today's sleepStop.
        struct tm wakeTm = currentTm;
        wakeTm.tm_hour = sleepStop.hour;
        wakeTm.tm_min = sleepStop.minute;
        wakeTm.tm_sec = 0;
        time_t sleepStopEpoch = mktime(&wakeTm);
        // If the computed sleepStop has already passed relative to currentEpoch, add 1 day.
        if (sleepStopEpoch <= currentEpoch)
        {
            wakeTm.tm_mday += 1;
            sleepStopEpoch = mktime(&wakeTm);
        }
        result.epoch = sleepStopEpoch;
        result.endpoint = defaultEndpoint;
        return result;
    }

    // Not currently sleeping; choose between the next scheduled wake and next top-of-hour.
    time_t nextHour = getNextTopOfHour(currentEpoch);
    WakeEntry scheduledWake = getNextScheduledWake(currentEpoch, wakes);

    time_t candidateEpoch;
    String candidateEndpoint;
    if (scheduledWake.epoch == (time_t)(-1))
    {
        candidateEpoch = nextHour;
        candidateEndpoint = defaultEndpoint;
    }
    else
    {
        // Pick the earlier wake time.
        if (scheduledWake.epoch < nextHour)
        {
            candidateEpoch = scheduledWake.epoch;
            candidateEndpoint = scheduledWake.endpoint;
        }
        else
        {
            candidateEpoch = nextHour;
            candidateEndpoint = defaultEndpoint;
        }
    }

    // Now check if the candidate falls within the sleep window.
    struct tm candidateTm;
    localtime_r(&candidateEpoch, &candidateTm);
    int candidateMinutes = candidateTm.tm_hour * 60 + candidateTm.tm_min;

    bool candidateInSleepWindow = false;
    if (!crossesMidnight)
    {
        candidateInSleepWindow = (candidateMinutes >= startMinutes && candidateMinutes < stopMinutes);
    }
    else
    {
        candidateInSleepWindow = (candidateMinutes >= startMinutes || candidateMinutes < stopMinutes);
    }

    if (candidateInSleepWindow)
    {
        // Adjust candidate to the sleepStop time.
        candidateTm.tm_hour = sleepStop.hour;
        candidateTm.tm_min = sleepStop.minute;
        candidateTm.tm_sec = 0;
        time_t adjustedEpoch = mktime(&candidateTm);
        if (adjustedEpoch <= candidateEpoch)
        {
            // If the sleepStop for the candidate day has already passed, add 1 day.
            candidateTm.tm_mday += 1;
            adjustedEpoch = mktime(&candidateTm);
        }
        candidateEpoch = adjustedEpoch;
        candidateEndpoint = defaultEndpoint;
    }

    result.epoch = candidateEpoch;
    result.endpoint = candidateEndpoint;
    return result;
}

// NTP sync function with timezone and retries
esp_err_t NTPSync(Inkplate &display, const JsonVariant &ntpConfig)
{
    if (!ntpConfig.is<JsonObject>())
    {
        Logger::logf(Logger::LOG_ERROR, "Invalid NTP config; expected JsonObject");
        return ESP_ERR_INVALID_ARG;
    }

    JsonObject ntpSettings = ntpConfig.as<JsonObject>();
    const char *server1 = ntpSettings["server1"] | "time.cloudflare.com";
    const char *server2 = ntpSettings["server2"] | "pool.ntp.org";
    const char *timezone = ntpSettings["timezone"] | "America/Los_Angeles";
    int retries = ntpSettings["retries"].as<int>() | 3;
    // User-specified offsets (in seconds)
    int gmtOffset = ntpSettings["gmtoffset"].as<int>() | 0;
    int daylightOffset = ntpSettings["daylightoffset"].as<int>() | 0;

    // Optionally update offsets from a timezone database API
    JsonObject timezonedb = ntpSettings["timezonedb"];
    if (timezonedb["enabled"].as<bool>() && timezonedb["api"] && timezonedb["key"])
    {
        const char *api = timezonedb["api"] | "https://api.timezonedb.com/v2.1/get-time-zone";

        URLParser::Parser parsed(api);
        parsed.setParam("format", "json");
        parsed.setParam("fields", "gmtOffset,dst");
        parsed.setParam("by", "zone");
        parsed.setParam("zone", timezone);
        parsed.setParam("key", timezonedb["key"] | "");

        Logger::logf(Logger::LOG_DEBUG, "Timezone request: %s", parsed.getURL().c_str());

        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient https;
        https.getStream().setNoDelay(true);
        https.getStream().setTimeout(1000);
        https.begin(client, parsed.getURL());

        int code = https.GET();
        if (code == HTTP_CODE_OK)
        {
            JsonDocument tzdata;
            deserializeJson(tzdata, https.getString());
            gmtOffset = tzdata["gmtOffset"].as<int>();
            if (tzdata["dst"].as<int>() == 1)
            {
                daylightOffset = 0;
            }
            else
            {
                daylightOffset = 3600;
            }
        }
        else
        {
            Logger::logf(Logger::LOG_ERROR, "Failed to get timezone data: %d", code);
        }
        https.end();
    }

    Logger::logf(Logger::LOG_INFO, "NTP Servers: %s, %s / Timezone: %s (GMT Offset: %d sec, Daylight Offset: %d sec) / Retries: %d",
                 server1, server2, timezone, gmtOffset, daylightOffset, retries);
    configTime(gmtOffset, daylightOffset, server1, server2);

    int attempts = 0;
    display.rtcReset();

    while (attempts++ < retries && !TimeSet)
    {
        Logger::logf(Logger::LOG_DEBUG, "Time sync attempt #%d...", attempts);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo))
        {
            Logger::logf(Logger::LOG_DEBUG, "Time sync successful!");
            delay(100);
            // mktime() converts the local time (which includes DST if active) into epoch time (UTC)
            int epoch = mktime(&timeinfo);
            display.rtcSetEpoch(epoch);
            if (!display.rtcIsSet())
            {
                Logger::logf(Logger::LOG_ERROR, "Failed to set RTC!");
            }
            else
            {
                TimeSet = true;
                Logger::logf(Logger::LOG_INFO, "Sync: %s, epoch=%u", fmtEpoch(epoch).c_str(), epoch);
            }
        }
        delay(1000);
    }

    return TimeSet ? ESP_OK : ESP_ERR_TIMEOUT;
}
