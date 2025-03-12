#ifndef LOGGER_H
#define LOGGER_H

#include <Inkplate.h>
#include <PubSubClient.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#endif

namespace Logger
{
    // Available log levels. Order is important here; higher levels are more serious
    enum LogLevel
    {
        LOG_CRITICAL = 0, // Critical errors that require immediate attention
        LOG_ERROR,        // Errors that prevent the firmware from working as expected
        LOG_WARNING,      // Warnings about unusual behavior
        LOG_NOTICE,       // Informational messages that are not errors
        LOG_INFO,         // Informational messages that are not errors
        LOG_DEBUG         // Debug messages for developers
    };

    // Initialize the logger by specifying the Stream to write logs to and the
    // Inkplate display used for showing messages on the screen
    void init(Stream &s, Inkplate &d);

    // MQTT-specific APIs
    void setMQTTClient(PubSubClient &client, const char *topic);
    void flushMQTT();

    // Wait until the queue is flushed or timeout
    void waitForFlush(unsigned long timeoutMs);

    // Cleanup: flush logs and disconnect MQTT
    void cleanup(unsigned long timeoutMs = 5000);

    // Logs a message with a specified log level
    void log(LogLevel level, const char *message);

    // Logs a formatted string with a specified log level
    void logf(LogLevel level, const char *format, ...);

    // onScreen shows a positional message on the display (top or bottom)
    void onScreen(LogLevel level, bool clear, int pos, int rotation, const char *format, ...);
}

#endif
