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
    const char *task_name;
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
Channel g_working;

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

void startChannel(Channel &ch,
                  const char *tag,
                  I2cBusId bus,
                  const char *task_name,
                  float offset_mv,
                  float air_mv) {
    ch.tag = tag;
    ch.bus = bus;
    ch.task_name = task_name;
    ch.offset_mv = offset_mv;
    ch.air_mv = air_mv;
    ch.air_percent = O2_AIR_PERCENT;
    ch.mutex = xSemaphoreCreateMutex();
    ch.ads_ready = beginAds(ch);
    if (ch.ads_ready) {
        ch.ads.setGain(GAIN_SIXTEEN);
        logPrintf("%s: ADS1115 init OK (0x%02X)", tag, ADS_ADDR);
    } else {
        logPrintf("%s: ADS1115 init failed", tag);
    }
    xTaskCreatePinnedToCore(ao02Task, task_name, 4096, &ch, 1, &ch.task, 1);
}

Ao02Snapshot snapshotOf(Channel &ch) {
    Ao02Snapshot snap{};
    if (ch.mutex == nullptr || xSemaphoreTake(ch.mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap.o2_percent = ch.o2_percent;
    snap.voltage_mv = ch.voltage_mv;
    snap.ok = ch.ok;
    xSemaphoreGive(ch.mutex);
    return snap;
}

}  // namespace

void ao02Init() {
    startChannel(g_hypoxia,
                 "AO-02 hypoxia",
                 I2cBusId::Hypoxia,
                 "ao02Hyp",
                 O2_HYPOXIA_OFFSET_MV,
                 O2_HYPOXIA_AIR_MV);
}

void workingO2Init() {
    startChannel(g_working,
                 "AO-02 working",
                 I2cBusId::Working,
                 "ao02Work",
                 O2_WORKING_OFFSET_MV,
                 O2_WORKING_AIR_MV);
}

Ao02Snapshot ao02GetSnapshot() {
    return snapshotOf(g_hypoxia);
}

Ao02Snapshot workingO2GetSnapshot() {
    return snapshotOf(g_working);
}
