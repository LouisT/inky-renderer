#ifndef LOGGER_H
#define LOGGER_H

#include <Inkplate.h>
#include <PubSubClient.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_DEBUG
#endif

namespace Logger
{
    enum LogLevel
    {
        LOG_CRITICAL = 0,
        LOG_ERROR,
        LOG_WARNING,
        LOG_NOTICE,
        LOG_INFO,
        LOG_DEBUG
    };

    void init(Stream &s, Inkplate &d);

    // MQTT-specific APIs
    void setMQTTClient(PubSubClient &client, const char *topic);
    void flushMQTT();

    // Wait until the queue is flushed or timeout
    void waitForFlush(unsigned long timeoutMs);

    // Cleanup: flush logs and disconnect MQTT
    void cleanup(unsigned long timeoutMs = 5000);

    void log(LogLevel level, const char *message);
    void logf(LogLevel level, const char *format, ...);

    // onScreen shows a positional message on the display (top or bottom)
    void onScreen(LogLevel level, bool clear, int pos, int rotation, const char *format, ...);
}

#endif
