#ifndef VARS_H
#define VARS_H

#ifndef CONFIG_FILE_PATH
#define CONFIG_FILE_PATH "/config.json"
#endif

#ifndef ROTATION
#define ROTATION 0
#endif

#ifndef BUILD_TYPE
#define BUILD_TYPE "debug"
#endif

#ifndef TEXT_SIZE
#define TEXT_SIZE 2
#endif

#ifndef MSG_BOX_HEIGHT
#define MSG_BOX_HEIGHT 30
#endif

#ifndef DITHERRING
#define DITHERRING 1
#endif

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

#define uS_TO_S_FACTOR 1000000UL

// Adjust the width or height of the eInk display when requesting an image
// TODO: Allow them to be adjusted via build flags
#ifdef ARDUINO_INKPLATE10V2
#define F_E_INK_WIDTH 1200
#define F_E_INK_HEIGHT 825
#endif
#ifdef ARDUINO_INKPLATECOLOR
#define F_E_INK_WIDTH 605
#define F_E_INK_HEIGHT 448
#endif

#endif