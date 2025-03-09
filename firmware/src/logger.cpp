#include <Arduino.h>
#include <stdarg.h>
#include <deque>
#include "logger.h"
#include <Inkplate.h>
#include "images/logo.h"
#include "time_utils.h"

static const char *levelNames[] = {
    "CRITICAL", "ERROR", "WARNING", "NOTICE", "INFO", "DEBUG"};

namespace Logger
{
    static Stream *stream = nullptr;
    static Inkplate *display = nullptr;
    static PubSubClient *mqttClient = nullptr;
    static String mqttTopic = "innky/logs";

    static std::deque<String> logQueue;

    void enqueueLog(const String &logMessage)
    {
        constexpr size_t MAX_LOGS = 15; // TODO: Move to config
        if (logQueue.size() >= MAX_LOGS)
        {
            logQueue.pop_front(); // More efficient than erase(begin)
        }
        logQueue.push_back(logMessage);
    }

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

    void cleanup(unsigned long timeoutMs)
    {
        waitForFlush(timeoutMs);

        if (mqttClient)
            mqttClient->disconnect();

        delay(500);
    }

    void setMQTTClient(PubSubClient &client, const char *topic)
    {
        mqttClient = &client;
        if (topic)
            mqttTopic = topic;
    }

    void init(Stream &s, Inkplate &d)
    {
        stream = &s;
        display = &d;
    }

    void log(LogLevel level, const char *message)
    {
        if (!stream || level > LOG_LEVEL)
            return;

        const char *levelName = (level <= LOG_DEBUG) ? levelNames[level] : "UNKNOWN";
        char formattedLevel[10];
        snprintf(formattedLevel, sizeof(formattedLevel), "%-8s", levelName);

        String timestamp = TimeSet ? getLocalTimestamp(time(nullptr)) : "";
        String logEntry = TimeSet ? String("[") + formattedLevel + "] (" + timestamp + "): " + message
                                  : String("[") + formattedLevel + "]: " + message;

        stream->println(logEntry);

        if (mqttClient)
        {
            enqueueLog(logEntry);
            flushMQTT();
        }
    }

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

        const bool isPortrait = (rotation % 2 == 0);
        int w = isPortrait ? E_INK_WIDTH : E_INK_HEIGHT;
        int h = 30;
        int y = (pos == 0) ? 0 : (pos == 1) ? (isPortrait ? E_INK_HEIGHT / 2 - h / 2 : E_INK_WIDTH / 2 - h / 2)
                                            : (isPortrait ? E_INK_HEIGHT - h : E_INK_WIDTH - h);

        if (clear)
        {
            display->clearDisplay();
            int logoX = (E_INK_WIDTH - logo_w) / 2;
            int logoY = (E_INK_HEIGHT - logo_h) / 2;
            display->drawBitmap(logoX, logoY, logo_img, logo_w, logo_h, 0);
        }

        display->setFont();
        display->setTextColor(0, 7);
        display->setTextSize(2);
        display->fillRect(0, y, w, h, 7);
        display->setCursor(8, y + 6);
        display->print(buffer);
    }
}
