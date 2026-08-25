#include "ao02.h"

#include <Adafruit_ADS1X15.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "i2c_bus.h"
#include "logger.h"

namespace {

constexpr uint8_t ADS_ADDR = ADDR_ADS1115;
constexpr float LSB_MV_GAIN_SIXTEEN = 0.0078125f;

Adafruit_ADS1115 g_ads;
SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;
float g_o2_percent = 0.0f;
float g_voltage_mv = 0.0f;
bool g_ok = false;
bool g_ads_ready = false;

void setSnapshot(float o2_percent, float voltage_mv, bool ok) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (ok) {
        g_o2_percent = o2_percent;
        g_voltage_mv = voltage_mv;
        g_ok = true;
    } else {
        g_o2_percent = 0.0f;
        g_voltage_mv = 0.0f;
        g_ok = false;
    }
    xSemaphoreGive(g_mutex);
}

bool readO2(float &o2_percent, float &voltage_mv) {
    if (!g_ads_ready || (O2_AIR_MV - O2_OFFSET_MV) <= 0.0f) {
        return false;
    }
    I2cLock lock;
    if (!lock.ok()) {
        return false;
    }
    const int16_t raw = g_ads.readADC_Differential_0_1();
    voltage_mv = static_cast<float>(raw) * LSB_MV_GAIN_SIXTEEN;
    o2_percent = (voltage_mv - O2_OFFSET_MV) / (O2_AIR_MV - O2_OFFSET_MV) * O2_AIR_PERCENT;
    return true;
}

void ao02Task(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;
    uint32_t fail_streak = 0;

    for (;;) {
        if (!g_ads_ready) {
            {
                I2cLock lock;
                g_ads_ready = lock.ok() && g_ads.begin(ADS_ADDR);
            }
            if (!g_ads_ready) {
                setSnapshot(0.0f, 0.0f, false);
                logPrintf("AO-02: ADS1115 not found, retry");
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            g_ads.setGain(GAIN_SIXTEEN);
            logPrintf("AO-02: ADS1115 ready, GAIN_SIXTEEN");
        }

        float o2_percent = 0.0f;
        float voltage_mv = 0.0f;
        const bool ok = readO2(o2_percent, voltage_mv);
        if (!ok) {
            if (++fail_streak >= 5) {
                setSnapshot(0.0f, 0.0f, false);
                g_ads_ready = false;
            }
        } else {
            fail_streak = 0;
            setSnapshot(o2_percent, voltage_mv, true);
        }

        const uint32_t now = millis();
        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const Ao02Snapshot snap = ao02GetSnapshot();
            if (snap.ok) {
                logPrintf("AO-02: O2=%.2f %%  mV=%.3f",
                          static_cast<double>(snap.o2_percent),
                          static_cast<double>(snap.voltage_mv));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(O2_POLL_INTERVAL_MS));
    }
}

}  // namespace

void ao02Init() {
    g_mutex = xSemaphoreCreateMutex();
    {
        I2cLock lock;
        g_ads_ready = lock.ok() && g_ads.begin(ADS_ADDR);
    }
    if (g_ads_ready) {
        g_ads.setGain(GAIN_SIXTEEN);
        logPrintf("AO-02: ADS1115 init OK (0x%02X)", ADS_ADDR);
    } else {
        logPrintf("AO-02: ADS1115 init failed");
    }
    xTaskCreatePinnedToCore(ao02Task, "ao02Task", 4096, nullptr, 1, &g_task, 1);
}

Ao02Snapshot ao02GetSnapshot() {
    Ao02Snapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap.o2_percent = g_o2_percent;
    snap.voltage_mv = g_voltage_mv;
    snap.ok = g_ok;
    xSemaphoreGive(g_mutex);
    return snap;
}
