#include "i2c_bus.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "logger.h"

namespace {

constexpr size_t BUS_COUNT = 2;

struct BusState {
    TwoWire *wire = nullptr;
    SemaphoreHandle_t mutex = nullptr;
    int sda = 0;
    int scl = 0;
    const char *name = "";
};

BusState g_buses[BUS_COUNT];
bool g_initialized = false;

BusState &busState(I2cBusId bus) {
    return g_buses[static_cast<size_t>(bus)];
}

const char *knownDeviceName(I2cBusId bus, uint8_t address) {
    if (bus == I2cBusId::Hypoxia) {
        switch (address) {
            case ADDR_ADS1115:
                return "ADS1115 (O₂ буфер гипоксии)";
            case ADDR_DPS310:
            case ADDR_DPS310_ALT:
                return "DPS310 (давление буфер гипоксии)";
            default:
                return "неизвестное устройство";
        }
    }
    switch (address) {
        case ADDR_SFM3300:
            return "SFM3300 (поток, рабочий)";
        case ADDR_ADS1115:
            return "ADS1115 (O₂ рабочий)";
        case ADDR_DPS310:
        case ADDR_DPS310_ALT:
#if WORKING_PRESSURE_BMP390
            return "BMP390 (давление рабочий)";
#else
            return "DPS310 (давление рабочий)";
#endif
        case ADDR_SCD41:
            return "SCD41 (CO₂ рабочий)";
        default:
            return "неизвестное устройство";
    }
}

void scanOne(I2cBusId bus) {
    const BusState &st = busState(bus);
    uint8_t found = 0;
    logPrintf("I2C %s: сканирование SDA=%d SCL=%d...", st.name, st.sda, st.scl);
    for (uint8_t address = 0x03; address < 0x78; ++address) {
        if (!i2cBusPing(bus, address)) {
            continue;
        }
        ++found;
        logPrintf("I2C %s:   0x%02X — %s", st.name, address, knownDeviceName(bus, address));
    }
    if (found == 0) {
        logPrintf("I2C %s: устройств не найдено", st.name);
    } else {
        logPrintf("I2C %s: найдено устройств: %u", st.name, static_cast<unsigned>(found));
    }
}

void logMissing(I2cBusId bus, const uint8_t *expected, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!i2cBusPing(bus, expected[i])) {
            logPrintf("I2C %s: ОТСУТСТВУЕТ 0x%02X — %s",
                      busState(bus).name,
                      expected[i],
                      knownDeviceName(bus, expected[i]));
        }
    }
}

void logMissingDps(I2cBusId bus) {
    if (i2cBusPing(bus, ADDR_DPS310) || i2cBusPing(bus, ADDR_DPS310_ALT)) {
        return;
    }
    logPrintf("I2C %s: ОТСУТСТВУЕТ 0x%02X/0x%02X — %s",
              busState(bus).name,
              ADDR_DPS310,
              ADDR_DPS310_ALT,
              knownDeviceName(bus, ADDR_DPS310));
}

void initBusState(BusState &st,
                  TwoWire *wire,
                  int sda,
                  int scl,
                  const char *name) {
    st.wire = wire;
    st.mutex = xSemaphoreCreateRecursiveMutex();
    st.sda = sda;
    st.scl = scl;
    st.name = name;
}

}  // namespace

void i2cBusInit() {
    if (g_initialized) {
        return;
    }

    initBusState(g_buses[static_cast<size_t>(I2cBusId::Hypoxia)],
                 &Wire,
                 I2C_SDA_PIN,
                 I2C_SCL_PIN,
                 "hypoxia");
    initBusState(g_buses[static_cast<size_t>(I2cBusId::Working)],
                 &Wire1,
                 I2C_WORKING_SDA_PIN,
                 I2C_WORKING_SCL_PIN,
                 "working");

    for (size_t i = 0; i < BUS_COUNT; ++i) {
        BusState &st = g_buses[i];
        st.wire->begin(st.sda, st.scl);
        st.wire->setClock(I2C_CLOCK_HZ);
        st.wire->setTimeOut(I2C_TIMEOUT_MS);
        logPrintf("I2C %s: SDA=%d SCL=%d clock=%lu Hz",
                  st.name,
                  st.sda,
                  st.scl,
                  static_cast<unsigned long>(I2C_CLOCK_HZ));
    }
    g_initialized = true;
}

TwoWire &i2cBusWire(I2cBusId bus) {
    return *busState(bus).wire;
}

bool i2cBusLock(I2cBusId bus, uint32_t timeout_ms) {
    SemaphoreHandle_t mutex = busState(bus).mutex;
    if (mutex == nullptr) {
        return false;
    }
    return xSemaphoreTakeRecursive(mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void i2cBusUnlock(I2cBusId bus) {
    SemaphoreHandle_t mutex = busState(bus).mutex;
    if (mutex == nullptr) {
        return;
    }
    xSemaphoreGiveRecursive(mutex);
}

bool i2cBusPing(I2cBusId bus, uint8_t address) {
    I2cLock lock(bus);
    if (!lock.ok()) {
        return false;
    }
    TwoWire &wire = i2cBusWire(bus);
    wire.beginTransmission(address);
    return wire.endTransmission() == 0;
}

void i2cBusScan() {
    scanOne(I2cBusId::Hypoxia);
    const uint8_t hypoxia_expected[] = {ADDR_ADS1115};
    logMissing(I2cBusId::Hypoxia, hypoxia_expected, sizeof(hypoxia_expected));
    logMissingDps(I2cBusId::Hypoxia);

    scanOne(I2cBusId::Working);
    const uint8_t working_expected[] = {ADDR_ADS1115, ADDR_SFM3300, ADDR_SCD41};
    logMissing(I2cBusId::Working, working_expected, sizeof(working_expected));
    logMissingDps(I2cBusId::Working);
}
