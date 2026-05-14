#ifndef WEB_CONFIG_H
#define WEB_CONFIG_H

#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include "config.h"
#include "config_manager.h"

// HTML page stored in flash
static const char CONFIG_PAGE[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Device Configuration</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body {
    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', system-ui, sans-serif;
    background: #0f172a;
    color: #e2e8f0;
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 20px;
  }
  .card {
    background: #1e293b;
    border-radius: 16px;
    padding: 32px;
    width: 100%;
    max-width: 420px;
    box-shadow: 0 4px 24px rgba(0,0,0,0.3);
  }
  .header {
    text-align: center;
    margin-bottom: 28px;
  }
  .header h1 {
    font-size: 20px;
    font-weight: 600;
    color: #f1f5f9;
    margin-bottom: 4px;
  }
  .header p {
    font-size: 13px;
    color: #64748b;
  }
  .section-label {
    font-size: 11px;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: #64748b;
    margin-bottom: 12px;
    margin-top: 24px;
  }
  .section-label:first-of-type {
    margin-top: 0;
  }
  .field {
    margin-bottom: 16px;
  }
  .field label {
    display: block;
    font-size: 13px;
    font-weight: 500;
    color: #94a3b8;
    margin-bottom: 6px;
  }
  .field input {
    width: 100%;
    padding: 10px 14px;
    background: #0f172a;
    border: 1px solid #334155;
    border-radius: 8px;
    color: #f1f5f9;
    font-size: 14px;
    font-family: inherit;
    outline: none;
    transition: border-color 0.2s;
  }
  .field input:focus {
    border-color: #3b82f6;
  }
  .field input::placeholder {
    color: #475569;
  }
  .port-row {
    display: grid;
    grid-template-columns: 1fr 120px;
    gap: 12px;
  }
  .btn {
    width: 100%;
    padding: 12px;
    border: none;
    border-radius: 8px;
    font-size: 14px;
    font-weight: 600;
    font-family: inherit;
    cursor: pointer;
    transition: opacity 0.2s;
  }
  .btn:hover { opacity: 0.9; }
  .btn:active { transform: scale(0.99); }
  .btn-primary {
    background: #3b82f6;
    color: #fff;
    margin-top: 28px;
  }
  .btn-danger {
    background: transparent;
    color: #ef4444;
    border: 1px solid #7f1d1d;
    margin-top: 12px;
    font-weight: 500;
    font-size: 13px;
    padding: 10px;
  }
  .btn-danger:hover {
    background: #7f1d1d22;
  }
  .status {
    text-align: center;
    margin-top: 16px;
    font-size: 13px;
    min-height: 20px;
  }
  .status.ok { color: #22c55e; }
  .status.err { color: #ef4444; }
  .divider {
    border: none;
    border-top: 1px solid #334155;
    margin: 24px 0 0 0;
  }
</style>
</head>
<body>
<div class="card">
  <div class="header">
    <h1>Device Configuration</h1>
    <p>ESP32 Relay Controller</p>
  </div>
  <form id="configForm">
    <div class="section-label">WiFi</div>
    <div class="field">
      <label for="ssid">SSID</label>
      <input type="text" id="ssid" name="ssid" placeholder="Network name" maxlength="63">
    </div>
    <div class="field">
      <label for="wpass">Password</label>
      <input type="password" id="wpass" name="wpass" placeholder="WiFi password" maxlength="63">
    </div>
    <div class="section-label">ThingsBoard</div>
    <div class="port-row">
      <div class="field">
        <label for="srv">Server</label>
        <input type="text" id="srv" name="srv" placeholder="mqtt.thingsboard.cloud" maxlength="127">
      </div>
      <div class="field">
        <label for="port">Port</label>
        <input type="number" id="port" name="port" placeholder="1883" min="1" max="65535">
      </div>
    </div>
    <div class="field">
      <label for="tok">Device Token</label>
      <input type="text" id="tok" name="tok" placeholder="Access token" maxlength="63">
    </div>
    <button type="submit" class="btn btn-primary">Save &amp; Reboot</button>
  </form>
  <hr class="divider">
  <button id="resetBtn" class="btn btn-danger">Factory Reset</button>
  <div id="status" class="status"></div>
</div>
<script>
  var status = document.getElementById('status');
  fetch('/config')
    .then(function(r) { return r.json(); })
    .then(function(cfg) {
      document.getElementById('ssid').value = cfg.ssid || '';
      document.getElementById('wpass').value = cfg.wpass || '';
      document.getElementById('srv').value = cfg.srv || '';
      document.getElementById('port').value = cfg.port || 1883;
      document.getElementById('tok').value = cfg.tok || '';
    })
    .catch(function() {});
  document.getElementById('configForm').addEventListener('submit', function(e) {
    e.preventDefault();
    status.textContent = 'Saving...';
    status.className = 'status';
    var data = {
      ssid: document.getElementById('ssid').value,
      wpass: document.getElementById('wpass').value,
      srv: document.getElementById('srv').value,
      port: parseInt(document.getElementById('port').value) || 1883,
      tok: document.getElementById('tok').value
    };
    fetch('/save', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(data)
    })
    .then(function(r) {
      if (r.ok) {
        status.textContent = 'Saved. Rebooting...';
        status.className = 'status ok';
      } else {
        throw new Error('Save failed');
      }
    })
    .catch(function() {
      status.textContent = 'Failed to save.';
      status.className = 'status err';
    });
  });
  document.getElementById('resetBtn').addEventListener('click', function() {
    if (!confirm('Factory reset? All settings will be erased.')) return;
    status.textContent = 'Resetting...';
    status.className = 'status';
    fetch('/reset', { method: 'POST' })
      .then(function(r) {
        if (r.ok) {
          status.textContent = 'Reset done. Rebooting...';
          status.className = 'status ok';
        } else {
          throw new Error('Reset failed');
        }
      })
      .catch(function() {
        status.textContent = 'Failed to reset.';
        status.className = 'status err';
      });
  });
</script>
</body>
</html>)rawliteral";

class WebConfig {
public:
    WebConfig(ConfigManager& cfg) : _cfg(cfg), _server(80) {}

    void begin() {
        _server.on("/", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/config", HTTP_GET, [this]() { handleGetConfig(); });
        _server.on("/save", HTTP_POST, [this]() { handleSave(); });
        _server.on("/reset", HTTP_POST, [this]() { handleReset(); });
        _server.begin();
        _running = true;
        Serial.println("[WEB] Config server started on port 80");
    }

    void stop() {
        if (_running) {
            _server.stop();
            _running = false;
            Serial.println("[WEB] Config server stopped");
        }
    }

    void handleClient() {
        if (_running) {
            _server.handleClient();
        }
    }

    bool isRunning() const { return _running; }

private:
    ConfigManager& _cfg;
    WebServer _server;
    bool _running = false;

    void handleRoot() {
        _server.send_P(200, "text/html", CONFIG_PAGE);
    }

    void handleGetConfig() {
        StaticJsonDocument<256> doc;
        doc["ssid"] = _cfg.wifiSsid;
        doc["wpass"] = _cfg.wifiPassword;
        doc["srv"] = _cfg.tbServer;
        doc["port"] = _cfg.tbPort;
        doc["tok"] = _cfg.tbToken;

        String json;
        serializeJson(doc, json);
        _server.send(200, "application/json", json);
    }

    void handleSave() {
        if (!_server.hasArg("plain")) {
            _server.send(400, "text/plain", "No body");
            return;
        }

        StaticJsonDocument<256> doc;
        DeserializationError err = deserializeJson(doc, _server.arg("plain"));
        if (err) {
            _server.send(400, "text/plain", "Invalid JSON");
            return;
        }

        if (doc["ssid"].is<const char*>())
            strlcpy(_cfg.wifiSsid, doc["ssid"].as<const char*>(), sizeof(_cfg.wifiSsid));
        if (doc["wpass"].is<const char*>())
            strlcpy(_cfg.wifiPassword, doc["wpass"].as<const char*>(), sizeof(_cfg.wifiPassword));
        if (doc["srv"].is<const char*>())
            strlcpy(_cfg.tbServer, doc["srv"].as<const char*>(), sizeof(_cfg.tbServer));
        if (doc["port"].is<int>())
            _cfg.tbPort = doc["port"].as<uint16_t>();
        if (doc["tok"].is<const char*>())
            strlcpy(_cfg.tbToken, doc["tok"].as<const char*>(), sizeof(_cfg.tbToken));

        _cfg.save();
        _server.send(200, "text/plain", "OK");

        Serial.println("[WEB] Config saved. Rebooting...");
        delay(500);
        ESP.restart();
    }

    void handleReset() {
        _cfg.factoryReset();
        _server.send(200, "text/plain", "OK");

        Serial.println("[WEB] Factory reset. Rebooting...");
        delay(500);
        ESP.restart();
    }
};

#endif // WEB_CONFIG_H
