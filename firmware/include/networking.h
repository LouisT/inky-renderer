#ifndef NETWORK_H
#define NETWORK_H

#include <Inkplate.h>
#include <esp_err.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

extern WiFiClient wifiClient;
extern WiFiClientSecure wifiSecureClient;
extern PubSubClient mqttClient;

esp_err_t WifiConnect(const char *ssid, const char *pass, int retries);

esp_err_t MqttConnect(const JsonVariant &mqttConfig);

esp_err_t DisplayImage(Inkplate &display, int rotation, const JsonVariant &imageConfig, const char *renderEndpoint);

#endif
