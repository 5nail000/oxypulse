#include "sht4x.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config.h"
#include "i2c_bus.h"
#include "logger.h"
#include "sensirion_i2c.h"

namespace {

constexpr uint8_t CMD_MEASURE_HIGH = 0xFD;

float rawToTempC(uint16_t raw) {
    return -45.0f + 175.0f * static_cast<float>(raw) / 65535.0f;
}

float rawToRh(uint16_t raw) {
    float rh = -6.0f + 125.0f * static_cast<float>(raw) / 65535.0f;
    if (rh < 0.0f) {
        rh = 0.0f;
    }
    if (rh > 100.0f) {
        rh = 100.0f;
    }
    return rh;
}

}  // namespace

bool sht4xRead(float &temp_c, float &rh_percent) {
    I2cLock lock(I2cBusId::Working);
    if (!lock.ok()) {
        return false;
    }
    TwoWire &wire = i2cBusWire(I2cBusId::Working);
    wire.beginTransmission(ADDR_SHT4X);
    wire.write(CMD_MEASURE_HIGH);
    if (wire.endTransmission() != 0) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    if (wire.requestFrom(static_cast<int>(ADDR_SHT4X), 6) != 6) {
        return false;
    }
    uint8_t raw[6];
    for (int i = 0; i < 6; ++i) {
        raw[i] = static_cast<uint8_t>(wire.read());
    }
    if (sensirionCrc8(raw, 2) != raw[2] || sensirionCrc8(raw + 3, 2) != raw[5]) {
        logPrintf("SHT4x: CRC error");
        return false;
    }
    const uint16_t t_raw = static_cast<uint16_t>((raw[0] << 8) | raw[1]);
    const uint16_t rh_raw = static_cast<uint16_t>((raw[3] << 8) | raw[4]);
    temp_c = rawToTempC(t_raw);
    rh_percent = rawToRh(rh_raw);
    return true;
}
