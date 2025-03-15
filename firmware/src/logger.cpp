#include <Arduino.h>
#include <stdarg.h>
#include <deque>
#include "logger.h"
#include <Inkplate.h>
#include "images/logo.h"
#include "time_utils.h"
#include "definitions.h"

// Log level names for easier debugging
static const char *levelNames[] = {
    "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};

namespace Logger
{
    // Pointers to stream, display, and MQTT client
    static Stream *stream = nullptr;
    static Inkplate *display = nullptr;
    static PubSubClient *mqttClient = nullptr;
    static String mqttTopic = "innky/logs";

    // Queue to store log messages before sending to MQTT
    static std::deque<String> logQueue;

    // Enqueues a log message to be sent via MQTT
    void enqueueLog(const String &logMessage)
    {
        if (logQueue.size() >= 15) // Limit to 15 messages due to RAM constraints
        {
            logQueue.pop_front();
        }
        logQueue.push_back(logMessage);
    }

    // Sends all queued log messages via MQTT
    void flushMQTT()
    {
        if (!mqttClient || !mqttClient->connected())
            return;

        while (!logQueue.empty())
        {
            const String &msg = logQueue.front();
            if (!mqttClient->publish(mqttTopic.c_str(), msg.c_str()))
            {
                Serial.println("[WARN] MQTT publish failed; will retry later.");
            }
            logQueue.pop_front();
        }
    }

    // Waits until all log messages are sent or timeout occurs
    void waitForFlush(unsigned long timeoutMs)
    {
        if (!mqttClient)
            return;

        unsigned long start = millis();
        while (!logQueue.empty() && millis() - start < timeoutMs)
        {
            mqttClient->loop();
            flushMQTT();
            delay(5);
        }
    }

    // Cleans up the logger by flushing and disconnecting MQTT
    void cleanup(unsigned long timeoutMs)
    {
        waitForFlush(timeoutMs);

        if (mqttClient)
            mqttClient->disconnect();

        delay(500);
    }

    // Sets the MQTT client and topic for logging
    void setMQTTClient(PubSubClient &client, const char *topic)
    {
        mqttClient = &client;
        if (topic)
            mqttTopic = topic;
    }

    // Initializes the logger with a stream and an Inkplate display
    void init(Stream &s, Inkplate &d)
    {
        stream = &s;
        display = &d;
    }

    // Logs a message to the stream and optionally MQTT
    void log(LogLevel level, const char *message)
    {
        if (!stream || level > LOG_LEVEL)
            return;

        // Format log level name
        const char *levelName = (level <= LOG_DEBUG) ? levelNames[level] : "UNKNOWN";
        char formattedLevel[10];
        snprintf(formattedLevel, sizeof(formattedLevel), "%-8s", levelName);

        // Generate timestamped log message
        String timestamp = display->rtcIsSet() ? getLocalTimestamp(display->rtcGetEpoch()) : "";
        String logEntry = timestamp.length() > 0 ? String("[") + formattedLevel + "] (" + timestamp + "): " + message
                                  : String("[") + formattedLevel + "]: " + message;

        // Print to stream
        stream->println(logEntry);

        // Send to MQTT if enabled
        if (mqttClient)
        {
            enqueueLog(logEntry);
            flushMQTT();
        }
    }

    // Logs a formatted message to the stream and MQTT
    void logf(LogLevel level, const char *format, ...)
    {
        if (!stream || level > LOG_LEVEL)
            return;

        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(level, buffer);
    }

    // Displays a log message on the Inkplate screen
    void onScreen(LogLevel level, bool clear, int pos, int rotation, const char *format, ...)
    {
        if (!display || level > LOG_LEVEL)
            return;

        char buffer[256];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        log(level, buffer);

        // Determine portrait or landscape mode
        const bool isPortrait = (rotation % 2 == 0);
        int w = isPortrait ? E_INK_WIDTH : E_INK_HEIGHT;
        int h = MSG_BOX_HEIGHT; // Height of the text box

        // Determine Y position based on pos value
        int y = (pos == 0) ? 0 : (pos == 1) ? (isPortrait ? E_INK_HEIGHT / 2 - h / 2 : E_INK_WIDTH / 2 - h / 2)
                                            : (isPortrait ? E_INK_HEIGHT - h : E_INK_WIDTH - h);

        // Clear the display and draw a logo if requested
        if (clear)
        {
            display->clearDisplay();
            int logoX = (E_INK_WIDTH - logo_w) / 2;
            int logoY = (E_INK_HEIGHT - logo_h) / 2;
            display->drawBitmap(logoX, logoY, logo_img, logo_w, logo_h, 0);
        }

        // Set font and text properties
        display->setFont();
        display->setTextColor(0, 7);
        display->setTextSize(2);
        display->fillRect(0, y, w, h, 7);
        display->setCursor(8, y + 6);
        display->print(buffer);
    }
}
