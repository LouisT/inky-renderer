#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <ArduinoJson.h>
#include <map>
#include "time.h"

// Global variable to track if NTP sync was successful
extern bool TimeSet;

// Define a structure to hold parsed time values.
struct ParsedTime
{
    int hour;
    int minute;
    bool valid; // Indicates whether parsing was successful.
};

// Used when calculating sleep window + wake schedule
struct WakeEntry
{
    time_t epoch;
    String endpoint;
    String time;
};

String getLocalTimestamp(time_t epochFallback = 0);

ParsedTime parseTime(const String &timeStr);

time_t getNextTopOfHour(time_t now);

WakeEntry getNextScheduledWake(time_t now, const std::map<String, String> &wakes);
WakeEntry calculateNextWake(
    time_t currentEpoch,
    const String &sleepStartStr,
    const String &sleepStopStr,
    const std::map<String, String> &wakes,
    const String &defaultEndpoint);

esp_err_t NTPSync(Inkplate &display, const JsonVariant &ntpConfig);

String fmtEpoch(time_t epoch);

#endif