#ifndef NETWORK_H
#define NETWORK_H

#include <Inkplate.h>
#include <esp_err.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

// Global variables and functions used for networking purposes
extern WiFiClient wifiClient;
extern WiFiClientSecure wifiSecureClient;
extern PubSubClient mqttClient;

// Connect to a WiFi network
esp_err_t WifiConnect(const char *ssid, const char *pass, int retries);

// Connect to an MQTT broker
esp_err_t MqttConnect(const JsonVariant &mqttConfig);

/// Fetch a remote image and display it on the screen
esp_err_t DisplayImage(Inkplate &display, int rotation, const char *api, const JsonVariant &imageConfig, const char *renderEndpoint);

#endif
