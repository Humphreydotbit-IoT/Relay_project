#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// DEFAULT VALUES (used on first boot, then stored in NVS flash)
// Change at runtime via web config page (AP mode)
// =============================================================================

// WiFi defaults (empty = device starts in AP config mode)
#define DEFAULT_WIFI_SSID ""
#define DEFAULT_WIFI_PASSWORD ""

// ThingsBoard defaults
#define DEFAULT_TB_SERVER "mqtt.thingsboard.cloud"
#define DEFAULT_TB_PORT 1883
#define DEFAULT_TB_TOKEN ""

// =============================================================================
// AP MODE SETTINGS (for web configuration)
// =============================================================================

#define AP_SSID "ESP32_Cfg"  // suffix _XXYYZZ (MAC last 3 bytes) appended at runtime
#define AP_PASSWORD "12345678"

// STA connection timeout before falling back to AP_STA mode
#define STA_CONNECT_TIMEOUT_MS 30000

// =============================================================================
// COMPILE-TIME CONSTANTS (not runtime configurable)
// =============================================================================

// Relay pin configuration (hardcoded per board variant)
// ESP32-S3 pins (4 relay):
// #define RELAY_COUNT 4
// #define RELAY_PINS {21, 47, 19, 20}
// #define RELAY_ACTIVE_HIGH false

// ESP32-WROOM-32 generic (4 relay):
// #define RELAY_COUNT 4
// #define RELAY_PINS {16, 17, 18, 19}
// #define RELAY_ACTIVE_HIGH false

// ESP32_RELAYS PCB — 10 relay (BC817 NPN drivers, active HIGH)
//   RELAY1=IO17, RELAY2=IO16, RELAY3=IO4,  RELAY4=IO32, RELAY5=IO33
//   RELAY6=IO14, RELAY7=IO12, RELAY8=IO13, RELAY9=IO25, RELAY10=IO26
#define RELAY_COUNT 10
#define RELAY_PINS {17, 16, 4, 13, 14, 32, 33, 25, 26, 27}
#define RELAY_ACTIVE_HIGH false

// Max relay capacity (array size limit)
#define MAX_RELAYS 32

// MQTT buffer and template params (require recompilation to change)
#define MAX_MESSAGE_SIZE 4096
#define MAX_RPC_RESPONSE 256

// ThingsBoard attribute key for relay bitmask
#define ATTR_RELAY_STATES "rs"

// Reconnect timing
#define WIFI_RECONNECT_MS 5000
#define TB_RECONNECT_MS 5000

// Firmware info
#define FIRMWARE_VERSION "2.0.0"
#define DEVICE_TYPE "ESP32_RELAY_CONTROLLER"

#endif // CONFIG_H
