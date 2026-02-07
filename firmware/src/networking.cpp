#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Inkplate.h>
#include <PubSubClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <esp_err.h>
#include <esp_partition.h>
#include <qrcode.h>
#include <vector>

#include "definitions.h"
#include "jpeg_utils.h"
#include "logger.h"
#include "networking.h"
#include "ota_html.h"
#include "urlparser.h"

// headers to collect from the HTTP response
const char *displayHeaders[] = {
    "Content-Type",     "Content-Length",   "Transfer-Encoding",
    "X-Image-Source",   "X-No-Dithering",   "X-Inky-Message-0",
    "X-Inky-Message-1", "X-Inky-Message-2",
};

// Global network clients
WiFiClient wifiClient;
WiFiClientSecure wifiClientSecure;
PubSubClient mqttClient;

// Global reference for the callback to access the display
static Inkplate *_apDisplay = nullptr;

// Draws a QR code on the Inkplate display at the specified coordinates
static void drawQRCode(Inkplate *display, const String &text, int x, int y,
                       int scale) {
  // QR code object
  QRCode qrcode;
  // Version 3 QR Code
  uint8_t qrcodeVersion = 3;
  // Buffer for Version 3 QR Code
  uint8_t qrcodeData[qrcode_getBufferSize(3)];

  // Initialize the QR code with the provided text
  qrcode_initText(&qrcode, qrcodeData, qrcodeVersion, 0, text.c_str());

  // Loop through the QR code modules to draw them
  for (uint8_t qy = 0; qy < qrcode.size; qy++) {
    for (uint8_t qx = 0; qx < qrcode.size; qx++) {
      // Check if the module is set (black)
      if (qrcode_getModule(&qrcode, qx, qy)) {
        display->fillRect(x + (qx * scale), y + (qy * scale), scale, scale, 0);
      }
    }
  }
}

// Callback function executing when WiFiManager enters configuration mode (AP
// active)
void configModeCallback(WiFiManager *myWiFiManager) {
  // Ensure display reference is valid
  if (!_apDisplay)
    return;

  // Get display dimensions to determine scaling
  int width = _apDisplay->width();
  // Larger scale for Inkplate 10, smaller for 6Color
  int scale = (width > 800) ? 8 : 6;
  // Calculate pixel size of the QR code (Version 3 is 29x29 modules)
  int qrSizePx = 29 * scale;

  // Layout coordinates for UI elements
  int yStart = 140;
  int xLeft = 20;
  int xRight = xLeft + qrSizePx + 80;

  // Retrieve SSID and IP for display
  String ssid = myWiFiManager->getConfigPortalSSID();
  String ipURL = "http://" + WiFi.softAPIP().toString();

  Logger::logf(Logger::LOG_INFO, "Entered config mode: %s", ssid.c_str());
  Logger::logf(Logger::LOG_INFO, "IP Address: %s", ipURL.c_str());

  // Prepare display
  _apDisplay->clearDisplay();
  _apDisplay->setTextColor(0);

  // Draw Header Text
  _apDisplay->setTextSize(3);
  _apDisplay->setCursor(20, 50);
  _apDisplay->println("WiFi Setup Required");
  _apDisplay->setTextSize(2);
  _apDisplay->setCursor(20, 90);
  _apDisplay->println("Follow the steps below to configure:");

  // Draw Left QR: Connect to WiFi
  // Format: WIFI:S:ssid;T:nopass;; (Open network)
  String wifiPayload = "WIFI:S:" + ssid + ";T:nopass;;";
  drawQRCode(_apDisplay, wifiPayload, xLeft, yStart, scale);

  // Draw labels for the first QR code
  _apDisplay->setTextSize(2);
  _apDisplay->setCursor(xLeft, yStart + qrSizePx + 20);
  _apDisplay->println("1. Scan to Connect");
  _apDisplay->setTextSize(1);
  _apDisplay->setCursor(xLeft, yStart + qrSizePx + 50);
  _apDisplay->printf("SSID: %s\n", ssid.c_str());

  // Draw Right QR: Open Browser
  drawQRCode(_apDisplay, ipURL, xRight, yStart, scale);

  // Draw labels for the second QR code
  _apDisplay->setTextSize(2);
  _apDisplay->setCursor(xRight, yStart + qrSizePx + 20);
  _apDisplay->println("2. Scan to Config");
  _apDisplay->setTextSize(1);
  _apDisplay->setCursor(xRight, yStart + qrSizePx + 50);
  _apDisplay->printf("URL: %s\n", ipURL.c_str());

  // Update the screen
  _apDisplay->display();
}

// Connects to WiFi or launches the captive portal if connection fails
esp_err_t WifiConnect(Inkplate &display, int timeoutSeconds, bool forceConfig) {
  WiFi.mode(WIFI_STA);
  _apDisplay = &display;

  // Initialize WiFiManager
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setAPCallback(configModeCallback);
  wm.setConfigPortalTimeout(timeoutSeconds);

  // Disable update page in the standard captive portal menu
  std::vector<const char *> menu = {"wifi", "info", "exit"};
  wm.setMenu(menu);

  bool res;
  // Check if we should force the config portal (e.g. via button press)
  if (forceConfig) {
    Logger::log(Logger::LOG_INFO,
                "Long press detected: Forcing Config Portal...");
    // Launch AP immediately without trying to connect first
    res = wm.startConfigPortal("Inky-Renderer");
  } else {
    // Try to connect to saved WiFi first, then launch AP if it fails
    res = wm.autoConnect("Inky-Renderer");
  }

  // Clear display reference
  _apDisplay = nullptr;

  // Check connection result
  if (!res) {
    Logger::log(Logger::LOG_ERROR,
                "WiFi Manager: Failed to connect and hit timeout.");
    return ESP_ERR_TIMEOUT;
  }

  Logger::logf(Logger::LOG_INFO, "WiFi Connected! IP: %s",
               WiFi.localIP().toString().c_str());
  return ESP_OK;
}

// Connects to the MQTT broker using the provided configuration
esp_err_t MqttConnect(const JsonVariant &mqttConfig) {
  // Validate config type
  if (!mqttConfig.is<JsonObject>()) {
    Logger::log(Logger::LOG_ERROR, "Invalid MQTT config; expected JsonObject");
    return ESP_ERR_INVALID_ARG;
  }

  // Extract MQTT settings with defaults
  const char *server = mqttConfig["server"] | "";
  const int port = mqttConfig["port"] | 1883;
  const char *user = mqttConfig["user"] | "";
  const char *pass = mqttConfig["pass"] | "";
  const char *deviceId = mqttConfig["device"] | "inky";
  int retries = mqttConfig["retries"] | 3;
  int maxtx = mqttConfig["maxtx"] | MQTT_MAX_PACKET_SIZE;
  int maxrx = mqttConfig["maxrx"] | MQTT_MAX_PACKET_SIZE;
  bool useTLS = mqttConfig["tls"] | false;

  Logger::logf(Logger::LOG_INFO, "MQTT: %s:%d (TLS=%s)", server, port,
               useTLS ? "true" : "false");

  // Configure client based on TLS setting
  if (useTLS) {
    // No cert verification for simplicity
    wifiClientSecure.setInsecure();
    mqttClient.setClient(wifiClientSecure);
  } else {
    mqttClient.setClient(wifiClient);
  }

  mqttClient.setServer(server, port);
  mqttClient.setBufferSize(maxrx, maxtx);

  // Attempt connection loop
  int attempts = 0;
  while (attempts++ < retries && !mqttClient.connected()) {
    Logger::logf(Logger::LOG_DEBUG, "MQTT connection attempt #%d/%d...",
                 attempts, retries);
    // Connect with or without credentials
    if (user && strlen(user) > 0) {
      mqttClient.connect(deviceId, user, pass);
    } else {
      mqttClient.connect(deviceId);
    }
    delay(500);
  }

  return mqttClient.connected() ? ESP_OK : ESP_ERR_TIMEOUT;
}

// Reads data from a WiFi stream into a byte vector, handling chunked transfer
// encoding
std::vector<uint8_t> readStream(WiFiClient &stream, unsigned long timeoutMillis,
                                bool isChunked, size_t contentLength) {
  std::vector<uint8_t> out;
  unsigned long start = millis();
  unsigned long deadline = start + timeoutMillis;

  // Reserve memory if size is known to avoid reallocations
  if (!isChunked && contentLength > 0) {
    out.reserve(contentLength);
  }

  // Buffer for reading data
  constexpr size_t BUF_SIZE = 256;
  uint8_t buf[BUF_SIZE];

  // Read loop
  while (millis() < deadline) {
    // Check availability
    if (stream.available() <= 0) {
      if (!stream.connected())
        break;
      delay(5);
      continue;
    }

    // Handle standard (non-chunked) transfer
    if (!isChunked) {
      // Determine how much to read
      size_t want = (contentLength > 0)
                        ? min<size_t>({BUF_SIZE, (size_t)stream.available(),
                                       contentLength - out.size()})
                        : min<size_t>(BUF_SIZE, (size_t)stream.available());

      // Read data and append to output vector
      int n = stream.readBytes(buf, want);
      if (n > 0) {
        out.insert(out.end(), buf, buf + n);
        // Reset timeout on successful read
        deadline = millis() + timeoutMillis;
      }

      // Stop if we have read the expected length
      if (contentLength > 0 && out.size() >= contentLength)
        break;
    }
    // Handle chunked transfer
    else {
      // Buffer for reading the hex-size line
      constexpr size_t LINE_BUF = 32;
      char line[LINE_BUF];

      // Read chunk-size line (up to '\n')
      int len = stream.readBytesUntil('\n', (uint8_t *)line, LINE_BUF - 1);
      if (len <= 0)
        break;
      line[len] = '\0';

      // Strip any trailing '\r'
      if (char *cr = strchr(line, '\r'))
        *cr = '\0';

      // Parse hex length from the line
      size_t chunkSize = strtoul(line, nullptr, 16);
      // Size 0 indicates end of chunks
      if (chunkSize == 0)
        break;

      size_t remaining = chunkSize;
      // Read the chunk data
      while (remaining && millis() < deadline) {
        size_t toRead = min<size_t>(remaining, BUF_SIZE);
        int n = stream.readBytes(buf, toRead);
        if (n <= 0)
          break;

        out.insert(out.end(), buf, buf + n);
        remaining -= n;
        deadline = millis() + timeoutMillis;
      }

      // Consume the trailing CRLF after the chunk data
      if (stream.available() >= 2) {
        char discard[2];
        stream.readBytes((uint8_t *)discard, 2);
      } else {
        stream.readBytesUntil('\n', (uint8_t *)line, LINE_BUF - 1);
      }
    }
  }
  return out;
}

// Fetches a JPEG image from a URL and renders it to the Inkplate
esp_err_t DisplayImage(Inkplate &display, int rotation, const char *api,
                       const JsonVariant &imageConfig, const char *endpoint) {
  // Validate inputs
  if (!imageConfig.is<JsonObject>())
    return ESP_ERR_INVALID_ARG;
  if (!api || strlen(api) == 0)
    return ESP_ERR_INVALID_ARG;

  // Parse configuration values
  const char *basepath = imageConfig["basepath"] | "/api/v1";
  const char *userAgent = imageConfig["userAgent"] | USER_AGENT;
  bool isPortrait = (rotation % 2 == 0);
  int retries = imageConfig["retries"] | 3;
  int timeout = imageConfig["timeout"] | 30;

  // Construct the full URL
  URLParser::Parser parsed(api);
  parsed.expandPath(basepath, endpoint);

  // Pass display dimensions for responsive sizing
  parsed.setParam("w", String(isPortrait ? E_INK_WIDTH : E_INK_HEIGHT));
  parsed.setParam("h", String(isPortrait ? E_INK_HEIGHT : E_INK_WIDTH));
  parsed.setParam("mbh", String(MSG_BOX_HEIGHT));

  Logger::logf(Logger::LOG_DEBUG, "Fetching image: %s",
               parsed.getURL(true).c_str());

  // Setup HTTP client
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient https;
  https.setUserAgent(userAgent);
  https.getStream().setNoDelay(true);
  https.getStream().setTimeout(15000); // Stream timeout

  // Retry loop for fetching image
  for (int i = 1; i <= retries; i++) {
    parsed.setParam("retries", String(retries));
    parsed.setParam("attempts", String(i));

    Logger::logf(Logger::LOG_DEBUG, "Attempt %d/%d...", i, retries);

    // Start connection
    if (https.begin(client, parsed.getURL())) {
      https.setTimeout(timeout * 1000);

      // Set Authorization Headers if needed
      URLParser::BasicAuth basicAuth = parsed.getBasicAuth();
      if (basicAuth.exists()) {
        https.addHeader("Authorization", "Basic " + basicAuth.encode());
      }

      // Collect custom headers
      https.collectHeaders(displayHeaders,
                           sizeof(displayHeaders) / sizeof(displayHeaders[0]));

      int code = https.GET();
      // Check for successful response
      if (code == HTTP_CODE_OK) {
        // Log Source if provided in headers
        if (https.hasHeader("X-Image-Source")) {
          Logger::logf(Logger::LOG_INFO, "Source: %s",
                       https.header("X-Image-Source").c_str());
        }

        // Validate Content-Type
        String contentType = https.header("Content-Type");
        if (contentType != "image/jpeg" && contentType != "image/jpg") {
          Logger::logf(Logger::LOG_ERROR, "Invalid content type: %s",
                       contentType.c_str());
          https.end();
          continue;
        }

        // Get content size info
        int32_t len = https.getSize();
        bool isChunked =
            (https.header("Transfer-Encoding").indexOf("chunked") >= 0);

        // Fallback to header if size is -1
        if (len <= 0 && https.hasHeader("Content-Length")) {
          len = atoi(https.header("Content-Length").c_str());
        }

        // Check for sensible size limits to prevent buffer overflow
        if (len > (E_INK_WIDTH * E_INK_HEIGHT * 8 + 100)) {
          Logger::log(Logger::LOG_ERROR, "Content too large");
          https.end();
          continue;
        }

        // Get the network stream
        WiFiClient *stream = https.getStreamPtr();
        if (stream) {
          // Read data into buffer
          std::vector<uint8_t> buffer =
              readStream(*stream, 1500, isChunked, len);
          https.end();

          if (!buffer.empty()) {
            // Render Image to Display
            display.clearDisplay();
            // Check if JPEG is compatible (baseline)
            if (!jpeg_utils::isBaseline(buffer.data(), buffer.size())) {
              Logger::log(Logger::LOG_ERROR, "JPEG not baseline");
              return ESP_ERR_INVALID_RESPONSE;
            }

            // Determine dithering setting
            int dither = static_cast<int>(DITHERING);
            if (https.hasHeader("X-No-Dithering") &&
                https.header("X-No-Dithering") == "true") {
              dither = 0;
            }

            // Draw the JPEG
            if (display.drawJpegFromBuffer(buffer.data(), buffer.size(), 0, 0,
                                           dither, 0)) {
              // Display header messages if present
              for (int m = 0; m <= 2; m++) {
                char h[20];
                snprintf(h, sizeof(h), "X-Inky-Message-%d", m);
                if (https.hasHeader(h)) {
                  Logger::onScreen(Logger::LOG_INFO, false, m, rotation,
                                   https.header(h).c_str());
                }
              }
              Logger::log(Logger::LOG_INFO, "Image rendered.");
              return ESP_OK;
            } else {
              Logger::log(Logger::LOG_ERROR, "Render failed");
            }
          }
        }
      } else {
        Logger::logf(Logger::LOG_ERROR, "HTTP Error: %d", code);
      }
      https.end();
    }
    delay(1000);
  }

  return ESP_ERR_TIMEOUT;
}

// Starts the OTA web server and blocks execution until timeout or reboot
void StartOTAServer(Inkplate &display, int rotation) {
  // 5-minute timeout to save battery if forgotten
  const unsigned long TIMEOUT_MS = 5 * 60 * 1000;
  unsigned long lastActivity = millis();

  // Get local IP address for display
  String ip = WiFi.localIP().toString();
  String sURL = "http://" + ip;

  // Draw UI on E-Ink
  int width = display.width();
  int scale = (width > 800) ? 8 : 6;
  int qrSizePx = 29 * scale;
  int xPos = 20;
  int yStart = 140;

  display.clearDisplay();
  display.setTextSize(3);
  display.setCursor(20, 50);
  display.println("OTA Maintenance Mode");

  display.setTextSize(2);
  display.setCursor(20, 90);
  display.printf("Connect to: %s", ip.c_str());

  // Draw QR for the URL
  drawQRCode(&display, sURL, xPos, yStart, scale);

  display.setTextSize(2);
  display.setCursor(xPos, yStart + qrSizePx + 20);
  display.println("Scan to Upload");
  display.setTextSize(2);
  display.setCursor(xPos, yStart + qrSizePx + 50);
  display.printf("URL: %s", sURL.c_str());

  display.display();

  // Setup Server
  WebServer server(80);

  // Root handler to serve the HTML page
  server.on("/", HTTP_GET, [&server, &lastActivity]() {
    server.send(200, "text/html", ota_html);
    lastActivity = millis();
  });

  // Update handler to process file uploads
  server.on(
      "/update", HTTP_POST,
      // Completion Handler
      [&server]() {
        bool success = !Update.hasError();
        server.send(200, "text/plain",
                    success ? "Success! Rebooting..." : "Failed.");
        delay(1000);
        ESP.restart();
      },
      // Upload Data Handler
      [&server, &lastActivity]() {
        HTTPUpload &upload = server.upload();
        lastActivity = millis();

        // Start of upload
        if (upload.status == UPLOAD_FILE_START) {
          Serial.printf("Update: %s\n", upload.filename.c_str());
          // Check if uploading to Filesystem (SPIFFS/LittleFS) or Flash (App)
          int command = (server.arg("type") == "fs") ? U_SPIFFS : U_FLASH;
          if (!Update.begin(UPDATE_SIZE_UNKNOWN, command)) {
            Update.printError(Serial);
          }
        }
        // Writing data chunk
        else if (upload.status == UPLOAD_FILE_WRITE) {
          if (Update.write(upload.buf, upload.currentSize) !=
              upload.currentSize) {
            Update.printError(Serial);
          }
        }
        // End of upload
        else if (upload.status == UPLOAD_FILE_END) {
          if (Update.end(true)) {
            Serial.printf("Success: %u bytes\n", upload.totalSize);
          } else {
            Update.printError(Serial);
          }
        }
      });

  // Start services
  server.begin();
  ArduinoOTA.begin();
  Logger::log(Logger::LOG_INFO, "OTA Server Listening.");

  // Blocking loop for handling clients
  while (true) {
    server.handleClient();
    ArduinoOTA.handle();

    // Check for inactivity timeout
    if (millis() - lastActivity > TIMEOUT_MS) {
      Logger::log(Logger::LOG_WARNING, "Timeout reached. Restarting...");
      delay(100);
      ESP.restart();
    }
    delay(5);
  }
}