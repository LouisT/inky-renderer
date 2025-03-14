
#define FS_NO_GLOBALS
#include <FS.h>
#ifdef FILE_READ
#undef FILE_READ
#endif
#ifdef FILE_WRITE
#undef FILE_WRITE
#endif

#include <Inkplate.h>
#include <ArduinoJson.h>
#include <esp_sleep.h>
#include <LittleFS.h>
#include <map>

#include "logger.h"
#include "networking.h"
#include "battery.h"
#include "time_utils.h"
#include "fonts/FreeSansBoldOblique24pt7b.h"

// Variables for build information
#ifndef CONFIG_FILE_PATH
#define CONFIG_FILE_PATH "/config.json"
#endif
#ifndef INKY_RENDERER_VERSION
#define INKY_RENDERER_VERSION "0.0.1-beta.1"
#endif
#ifndef ROTATION
#define ROTATION 0
#endif
#ifndef BUILD_TYPE
#define BUILD_TYPE "debug"
#endif

// Constants
#define uS_TO_S_FACTOR 1000000UL

Inkplate display(INKPLATE_3BIT);

// Allocate the JSON document
JsonDocument config;

// Misc settings and flags.
int deepSleepTime = 3600; // (in seconds)
bool showBattery = false;
const char *deepSleepStopTime = "10:30pm";
const char *deepSleepStartTime = "7:30am";

// Use an RTC variable to see if initial boot has been done
RTC_DATA_ATTR bool hideSplashScreen = false;
RTC_DATA_ATTR char nextWakeTime[10] = {0};

// Draw battery percentage + render screen
void draw(const bool render = true, int rotation = display.Adafruit_GFX::getRotation())
{
    double batteryVoltage = display.readBattery();
    int batteryPercent = getBatteryPercentage(batteryVoltage);

    if (batteryPercent <= 10 || !hideSplashScreen || showBattery)
    {
        Logger::onScreen(Logger::LOG_INFO, false, 0, rotation, "Battery: %.2fv (%d%%)", batteryVoltage, batteryPercent);
    }

    if (render)
    {
        display.display();
    }
}

// Enter deep sleep mode
void deepSleep(const bool render = true, const JsonVariant &jsonRenderer = config["renderer"])
{
    if (render)
        draw(true);

    Logger::log(Logger::LOG_DEBUG, "Preparing to deep sleep...");
    delay(1000);
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_36, LOW);

    if (display.rtcIsSet() && jsonRenderer.is<JsonObject>())
    {
        JsonObject wakesObj = jsonRenderer["wakes"];
        std::map<String, String> wakesMap;
        for (JsonPair kv : wakesObj)
        {
            wakesMap[kv.key().c_str()] = kv.value().as<const char *>();
        }

        String sleepStart = jsonRenderer["sleepwindow"]["start"] | "";
        String sleepStop = jsonRenderer["sleepwindow"]["stop"] | "";
        String defaultEndpoint = jsonRenderer["default"] | "/render/unsplash,wallhaven";
        String intervalStr = jsonRenderer["wake-interval"] | "";

        WakeEntry wake = calculateNextWake(display.rtcGetEpoch(), sleepStart, sleepStop, wakesMap, defaultEndpoint, intervalStr);
        strncpy(nextWakeTime, wake.time.c_str(), sizeof(nextWakeTime) - 1);
        nextWakeTime[sizeof(nextWakeTime) - 1] = '\0';

        display.rtcSetAlarmEpoch(wake.epoch, RTC_ALARM_MATCH_DHHMMSS);

        Logger::logf(Logger::LOG_INFO, "Next RTC Wake: %s, Endpoint: %s", fmtEpoch(wake.epoch).c_str(), wake.endpoint.c_str());
        esp_sleep_enable_ext1_wakeup(GPIO_SEL_39, ESP_EXT1_WAKEUP_ALL_LOW);
    }
    else
    {
        Logger::logf(Logger::LOG_INFO, "RTC unset, sleeping %d seconds.", deepSleepTime);
        esp_sleep_enable_timer_wakeup(deepSleepTime * uS_TO_S_FACTOR);
    }

    delay(1000);
    Logger::cleanup(5000);
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);

    delay(500);
    esp_deep_sleep_start();
}

void setup()
{
    pinMode(39, INPUT_PULLUP);
    Serial.begin(115200);
    display.begin();

#if defined(RTC_OFFSET_MODE) && defined(RTC_OFFSET_VALUE)
    display.rtcSetClockOffset(RTC_OFFSET_MODE, RTC_OFFSET_VALUE);
#endif

    display.rtcGetRtcData();
    display.rtcClearAlarmFlag();
    display.setRotation(ROTATION);

    // Delay for startup
    delay(100);

    // Initialize logger
    Logger::init(Serial, display);

    // Get rotation from display instead of build flag0
    int rotation = display.Adafruit_GFX::getRotation();

    // Mount LittleFS
    if (!LittleFS.begin(true))
    {
        Logger::onScreen(Logger::LOG_CRITICAL, true, 2, rotation, "Failed to mount LittleFS!");
        deepSleep();
        return;
    }

    // Load config.json
    fs::File file = LittleFS.open(CONFIG_FILE_PATH, "r");
    if (!file || file.size() == 0)
    {
        Logger::onScreen(Logger::LOG_CRITICAL, true, 2, rotation, "Config file missing or empty!");
        deepSleep();
        return;
    }

    // Read config file
    size_t fileSize = file.size();
    std::string fileContent(fileSize, '\0');
    file.readBytes(&fileContent[0], fileSize);
    file.close();

    // Parse config file as JSON
    if (deserializeJson(config, fileContent))
    {
        Logger::onScreen(Logger::LOG_CRITICAL, true, 2, rotation, "Failed to parse config.json!");
        deepSleep();
        return;
    }

    // Enable MQTT logging queue if MQTT is enabled
    if (config["mqtt"]["enabled"] == true)
    {
        Logger::setMQTTClient(mqttClient, config["mqtt"]["topic"] | "inky-renderer");
    }

#if defined(RTC_OFFSET_MODE) && defined(RTC_OFFSET_VALUE)
    Logger::logf(Logger::LOG_INFO, "RTC offset mode: %d, value: %d", RTC_OFFSET_MODE, RTC_OFFSET_VALUE);
#endif

    // Print wakeup reason
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
    switch (wakeup_reason)
    {
    case ESP_SLEEP_WAKEUP_EXT0:
        showBattery = true;
        Logger::log(Logger::LOG_NOTICE, "wakeup caused by external signal using RTC_IO.");
        break;
    case ESP_SLEEP_WAKEUP_EXT1:
        Logger::log(Logger::LOG_NOTICE, "wakeup caused by external signal using RTC_CNTL.");
        break;
    case ESP_SLEEP_WAKEUP_TIMER:
        Logger::log(Logger::LOG_NOTICE, "wakeup caused by timer.");
        break;
    case ESP_SLEEP_WAKEUP_ULP:
        Logger::log(Logger::LOG_NOTICE, "wakeup caused by ULP program.");
        break;
    default:
        hideSplashScreen = false;
        Logger::log(Logger::LOG_NOTICE, "wakeup caused by RST pin or power button");
        break;
    }

    // Log some basic information
    Logger::logf(Logger::LOG_DEBUG, "Config file: %s (bytes=%d, version=%s)", CONFIG_FILE_PATH, fileSize, config["version"].as<String>().c_str());
    if (!hideSplashScreen)
    {
        Logger::onScreen(Logger::LOG_INFO, true, 2, rotation, "--- Inky Renderer (%s, v%s) ---", BUILD_TYPE, INKY_RENDERER_VERSION);
        draw();
        hideSplashScreen = true;
    }
    else
    {
        // Get battery voltage and percentage
        double bvolt = display.readBattery();
        int battRemaining = getBatteryPercentage(bvolt);

        // Log battery voltage and percentage
        Logger::logf(Logger::LOG_INFO, "--- Inky Renderer (%s, v%s) - Rotation: %d", BUILD_TYPE, INKY_RENDERER_VERSION, rotation);
        Logger::logf(Logger::LOG_INFO, "Battery: %sv (est: %d%%)", String(bvolt, 2), battRemaining);
    }

    // Verify API URL
    const char *api = config["api"].as<const char *>();
    if (!api || strlen(api) == 0)
    {
        Logger::onScreen(Logger::LOG_CRITICAL, true, 2, rotation, "API URL not specified!");
        deepSleep();
        return;
    }

    // Connect to WiFi
    if (WifiConnect(config["wifi"]["ssid"], config["wifi"]["pass"], config["wifi"]["retries"]) != ESP_OK)
    {
        Logger::onScreen(Logger::LOG_CRITICAL, true, 2, rotation, "WiFi connection failed!");
        deepSleep();
        return;
    }

    // Connect MQTT
    if (config["mqtt"]["enabled"] && MqttConnect(config["mqtt"]) != ESP_OK)
    {
        Logger::log(Logger::LOG_ERROR, "MQTT connection failed.");
    }
    else
    {
        Logger::log(Logger::LOG_INFO, "MQTT connected.");
    }

    // NTP synchronization
    if (config["ntp"]["enabled"])
    {
        if (NTPSync(display, api, config["ntp"]) != ESP_OK)
            Logger::log(Logger::LOG_ERROR, "NTP sync failed; using fallback timing.");
    }
    else
    {
        display.rtcReset();
        Logger::log(Logger::LOG_INFO, "NTP disabled; using hourly fallback.");
    }

    // If rendere.standby is set to true, display the loading image before pulling the image from the renderer.
    if (config["renderer"]["cleardisplay"])
    {
        const char *psb = "Please Stand By";
        display.clearDisplay();
        display.setTextWrap(false);
        if (rotation == 0 || rotation == 2)
        {

            display.fillRect(0, 0, 1200, 825, 5);
            display.fillRect(20, 303, 1160, 219, 2);
            display.setTextColor(0);
            display.setTextSize(12);
            display.setCursor(85, 390);
            display.print(psb);
            display.setTextColor(5);
            display.setCursor(67, 378);
            display.print(psb);
            display.setTextColor(7);
            display.setCursor(66, 371);
            display.print(psb);
        }
        else
        {
            display.fillRect(0, 0, 825, 1200, 5);
            display.fillRect(0, 491, 825, 219, 2);
            display.setTextColor(0);
            display.setTextSize(9);
            display.setTextWrap(false);
            display.setCursor(19, 576);
            display.print(psb);
            display.setTextColor(5);
            display.setCursor(12, 569);
            display.print(psb);
            display.setTextColor(7);
            display.setCursor(12, 565);
            display.print(psb);
        }
        draw();
    }

    // Disable showBattery, it should only display on "stand by" screen
    // we don't want to block the displayed content unless the battery is low.
    showBattery = false;

    // Determine endpoint
    const char *endpoint = (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0 && config["renderer"]["button"].as<const char *>())
                               ? config["renderer"]["button"].as<const char *>() // Render wake buton endpoint
                               : (strlen(nextWakeTime) > 0
                                      ? config["renderer"]["wakes"][nextWakeTime].as<const char *>() // Render last wake endpoint
                                      : config["renderer"]["default"].as<const char *>());           // Render default endpoint
    if (endpoint == nullptr)
    {
        delay(5000); // WARN: Don't burn out the screen!
        Logger::onScreen(Logger::LOG_CRITICAL, true, 2, rotation, "No renderer endpoint specified!");
        deepSleep();
        return;
    }

    // Fetch and render image
    if (DisplayImage(display, rotation, api, config["renderer"].as<JsonVariant>(), endpoint) != ESP_OK)
    {
        Logger::onScreen(Logger::LOG_ERROR, true, 2, rotation, "Image fetch/render failed!");
    }

    deepSleep(true, config["renderer"]);
}

void loop()
{
    // Never here, as deepsleep restarts esp32
}