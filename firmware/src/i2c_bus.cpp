#include "i2c_bus.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "logger.h"

namespace {

SemaphoreHandle_t g_bus_mutex = nullptr;
bool g_initialized = false;

const char *knownDeviceName(uint8_t address) {
    switch (address) {
        case ADDR_SFM3300:
            return "SFM3300 (поток)";
        case ADDR_ADS1115:
            return "ADS1115 (AO-02)";
        case ADDR_DPS310:
        case ADDR_DPS310_ALT:
            return "DPS310 (давление)";
        case ADDR_SCD41:
            return "SCD41 (CO2)";
        default:
            return "неизвестное устройство";
    }
}

}  // namespace

void i2cBusInit() {
    if (g_initialized) {
        return;
    }
    g_bus_mutex = xSemaphoreCreateRecursiveMutex();
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    Wire.setClock(I2C_CLOCK_HZ);
    Wire.setTimeOut(I2C_TIMEOUT_MS);
    g_initialized = true;
    logPrintf("I2C: SDA=%d SCL=%d clock=%lu Hz",
              I2C_SDA_PIN,
              I2C_SCL_PIN,
              static_cast<unsigned long>(I2C_CLOCK_HZ));
}

bool i2cBusLock(uint32_t timeout_ms) {
    if (g_bus_mutex == nullptr) {
        return false;
    }
    return xSemaphoreTakeRecursive(g_bus_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void i2cBusUnlock() {
    if (g_bus_mutex == nullptr) {
        return;
    }
    xSemaphoreGiveRecursive(g_bus_mutex);
}

bool i2cBusPing(uint8_t address) {
    I2cLock lock;
    if (!lock.ok()) {
        return false;
    }
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

void i2cBusScan() {
    uint8_t found = 0;
    logPrintf("I2C: сканирование шины...");
    for (uint8_t address = 0x03; address < 0x78; ++address) {
        if (!i2cBusPing(address)) {
            continue;
        }
        ++found;
        logPrintf("I2C:   0x%02X — %s", address, knownDeviceName(address));
    }
    if (found == 0) {
        logPrintf("I2C: устройств не найдено (проверьте питание и подтяжки)");
    } else {
        logPrintf("I2C: найдено устройств: %u", static_cast<unsigned>(found));
    }

    const uint8_t expected[] = {ADDR_SFM3300, ADDR_ADS1115, ADDR_DPS310, ADDR_SCD41};
    for (uint8_t address : expected) {
        if (!i2cBusPing(address)) {
            logPrintf("I2C: ОТСУТСТВУЕТ 0x%02X — %s", address, knownDeviceName(address));
        }
    }
}
