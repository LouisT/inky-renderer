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

// If Transfer-Encoding is chunked, read the stream until CRLF
std::vector<uint8_t> readChunkedStream(WiFiClient *stream, unsigned long timeoutMillis)
{
    std::vector<uint8_t> output;
    unsigned long deadline = millis() + timeoutMillis;

    // Read chunk header, then the chunk data, then skip CRLF
    while (millis() < deadline)
    {
        // Read a line (ends with '\n') that contains the hexadecimal chunk size
        String line = stream->readStringUntil('\n');
        line.trim(); // Remove any whitespace and the carriage return

        // If we got an empty line, break out (may be end or timeout)
        if (line.length() == 0)
        {
            break;
        }

        // Convert the header line from hexadecimal to an integer
        long chunkSize = strtol(line.c_str(), NULL, 16);

        // If the chunk size is invalid, break out
        if (chunkSize <= 0)
        {
            break;
        }

        // Read the chunk data.
        size_t remaining = chunkSize;
        while (remaining > 0 && millis() < deadline)
        {
            // Use a fixed-size buffer for each read iteration
            uint8_t buf[256];
            size_t toRead = remaining < sizeof(buf) ? remaining : sizeof(buf);
            int bytesRead = stream->readBytes(buf, toRead);
            if (bytesRead <= 0)
            {
                // No bytes read (error or timeout)
                break;
            }
            output.insert(output.end(), buf, buf + bytesRead);
            remaining -= bytesRead;
        }

        // Discard the trailing CRLF following the chunk.
        // (We read until '\n' so the line-ending should be consumed.)
        stream->readStringUntil('\n');
    }
    return output;
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

            unsigned long startMillis = millis();
            unsigned long timeoutMillis = timeout * 1000;
            std::vector<uint8_t> imageBuffer;

            // If chunked, read the stream until CRLF
            if (isChunked)
            {
                imageBuffer = readChunkedStream(stream, timeoutMillis);
            }
            else
            {
                const size_t CHUNK_SIZE = 1024;
                size_t totalBytesRead = 0;
                imageBuffer.reserve(contentLength);

                // Loop until either the timeout is reached or all expected data has been read.
                while ((millis() - startMillis) < timeoutMillis)
                {
                    // Check how many bytes are currently available in the stream
                    size_t availableBytes = stream->available();
                    if (availableBytes > 0)
                    {
                        // Create a temporary buffer to hold data read from the stream
                        uint8_t temp[CHUNK_SIZE];

                        // Determine the number of bytes to read this iteration
                        size_t toRead = (availableBytes > CHUNK_SIZE) ? CHUNK_SIZE : availableBytes;

                        // If reading 'toRead' bytes would exceed the total expected content length,
                        // then adjust to only read the remaining bytes
                        if (toRead > (contentLength - totalBytesRead))
                        {
                            toRead = contentLength - totalBytesRead;
                        }

                        // Attempt to read 'toRead' bytes from the stream into the temporary buffer
                        int bytesReadNow = stream->readBytes(temp, toRead);
                        if (bytesReadNow <= 0)
                        {
                            Logger::log(Logger::LOG_ERROR, "Stream read failed");
                            break;
                        }

                        // Append the bytes successfully read into the imageBuffer
                        imageBuffer.insert(imageBuffer.end(), temp, temp + bytesReadNow);
                        totalBytesRead += bytesReadNow;

                        // If we have read the entire expected content length, exit the loop
                        if (totalBytesRead >= (size_t)contentLength)
                        {
                            break;
                        }
                    }
                    else
                    {
                        // If no data is currently available, check if the HTTP connection is still alive
                        if (!https.connected())
                            break;
                        delay(10);
                    }
                }
            }

            // Verify that we actually got some data
            if (imageBuffer.empty())
            {
                Logger::log(Logger::LOG_ERROR, "No data read from stream");
                https.end();
                continue; // try again
            }

            // Clear the screen before drawing
            display.clearDisplay();

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
        delay(1000); // short delay before next attempt
    }

    Logger::log(Logger::LOG_ERROR, "Image fetch failed after all retries.");
    return ESP_ERR_TIMEOUT;
}