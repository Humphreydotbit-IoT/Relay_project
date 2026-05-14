#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

class ConfigManager {
public:
    // WiFi
    char wifiSsid[64];
    char wifiPassword[64];

    // ThingsBoard
    char tbServer[128];
    uint16_t tbPort;
    char tbToken[64];

    void begin() {
        _prefs.begin("cfg", false);

        bool initialized = _prefs.getBool("init", false);

        if (!initialized) {
            Serial.println("[CFG] First boot - saving defaults to NVS");
            loadDefaults();
            save();
            _prefs.putBool("init", true);
        } else {
            load();
        }

        printConfig();
    }

    void save() {
        _prefs.putString("ssid", wifiSsid);
        _prefs.putString("wpass", wifiPassword);
        _prefs.putString("srv", tbServer);
        _prefs.putUShort("port", tbPort);
        _prefs.putString("tok", tbToken);
        Serial.println("[CFG] Config saved to NVS");
    }

    void loadDefaults() {
        strlcpy(wifiSsid, DEFAULT_WIFI_SSID, sizeof(wifiSsid));
        strlcpy(wifiPassword, DEFAULT_WIFI_PASSWORD, sizeof(wifiPassword));
        strlcpy(tbServer, DEFAULT_TB_SERVER, sizeof(tbServer));
        tbPort = DEFAULT_TB_PORT;
        strlcpy(tbToken, DEFAULT_TB_TOKEN, sizeof(tbToken));
    }

    void factoryReset() {
        _prefs.clear();
        Serial.println("[CFG] NVS cleared. Will load defaults on next boot.");
    }

    bool hasWifiConfig() const {
        return strlen(wifiSsid) > 0;
    }

private:
    Preferences _prefs;

    void load() {
        getString("ssid", wifiSsid, sizeof(wifiSsid), DEFAULT_WIFI_SSID);
        getString("wpass", wifiPassword, sizeof(wifiPassword), DEFAULT_WIFI_PASSWORD);
        getString("srv", tbServer, sizeof(tbServer), DEFAULT_TB_SERVER);
        tbPort = _prefs.getUShort("port", DEFAULT_TB_PORT);
        getString("tok", tbToken, sizeof(tbToken), DEFAULT_TB_TOKEN);

        Serial.println("[CFG] Config loaded from NVS");
    }

    void getString(const char* key, char* dest, size_t maxLen, const char* defaultVal) {
        String val = _prefs.getString(key, defaultVal);
        strlcpy(dest, val.c_str(), maxLen);
    }

    void printConfig() {
        Serial.println("[CFG] Current config:");
        Serial.printf("  WiFi SSID:     %s\n", strlen(wifiSsid) > 0 ? wifiSsid : "(empty)");
        Serial.printf("  TB Server:     %s:%d\n", tbServer, tbPort);
    }
};

#endif // CONFIG_MANAGER_H
