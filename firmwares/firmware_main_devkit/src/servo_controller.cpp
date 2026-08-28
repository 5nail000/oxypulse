#include "servo_controller.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "gpio_controller.h"
#include "logger.h"

namespace {

struct ServoEntry {
    uint8_t pin = 0;
    uint8_t angle = 0;
    bool auto_enabled = false;
    uint32_t pause_ms = SERVO_AUTO_PAUSE_MS;
    uint8_t channel = 0;
    bool active = false;
    uint32_t phase_start_ms = 0;
    bool phase_high = false;
};

ServoEntry g_servos[MAX_SERVOS];
TaskHandle_t g_auto_task = nullptr;
uint8_t g_next_channel = 0;

uint16_t angleToPulseUs(uint8_t angle) {
    if (angle > SERVO_ANGLE_MAX) {
        angle = SERVO_ANGLE_MAX;
    }
    return static_cast<uint16_t>(
        PULSE_US_MIN +
        (static_cast<uint32_t>(angle) * (PULSE_US_MAX - PULSE_US_MIN)) / SERVO_ANGLE_MAX);
}

void writePulse(uint8_t channel, uint16_t pulse_us) {
    const uint32_t duty =
        (static_cast<uint32_t>(pulse_us) * ((1U << SERVO_RESOLUTION_BITS) - 1U)) / SERVO_PERIOD_US;
    ledcWrite(channel, duty);
}

ServoEntry *findServo(uint8_t pin) {
    for (size_t i = 0; i < MAX_SERVOS; ++i) {
        if (g_servos[i].active && g_servos[i].pin == pin) {
            return &g_servos[i];
        }
    }
    return nullptr;
}

ServoEntry *allocServo(uint8_t pin) {
    ServoEntry *existing = findServo(pin);
    if (existing != nullptr) {
        return existing;
    }
    for (size_t i = 0; i < MAX_SERVOS; ++i) {
        if (!g_servos[i].active) {
            g_servos[i].pin = pin;
            g_servos[i].angle = 0;
            g_servos[i].auto_enabled = false;
            g_servos[i].pause_ms = SERVO_AUTO_PAUSE_MS;
            g_servos[i].channel = g_next_channel % 8;
            ++g_next_channel;
            g_servos[i].active = true;
            g_servos[i].phase_start_ms = millis();
            g_servos[i].phase_high = false;

            ledcSetup(g_servos[i].channel, SERVO_FREQ_HZ, SERVO_RESOLUTION_BITS);
            ledcAttachPin(pin, g_servos[i].channel);
            return &g_servos[i];
        }
    }
    return nullptr;
}

void applyAngle(ServoEntry *entry, uint8_t angle) {
    entry->angle = angle;
    writePulse(entry->channel, angleToPulseUs(angle));
}

void autoTask(void *param) {
    (void)param;
    for (;;) {
        const uint32_t now = millis();
        for (size_t i = 0; i < MAX_SERVOS; ++i) {
            ServoEntry &s = g_servos[i];
            if (!s.active || !s.auto_enabled) {
                continue;
            }
            if ((now - s.phase_start_ms) >= s.pause_ms) {
                s.phase_high = !s.phase_high;
                s.phase_start_ms = now;
                const uint8_t angle = s.phase_high ? SERVO_ANGLE_MAX : 0;
                applyAngle(&s, angle);
                logPrintf("servo auto: pin %u -> %u deg", s.pin, angle);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

}  // namespace

void servoControllerInit() {
    xTaskCreatePinnedToCore(autoTask, "servoAuto", 4096, nullptr, 1, &g_auto_task, 1);
}

bool servoSetAngle(uint8_t pin, uint8_t angle) {
    if (!gpioIsPinAllowed(pin)) {
        logPrintf("servo: pin %u не разрешён", pin);
        return false;
    }
    if (angle > SERVO_ANGLE_MAX) {
        angle = SERVO_ANGLE_MAX;
    }

    ServoEntry *entry = allocServo(pin);
    if (entry == nullptr) {
        logPrintf("servo: нет слота для pin %u", pin);
        return false;
    }

    entry->auto_enabled = false;
    applyAngle(entry, angle);
    logPrintf("servo: pin %u angle %u (%u us)", pin, angle, angleToPulseUs(angle));
    return true;
}

bool servoSetAuto(uint8_t pin, bool enabled, uint32_t pause_ms) {
    if (!gpioIsPinAllowed(pin)) {
        logPrintf("servo auto: pin %u не разрешён", pin);
        return false;
    }

    ServoEntry *entry = allocServo(pin);
    if (entry == nullptr) {
        logPrintf("servo auto: нет слота для pin %u", pin);
        return false;
    }

    if (pause_ms < 500) {
        pause_ms = 500;
    }
    if (pause_ms > 60000) {
        pause_ms = 60000;
    }

    entry->auto_enabled = enabled;
    entry->pause_ms = pause_ms;
    entry->phase_start_ms = millis();
    entry->phase_high = false;

    if (enabled) {
        applyAngle(entry, 0);
        logPrintf("servo auto: pin %u ON, pause %lu ms", pin, static_cast<unsigned long>(pause_ms));
    } else {
        logPrintf("servo auto: pin %u OFF", pin);
    }
    return true;
}

size_t servoExportEntries(uint8_t *pins, uint8_t *angles, bool *auto_flags, size_t max_count) {
    size_t count = 0;
    for (size_t i = 0; i < MAX_SERVOS && count < max_count; ++i) {
        if (g_servos[i].active) {
            pins[count] = g_servos[i].pin;
            angles[count] = g_servos[i].angle;
            auto_flags[count] = g_servos[i].auto_enabled;
            ++count;
        }
    }
    return count;
}
