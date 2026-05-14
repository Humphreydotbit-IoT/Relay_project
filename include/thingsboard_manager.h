#ifndef THINGSBOARD_MANAGER_H
#define THINGSBOARD_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <Server_Side_RPC.h>
#include <ThingsBoard.h>
#include "config.h"
#include "config_manager.h"
#include "relay_controller.h"

// Forward declaration
class ThingsBoardManager;

// Static instance pointer for callbacks
static ThingsBoardManager* g_tbManagerInstance = nullptr;

class ThingsBoardManager {
public:
    ThingsBoardManager(RelayController& relayCtrl, ConfigManager& cfg)
        : _relayCtrl(relayCtrl)
        , _cfg(cfg)
        , _mqttClient(_wifiClient)
        , _serverRpc()
        , _apis{&_serverRpc}
        , _tb(_mqttClient, MAX_MESSAGE_SIZE, Default_Max_Stack_Size, _apis)
        , _rpcSubscribed(false) {
        g_tbManagerInstance = this;
    }

    void begin() {
        _wifiClient.setTimeout(10);
        Serial.println("[TB] Manager initialized");
    }

    void update() {
        if (!isConnected()) {
            connectThingsBoard();
            return;
        }

        _tb.loop();

        if (!_rpcSubscribed) {
            subscribeToRPC();
        }
    }

    bool isConnected() {
        return _tb.connected();
    }

    void sendRelayStates() {
        if (!isConnected()) return;

        uint32_t mask = _relayCtrl.getStateBitmask();
        _tb.sendAttributeData(ATTR_RELAY_STATES, mask);
        Serial.printf("[TB] Sent %s = %u (0b", ATTR_RELAY_STATES, mask);
        for (int i = _relayCtrl.getCount() - 1; i >= 0; i--) {
            Serial.print((mask >> i) & 1);
        }
        Serial.println(")");
    }

private:
    RelayController& _relayCtrl;
    ConfigManager& _cfg;

    WiFiClient _wifiClient;
    Arduino_MQTT_Client _mqttClient;

    Server_Side_RPC<1U, MAX_RPC_RESPONSE> _serverRpc;
    const std::array<IAPI_Implementation*, 1U> _apis;
    ThingsBoard _tb;

    bool _rpcSubscribed;

    void connectThingsBoard() {
        static unsigned long lastAttempt = 0;

        if (millis() - lastAttempt < TB_RECONNECT_MS) return;
        lastAttempt = millis();

        Serial.printf("[TB] Connecting to %s...\n", _cfg.tbServer);

        if (_tb.connect(_cfg.tbServer, _cfg.tbToken, _cfg.tbPort)) {
            Serial.println("[TB] Connected!");
            sendRelayStates();
        } else {
            Serial.println("[TB] Connection failed, will retry...");
        }
    }

    void subscribeToRPC() {
        Serial.println("[RPC] Subscribing to server-side RPC...");

        const std::array<RPC_Callback, 1U> callbacks = {
            RPC_Callback("setValue", &ThingsBoardManager::onSetValue)
        };

        _rpcSubscribed = true;

        if (_serverRpc.RPC_Subscribe(callbacks.begin(), callbacks.end())) {
            Serial.println("[RPC] Subscribed to setValue");
        } else {
            Serial.println("[RPC] MQTT topic subscribe failed (will auto-retry on reconnect)");
        }
    }

    // =========================================================================
    // Static RPC Callbacks
    // =========================================================================

    static void onSetValue(JsonVariantConst const & params, JsonDocument & response) {
        if (!g_tbManagerInstance) return;

        Serial.println("[RPC] ===== setValue received =====");
        String rawJson;
        serializeJson(params, rawJson);
        Serial.printf("[RPC] Raw params: %s\n", rawJson.c_str());

        if (!params.containsKey("relay")) {
            Serial.println("[RPC] ERROR: 'relay' key missing");
            return;
        }
        int relayNum = params["relay"].as<int>();
        Serial.printf("[RPC] Parsed relay number: %d\n", relayNum);

        if (!params.containsKey("value")) {
            Serial.println("[RPC] ERROR: 'value' key missing");
            return;
        }
        bool value = params["value"].as<bool>();
        Serial.printf("[RPC] Parsed value: %s\n", value ? "true (ON)" : "false (OFF)");

        uint8_t index = relayNum - 1;
        auto& relay = g_tbManagerInstance->_relayCtrl;
        if (index >= relay.getCount()) {
            Serial.printf("[RPC] ERROR: relay %d out of range (max %d)\n",
                          relayNum, relay.getCount());
            return;
        }
        Serial.printf("[RPC] Relay index: %d, GPIO pin: %d\n",
                      index, relay.getPin(index));

        relay.setState(index, value);

        g_tbManagerInstance->sendRelayStates();
        Serial.printf("[RPC] Relay %d → %s DONE\n", relayNum, value ? "ON" : "OFF");
        Serial.println("[RPC] =============================");
    }
};

#endif // THINGSBOARD_MANAGER_H
