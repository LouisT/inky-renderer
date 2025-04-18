#include <Inkplate.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <esp_err.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <vector>

#include "networking.h"
#include "logger.h"
#include "urlparser.h"
#include "definitions.h"
#include "jpeg_utils.h"

const char *displayHeaders[] = {
    "Content-Type",      // Must be image/jpeg
    "Content-Length",    // Use this to determine size
    "Transfer-Encoding", // Use this to determine if a chunked transfer is used
    "X-Image-Source",    // URL of the image, for logging
    "X-No-Dithering",    // Disable dithering on the display if set to true
    "X-Inky-Message-0",  // Fow showing a message on the display (top)
    "X-Inky-Message-1",  // Fow showing a message on the display (middle)
    "X-Inky-Message-2",  // Fow showing a message on the display (bottom)
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

// Read a stream into a buffer, used to pull remote images
std::vector<uint8_t> readStream(WiFiClient &stream,
                                unsigned long timeoutMillis,
                                bool isChunked = true,
                                size_t contentLength = 0)
{
    std::vector<uint8_t> out;
    unsigned long start = millis();
    unsigned long deadline = start + timeoutMillis;

    // If we know the total length up front, reserve once
    if (!isChunked && contentLength > 0)
    {
        out.reserve(contentLength);
    }

    // A single reusable read buffer
    constexpr size_t BUF_SIZE = 256;
    uint8_t buf[BUF_SIZE];

    // Helper to check timeout
    auto timedOut = [&]()
    {
        return millis() >= deadline;
    };

    if (!isChunked)
    {
        while (!timedOut())
        {
            unsigned int avail = stream.available();
            if (avail <= 0)
            {
                if (!stream.connected())
                    break;
                delay(5); // give background tasks a chance
                continue;
            }

            // How much to read this iteration?
            size_t want = (contentLength > 0)
                              ? min<size_t>({BUF_SIZE, avail, contentLength - out.size()})
                              : min<size_t>(BUF_SIZE, avail);

            int n = stream.readBytes(buf, want);
            if (n <= 0)
                break;

            out.insert(out.end(), buf, buf + n);
            deadline = millis() + timeoutMillis; // reset timeout on progress

            // If we've got it all, stop
            if (contentLength > 0 && out.size() >= contentLength)
                break;
        }
    }
    else
    {
        // Buffer for reading the hex-size line
        constexpr size_t LINE_BUF = 32;
        char line[LINE_BUF];

        while (!timedOut())
        {
            // Read chunk-size line (up to '\n')
            int len = stream.readBytesUntil('\n', (uint8_t *)line, LINE_BUF - 1);
            if (len <= 0)
                break;
            line[len] = '\0';

            // Strip any trailing '\r'
            if (char *cr = strchr(line, '\r'))
                *cr = '\0';

            // Parse hex length
            size_t chunkSize = strtoul(line, nullptr, 16);
            if (chunkSize == 0)
                break; // end‑of‑chunks

            size_t remaining = chunkSize;
            while (remaining && !timedOut())
            {
                size_t toRead = min<size_t>(remaining, BUF_SIZE);
                int n = stream.readBytes(buf, toRead);
                if (n <= 0)
                    break;

                out.insert(out.end(), buf, buf + n);
                remaining -= n;
                deadline = millis() + timeoutMillis;
            }

            // Consume the trailing CRLF after each chunk
            //    either by peeking two bytes, or skip to end of line
            if (stream.available() >= 2)
            {
                char discard[2];
                stream.readBytes((uint8_t *)discard, 2);
            }
            else
            {
                stream.readBytesUntil('\n', (uint8_t *)line, LINE_BUF - 1);
            }
        }
    }

    return out;
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
    const bool isPortrait = (rotation % 2 == 0);
    int retries = imageConfig["retries"] | 3;
    int timeout = imageConfig["timeout"] | 30;

    if (!api || strlen(api) == 0)
    {
        Logger::log(Logger::LOG_ERROR, "Invalid URL");
        return ESP_ERR_INVALID_ARG;
    }

    // Build the URL
    URLParser::Parser parsed(api);
    parsed.expandPath(basepath, endpoint);

    // Use rotation to pass the width and height of the board to support different aspect ratios
    parsed.setParam("w", String(isPortrait ? E_INK_WIDTH : E_INK_HEIGHT));
    parsed.setParam("h", String(isPortrait ? E_INK_HEIGHT : E_INK_WIDTH));
    parsed.setParam("mbh", String(MSG_BOX_HEIGHT));

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
            // Log optional X-Image-Source
            if (https.hasHeader("X-Image-Source"))
            {
                Logger::logf(Logger::LOG_INFO, "Image source: %s", https.header("X-Image-Source").c_str());
            }

            String contentType = https.header("Content-Type");
            if (contentType != "image/jpeg" && contentType != "image/jpg")
            {
                Logger::log(Logger::LOG_DEBUG, "Invalid image type; try again...");
                https.end();
                continue; // try again
            }

            // Attempt to fetch the Content-Length
            int32_t contentLength = https.getSize();
            String transferEncoding = https.header("Transfer-Encoding");
            bool isChunked = transferEncoding.indexOf("chunked") >= 0;
            if (contentLength <= 0 && https.hasHeader("Content-Length"))
            {
                contentLength = atoi(https.header("Content-Length").c_str());
            }

            // If we do have a known Content-Length, do a sanity check
            if (contentLength > 0 && contentLength > (E_INK_WIDTH * E_INK_HEIGHT * 8 + 100))
            {
                Logger::log(Logger::LOG_ERROR, "Invalid (too large) Content-Length; try again..");
                https.end();
                continue; // try again
            }

            // Read the stream; if unknown length (contentLength == -1), read until closed or timeout
            WiFiClient *stream = https.getStreamPtr();
            if (!stream)
            {
                Logger::log(Logger::LOG_ERROR, "Failed to obtain stream");
                https.end();
                return ESP_ERR_INVALID_RESPONSE;
            }

            // Read the stream
            std::vector<uint8_t> imageBuffer = readStream(*stream, 1500, isChunked, contentLength);
            https.end(); // No need to keep the connection open

            // Verify that we actually got some data
            if (imageBuffer.empty())
            {
                Logger::log(Logger::LOG_ERROR, "No data read from stream");
                continue; // try again
            }
            else
            {
                Logger::logf(Logger::LOG_DEBUG, "Read %d bytes from stream", imageBuffer.size());
            }

            // Clear the screen before drawing
            display.clearDisplay();

            // Check if the image is progressive
            if (!jpeg_utils::isBaseline(imageBuffer.data(), imageBuffer.size()))
            {
                Logger::log(Logger::LOG_INFO, "Invalid JPEG image; try again...");
                return ESP_ERR_INVALID_RESPONSE;
            }

            // Allow the renderer to override the dithering setting
            int dither = static_cast<int>(DITHERING);
            if (https.hasHeader("X-No-Dithering") && https.header("X-No-Dithering") == "true")
            {
                dither = 0;
            }
            Logger::logf(Logger::LOG_DEBUG, "Dithering: %d / Chunked: %d / Image size: %d", dither, isChunked, imageBuffer.size());

            // Display the raw jpeg image from the buffer
            if (display.drawJpegFromBuffer(imageBuffer.data(), imageBuffer.size(), 0, 0, dither, 0))
            {
                // If the server returned messages in headers like "X-Inky-Message-0", ...
                for (int i = 0; i <= 2; i++)
                {
                    char headerName[20];
                    snprintf(headerName, sizeof(headerName), "X-Inky-Message-%d", i);
                    if (https.hasHeader(headerName))
                    {
                        Logger::onScreen(Logger::LOG_INFO, false, i, rotation, https.header(headerName).c_str());
                    }
                }

                Logger::log(Logger::LOG_INFO, "Image successfully displayed.");
                return ESP_OK;
            }
            Logger::log(Logger::LOG_ERROR, "Failed to render JPEG image");
        }
        else
        {
            Logger::logf(Logger::LOG_DEBUG, "Image fetch failed. HTTP error code: %d", code);
        }
        delay(1000); // short delay before next attempt
    }

    Logger::log(Logger::LOG_ERROR, "Image fetch failed after all retries.");
    return ESP_ERR_TIMEOUT;
}