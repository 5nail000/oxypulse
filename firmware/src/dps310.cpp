#include "dps310.h"

#include <Adafruit_DPS310.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "i2c_bus.h"
#include "logger.h"

namespace {

Adafruit_DPS310 g_dps;
SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;
uint8_t g_address = ADDR_DPS310;
float g_pressure_hpa = 0.0f;
float g_temp_c = 0.0f;
bool g_ok = false;
bool g_ready = false;

void setSnapshot(float pressure_hpa, float temp_c, bool ok) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_pressure_hpa = pressure_hpa;
    g_temp_c = temp_c;
    g_ok = ok;
    xSemaphoreGive(g_mutex);
}

void resetDriver() {
    g_dps.~Adafruit_DPS310();
    new (&g_dps) Adafruit_DPS310();
}

bool detectAddress(uint8_t &address) {
    if (i2cBusPing(ADDR_DPS310)) {
        address = ADDR_DPS310;
        return true;
    }
    if (i2cBusPing(ADDR_DPS310_ALT)) {
        address = ADDR_DPS310_ALT;
        return true;
    }
    return false;
}

bool initSensor() {
    uint8_t address = 0;
    if (!detectAddress(address)) {
        return false;
    }

    resetDriver();

    I2cLock lock;
    if (!lock.ok()) {
        return false;
    }
    if (!g_dps.begin_I2C(address)) {
        return false;
    }

    g_address = address;
    g_dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    g_dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
    g_dps.setMode(DPS310_CONT_PRESTEMP);
    logPrintf("DPS310: ready at 0x%02X", g_address);
    return true;
}

void dpsTask(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;
    uint32_t fail_streak = 0;

    for (;;) {
        if (!g_ready) {
            g_ready = initSensor();
            if (!g_ready) {
                setSnapshot(0.0f, 0.0f, false);
                logPrintf("DPS310: not found, retry");
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
        }

        float pressure_hpa = 0.0f;
        float temp_c = 0.0f;
        bool ok = false;
        {
            I2cLock lock;
            if (lock.ok()) {
                sensors_event_t temp_event;
                sensors_event_t pressure_event;
                g_dps.getEvents(&temp_event, &pressure_event);
                pressure_hpa = pressure_event.pressure;
                temp_c = temp_event.temperature;
                ok = true;
            }
        }

        if (!ok) {
            if (++fail_streak >= 10) {
                fail_streak = 0;
                g_ready = false;
                setSnapshot(0.0f, 0.0f, false);
            }
        } else {
            fail_streak = 0;
            setSnapshot(pressure_hpa, temp_c, true);
        }

        const uint32_t now = millis();
        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const Dps310Snapshot snap = dps310GetSnapshot();
            if (snap.ok) {
                logPrintf("DPS310: %.1f hPa  T=%.1f C",
                          static_cast<double>(snap.pressure_hpa),
                          static_cast<double>(snap.temp_c));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(DPS_POLL_INTERVAL_MS));
    }
}

}  // namespace

void dps310Init() {
    g_mutex = xSemaphoreCreateMutex();
    g_ready = initSensor();
    if (!g_ready) {
        logPrintf("DPS310: init failed");
    }
    xTaskCreatePinnedToCore(dpsTask, "dpsTask", 4096, nullptr, 1, &g_task, 1);
}

Dps310Snapshot dps310GetSnapshot() {
    Dps310Snapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap.pressure_hpa = g_pressure_hpa;
    snap.temp_c = g_temp_c;
    snap.ok = g_ok;
    xSemaphoreGive(g_mutex);
    return snap;
}
