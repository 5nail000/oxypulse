#pragma once

#include <cstdint>
#include <cstddef>

constexpr size_t BLE_NAME_MAX = 24;
constexpr size_t BLE_ERROR_MAX = 48;

struct WellueSnapshot {
    bool connected = false;
    bool ok = false;
    char name[BLE_NAME_MAX] = "";
    uint8_t spo2 = 0;
    uint8_t hr = 0;
    uint8_t battery = 0;
    uint8_t motion = 0;
    bool contact = false;
    char error[BLE_ERROR_MAX] = "";
};

struct CoospoSnapshot {
    bool connected = false;
    bool ok = false;
    char name[BLE_NAME_MAX] = "";
    uint16_t bpm = 0;
    uint16_t rr_ms = 0;
    bool contact = false;
    char error[BLE_ERROR_MAX] = "";
};

struct BridgeSnapshot {
    bool ok = false;
    char error[BLE_ERROR_MAX] = "";
};
