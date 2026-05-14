#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "config_manager.h"
#include "relay_controller.h"
#include "thingsboard_manager.h"
#include "web_config.h"

// =============================================================================
// WiFi State Machine
// =============================================================================

enum WiFiState {
    AP_ONLY,            // No SSID configured, AP mode for first-time setup
    AP_STA_FALLBACK,    // STA failed, AP for config + STA retrying in background
    STA_CONNECTED,      // Normal operation, STA connected
    STA_RECONNECTING    // STA dropped, retrying without AP
};

// =============================================================================
// GLOBAL INSTANCES
// =============================================================================

ConfigManager configManager;
RelayController relayController;
ThingsBoardManager tbManager(relayController, configManager);
WebConfig webConfig(configManager);

WiFiState wifiState;
unsigned long lastReconnectAttempt = 0;
unsigned long staLostTime = 0;

// =============================================================================
// HELPERS
// =============================================================================

void startAP() {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char apSsid[32];
    snprintf(apSsid, sizeof(apSsid), "%s_%02X%02X%02X", AP_SSID, mac[3], mac[4], mac[5]);
    WiFi.softAP(apSsid, AP_PASSWORD);
    delay(100);
    Serial.printf("[WiFi] AP started: %s (password: %s)\n", apSsid, AP_PASSWORD);
    Serial.printf("[WiFi] Config page: http://%s\n", WiFi.softAPIP().toString().c_str());
}

void stopAP() {
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    Serial.println("[WiFi] AP stopped");
}

bool tryConnectSTA(unsigned long timeoutMs) {
    Serial.printf("[WiFi] Connecting to %s", configManager.wifiSsid);
    WiFi.begin(configManager.wifiSsid, configManager.wifiPassword);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(500);
        Serial.print(".");
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        return true;
    }

    Serial.println("[WiFi] Connection failed.");
    return false;
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.printf(" %s v%s\n", DEVICE_TYPE, FIRMWARE_VERSION);
    Serial.println("========================================");

    // Load config from NVS (or defaults on first boot)
    configManager.begin();

    // Initialize relays with hardcoded config
    const uint8_t relayPins[] = RELAY_PINS;
    relayController.begin(relayPins, RELAY_COUNT, RELAY_ACTIVE_HIGH);

    // WiFi state machine - decide initial state
    if (!configManager.hasWifiConfig()) {
        // No SSID configured — AP only mode (first-time setup)
        Serial.println("[WiFi] No SSID configured. Starting AP for setup...");
        WiFi.mode(WIFI_AP);
        startAP();
        webConfig.begin();
        wifiState = AP_ONLY;

    } else {
        // SSID exists — try STA connection
        WiFi.mode(WIFI_STA);
        if (tryConnectSTA(STA_CONNECT_TIMEOUT_MS)) {
            // Connected — normal operation
            wifiState = STA_CONNECTED;
            tbManager.begin();
        } else {
            // STA failed — AP_STA fallback (config + keep retrying)
            Serial.println("[WiFi] STA failed. Starting AP+STA fallback...");
            WiFi.mode(WIFI_AP_STA);
            startAP();
            webConfig.begin();
            // Re-initiate STA in background
            WiFi.begin(configManager.wifiSsid, configManager.wifiPassword);
            wifiState = AP_STA_FALLBACK;
        }
    }

    Serial.printf("[Main] Setup complete! State: %s\n",
        wifiState == AP_ONLY ? "AP_ONLY" :
        wifiState == AP_STA_FALLBACK ? "AP_STA_FALLBACK" :
        "STA_CONNECTED");
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
    switch (wifiState) {

        case AP_ONLY:
            // Just serve config page, nothing else
            webConfig.handleClient();
            break;

        case AP_STA_FALLBACK:
            // Serve config page + check if STA reconnected
            webConfig.handleClient();

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[WiFi] STA recovered! IP: %s\n", WiFi.localIP().toString().c_str());
                Serial.println("[WiFi] Switching to STA only...");
                webConfig.stop();
                stopAP();
                tbManager.begin();
                wifiState = STA_CONNECTED;
            }
            break;

        case STA_CONNECTED:
            // Normal operation — ThingsBoard communication
            tbManager.update();

            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("[WiFi] Connection lost. Retrying...");
                wifiState = STA_RECONNECTING;
                lastReconnectAttempt = millis();
                staLostTime = millis();
            }
            break;

        case STA_RECONNECTING:
            // If STA has been down too long, fall back to AP_STA
            if (millis() - staLostTime > STA_CONNECT_TIMEOUT_MS) {
                Serial.println("[WiFi] STA reconnect timeout. Starting AP+STA fallback...");
                WiFi.mode(WIFI_AP_STA);
                startAP();
                webConfig.begin();
                WiFi.begin(configManager.wifiSsid, configManager.wifiPassword);
                wifiState = AP_STA_FALLBACK;
                break;
            }

            // Throttled STA reconnect attempts
            if (millis() - lastReconnectAttempt > WIFI_RECONNECT_MS) {
                Serial.printf("[WiFi] Reconnecting to %s...\n", configManager.wifiSsid);
                WiFi.begin(configManager.wifiSsid, configManager.wifiPassword);
                lastReconnectAttempt = millis();
            }

            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[WiFi] Reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
                tbManager.begin();
                wifiState = STA_CONNECTED;
            }
            break;
    }
}
