#include "dps310.h"

#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "i2c_bus.h"
#include "logger.h"

#include <Adafruit_DPS310.h>
#if WORKING_PRESSURE_BMP390
#include <Adafruit_BMP3XX.h>
#endif

namespace {

template <typename Sensor>
struct Channel {
    const char *tag;
    I2cBusId bus;
    const char *task_name;
    Sensor sensor;
    SemaphoreHandle_t mutex = nullptr;
    TaskHandle_t task = nullptr;
    uint8_t address = ADDR_DPS310;
    float pressure_hpa = 0.0f;
    float temp_c = 0.0f;
    float baseline_hpa = 0.0f;
    bool ok = false;
    bool ready = false;
    bool baseline_ok = false;
};

using DpsChannel = Channel<Adafruit_DPS310>;
#if WORKING_PRESSURE_BMP390
using WorkingChannel = Channel<Adafruit_BMP3XX>;
#else
using WorkingChannel = DpsChannel;
#endif

DpsChannel g_hypoxia;
WorkingChannel g_working;

void resetDriver(Adafruit_DPS310 &dps) {
    dps.~Adafruit_DPS310();
    new (&dps) Adafruit_DPS310();
}

#if WORKING_PRESSURE_BMP390
void resetDriver(Adafruit_BMP3XX &) {
    // begin_I2C() сам удаляет предыдущий Adafruit_I2CDevice
}
#endif

bool beginSensor(Adafruit_DPS310 &dps, uint8_t address, TwoWire &wire) {
    if (!dps.begin_I2C(address, &wire)) {
        return false;
    }
    dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
    dps.setMode(DPS310_CONT_PRESTEMP);
    return true;
}

#if WORKING_PRESSURE_BMP390
bool beginSensor(Adafruit_BMP3XX &bmp, uint8_t address, TwoWire &wire) {
    if (!bmp.begin_I2C(address, &wire)) {
        return false;
    }
    bmp.setTemperatureOversampling(BMP3_OVERSAMPLING_8X);
    bmp.setPressureOversampling(BMP3_OVERSAMPLING_4X);
    bmp.setIIRFilterCoeff(BMP3_IIR_FILTER_COEFF_3);
    bmp.setOutputDataRate(BMP3_ODR_50_HZ);
    return true;
}
#endif

bool readSensor(Adafruit_DPS310 &dps, float &pressure_hpa, float &temp_c) {
    sensors_event_t temp_event;
    sensors_event_t pressure_event;
    dps.getEvents(&temp_event, &pressure_event);
    pressure_hpa = pressure_event.pressure;
    temp_c = temp_event.temperature;
    return true;
}

#if WORKING_PRESSURE_BMP390
bool readSensor(Adafruit_BMP3XX &bmp, float &pressure_hpa, float &temp_c) {
    if (!bmp.performReading()) {
        return false;
    }
    pressure_hpa = static_cast<float>(bmp.pressure / 100.0);
    temp_c = static_cast<float>(bmp.temperature);
    return true;
}
#endif

template <typename Sensor>
void setSnapshot(Channel<Sensor> &ch, float pressure_hpa, float temp_c, bool ok) {
    if (ch.mutex == nullptr || xSemaphoreTake(ch.mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    ch.pressure_hpa = pressure_hpa;
    ch.temp_c = temp_c;
    ch.ok = ok;
    xSemaphoreGive(ch.mutex);
}

template <typename Sensor>
bool detectAddress(Channel<Sensor> &ch, uint8_t &address) {
    if (i2cBusPing(ch.bus, ADDR_DPS310)) {
        address = ADDR_DPS310;
        return true;
    }
    if (i2cBusPing(ch.bus, ADDR_DPS310_ALT)) {
        address = ADDR_DPS310_ALT;
        return true;
    }
    return false;
}

template <typename Sensor>
bool calibrateBaseline(Channel<Sensor> &ch) {
    constexpr size_t kSamples = 24;
    constexpr float kBandHpa = 1.5f;
    float samples[kSamples];
    size_t count = 0;

    for (size_t i = 0; i < kSamples; ++i) {
        float pressure_hpa = 0.0f;
        float temp_c = 0.0f;
        I2cLock lock(ch.bus);
        if (lock.ok() && readSensor(ch.sensor, pressure_hpa, temp_c)) {
            samples[count++] = pressure_hpa;
        }
        vTaskDelay(pdMS_TO_TICKS(DPS_POLL_INTERVAL_MS));
    }
    if (count < 8) {
        return false;
    }

    std::sort(samples, samples + count);
    const float median = samples[count / 2];

    float sum = 0.0f;
    size_t good = 0;
    for (size_t i = 0; i < count; ++i) {
        if (std::fabs(samples[i] - median) <= kBandHpa) {
            sum += samples[i];
            ++good;
        }
    }
    if (good < 6) {
        return false;
    }

    if (ch.mutex == nullptr || xSemaphoreTake(ch.mutex, portMAX_DELAY) != pdTRUE) {
        return false;
    }
    ch.baseline_hpa = sum / static_cast<float>(good);
    ch.baseline_ok = true;
    xSemaphoreGive(ch.mutex);
    logPrintf("%s: baseline %.1f hPa (%u/%u samples)",
              ch.tag,
              static_cast<double>(ch.baseline_hpa),
              static_cast<unsigned>(good),
              static_cast<unsigned>(count));
    return true;
}

template <typename Sensor>
bool initSensor(Channel<Sensor> &ch) {
    uint8_t address = 0;
    if (!detectAddress(ch, address)) {
        return false;
    }

    resetDriver(ch.sensor);

    I2cLock lock(ch.bus);
    if (!lock.ok()) {
        return false;
    }
    if (!beginSensor(ch.sensor, address, i2cBusWire(ch.bus))) {
        return false;
    }

    ch.address = address;
    logPrintf("%s: ready at 0x%02X", ch.tag, ch.address);
    return true;
}

template <typename Sensor>
void pressureTask(void *param) {
    Channel<Sensor> *ch = static_cast<Channel<Sensor> *>(param);
    uint32_t last_log_ms = 0;
    uint32_t fail_streak = 0;

    for (;;) {
        if (!ch->ready) {
            ch->ready = initSensor(*ch);
            if (!ch->ready) {
                setSnapshot(*ch, 0.0f, 0.0f, false);
                logPrintf("%s: not found, retry", ch->tag);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            ch->baseline_ok = false;
            if (!calibrateBaseline(*ch)) {
                logPrintf("%s: baseline calibration failed", ch->tag);
            }
        }

        float pressure_hpa = 0.0f;
        float temp_c = 0.0f;
        bool ok = false;
        {
            I2cLock lock(ch->bus);
            if (lock.ok()) {
                ok = readSensor(ch->sensor, pressure_hpa, temp_c);
            }
        }

        if (!ok) {
            if (++fail_streak >= 10) {
                fail_streak = 0;
                ch->ready = false;
                ch->baseline_ok = false;
                setSnapshot(*ch, 0.0f, 0.0f, false);
            }
        } else {
            fail_streak = 0;
            setSnapshot(*ch, pressure_hpa, temp_c, true);
        }

        const uint32_t now = millis();
        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            if (ch->ok) {
                logPrintf("%s: %.1f hPa  T=%.1f C",
                          ch->tag,
                          static_cast<double>(ch->pressure_hpa),
                          static_cast<double>(ch->temp_c));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(DPS_POLL_INTERVAL_MS));
    }
}

void hypoxiaTask(void *param) {
    pressureTask<Adafruit_DPS310>(param);
}

void workingTask(void *param) {
#if WORKING_PRESSURE_BMP390
    pressureTask<Adafruit_BMP3XX>(param);
#else
    pressureTask<Adafruit_DPS310>(param);
#endif
}

template <typename Sensor>
void startChannel(Channel<Sensor> &ch,
                  const char *tag,
                  I2cBusId bus,
                  const char *task_name,
                  TaskFunction_t task_fn) {
    ch.tag = tag;
    ch.bus = bus;
    ch.task_name = task_name;
    ch.mutex = xSemaphoreCreateMutex();
    ch.ready = initSensor(ch);
    if (!ch.ready) {
        logPrintf("%s: init failed", tag);
    }
    xTaskCreatePinnedToCore(task_fn, task_name, 4096, &ch, 1, &ch.task, 1);
}

template <typename Sensor>
Dps310Snapshot snapshotOf(Channel<Sensor> &ch) {
    Dps310Snapshot snap{};
    if (ch.mutex == nullptr || xSemaphoreTake(ch.mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap.pressure_hpa = ch.pressure_hpa;
    snap.temp_c = ch.temp_c;
    snap.baseline_hpa = ch.baseline_hpa;
    snap.ok = ch.ok;
    snap.baseline_ok = ch.baseline_ok;
    xSemaphoreGive(ch.mutex);
    return snap;
}

}  // namespace

void dps310Init() {
    startChannel(g_hypoxia, "DPS310 hypoxia", I2cBusId::Hypoxia, "dpsHyp", hypoxiaTask);
}

void workingPressureInit() {
#if WORKING_PRESSURE_BMP390
    startChannel(g_working, "BMP390 working", I2cBusId::Working, "bmpWork", workingTask);
#else
    startChannel(g_working, "DPS310 working", I2cBusId::Working, "dpsWork", workingTask);
#endif
}

Dps310Snapshot dps310GetSnapshot() {
    return snapshotOf(g_hypoxia);
}

Dps310Snapshot workingPressureGetSnapshot() {
    return snapshotOf(g_working);
}
