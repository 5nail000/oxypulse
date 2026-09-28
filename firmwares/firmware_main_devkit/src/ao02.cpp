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

struct Channel {
    const char *tag;
    I2cBusId bus;
    Adafruit_ADS1115 ads;
    SemaphoreHandle_t mutex = nullptr;
    TaskHandle_t task = nullptr;
    float offset_mv = 0.0f;
    float air_mv = 0.0f;
    float air_percent = O2_AIR_PERCENT;
    float o2_percent = 0.0f;
    float voltage_mv = 0.0f;
    bool ok = false;
    bool ads_ready = false;
};

Channel g_hypoxia;

void setSnapshot(Channel &ch, float o2_percent, float voltage_mv, bool ok) {
    if (ch.mutex == nullptr || xSemaphoreTake(ch.mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    if (ok) {
        ch.o2_percent = o2_percent;
        ch.voltage_mv = voltage_mv;
        ch.ok = true;
    } else {
        ch.o2_percent = 0.0f;
        ch.voltage_mv = 0.0f;
        ch.ok = false;
    }
    xSemaphoreGive(ch.mutex);
}

bool readO2(Channel &ch, float &o2_percent, float &voltage_mv) {
    if (!ch.ads_ready || (ch.air_mv - ch.offset_mv) <= 0.0f) {
        return false;
    }
    I2cLock lock(ch.bus);
    if (!lock.ok()) {
        return false;
    }
    const int16_t raw = ch.ads.readADC_Differential_0_1();
    voltage_mv = static_cast<float>(raw) * LSB_MV_GAIN_SIXTEEN;
    o2_percent = (voltage_mv - ch.offset_mv) / (ch.air_mv - ch.offset_mv) * ch.air_percent;
    return true;
}

bool beginAds(Channel &ch) {
    I2cLock lock(ch.bus);
    if (!lock.ok()) {
        return false;
    }
    return ch.ads.begin(ADS_ADDR, &i2cBusWire(ch.bus));
}

void ao02Task(void *param) {
    Channel *ch = static_cast<Channel *>(param);
    uint32_t last_log_ms = 0;
    uint32_t fail_streak = 0;

    for (;;) {
        if (!ch->ads_ready) {
            ch->ads_ready = beginAds(*ch);
            if (!ch->ads_ready) {
                setSnapshot(*ch, 0.0f, 0.0f, false);
                logPrintf("%s: ADS1115 not found, retry", ch->tag);
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }
            ch->ads.setGain(GAIN_SIXTEEN);
            logPrintf("%s: ADS1115 ready, GAIN_SIXTEEN", ch->tag);
        }

        float o2_percent = 0.0f;
        float voltage_mv = 0.0f;
        const bool ok = readO2(*ch, o2_percent, voltage_mv);
        if (!ok) {
            if (++fail_streak >= 5) {
                setSnapshot(*ch, 0.0f, 0.0f, false);
                ch->ads_ready = false;
            }
        } else {
            fail_streak = 0;
            setSnapshot(*ch, o2_percent, voltage_mv, true);
        }

        const uint32_t now = millis();
        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            if (ch->ok) {
                logPrintf("%s: O2=%.2f %%  mV=%.3f",
                          ch->tag,
                          static_cast<double>(ch->o2_percent),
                          static_cast<double>(ch->voltage_mv));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(O2_POLL_INTERVAL_MS));
    }
}

}  // namespace

void ao02Init() {
    g_hypoxia.tag = "AO-02 hypoxia";
    g_hypoxia.bus = I2cBusId::Hypoxia;
    g_hypoxia.offset_mv = O2_HYPOXIA_OFFSET_MV;
    g_hypoxia.air_mv = O2_HYPOXIA_AIR_MV;
    g_hypoxia.air_percent = O2_AIR_PERCENT;
    g_hypoxia.mutex = xSemaphoreCreateMutex();
    g_hypoxia.ads_ready = beginAds(g_hypoxia);
    if (g_hypoxia.ads_ready) {
        g_hypoxia.ads.setGain(GAIN_SIXTEEN);
        logPrintf("%s: ADS1115 init OK (0x%02X)", g_hypoxia.tag, ADS_ADDR);
    } else {
        logPrintf("%s: ADS1115 init failed", g_hypoxia.tag);
    }
    xTaskCreatePinnedToCore(ao02Task, "ao02Hyp", 4096, &g_hypoxia, 1, &g_hypoxia.task, 1);
}

Ao02Snapshot ao02GetSnapshot() {
    Ao02Snapshot snap{};
    if (g_hypoxia.mutex == nullptr ||
        xSemaphoreTake(g_hypoxia.mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap.o2_percent = g_hypoxia.o2_percent;
    snap.voltage_mv = g_hypoxia.voltage_mv;
    snap.ok = g_hypoxia.ok;
    xSemaphoreGive(g_hypoxia.mutex);
    return snap;
}
