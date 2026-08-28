#pragma once

#include <cstdint>

#include <Wire.h>

enum class I2cBusId : uint8_t {
    Hypoxia = 0,
    Working = 1,
};

void i2cBusInit();
TwoWire &i2cBusWire(I2cBusId bus);
bool i2cBusLock(I2cBusId bus, uint32_t timeout_ms = 1000);
void i2cBusUnlock(I2cBusId bus);
void i2cBusScan();
bool i2cBusPing(I2cBusId bus, uint8_t address);

class I2cLock {
public:
    explicit I2cLock(I2cBusId bus, uint32_t timeout_ms = 1000)
        : bus_(bus), locked_(i2cBusLock(bus, timeout_ms)) {}
    ~I2cLock() {
        if (locked_) {
            i2cBusUnlock(bus_);
        }
    }

    I2cLock(const I2cLock &) = delete;
    I2cLock &operator=(const I2cLock &) = delete;

    bool ok() const { return locked_; }
    I2cBusId bus() const { return bus_; }

private:
    I2cBusId bus_;
    bool locked_;
};
