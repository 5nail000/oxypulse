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
        // Логическое состояние для API: тап — импульс, после него всегда OFF.
        entry->state = false;
        pinMode(pin, OUTPUT);
        digitalWrite(pin, HIGH);
        logPrintf("GPIO: pin %u TAP HIGH %lu ms",
                  pin,
                  static_cast<unsigned long>(pulse_ms));
        vTaskDelay(pdMS_TO_TICKS(pulse_ms));
        digitalWrite(pin, LOW);
        entry->state = false;
        logPrintf("GPIO: pin %u -> OFF (tap)", pin);
    }

    g_tap_busy = false;
    vTaskDelete(nullptr);
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

bool gpioSet(uint8_t pin, bool state) {
    if (!gpioIsPinAllowed(pin)) {
        logPrintf("GPIO: pin %u не разрешён", pin);
        return false;
    }

    GpioEntry *entry = allocEntry(pin);
    if (entry == nullptr) {
        logPrintf("GPIO: нет слота для pin %u", pin);
        return false;
    }

    pinMode(pin, OUTPUT);
    digitalWrite(pin, state ? HIGH : LOW);
    entry->state = state;
    logPrintf("GPIO: pin %u -> %s", pin, state ? "ON" : "OFF");
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

    if (pulse_ms == 0) {
        pulse_ms = GPIO_TAP_MS;
    }
    if (pulse_ms > GPIO_TAP_MAX_MS) {
        pulse_ms = GPIO_TAP_MAX_MS;
    }

    if (allocEntry(pin) == nullptr) {
        logPrintf("GPIO: нет слота для pin %u", pin);
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
