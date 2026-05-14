#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include <Preferences.h>
#include "config.h"

class RelayController {
public:
    void begin(const uint8_t* pins, uint8_t count, bool activeHigh) {
        _count = count > MAX_RELAYS ? MAX_RELAYS : count;
        _activeHigh = activeHigh;
        memcpy(_pins, pins, _count);

        _prefs.begin("relay", false);
        uint32_t saved = _prefs.getUInt("rs", 0);
        Serial.printf("[Relay] Loaded saved state from flash: rs=%u (0b", saved);
        for (int i = _count - 1; i >= 0; i--) {
            Serial.print((saved >> i) & 1);
        }
        Serial.println(")");

        _initialized = false;
        for (uint8_t i = 0; i < _count; i++) {
            pinMode(_pins[i], OUTPUT);
            bool state = (saved >> i) & 1;
            setState(i, state);
        }
        _initialized = true;
        Serial.printf("[Relay] Initialized %d relays from flash\n", _count);
    }

    bool setState(uint8_t index, bool state) {
        if (index >= _count) {
            Serial.printf("[Relay] Error: Index %d out of range\n", index);
            return false;
        }

        _states[index] = state;

        if (_activeHigh) {
            digitalWrite(_pins[index], state ? HIGH : LOW);
        } else {
            digitalWrite(_pins[index], state ? LOW : HIGH);
        }

        Serial.printf("[Relay] Relay %d set to %s\n", index + 1, state ? "ON" : "OFF");
        saveState();
        return true;
    }

    bool getState(uint8_t index) const {
        if (index >= _count) return false;
        return _states[index];
    }

    bool toggle(uint8_t index) {
        if (index >= _count) return false;
        return setState(index, !_states[index]);
    }

    void setAll(bool state) {
        for (uint8_t i = 0; i < _count; i++) {
            setState(i, state);
        }
    }

    uint8_t getCount() const { return _count; }
    uint8_t getPin(uint8_t index) const { return index < _count ? _pins[index] : 0; }

    uint32_t getStateBitmask() const {
        uint32_t mask = 0;
        for (uint8_t i = 0; i < _count; i++) {
            if (_states[i]) {
                mask |= (1U << i);
            }
        }
        return mask;
    }

private:
    uint8_t _pins[MAX_RELAYS] = {};
    bool _states[MAX_RELAYS] = {};
    uint8_t _count = 0;
    bool _activeHigh = false;
    bool _initialized = false;
    Preferences _prefs;

    void saveState() {
        if (!_initialized) return;
        uint32_t mask = getStateBitmask();
        _prefs.putUInt("rs", mask);
        Serial.printf("[Relay] Saved to flash: rs=%u\n", mask);
    }
};

#endif // RELAY_CONTROLLER_H
