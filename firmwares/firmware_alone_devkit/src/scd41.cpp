#include "scd41.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "logger.h"
#include "sensirion_i2c.h"

namespace {

constexpr uint16_t CMD_START_PERIODIC = 0x21B1;
constexpr uint16_t CMD_STOP_PERIODIC = 0x3F86;
constexpr uint16_t CMD_READ_MEASUREMENT = 0xEC05;
constexpr uint16_t CMD_GET_DATA_READY = 0xE4B8;

SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;

Scd41Snapshot g_snap;
uint32_t g_last_measurement_ms = 0;
uint32_t g_start_ms = 0;
bool g_ready = false;

void setSnapshot(const Scd41Snapshot &snap) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_snap = snap;
    xSemaphoreGive(g_mutex);
}

bool startPeriodic() {
    sensirionSendCommand(ADDR_SCD41, CMD_STOP_PERIODIC);
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!sensirionSendCommand(ADDR_SCD41, CMD_START_PERIODIC)) {
        return false;
    }
    g_start_ms = millis();
    return true;
}

bool isDataReady() {
    uint16_t status = 0;
    if (!sensirionReadAfterCommand(ADDR_SCD41, CMD_GET_DATA_READY, &status, 1, 2)) {
        return false;
    }
    return (status & 0x07FF) != 0;
}

bool readMeasurement(uint16_t &co2_ppm, float &temp_c, float &rh_percent) {
    uint16_t words[3] = {0, 0, 0};
    if (!sensirionReadAfterCommand(ADDR_SCD41, CMD_READ_MEASUREMENT, words, 3, 2)) {
        return false;
    }
    co2_ppm = words[0];
    temp_c = -45.0f + 175.0f * static_cast<float>(words[1]) / 65535.0f;
    rh_percent = 100.0f * static_cast<float>(words[2]) / 65535.0f;
    return true;
}

void scd41Task(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;

    for (;;) {
        if (!g_ready) {
            g_ready = startPeriodic();
            if (!g_ready) {
                setSnapshot(Scd41Snapshot{});
                logPrintf("SCD41: not found (0x%02X), retry", ADDR_SCD41);
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            logPrintf("SCD41: periodic measurement started (5 s interval)");
        }

        const uint32_t now = millis();
        if (isDataReady()) {
            uint16_t co2 = 0;
            float temp = 0.0f;
            float rh = 0.0f;
            if (readMeasurement(co2, temp, rh) && co2 != 0) {
                g_last_measurement_ms = now;
                Scd41Snapshot snap;
                snap.co2_ppm = co2;
                snap.temp_c = temp;
                snap.rh_percent = rh;
                snap.warming_up = (now - g_start_ms) < SCD41_WARMUP_MS;
                snap.ok = true;
                setSnapshot(snap);
            }
        } else if (g_last_measurement_ms != 0) {
            Scd41Snapshot snap = scd41GetSnapshot();
            if ((now - g_last_measurement_ms) > 15000) {
                g_ready = false;
                g_last_measurement_ms = 0;
                snap = Scd41Snapshot{};
            }
            setSnapshot(snap);
        }

        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const Scd41Snapshot snap = scd41GetSnapshot();
            if (snap.ok) {
                logPrintf("SCD41: CO2=%u ppm  T=%.2f C  RH=%.1f %%%s",
                          static_cast<unsigned>(snap.co2_ppm),
                          static_cast<double>(snap.temp_c),
                          static_cast<double>(snap.rh_percent),
                          snap.warming_up ? "  (warmup)" : "");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SCD41_POLL_INTERVAL_MS));
    }
}

}  // namespace

void scd41Init() {
    g_mutex = xSemaphoreCreateMutex();
    g_ready = startPeriodic();
    if (g_ready) {
        logPrintf("SCD41: init OK (0x%02X)", ADDR_SCD41);
    } else {
        logPrintf("SCD41: init failed");
    }
    xTaskCreatePinnedToCore(scd41Task, "scd41Task", 4096, nullptr, 1, &g_task, 1);
}

Scd41Snapshot scd41GetSnapshot() {
    Scd41Snapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap = g_snap;
    xSemaphoreGive(g_mutex);
    return snap;
}
