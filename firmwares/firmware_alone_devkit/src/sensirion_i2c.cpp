#include "sensirion_i2c.h"

#include <Arduino.h>
#include <Wire.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "i2c_bus.h"
#include "logger.h"

namespace {

constexpr size_t MAX_WORDS = 8;
constexpr uint32_t READ_ERROR_LOG_INTERVAL_MS = 2000;
constexpr uint8_t CRC_INIT_SCD4X = 0xFF;
constexpr uint8_t CRC_INIT_MASS_FLOW = 0x00;
constexpr I2cBusId kBus = I2cBusId::Working;

TwoWire &busWire() {
    return i2cBusWire(kBus);
}

enum class ReadFailReason : uint8_t {
    ShortRead,
    CrcError,
};

bool sendCommandLocked(uint8_t address, uint16_t command) {
    TwoWire &wire = busWire();
    wire.beginTransmission(address);
    wire.write(static_cast<uint8_t>(command >> 8));
    wire.write(static_cast<uint8_t>(command & 0xFF));
    return wire.endTransmission() == 0;
}

uint8_t crc8WithInit(const uint8_t *data, size_t len, uint8_t init) {
    uint8_t crc = init;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                               : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

void logReadFailure(uint8_t address,
                    ReadFailReason reason,
                    size_t expected_bytes,
                    size_t got_bytes,
                    size_t word_index) {
    static uint32_t last_log_ms = 0;
    const uint32_t now = millis();
    if ((now - last_log_ms) < READ_ERROR_LOG_INTERVAL_MS) {
        return;
    }
    last_log_ms = now;

    if (reason == ReadFailReason::ShortRead) {
        logPrintf("Sensirion I2C 0x%02X: read %u/%u bytes (timeout?)",
                  address,
                  static_cast<unsigned>(got_bytes),
                  static_cast<unsigned>(expected_bytes));
        return;
    }

    logPrintf("Sensirion I2C 0x%02X: CRC error at word %u (%u/%u bytes)",
              address,
              static_cast<unsigned>(word_index),
              static_cast<unsigned>(got_bytes),
              static_cast<unsigned>(expected_bytes));
}

bool readWordsLocked(uint8_t address, uint16_t *words, size_t count, uint8_t crc_init) {
    if (count == 0 || count > MAX_WORDS) {
        return false;
    }

    const size_t expected_bytes = count * 3;
    TwoWire &wire = busWire();
    const size_t got_bytes = wire.requestFrom(address, static_cast<uint8_t>(expected_bytes));
    if (got_bytes != expected_bytes) {
        logReadFailure(address, ReadFailReason::ShortRead, expected_bytes, got_bytes, 0);
        return false;
    }

    for (size_t i = 0; i < count; ++i) {
        uint8_t bytes[2];
        bytes[0] = static_cast<uint8_t>(wire.read());
        bytes[1] = static_cast<uint8_t>(wire.read());
        const uint8_t crc = static_cast<uint8_t>(wire.read());
        if (crc8WithInit(bytes, 2, crc_init) != crc) {
            logReadFailure(address, ReadFailReason::CrcError, expected_bytes, got_bytes, i);
            return false;
        }
        words[i] = static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) | bytes[1]);
    }
    return true;
}

bool readWordsLockedScd4x(uint8_t address, uint16_t *words, size_t count) {
    return readWordsLocked(address, words, count, CRC_INIT_SCD4X);
}

bool readWordsLockedMassFlow(uint8_t address, uint16_t *words, size_t count) {
    return readWordsLocked(address, words, count, CRC_INIT_MASS_FLOW);
}

bool readAfterCommandLocked(uint8_t address,
                            uint16_t command,
                            uint16_t *words,
                            size_t count,
                            uint32_t delay_ms,
                            uint8_t crc_init) {
    if (!sendCommandLocked(address, command)) {
        return false;
    }
    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
    return readWordsLocked(address, words, count, crc_init);
}

}  // namespace

uint8_t sensirionCrc8(const uint8_t *data, size_t len) {
    return crc8WithInit(data, len, CRC_INIT_SCD4X);
}

uint8_t sensirionCrc8MassFlow(const uint8_t *data, size_t len) {
    return crc8WithInit(data, len, CRC_INIT_MASS_FLOW);
}

bool sensirionSendCommand(uint8_t address, uint16_t command) {
    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    return sendCommandLocked(address, command);
}

bool sensirionSendCommandArg(uint8_t address, uint16_t command, uint16_t argument) {
    const uint8_t payload[2] = {static_cast<uint8_t>(argument >> 8),
                                static_cast<uint8_t>(argument & 0xFF)};

    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    TwoWire &wire = busWire();
    wire.beginTransmission(address);
    wire.write(static_cast<uint8_t>(command >> 8));
    wire.write(static_cast<uint8_t>(command & 0xFF));
    wire.write(payload[0]);
    wire.write(payload[1]);
    wire.write(sensirionCrc8(payload, 2));
    return wire.endTransmission() == 0;
}

bool sensirionReadWords(uint8_t address, uint16_t *words, size_t count) {
    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    return readWordsLockedScd4x(address, words, count);
}

bool sensirionReadAfterCommand(uint8_t address,
                               uint16_t command,
                               uint16_t *words,
                               size_t count,
                               uint32_t delay_ms) {
    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    return readAfterCommandLocked(address, command, words, count, delay_ms, CRC_INIT_SCD4X);
}

bool sensirionReadWordsMassFlow(uint8_t address, uint16_t *words, size_t count) {
    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    return readWordsLockedMassFlow(address, words, count);
}

bool sensirionReadAfterCommandMassFlow(uint8_t address,
                                       uint16_t command,
                                       uint16_t *words,
                                       size_t count,
                                       uint32_t delay_ms) {
    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    return readAfterCommandLocked(address, command, words, count, delay_ms, CRC_INIT_MASS_FLOW);
}

bool sensirionStartContinuousMassFlow(uint8_t address,
                                      uint16_t command,
                                      uint16_t *first_word,
                                      uint32_t delay_ms,
                                      uint8_t attempts) {
    if (first_word == nullptr || attempts == 0) {
        return false;
    }

    I2cLock lock(kBus);
    if (!lock.ok()) {
        return false;
    }
    if (!sendCommandLocked(address, command)) {
        return false;
    }
    if (delay_ms > 0) {
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }

    for (uint8_t i = 0; i < attempts; ++i) {
        if (readWordsLockedMassFlow(address, first_word, 1)) {
            return true;
        }
        if (i + 1 < attempts) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    return false;
}
