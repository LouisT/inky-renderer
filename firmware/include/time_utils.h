#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ArduinoJson.h>
#include <map>
#include "time.h"

// Define a structure to hold parsed time values
struct ParsedTime
{
    int hour;
    int minute;
    bool valid; // Indicates whether parsing was successful
};

// Used when calculating sleep window + wake schedule
struct WakeEntry
{
    time_t epoch;
    String endpoint;
    String time;
};

// Get the current local time as a string (e.g. "2025-01-01 12:00:00 AM")
String getLocalTimestamp(time_t epochFallback = 0);

// Format a time_t value into a string (e.g. "2025-01-01 12:00:00 AM")
String fmtEpoch(time_t epoch);

// Parse a time string (e.g. "10:30am") into a ParsedTime structure
ParsedTime parseTime(const String &timeStr);

// Parse duration string into seconds, returns -1 on error
// Example: 1d2h3m4s -> 86400 + 7200 + 180 + 4 = 90164
int parseDuration(const String &durationStr);

// Return the epoch time at the *next* boundary of 'intervalStr' from the current local day.
// E.g., if interval=90 minutes, the boundaries each day are 00:00, 01:30, 03:00, 04:30, ...
// If the parsed duration is invalid, we fall back to once‐an‐hour “top of the hour.”
time_t getNextIntervalTime(time_t now, const String &intervalStr = "");

// Find the earliest scheduled wake time that is strictly after the given time
WakeEntry getNextScheduledWake(time_t now, const std::map<String, String> &wakes);

// Calculates the next wake time based on the sleep window and wake schedule
WakeEntry calculateNextWake(
    time_t currentEpoch,
    const String &sleepStartStr,
    const String &sleepStopStr,
    const std::map<String, String> &wakes,
    const String &defaultEndpoint,
    const String &intervalStr = "");

// Synchronizes the system time using NTP
esp_err_t NTPSync(Inkplate &display, const char *api, const JsonVariant &ntpConfig);

#endif