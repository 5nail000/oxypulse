#include "gpio_controller.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "logger.h"

namespace {

struct GpioEntry {
    uint8_t pin = 0;
    bool state = false;
    bool active = false;
};

struct TapRequest {
    uint8_t pin = 0;
    uint32_t pulse_ms = 0;
};

GpioEntry g_entries[MAX_GPIO_TRACKED];
volatile bool g_tap_busy = false;

GpioEntry *findEntry(uint8_t pin) {
    for (size_t i = 0; i < MAX_GPIO_TRACKED; ++i) {
        if (g_entries[i].active && g_entries[i].pin == pin) {
            return &g_entries[i];
        }
    }
    return nullptr;
}

bool isValvePin(uint8_t pin) {
    return pin == VALVE_IN_HYPO_PIN || pin == VALVE_IN_HYPER_PIN || pin == VALVE_IN_WORK_PIN ||
           pin == VALVE_OUT_DEP_HYPO_PIN || pin == VALVE_OUT_DEP_HYPER_PIN ||
           pin == VALVE_OUT_RICH_HYPER_PIN;
}

uint8_t lineLevelForPin(uint8_t pin, bool logical_on) {
    const bool active_low = GPIO_VALVE_ACTIVE_LOW_B && isValvePin(pin);
    if (active_low) {
        return logical_on ? LOW : HIGH;
    }
    return logical_on ? HIGH : LOW;
}

void writePinLevel(uint8_t pin, bool logical_on) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, lineLevelForPin(pin, logical_on));
}

GpioEntry *allocEntry(uint8_t pin) {
    GpioEntry *existing = findEntry(pin);
    if (existing != nullptr) {
        return existing;
    }
    for (size_t i = 0; i < MAX_GPIO_TRACKED; ++i) {
        if (!g_entries[i].active) {
            g_entries[i].pin = pin;
            g_entries[i].state = false;
            g_entries[i].active = true;
            return &g_entries[i];
        }
    }
    return nullptr;
}

void tapTask(void *param) {
    TapRequest *req = static_cast<TapRequest *>(param);
    if (req == nullptr) {
        g_tap_busy = false;
        vTaskDelete(nullptr);
        return;
    }

    const uint8_t pin = req->pin;
    const uint32_t pulse_ms = req->pulse_ms;
    delete req;

    GpioEntry *entry = allocEntry(pin);
    if (entry != nullptr) {
        if (pin == O2_MOTOR_PIN) {
            pinMode(pin, OUTPUT);
            digitalWrite(pin, HIGH);
            logPrintf("GPIO: pin %u TAP HIGH %lu ms",
                      pin,
                      static_cast<unsigned long>(pulse_ms));
            vTaskDelay(pdMS_TO_TICKS(pulse_ms));
            digitalWrite(pin, LOW);
            entry->state = !entry->state;
            logPrintf("GPIO: pin %u motor -> %s (toggle tap)", pin, entry->state ? "ON" : "OFF");
        } else {
            writePinLevel(pin, true);
            logPrintf("GPIO: pin %u TAP ON %lu ms",
                      pin,
                      static_cast<unsigned long>(pulse_ms));
            vTaskDelay(pdMS_TO_TICKS(pulse_ms));
            writePinLevel(pin, false);
            entry->state = false;
            logPrintf("GPIO: pin %u -> OFF (tap)", pin);
        }
    }

    g_tap_busy = false;
    vTaskDelete(nullptr);
}

GpioEntry *motorEntry() {
    if (!gpioIsPinAllowed(O2_MOTOR_PIN)) {
        return nullptr;
    }
    return allocEntry(O2_MOTOR_PIN);
}

bool waitMotorTapIdle(uint32_t timeout_ms) {
    const uint32_t wait_start = millis();
    while (g_tap_busy && (millis() - wait_start) < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return !g_tap_busy;
}

bool o2MotorTapSync() {
    if (!waitMotorTapIdle(500U)) {
        logPrintf("GPIO: motor tap busy");
        return false;
    }

    GpioEntry *entry = motorEntry();
    if (entry == nullptr) {
        logPrintf("GPIO: нет слота для motor pin");
        return false;
    }

    pinMode(O2_MOTOR_PIN, OUTPUT);
    digitalWrite(O2_MOTOR_PIN, HIGH);
    logPrintf("GPIO: pin %u motor TAP %lu ms",
              static_cast<unsigned>(O2_MOTOR_PIN),
              static_cast<unsigned long>(O2_MOTOR_TAP_MS));
    vTaskDelay(pdMS_TO_TICKS(O2_MOTOR_TAP_MS));
    digitalWrite(O2_MOTOR_PIN, LOW);
    entry->state = !entry->state;
    logPrintf("GPIO: pin %u motor -> %s (sync tap)", static_cast<unsigned>(O2_MOTOR_PIN),
              entry->state ? "ON" : "OFF");
    return true;
}

}  // namespace

bool gpioIsPinAllowed(uint8_t pin) {
    for (size_t i = 0; i < GPIO_WHITELIST_SIZE; ++i) {
        if (GPIO_WHITELIST[i] == pin) {
            return true;
        }
    }
    return false;
}

void valvesInit() {
    static const uint8_t k_valve_pins[] = {
        VALVE_IN_HYPO_PIN,
        VALVE_IN_HYPER_PIN,
        VALVE_IN_WORK_PIN,
        VALVE_OUT_DEP_HYPO_PIN,
        VALVE_OUT_DEP_HYPER_PIN,
        VALVE_OUT_RICH_HYPER_PIN,
    };
    for (uint8_t pin : k_valve_pins) {
        GpioEntry *entry = allocEntry(pin);
        if (entry != nullptr) {
            entry->state = false;
            writePinLevel(pin, false);
        }
    }
    logPrintf("GPIO: valves init OFF (active_%s)",
              GPIO_VALVE_ACTIVE_LOW_B ? "LOW" : "HIGH");
}

bool gpioSet(uint8_t pin, bool state) {
    if (!gpioIsPinAllowed(pin)) {
        logPrintf("GPIO: pin %u не разрешён", pin);
        return false;
    }
    if (pin == SERVO1_PIN) {
        logPrintf("GPIO: pin %u занят Servo1 (PWM)", pin);
        return false;
    }
    if (pin == O2_MOTOR_PIN) {
        logPrintf("GPIO: pin %u мотор — только tap (toggle)", pin);
        return false;
    }

    GpioEntry *entry = allocEntry(pin);
    if (entry == nullptr) {
        logPrintf("GPIO: pin %u нет слота", pin);
        return false;
    }

    writePinLevel(pin, state);
    entry->state = state;
    logPrintf("GPIO: pin %u -> %s (line=%s)",
              pin,
              state ? "ON" : "OFF",
              lineLevelForPin(pin, state) ? "HIGH" : "LOW");
    return true;
}

bool gpioTap(uint8_t pin, uint32_t pulse_ms) {
    if (g_tap_busy) {
        logPrintf("GPIO: tap already in progress");
        return false;
    }

    if (!gpioIsPinAllowed(pin)) {
        logPrintf("GPIO: pin %u не разрешён", pin);
        return false;
    }
    if (pin == SERVO1_PIN) {
        logPrintf("GPIO: pin %u занят Servo1 (PWM)", pin);
        return false;
    }

    if (pulse_ms == 0) {
        pulse_ms = GPIO_TAP_MS;
    }
    if (pulse_ms > GPIO_TAP_MAX_MS) {
        pulse_ms = GPIO_TAP_MAX_MS;
    }

    if (allocEntry(pin) == nullptr) {
        logPrintf("GPIO: pin %u нет слота", pin);
        return false;
    }

    TapRequest *req = new TapRequest();
    req->pin = pin;
    req->pulse_ms = pulse_ms;
    g_tap_busy = true;
    if (xTaskCreate(tapTask, "gpioTap", 2048, req, 1, nullptr) != pdPASS) {
        g_tap_busy = false;
        delete req;
        logPrintf("GPIO: failed to start tap task");
        return false;
    }

    return true;
}

bool gpioTapInProgress() {
    return g_tap_busy;
}

bool gpioGet(uint8_t pin, bool *state) {
    if (state == nullptr) {
        return false;
    }
    const GpioEntry *entry = findEntry(pin);
    if (entry == nullptr) {
        return false;
    }
    *state = entry->state;
    return true;
}

size_t gpioExportEntries(uint8_t *pins, bool *states, size_t max_count) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_GPIO_TRACKED && count < max_count; ++i) {
        if (g_entries[i].active) {
            pins[count] = g_entries[i].pin;
            states[count] = g_entries[i].state;
            ++count;
        }
    }
    return count;
}

void o2MotorInit() {
    GpioEntry *entry = motorEntry();
    if (entry == nullptr) {
        logPrintf("GPIO: motor init failed, no slot");
        return;
    }
    entry->state = false;
    pinMode(O2_MOTOR_PIN, OUTPUT);
    digitalWrite(O2_MOTOR_PIN, LOW);
    logPrintf("GPIO: pin %u motor init OFF (logical + line LOW)", static_cast<unsigned>(O2_MOTOR_PIN));
}

bool o2MotorIsOn() {
    bool on = false;
    if (!gpioGet(O2_MOTOR_PIN, &on)) {
        return false;
    }
    return on;
}

bool o2MotorEnsureOn() {
    GpioEntry *entry = motorEntry();
    if (entry == nullptr) {
        return false;
    }
    if (entry->state) {
        return true;
    }
    if (!o2MotorTapSync()) {
        return false;
    }
    return entry->state;
}

bool o2MotorForceOff() {
    GpioEntry *entry = motorEntry();
    if (entry == nullptr) {
        return false;
    }
    if (entry->state) {
        if (!o2MotorTapSync()) {
            entry->state = false;
            digitalWrite(O2_MOTOR_PIN, LOW);
            return false;
        }
    }
    entry->state = false;
    pinMode(O2_MOTOR_PIN, OUTPUT);
    digitalWrite(O2_MOTOR_PIN, LOW);
    logPrintf("GPIO: pin %u motor forced OFF", static_cast<unsigned>(O2_MOTOR_PIN));
    return true;
}
