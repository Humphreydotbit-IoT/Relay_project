# ESP32 Relay Controller - User Guide

**Firmware:** v2.0.0 | **Board:** ESP32_RELAYS PCB (10-channel)

---

## Table of Contents

1. [Quick Start](#1-quick-start)
2. [WiFi Behavior](#2-wifi-behavior)
3. [Accessing the Config Page](#3-accessing-the-config-page)
4. [Configuration Fields](#4-configuration-fields)
5. [ThingsBoard Setup](#5-thingsboard-setup)
6. [Changing WiFi Network](#6-changing-wifi-network)
7. [Factory Reset](#7-factory-reset)
8. [Flash Erase vs Factory Reset](#8-flash-erase-vs-factory-reset)
9. [Flashing Firmware](#9-flashing-firmware)
10. [Troubleshooting](#10-troubleshooting)

---

## 1. Quick Start

### What You Need

- ESP32_RELAYS PCB board
- A phone or laptop with WiFi
- A ThingsBoard account (cloud or local server)
- A ThingsBoard device access token

### First-Time Setup (3 Steps)

**Step 1:** Power on the board. It starts in setup mode automatically.

**Step 2:** On your phone/laptop, connect to this WiFi network:
- **Network name:** `ESP32_Config`
- **Password:** `12345678`

**Step 3:** Open a browser and go to: **http://192.168.4.1**

Fill in:
- **WiFi SSID** — your WiFi network name
- **WiFi Password** — your WiFi password
- **ThingsBoard Server** — `mqtt.thingsboard.cloud` (or your local server IP)
- **Port** — `1883`
- **Device Token** — your device's access token from ThingsBoard

Click **"Save & Reboot"**. The device will restart and connect to your WiFi and ThingsBoard.

---

## 2. WiFi Behavior

The device manages WiFi automatically. Here's what happens in different situations:

### Brand New Device / After Factory Reset

No WiFi is configured. The device creates its own WiFi network (`ESP32_Config`) so you can configure it.

### Normal Operation

The device connects to your WiFi and communicates with ThingsBoard. No setup WiFi network is visible.

### WiFi Goes Down Briefly (< 30 seconds)

The device retries automatically. If WiFi comes back within 30 seconds, it reconnects without any action needed. No setup network appears.

### WiFi Down for More Than 30 Seconds

The device starts its setup WiFi network (`ESP32_Config`) so you can reconfigure if needed. **It continues retrying your WiFi in the background.** If your WiFi comes back, the device auto-recovers and shuts down the setup network.

### WiFi Password Changed / Wrong Credentials

The device can't connect, waits 30 seconds, then starts the setup network. Connect to `ESP32_Config` and enter the new WiFi credentials.

### Summary Table

| Situation | What Happens | `ESP32_Config` Visible? |
|-----------|-------------|------------------------|
| First power on (factory) | Setup mode | Yes |
| Connected to WiFi | Normal operation | No |
| WiFi drops < 30s | Auto-reconnect | No |
| WiFi drops > 30s | Setup network starts + keeps retrying | Yes |
| Wrong WiFi credentials | Setup network after 30s | Yes |
| WiFi router comes back | Auto-recovers, stops setup network | Disappears |

---

## 3. Accessing the Config Page

### When Is the Config Page Available?

The config page is only accessible when the device is broadcasting the `ESP32_Config` WiFi network. This happens:

- On first boot (no WiFi configured)
- When the device can't connect to WiFi for 30+ seconds
- After a factory reset

### How to Access

1. Look for `ESP32_Config` in your WiFi networks
2. Connect to it (password: `12345678`)
3. Open a browser
4. Go to: **http://192.168.4.1**

### Finding the IP Without Serial Access

The config page IP is **always** `192.168.4.1`. This is the standard ESP32 access point IP and does not change. You do not need serial access to find it.

If the page doesn't load:
- Make sure you're connected to `ESP32_Config`, not your regular WiFi
- Your phone may try to switch back to mobile data — disable mobile data temporarily
- Try `http://192.168.4.1` (not https)

---

## 4. Configuration Fields

| Field | Description | Example |
|-------|-------------|---------|
| **SSID** | Your WiFi network name | `MyHomeWiFi` |
| **Password** | Your WiFi password | `mypassword123` |
| **Server** | ThingsBoard MQTT server address | `mqtt.thingsboard.cloud` |
| **Port** | MQTT port number | `1883` |
| **Device Token** | ThingsBoard device access token | `A1_TEST_TOKEN` |

### Where to Find the Device Token

1. Log in to ThingsBoard
2. Go to **Devices**
3. Click on your device
4. Go to **Details** tab
5. Click **Copy Access Token**

---

## 5. ThingsBoard Setup

### Creating a Device

1. Log in to ThingsBoard (cloud: https://thingsboard.cloud)
2. Go to **Devices** → **Add new device**
3. Give it a name (e.g., "Relay Controller")
4. Copy the **Access Token** — you'll need this for the ESP32 config page

### Adding Switch Widgets (for each relay)

1. Go to **Dashboards** → Create or open a dashboard
2. Click **Edit** → **Add widget** → **Control widgets** → **Switch**
3. Configure the widget:

| Setting | Value |
|---------|-------|
| Target device | Your relay controller device |
| RPC set value method | `setValue` |
| RPC set value params | `{"relay":1,"value":{value}}` |
| Retrieve value using | Subscribe to attribute |
| Attribute scope | Client attribute |
| Attribute key | `rs` |
| Parse value function | `return (data >> 0) & 1 ? true : false;` |

4. For **Relay 2**, change:
   - RPC params to `{"relay":2,"value":{value}}`
   - Parse function to `return (data >> 1) & 1 ? true : false;`

5. For **Relay N**, change:
   - RPC params to `{"relay":N,"value":{value}}`
   - Parse function to `return (data >> (N-1)) & 1 ? true : false;`

### Quick Reference: Parse Functions for All 10 Relays

| Relay | RPC Params | Parse Function |
|-------|-----------|---------------|
| 1 | `{"relay":1,"value":{value}}` | `return (data >> 0) & 1 ? true : false;` |
| 2 | `{"relay":2,"value":{value}}` | `return (data >> 1) & 1 ? true : false;` |
| 3 | `{"relay":3,"value":{value}}` | `return (data >> 2) & 1 ? true : false;` |
| 4 | `{"relay":4,"value":{value}}` | `return (data >> 3) & 1 ? true : false;` |
| 5 | `{"relay":5,"value":{value}}` | `return (data >> 4) & 1 ? true : false;` |
| 6 | `{"relay":6,"value":{value}}` | `return (data >> 5) & 1 ? true : false;` |
| 7 | `{"relay":7,"value":{value}}` | `return (data >> 6) & 1 ? true : false;` |
| 8 | `{"relay":8,"value":{value}}` | `return (data >> 7) & 1 ? true : false;` |
| 9 | `{"relay":9,"value":{value}}` | `return (data >> 8) & 1 ? true : false;` |
| 10 | `{"relay":10,"value":{value}}` | `return (data >> 9) & 1 ? true : false;` |

---

## 6. Changing WiFi Network

### Option A: Wait for AP Mode

If the device can't connect to the old WiFi (router changed, moved to new location):

1. Power on the device
2. Wait 30 seconds — the device will start `ESP32_Config` WiFi
3. Connect to `ESP32_Config` (password: `12345678`)
4. Go to `http://192.168.4.1`
5. Enter new WiFi credentials
6. Click "Save & Reboot"

### Option B: Factory Reset First

If you want to start fresh:

1. Connect to the device's `ESP32_Config` AP (if available)
2. Go to `http://192.168.4.1`
3. Click **"Factory Reset"** and confirm
4. Device reboots into setup mode
5. Connect to `ESP32_Config` and configure from scratch

---

## 7. Factory Reset

Factory reset erases all saved settings (WiFi credentials, ThingsBoard config) and resets relay states to all OFF.

### How to Factory Reset

1. Access the config page (see [Section 3](#3-accessing-the-config-page))
2. Click the **"Factory Reset"** button at the bottom
3. Confirm when prompted
4. Device reboots into setup mode (AP_ONLY)

### What Gets Reset

| Data | After Factory Reset |
|------|-------------------|
| WiFi SSID & password | Cleared (empty) |
| ThingsBoard server | Reset to `mqtt.thingsboard.cloud` |
| ThingsBoard port | Reset to `1883` |
| Device token | Cleared (empty) |
| Relay states | All OFF |

---

## 8. Flash Erase vs Factory Reset

There are two ways to reset the device. Use the right one for your situation.

### Factory Reset (via Web Page)

- **How:** Click "Factory Reset" on the config page at `192.168.4.1`
- **What it does:** Clears saved settings in NVS flash. Device loads compile-time defaults on next boot.
- **When to use:** Changing WiFi, reconfiguring from scratch, giving device to someone else.
- **Requires:** WiFi connection to device's AP + browser.

### Flash Erase (via USB + esptool)

- **How:** Connect USB cable, run `esptool.py erase_flash`
- **What it does:** Erases **everything** on the ESP32 flash — firmware, NVS, all data. Device will not boot until firmware is reflashed.
- **When to use:**
  - Before flashing a **new firmware version** that changed default settings (ensures old NVS values don't persist)
  - If the device is completely unresponsive (boot loop, crash)
  - If factory reset via web page is not accessible
- **Requires:** USB cable, computer with esptool.py installed.

### When to Use Which

| Scenario | Use |
|----------|-----|
| Change WiFi network | Factory Reset |
| Give device to new user | Factory Reset |
| Update firmware (same defaults) | Just flash new firmware |
| Update firmware (changed defaults) | Flash erase → flash new firmware |
| Device won't boot / boot loop | Flash erase → flash new firmware |
| Can't access config page at all | Flash erase → flash new firmware |

### Flash Erase Commands

```bash
# Find connected port
ls /dev/ttyUSB*        # Linux
ls /dev/cu.usb*        # macOS

# Erase entire flash
esptool.py --chip esp32 --port /dev/ttyUSB0 erase_flash

# Then reflash firmware
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash 0x0 firmware_merged.bin
```

---

## 9. Flashing Firmware

### Using Merged Binary (Recommended)

The build produces a single `firmware_merged.bin` file that includes bootloader, partition table, and application.

```bash
esptool.py --chip esp32 --port /dev/ttyUSB0 --baud 921600 write_flash 0x0 firmware_merged.bin
```

### Using PlatformIO

If you have the source code and PlatformIO installed:

```bash
pio run -t upload
```

### After Flashing New Firmware

- If this is the **first time** flashing (or after flash erase): device starts in setup mode automatically
- If the device already had settings saved: old settings are kept (NVS persists across firmware updates). To clear them, do a factory reset or flash erase.

---

## 10. Troubleshooting

### "ESP32_Config" WiFi doesn't appear

- **Wait 30 seconds** after powering on. The device tries to connect to saved WiFi first.
- If it still doesn't appear, the device may be connected to WiFi successfully (normal operation).
- If you need to reconfigure: do a flash erase and reflash to force setup mode.

### Config page doesn't load at 192.168.4.1

- Make sure you're connected to `ESP32_Config`, not your home WiFi
- Disable mobile data on your phone (Android may prefer mobile data over an AP with no internet)
- Use `http://` not `https://`
- Try a different browser

### Device connects to WiFi but not to ThingsBoard

- Verify the **device token** is correct (copy from ThingsBoard device details)
- Verify the **server address**: `mqtt.thingsboard.cloud` for ThingsBoard Cloud, or your server's IP for local installations
- Verify **port 1883** is not blocked by your network/firewall
- Check ThingsBoard: device should show "Active" if connected

### Relays don't respond to dashboard commands

- Check that the device shows "Active" in ThingsBoard
- Verify widget RPC params: `{"relay":N,"value":{value}}` (N = relay number 1-10)
- Verify widget parse function: `return (data >> (N-1)) & 1 ? true : false;`
- Check that attribute key is `rs` and scope is "Client attribute"

### Relays are in wrong state after power cycle

- Relay states are saved to flash and restored on boot. If a relay should be OFF but turns ON after reboot, verify the correct state was saved before the power loss.
- If relay states seem corrupted: do a factory reset (resets all relays to OFF).

### Device keeps rebooting (boot loop)

- This usually means the firmware doesn't match the board. The 10-relay firmware is for ESP32-WROOM-32 only, **not** ESP32-S3.
- Fix: connect USB, do a flash erase, then flash the correct firmware for your board.

---

*ESP32 Relay Controller v2.0.0*
