#pragma once

#include <cstdint>
#include <cstddef>

constexpr uint32_t UART_BRIDGE_BAUD = 921600;
constexpr int UART_BRIDGE_VER = 1;
constexpr uint32_t UART_BRIDGE_STALE_MS = 3000;
constexpr uint32_t UART_SNAPSHOT_INTERVAL_MS = 1000;
constexpr size_t UART_LINE_MAX = 384;
constexpr size_t UART_JSON_MAX = 320;

// ESP32-DEVKIT CH340 (firmware_main_devkit): hardware Serial2. Do not use RX0/TX0 (CH340).
constexpr int MAIN_UART_RX_PIN = 16;
constexpr int MAIN_UART_TX_PIN = 17;

// BLE-узел: пины UART в config.h прошивки (firmware_ble_s3 / firmware_ble_devkit).

constexpr const char *UART_T_HELLO = "hello";
constexpr const char *UART_T_WELLUE = "wellue";
constexpr const char *UART_T_COOSPO = "coospo";
constexpr const char *UART_T_RR = "rr";
