#pragma once

#include <cstdint>

void i2cBusInit();
bool i2cBusLock(uint32_t timeout_ms = 1000);
void i2cBusUnlock();
void i2cBusScan();
bool i2cBusPing(uint8_t address);

class I2cLock {
public:
    explicit I2cLock(uint32_t timeout_ms = 1000) : locked_(i2cBusLock(timeout_ms)) {}
    ~I2cLock() {
        if (locked_) {
            i2cBusUnlock();
        }
    }

    I2cLock(const I2cLock &) = delete;
    I2cLock &operator=(const I2cLock &) = delete;

    bool ok() const { return locked_; }

private:
    bool locked_;
};
