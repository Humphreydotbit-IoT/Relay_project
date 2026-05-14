# ESP32 Relay Controller - Technical Documentation

**Firmware Version:** 2.0.0
**Target Board:** ESP32-WROOM-32 (ESP32_RELAYS PCB)
**Framework:** Arduino (PlatformIO)
**IoT Platform:** ThingsBoard (MQTT)

---

## Table of Contents

1. [System Architecture](#1-system-architecture)
2. [WiFi State Machine](#2-wifi-state-machine)
3. [Boot Sequence](#3-boot-sequence)
4. [RPC Command Flow](#4-rpc-command-flow)
5. [State Feedback Architecture](#5-state-feedback-architecture)
6. [Web Configuration Mode](#6-web-configuration-mode)
7. [Relay Hardware](#7-relay-hardware)
8. [Use Cases](#8-use-cases)
9. [File Structure](#9-file-structure)
10. [ThingsBoard Widget Configuration](#10-thingsboard-widget-configuration)
11. [Limitations](#11-limitations)
12. [Design Choices](#12-design-choices)

---

## 1. System Architecture

```
┌──────────────────────────────────────────────────────────────────────┐
│                        ThingsBoard Server                            │
│                  (mqtt.thingsboard.cloud or local)                   │
│                                                                      │
│  ┌─────────────┐    ┌──────────────────┐    ┌────────────────────┐  │
│  │  Dashboard   │    │  Device Profile   │    │  MQTT Broker       │  │
│  │  (Widgets)   │◄──►│  (RPC + Attrs)    │◄──►│  (port 1883)       │  │
│  └──────┬──────┘    └──────────────────┘    └─────────┬──────────┘  │
│         │                                              │             │
└─────────┼──────────────────────────────────────────────┼─────────────┘
          │  Subscribe to                                │
          │  client attribute "rs"                       │  MQTT
          │                                              │
          │  RPC: setValue({relay:N, value:bool})         │
          │                                              │
┌─────────┼──────────────────────────────────────────────┼─────────────┐
│         ▼                                              ▼             │
│  ┌──────────────────────────────────────────────────────────────┐   │
│  │                    ThingsBoard Arduino SDK                    │   │
│  │              (MQTT Client + Server_Side_RPC)                  │   │
│  └──────────────────────────┬───────────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────────▼───────────────────────────────────┐   │
│  │                   ThingsBoardManager                          │   │
│  │  - MQTT connection management                                 │   │
│  │  - RPC callback routing (setValue)                            │   │
│  │  - Client attribute publishing (rs bitmask)                   │   │
│  └──────────────────────────┬───────────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────────▼───────────────────────────────────┐   │
│  │                   RelayController                             │   │
│  │  - GPIO pin management (10 channels)                          │   │
│  │  - Active HIGH logic (BC817 NPN drivers)                      │   │
│  │  - State bitmask encoding                                     │   │
│  │  - NVS flash persistence (namespace: "relay", key: "rs")     │   │
│  └──────────────────────────┬───────────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────────▼───────────────────────────────────┐   │
│  │                    GPIO Output Pins                            │   │
│  │  IO17  IO16  IO4  IO32  IO33  IO14  IO12  IO13  IO25  IO26  │   │
│  │  (R1)  (R2)  (R3) (R4)  (R5)  (R6)  (R7)  (R8)  (R9) (R10) │   │
│  └──────────────────────────┬───────────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────────▼───────────────────────────────────┐   │
│  │  WiFi State Machine (main.cpp)                                │   │
│  │  - AP mode for first-time setup / STA failure fallback        │   │
│  │  - STA mode for normal operation                              │   │
│  │  - Auto-recovery from STA drops                               │   │
│  └──────────────────────────┬───────────────────────────────────┘   │
│                             │                                       │
│  ┌──────────────────────────▼───────────────────────────────────┐   │
│  │  WebConfig (HTTP server on port 80, AP mode only)             │   │
│  │  - Serves config page at 192.168.4.1                          │   │
│  │  - JSON API for get/save config + factory reset               │   │
│  └──────────────────────────────────────────────────────────────┘   │
│                                                                     │
│                     ESP32-WROOM-32 (ESP32_RELAYS PCB)               │
└─────────────────────────────────────────────────────────────────────┘
                              │
                    ┌─────────▼─────────┐
                    │  BC817 NPN Driver  │
                    │  → Relay Coil     │
                    └─────────┬─────────┘
                              │
                    ┌─────────▼─────────┐
                    │   Load (AC/DC)     │
                    └───────────────────┘
```

### Component Responsibilities

| Component | File | Responsibility |
|-----------|------|----------------|
| `main.cpp` | `src/main.cpp` | WiFi state machine, initialization, main loop |
| `ConfigManager` | `include/config_manager.h` | NVS-based runtime configuration (WiFi + ThingsBoard) |
| `RelayController` | `include/relay_controller.h` | GPIO relay control + state persistence |
| `ThingsBoardManager` | `include/thingsboard_manager.h` | MQTT connection + RPC handling |
| `WebConfig` | `include/web_config.h` | HTTP config server (AP mode only) |
| `config.h` | `include/config.h` | Compile-time constants and defaults |

---

## 2. WiFi State Machine

The device uses a 4-state WiFi state machine managed in `main.cpp`:

```
                         ┌─────────────┐
                         │  Power ON /  │
                         │   Reset      │
                         └──────┬──────┘
                                │
                         ┌──────▼──────────┐
                         │ Load config     │
                         │ from NVS        │
                         └──────┬──────────┘
                                │
                    ┌───────────▼───────────┐
                    │  SSID configured?     │
                    └───┬───────────────┬───┘
                        │               │
                       NO              YES
                        │               │
                ┌───────▼──────┐  ┌─────▼────────────────┐
                │ WiFi AP mode │  │ Try STA connection   │
                │ Start web    │  │ (30s timeout)        │
                │ server       │  └─────┬──────────┬─────┘
                │              │        │          │
                │ State:       │     CONNECTED    FAILED
                │ AP_ONLY      │        │          │
                └──────────────┘  ┌─────▼────┐  ┌─▼──────────────┐
                                  │ STA_     │  │ AP_STA mode    │
                                  │ CONNECTED│  │ Start web      │
                                  │          │  │ server + keep  │
                                  │ (Normal  │  │ retrying STA   │
                                  │  ops)    │  │                │
                                  └─────┬────┘  │ State:         │
                                        │       │ AP_STA_FALLBACK│
                                        │       └───────┬────────┘
                                        │               │
                                        │      STA connects
                                        │               │
                                        │       ┌───────▼────────┐
                                        │       │ Stop AP + web  │
                                        │       │ Switch to STA  │
                                        │       └───────┬────────┘
                                        │               │
                                        ◄───────────────┘
                                        │
                                  WiFi drops
                                        │
                                  ┌─────▼────────────┐
                                  │ STA_RECONNECTING │
                                  │ Retry STA every  │
                                  │ 5 seconds        │
                                  └──┬────────────┬──┘
                                     │            │
                               Reconnected   30s timeout
                                     │            │
                                     │    ┌───────▼────────┐
                                     │    │ AP_STA_FALLBACK│
                                     │    │ (start AP +    │
                                     │    │  web server)   │
                                     │    └────────────────┘
                                     │
                               ┌─────▼────┐
                               │ STA_     │
                               │ CONNECTED│
                               └──────────┘
```

### State Summary

| State | WiFi Mode | Web Server | ThingsBoard | When |
|-------|-----------|------------|-------------|------|
| `AP_ONLY` | AP | Running | Off | No SSID configured (factory default) |
| `AP_STA_FALLBACK` | AP+STA | Running | Off | STA failed at boot or reconnect timeout (30s) |
| `STA_CONNECTED` | STA | Off | Active | Normal operation |
| `STA_RECONNECTING` | STA | Off | Off | WiFi dropped, retrying (up to 30s) |

### State Transitions

| From | To | Trigger |
|------|----|---------|
| `AP_ONLY` | *(reboot)* | User saves config via web page → ESP restarts |
| `AP_STA_FALLBACK` | `STA_CONNECTED` | STA connects in background |
| `STA_CONNECTED` | `STA_RECONNECTING` | `WiFi.status() != WL_CONNECTED` |
| `STA_RECONNECTING` | `STA_CONNECTED` | WiFi reconnects |
| `STA_RECONNECTING` | `AP_STA_FALLBACK` | 30s timeout without reconnection |

---

## 3. Boot Sequence

```
                    ┌─────────────┐
                    │  Power ON /  │
                    │   Reset      │
                    └──────┬──────┘
                           │
                    ┌──────▼──────┐
                    │ Serial.begin │
                    │  (115200)    │
                    └──────┬──────┘
                           │
                    ┌──────▼──────────────┐
                    │ ConfigManager.begin()│
                    └──────┬──────────────┘
                           │
                  ┌────────▼────────┐
                  │ NVS "init" flag │
                  │    exists?      │
                  └───┬─────────┬───┘
                      │         │
                     NO        YES
                      │         │
              ┌───────▼──────┐  │
              │ Load defaults │  │
              │ from #defines │  │
              │ Save to NVS   │  │
              │ Set init=true │  │
              └───────┬──────┘  │
                      │   ┌─────▼──────┐
                      │   │ Load from  │
                      │   │   NVS      │
                      │   └─────┬──────┘
                      └────┬────┘
                           │
                    ┌──────▼──────────────────┐
                    │ RelayController.begin()  │
                    │ - Hardcoded 10 pins      │
                    │ - Set GPIO OUTPUT modes  │
                    │ - Load saved bitmask     │
                    │   from NVS ("relay/rs")  │
                    │ - Restore relay states   │
                    │   instantly (no network) │
                    └──────┬──────────────────┘
                           │
                    ┌──────▼──────────────────┐
                    │ WiFi State Decision      │
                    │ (see state machine above)│
                    └──────┬──────────────────┘
                           │
                    ┌──────▼──────┐
                    │  Main Loop  │
                    │  (repeat)   │
                    └─────────────┘
```

---

## 4. RPC Command Flow

When a ThingsBoard dashboard switch widget is toggled:

```
┌──────────┐   RPC: setValue           ┌───────────────┐
│ Dashboard │──────────────────────────►│  ThingsBoard  │
│  Widget   │   {relay:1, value:true}  │    Server     │
│ (Switch)  │                          └───────┬───────┘
└──────────┘                                   │
                                               │ MQTT publish to
                                               │ v1/devices/me/rpc/request/{id}
                                               │
                                        ┌──────▼──────┐
                                        │   ESP32     │
                                        │ onSetValue()│
                                        └──────┬──────┘
                                               │
                                    ┌──────────▼──────────┐
                                    │ Parse JSON params   │
                                    │ relay=1, value=true │
                                    └──────────┬──────────┘
                                               │
                                    ┌──────────▼──────────┐
                                    │ Validate relay index│
                                    │ (1-based → 0-based) │
                                    └──────────┬──────────┘
                                               │
                                    ┌──────────▼──────────┐
                                    │ relay.setState()    │
                                    │ - Set GPIO          │
                                    │ - Save to NVS flash │
                                    └──────────┬──────────┘
                                               │
                              ┌────────────────┴────────────────┐
                              │                                 │
                    ┌─────────▼──────────┐            ┌─────────▼──────────┐
                    │ Publish attribute  │            │ NVS flash saved    │
                    │ "rs" = bitmask     │            │ (survives reboot)  │
                    │ → all widgets sync │            └────────────────────┘
                    └────────────────────┘
```

### RPC Payload Format

**Request** (ThingsBoard → ESP32):
```json
{
  "method": "setValue",
  "params": {
    "relay": 1,
    "value": true
  }
}
```

- `relay`: 1-based relay number (1–10)
- `value`: `true` (ON) or `false` (OFF)

No RPC response is sent. State feedback is handled entirely via client attribute `"rs"` publish.

---

## 5. State Feedback Architecture

### Bitmask Encoding

Relay states are encoded as a single `uint32_t` bitmask in the client attribute `"rs"`:

```
Bit position:   9   8   7   6   5   4   3   2   1   0
Relay number:  10   9   8   7   6   5   4   3   2   1

Example: rs = 517 (decimal) = 0b1000000101
  Relay 1  (bit 0)  = 1 → ON
  Relay 2  (bit 1)  = 0 → OFF
  Relay 3  (bit 2)  = 1 → ON
  Relay 4  (bit 3)  = 0 → OFF
  Relay 5  (bit 4)  = 0 → OFF
  Relay 6  (bit 5)  = 0 → OFF
  Relay 7  (bit 6)  = 0 → OFF
  Relay 8  (bit 7)  = 0 → OFF
  Relay 9  (bit 8)  = 0 → OFF
  Relay 10 (bit 9)  = 1 → ON
```

### When States Are Published

| Event | Published? | Why |
|-------|-----------|-----|
| After RPC setValue | Yes | Sync all widgets immediately |
| On MQTT connect/reconnect | Yes | Sync dashboard with current device state |
| On boot (after NVS restore) | Yes | First connect publishes restored state |
| Periodic heartbeat | No | Not needed — attribute persists on server |

---

## 6. Web Configuration Mode

### When It Runs

The web server runs **only** in AP mode (`AP_ONLY` or `AP_STA_FALLBACK` states). It does not run during normal STA operation to save resources.

### Access

- **AP SSID:** `ESP32_Config`
- **AP Password:** `12345678`
- **Config page:** `http://192.168.4.1`

### HTTP API

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/` | Serve HTML config page |
| `GET` | `/config` | Return current config as JSON |
| `POST` | `/save` | Save config + reboot |
| `POST` | `/reset` | Factory reset + reboot |

### GET /config Response

```json
{
  "ssid": "MyNetwork",
  "wpass": "password123",
  "srv": "mqtt.thingsboard.cloud",
  "port": 1883,
  "tok": "device_access_token"
}
```

### POST /save Request Body

```json
{
  "ssid": "NewNetwork",
  "wpass": "newpassword",
  "srv": "mqtt.thingsboard.cloud",
  "port": 1883,
  "tok": "new_token"
}
```

On success: saves to NVS, responds 200, reboots after 500ms.

### POST /reset

No body required. Clears NVS, responds 200, reboots after 500ms. Next boot loads compile-time defaults (empty SSID → AP_ONLY mode).

### HTML Page

The config page is embedded as a PROGMEM raw string literal in `web_config.h`. It uses a dark theme and includes:

- WiFi SSID and password fields
- ThingsBoard server, port, and token fields
- "Save & Reboot" button
- "Factory Reset" button with confirmation dialog
- Status messages for save/reset operations

---

## 7. Relay Hardware

### ESP32_RELAYS PCB Pin Mapping

The board uses BC817 NPN transistor drivers (active HIGH). GPIO HIGH energizes the relay coil.

| Relay | GPIO | PCB Label |
|-------|------|-----------|
| 1 | IO17 | RELAY1 |
| 2 | IO16 | RELAY2 |
| 3 | IO4 | RELAY3 |
| 4 | IO32 | RELAY4 |
| 5 | IO33 | RELAY5 |
| 6 | IO14 | RELAY6 |
| 7 | IO12 | RELAY7 |
| 8 | IO13 | RELAY8 |
| 9 | IO25 | RELAY9 |
| 10 | IO26 | RELAY10 |

### Active HIGH Logic

```
                  _activeHigh = true (ESP32_RELAYS PCB)
                  ──────────────────────────────────────
Relay ON  (true)  → digitalWrite(pin, HIGH) → NPN conducts → relay energized
Relay OFF (false) → digitalWrite(pin, LOW)  → NPN off      → relay de-energized
```

The BC817 NPN transistor acts as a switch: GPIO HIGH → base current → collector-emitter conducts → relay coil energized.

### Hardcoded Configuration

Relay pins are defined at compile time in `config.h`:

```c
#define RELAY_COUNT 10
#define RELAY_PINS {17, 16, 4, 32, 33, 14, 12, 13, 25, 26}
#define RELAY_ACTIVE_HIGH true
```

These are passed directly to `RelayController::begin()` in `main.cpp`. Changing pin assignments requires recompilation.

### Other Board Variants (commented in config.h)

```c
// ESP32-S3 pins (4 relay):
// #define RELAY_COUNT 4
// #define RELAY_PINS {21, 47, 19, 20}
// #define RELAY_ACTIVE_HIGH false

// ESP32-WROOM-32 generic (4 relay):
// #define RELAY_COUNT 4
// #define RELAY_PINS {16, 17, 18, 19}
// #define RELAY_ACTIVE_HIGH false
```

### State Persistence

Relay states are stored in NVS flash (namespace `"relay"`, key `"rs"`) as a `uint32_t` bitmask. On every state change, the bitmask is saved. On boot, relays are restored to their last state immediately — before WiFi connects.

---

## 8. Use Cases

### UC1: First-Time Setup

1. Power on the device (factory defaults: empty SSID)
2. Device starts in `AP_ONLY` mode
3. Connect phone/laptop to `ESP32_Config` WiFi (password: `12345678`)
4. Open `http://192.168.4.1` in a browser
5. Enter WiFi credentials and ThingsBoard server/token
6. Click "Save & Reboot"
7. Device reboots, connects to WiFi, begins ThingsBoard communication

### UC2: Toggle a Relay from Dashboard

1. User clicks switch widget on ThingsBoard dashboard
2. Widget sends RPC `setValue` with `{relay:N, value:true/false}`
3. ESP32 receives RPC, sets GPIO pin, saves state to NVS flash
4. ESP32 publishes updated `rs` bitmask as client attribute
5. All switch widgets on the dashboard update via attribute subscription

### UC3: Dashboard Refresh / Page Load

1. User opens or refreshes the ThingsBoard dashboard
2. Each switch widget subscribes to client attribute `rs`
3. ThingsBoard server sends the last known `rs` value
4. Each widget's parse function extracts its relay bit from the bitmask
5. Widgets display correct ON/OFF state without polling the device

### UC4: Power Cycle / Reboot

1. ESP32 powers on or resets
2. ConfigManager loads WiFi/TB config from NVS
3. RelayController loads saved relay bitmask from NVS (`relay/rs`)
4. GPIO pins are set to restored states immediately (no network needed)
5. ESP32 connects to WiFi, then MQTT
6. On MQTT connect, publishes current `rs` bitmask to ThingsBoard
7. Dashboard widgets sync with actual device state

### UC5: WiFi Goes Down Temporarily

1. Device is in `STA_CONNECTED` state, operating normally
2. WiFi router reboots or signal drops
3. Device transitions to `STA_RECONNECTING`, retries every 5 seconds
4. If WiFi returns within 30 seconds: reconnects, resumes `STA_CONNECTED`
5. If WiFi stays down >30 seconds: starts AP mode (`AP_STA_FALLBACK`) so user can reconfigure
6. If WiFi returns while in fallback: auto-recovers to `STA_CONNECTED`, stops AP

### UC6: Change WiFi Network

1. Device can't connect to old WiFi → enters `AP_STA_FALLBACK` after 30s
2. Or: perform factory reset (see UC7) to force `AP_ONLY`
3. Connect to `ESP32_Config` AP, open `192.168.4.1`
4. Enter new WiFi credentials
5. Click "Save & Reboot"

### UC7: Factory Reset

1. Connect to device's AP (if available) or wait for AP mode
2. Open `http://192.168.4.1`
3. Click "Factory Reset" and confirm
4. NVS is cleared, device reboots
5. On next boot: empty SSID → `AP_ONLY` mode
6. Relay states reset to all OFF

---

## 9. File Structure

```
relay_pj_mm/
├── platformio.ini              # PlatformIO build config (esp32wroom)
├── merge_bin.py                # Post-build script for merged binary
├── DOCUMENTATION.md            # Technical documentation (this file)
├── USER_GUIDE.md               # User setup and operation guide
├── config_page.html            # HTML design reference (embedded in web_config.h)
├── include/
│   ├── config.h                # Compile-time constants (pins, timeouts, defaults)
│   ├── config_manager.h        # NVS runtime config (WiFi + ThingsBoard)
│   ├── relay_controller.h      # GPIO relay control + state persistence
│   ├── thingsboard_manager.h   # MQTT + RPC handling
│   └── web_config.h            # HTTP config server + embedded HTML
└── src/
    └── main.cpp                # WiFi state machine, setup, main loop
```

### NVS Namespaces and Keys

| Namespace | Key | Type | Description |
|-----------|-----|------|-------------|
| `cfg` | `init` | bool | First-boot flag |
| `cfg` | `ssid` | string | WiFi SSID |
| `cfg` | `wpass` | string | WiFi password |
| `cfg` | `srv` | string | ThingsBoard server |
| `cfg` | `port` | uint16 | ThingsBoard MQTT port |
| `cfg` | `tok` | string | Device access token |
| `relay` | `rs` | uint32 | Relay state bitmask |

---

## 10. ThingsBoard Widget Configuration

### Switch Widget Settings (for Relay N)

| Setting | Value |
|---------|-------|
| **Target device** | Your ESP32 device |
| **RPC set value method** | `setValue` |
| **RPC set value params** | `{"relay":N,"value":{value}}` |
| **Retrieve value using** | Subscribe to attribute |
| **Attribute scope** | Client attribute |
| **Attribute key** | `rs` |
| **Parse value function** | See below |

### Parse Value Function

Each switch widget extracts its relay bit from the bitmask.

**For Relay 1 (bit 0):**
```javascript
return (data >> 0) & 1 ? true : false;
```

**For Relay 2 (bit 1):**
```javascript
return (data >> 1) & 1 ? true : false;
```

**For Relay N (bit N-1):**
```javascript
return (data >> (N-1)) & 1 ? true : false;
```

Where `N` is the 1-based relay number matching the RPC `relay` parameter.

---

## 11. Limitations

### Hardware

| Limitation | Detail |
|------------|--------|
| **No hardware relay feedback** | The ESP32 cannot detect if a relay coil or contact has physically failed. True feedback would require current sensors or contact-side optocouplers. |
| **Max 32 relays** | The bitmask uses `uint32_t`. Extending beyond 32 would require `uint64_t` or multiple attributes. |
| **Board-specific pins** | The 10-relay pin mapping is hardcoded for the ESP32_RELAYS PCB. Using a different board requires editing `config.h` and recompiling. |

### Software

| Limitation | Detail |
|------------|--------|
| **Single RPC method** | Only `setValue` is registered. State is read via client attributes, not RPC. |
| **No TLS/SSL** | MQTT uses plain TCP (port 1883). Not secure over the internet without a VPN or tunnel. |
| **Blocking STA connect at boot** | `tryConnectSTA()` blocks for up to 30 seconds on first boot. Relays are already restored from NVS before this. |
| **NVS flash wear** | Every relay state change writes to NVS. ESP32's wear-leveling handles typical use (a few toggles per day) for years. Extremely frequent toggling (every second) could eventually wear the flash. |
| **No OTA updates** | Firmware updates require USB serial connection. |
| **Static global instance pointer** | `g_tbManagerInstance` is used for RPC callbacks. Only one ThingsBoardManager instance is supported. |
| **Web server only in AP mode** | The config web server is not available during normal STA operation. To reconfigure, the device must be in AP mode. |

---

## 12. Design Choices

### 12.1 WiFi Management: State Machine in main.cpp vs Inside ThingsBoardManager

| Option | Pros | Cons |
|--------|------|------|
| **A. In main.cpp (chosen)** | Clear separation, main.cpp orchestrates WiFi + web server + TB | ThingsBoardManager doesn't know WiFi state |
| **B. In ThingsBoardManager** | Self-contained | Mixes WiFi management with MQTT, harder to add web server |

**Chosen: A** — WiFi state affects multiple components (web server, ThingsBoard). Keeping the state machine in `main.cpp` makes the flow explicit and lets each component focus on its own concern.

### 12.2 Configuration: Web Server (AP Mode) vs Serial Menu

| Option | Pros | Cons |
|--------|------|------|
| **A. Web server in AP mode (chosen)** | No USB needed, phone-friendly, can be done in the field | Requires WiFi AP, more code, HTML embedded in flash |
| **B. Serial +++ menu** | Simple, low overhead | Requires USB cable + terminal software, not field-friendly |

**Chosen: A** — The device may be installed in locations without easy serial access. A web config page accessible via phone WiFi is more practical for deployment and end-user reconfiguration.

### 12.3 State Encoding: Bitmask vs Individual Attributes

| Option | Pros | Cons |
|--------|------|------|
| **A. Bitmask uint32 (chosen)** | One MQTT message, compact, scales to 32 relays | Requires parse function per widget |
| **B. Individual attributes** | Simple per widget | N attributes, N messages per update |

**Chosen: A** — A single MQTT publish carries state of all 10 relays. Efficient for bandwidth and processing. Each widget needs one line of JavaScript to extract its bit.

### 12.4 State Persistence: ESP32 Flash vs ThingsBoard Server

| Option | Pros | Cons |
|--------|------|------|
| **A. ESP32 NVS flash (chosen)** | Instant restore on boot, no network dependency | Flash wear (mitigated by wear-leveling) |
| **B. Request from server on boot** | No flash writes | Requires network, fails if server is down |

**Chosen: A** — Relays restore to last state immediately on power-up, before WiFi connects. Critical for physical loads (lights, pumps) where a reboot should not disrupt equipment.

### 12.5 AP Behavior: Always-On vs On-Demand

| Option | Pros | Cons |
|--------|------|------|
| **A. On-demand (chosen)** | Saves resources, no unnecessary AP when STA works | Must wait for AP mode to reconfigure |
| **B. Always-on AP** | Config always accessible | Uses more power, potential security surface |

**Chosen: A** — AP runs only when needed: first-time setup, STA boot failure, or STA reconnect timeout. Once STA connects, AP stops. This minimizes resource usage and attack surface.

### 12.6 Relay Pins: Hardcoded vs Runtime Configurable

| Option | Pros | Cons |
|--------|------|------|
| **A. Hardcoded (chosen)** | Simple, no misconfiguration risk, pin mapping matches PCB | Requires recompilation for different board |
| **B. Runtime via web config** | Flexible | User could set wrong pins, crash the device |

**Chosen: A** — The ESP32_RELAYS PCB has a fixed pin layout. Runtime configuration adds complexity and risk of misconfiguration. Different boards get different firmware builds with appropriate pin defines.

---

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| ThingsBoard Arduino SDK | ^0.14.0 | MQTT client + RPC + attribute handling |
| ArduinoJson | ^6.21.5 | JSON parsing for RPC params and web config API |
| PubSubClient | ^2.8 | Low-level MQTT (ThingsBoard SDK dependency) |
| ArduinoHttpClient | ^0.6.1 | HTTP client (ThingsBoard SDK dependency) |
| Preferences (built-in) | — | ESP32 NVS flash storage |
| WebServer (built-in) | — | HTTP server for config page |

---

*ESP32 Relay Controller v2.0.0 — ESP32_RELAYS PCB (10-channel)*
