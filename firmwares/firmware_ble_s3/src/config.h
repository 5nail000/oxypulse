#pragma once

#include <cstddef>
#include <cstdint>

constexpr const char *BLE_DEVICE_NAME = "OxyPulse-BLE";

// Serial1 → main. Прямая коммутация: GPIO16↔16, GPIO17↔17 (TX=16, RX=17).
// На S3 не использовать GPIO19/20 (USB) и 26–37 (OPI flash/PSRAM).
constexpr int BLE_UART_TX_PIN = 16;
constexpr int BLE_UART_RX_PIN = 17;

constexpr uint16_t BLE_CLIENT_MTU = 517;
constexpr uint32_t BLE_SCAN_DURATION_S = 3;
constexpr uint32_t BLE_SCAN_PAUSE_MS = 2000;
constexpr uint8_t BLE_CONNECT_TIMEOUT_S = 10;
constexpr uint32_t BLE_SAMPLE_STALE_MS = 3000;
constexpr uint32_t WELLUE_POLL_INTERVAL_MS = 1000;
constexpr size_t OXYII_RX_MAX = 512;

constexpr const char *OXYII_SERVICE_UUID = "e8fb0001-a14b-98f9-831b-4e2941d01248";
constexpr const char *OXYII_WRITE_UUID = "e8fb0002-a14b-98f9-831b-4e2941d01248";
constexpr const char *OXYII_NOTIFY_UUID = "e8fb0003-a14b-98f9-831b-4e2941d01248";
constexpr uint16_t HR_SERVICE_UUID = 0x180D;
constexpr uint16_t HR_MEAS_CHAR_UUID = 0x2A37;
constexpr uint16_t WELLUE_MFG_OXYII = 0xF34E;
constexpr uint16_t WELLUE_MFG_VIATOM = 0x036F;

constexpr size_t HRV_RR_PER_NOTIFY_MAX = 16;
constexpr uint16_t HRV_RR_MIN_MS = 250;
constexpr uint16_t HRV_RR_MAX_MS = 2000;
