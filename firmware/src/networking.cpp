#include <Inkplate.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_err.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#include "networking.h"
#include "logger.h"
#include "urlparser.h"

#ifndef INKY_RENDERER_VERSION
#define INKY_RENDERER_VERSION "0.0.1-beta.1"
#endif
#ifndef USER_AGENT
#define USER_AGENT "Inky Renderer/v" INKY_RENDERER_VERSION
#endif

#ifdef MQTT_MAX_PACKET_SIZE
#undef MQTT_MAX_PACKET_SIZE
#endif
#define MQTT_MAX_PACKET_SIZE 1024

const char *displayHeaders[] = {
    "Content-Type",     // Must be image/jpeg
    "Content-Length",   // Use this to determine size
    "X-Image-Source",   // URL of the image, for logging
    "X-Inky-Message-0", // Fow showing a message on the display (top)
    "X-Inky-Message-1", // Fow showing a message on the display (middle)
    "X-Inky-Message-2", // Fow showing a message on the display (bottom)
};

// Create both a non-secure and a secure Wi-Fi client
WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;

// Create a PubSubClient without immediately binding to a client
PubSubClient mqttClient;

esp_err_t WifiConnect(const char *ssid, const char *pass, int retries)
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    Logger::logf(Logger::LOG_INFO, "Connecting to WiFi (SSID: %s)...", ssid);

    int attempts = 0;
    while (attempts++ < retries && WiFi.status() != WL_CONNECTED)
    {
        Logger::logf(Logger::LOG_DEBUG, "Connection attempt #%d...", attempts);
        delay(1000);
    }

    // If still not connected, error with timeout
    if (WiFi.status() != WL_CONNECTED)
    {
        return ESP_ERR_TIMEOUT;
    }

    // Print the IP address
    Logger::logf(Logger::LOG_INFO, "IP address: %s", WiFi.localIP().toString());

    return ESP_OK;
}

// Connect to the MQTT broker
esp_err_t MqttConnect(const JsonVariant &mqttConfig)
{
    if (!mqttConfig.is<JsonObject>())
    {
        Logger::log(Logger::LOG_ERROR, "Invalid MQTT config; expected JsonObject");
        return ESP_ERR_INVALID_ARG;
    }

    const char *server = mqttConfig["server"] | ""; // TODO: Add echo server?
    const int port = mqttConfig["port"] | 1883;
    const char *user = mqttConfig["user"] | "";
    const char *pass = mqttConfig["pass"] | "";
    int retries = mqttConfig["retries"] | 3;
    const char *deviceId = mqttConfig["device"] | "inky";
    int maxtx = mqttConfig["maxtx"] | MQTT_MAX_PACKET_SIZE;
    int maxrx = mqttConfig["maxrx"] | MQTT_MAX_PACKET_SIZE;

    // Decide whether or not to use TLS
    bool useTLS = mqttConfig["tls"] | false;

    Logger::logf(Logger::LOG_INFO,
                 "MQTT: %s:%d (maxrx=%d, maxtx=%d), useTLS=%s",
                 server, port, maxrx, maxtx, useTLS ? "true" : "false");

    // Select which client to use
    if (useTLS)
    {
        // The following line configures the TLS client to accept all certs
        // (i.e., no verification). This may or may not be what you want
        wifiClientSecure.setInsecure();
        mqttClient.setClient(wifiClientSecure);
    }
    else
    {
        mqttClient.setClient(wifiClient);
    }

    mqttClient.setServer(server, port);
    mqttClient.setBufferSize(maxrx, maxtx);

    int attempts = 0;
    while (attempts++ < retries && !mqttClient.connected())
    {
        Logger::logf(Logger::LOG_DEBUG, "MQTT connection attempt #%d/%d...", attempts, retries);
        if (user && pass)
        {
            mqttClient.connect(deviceId, user, pass);
        }
        else
        {
            mqttClient.connect(deviceId);
        }
        delay(500); // short retry delay
    }

    // If still not connected, error with timeout
    if (!mqttClient.connected())
    {
        return ESP_ERR_TIMEOUT;
    }

    // FOO BAR BAZ
    return ESP_OK;
}

// Fetch a remote jpeg image and display it on the screen
esp_err_t DisplayImage(Inkplate &display, int rotation, const char *api, const JsonVariant &imageConfig, const char *endpoint)
{
    if (!imageConfig.is<JsonObject>())
    {
        Logger::log(Logger::LOG_ERROR, "Invalid renderer config; expected JsonObject");
        return ESP_ERR_INVALID_ARG;
    }

    const char *basepath = imageConfig["basepath"] | "/api/v1";
    const char *userAgent = imageConfig["userAgent"] | USER_AGENT;
    int retries = imageConfig["retries"] | 3;
    int timeout = imageConfig["timeout"] | 30;

    if (!api || strlen(api) == 0)
    {
        Logger::log(Logger::LOG_ERROR, "Invalid URL");
        return ESP_ERR_INVALID_ARG;
    }

    URLParser::Parser parsed(api);
    parsed.expandPath(basepath, endpoint);
    parsed.setParam("rotation", String(rotation));
    parsed.setParam("transform", "true");

    Logger::logf(Logger::LOG_DEBUG, "Fetching image from %s", parsed.getURL(true).c_str());

    // Stack-allocated WiFiClientSecure
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient https;
    https.setUserAgent(userAgent);

    int attempts = 0;
    while (attempts++ < retries)
    {
        parsed.setParam("retries", String(retries));
        parsed.setParam("attempts", String(attempts));

        Logger::logf(Logger::LOG_DEBUG, "Image fetch attempt #%d/%d...", attempts, retries);

        https.getStream().setNoDelay(true);
        https.getStream().setTimeout(15000);
        https.begin(client, parsed.getURL());
        https.setTimeout(timeout * 1000);

        URLParser::BasicAuth basicAuth = parsed.getBasicAuth();
        if (basicAuth.exists())
        {
            https.addHeader("Authorization", "Basic " + basicAuth.encode());
        }
        https.collectHeaders(displayHeaders, sizeof(displayHeaders) / sizeof(displayHeaders[0]));

        int code = https.GET();
        if (code == HTTP_CODE_OK)
        {
            if (https.hasHeader("X-Image-Source"))
            {
                Logger::logf(Logger::LOG_INFO, "Image source: %s", https.header("X-Image-Source").c_str());
            }

            String contentType = https.header("Content-Type");
            if (contentType != "image/jpeg" && contentType != "image/jpg")
            {
                Logger::log(Logger::LOG_DEBUG, "Invalid image type; try again...");
                continue; // try again
            }

            int32_t len = https.getSize();
            if (len <= 0 && https.hasHeader("Content-Length"))
            {
                len = atoi(https.header("Content-Length").c_str());
            }

            if (len <= 0 || len > (E_INK_WIDTH * E_INK_HEIGHT * 8 + 100))
            {
                Logger::log(Logger::LOG_ERROR, "Invalid image size; try again..");
                continue; // try again
            }

            std::unique_ptr<uint8_t[]> buffer(new (std::nothrow) uint8_t[len]);
            if (!buffer)
            {
                Logger::log(Logger::LOG_ERROR, "Failed to allocate memory for image buffer");
                https.end();
                return ESP_ERR_NO_MEM; // out of memory
            }

            // Get a pointer to the stream
            WiFiClient *stream = https.getStreamPtr();
            if (!stream)
            {
                Logger::log(Logger::LOG_ERROR, "Failed to obtain stream");
                https.end();
                return ESP_ERR_INVALID_RESPONSE; // invalid response
            }

            uint8_t *buffPtr = buffer.get();
            int bytesRead = 0;
            unsigned long startMillis = millis();
            const unsigned long timeoutMillis = timeout * 1000;

            // Read the image into the buffer
            while (https.connected() && bytesRead < len)
            {
                if (millis() - startMillis > timeoutMillis)
                {
                    Logger::log(Logger::LOG_ERROR, "Image download timed out");
                    break;
                }

                size_t availableBytes = stream->available();
                if (availableBytes)
                {
                    int toRead = std::min((size_t)(len - bytesRead), availableBytes);
                    int c = stream->readBytes(buffPtr, toRead);

                    if (c > 0)
                    {
                        buffPtr += c;
                        bytesRead += c;
                    }
                    else
                    {
                        Logger::log(Logger::LOG_ERROR, "Stream read failed");
                        break;
                    }
                }
                else
                {
                    delay(10);
                }
            }

            if (bytesRead != len)
            {
                Logger::log(Logger::LOG_ERROR, "Incomplete image download");
                continue; // try again
            }

            // Display the raw jpeg image
            if (display.drawJpegFromBuffer(buffer.get(), bytesRead, 0, 0, 1, 0))
            {
                for (int i = 0; i <= 2; i++)
                {
                    char header[20];
                    snprintf(header, sizeof(header), "X-Inky-Message-%d", i);
                    if (https.hasHeader(header))
                    {
                        Logger::onScreen(Logger::LOG_INFO, false, i, rotation, https.header(header).c_str());
                    }
                }

                Logger::log(Logger::LOG_INFO, "Image successfully displayed.");
                https.end();
                return ESP_OK;
            }

            Logger::log(Logger::LOG_ERROR, "Failed to render JPEG image");
        }
        else
        {
            Logger::logf(Logger::LOG_DEBUG, "Image fetch failed. HTTP error code: %d", code);
        }

        https.end();
        delay(1000);
    }

    Logger::log(Logger::LOG_ERROR, "Image fetch failed after all retries.");
    return ESP_ERR_TIMEOUT;
}
