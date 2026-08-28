#include "uart_tx.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "ble_sensors.h"
#include "config.h"
#include "logger.h"
#include "uart_codec.h"
#include "uart_protocol.h"

namespace {

HardwareSerial &g_uart = Serial1;
SemaphoreHandle_t g_tx_mutex = nullptr;
TaskHandle_t g_task = nullptr;
char g_json[UART_JSON_MAX];

void sendLine(const char *line, size_t len, TickType_t wait) {
    if (line == nullptr || len == 0 || g_tx_mutex == nullptr) {
        return;
    }
    if (xSemaphoreTake(g_tx_mutex, wait) != pdTRUE) {
        return;
    }
    g_uart.write(reinterpret_cast<const uint8_t *>(line), len);
    g_uart.write('\n');
    xSemaphoreGive(g_tx_mutex);
}

void sendHello() {
    const size_t n = uartEncodeHello(g_json, sizeof(g_json));
    sendLine(g_json, n, pdMS_TO_TICKS(50));
}

void snapshotTask(void *param) {
    (void)param;
    sendHello();
    logPrintf("UART TX: hello sent");
    for (;;) {
        const WellueSnapshot wellue = bleSensorsGetWellue();
        const CoospoSnapshot coospo = bleSensorsGetCoospo();
        const size_t nw = uartEncodeWellue(g_json, sizeof(g_json), wellue);
        sendLine(g_json, nw, pdMS_TO_TICKS(50));
        const size_t nc = uartEncodeCoospo(g_json, sizeof(g_json), coospo);
        sendLine(g_json, nc, pdMS_TO_TICKS(50));
        vTaskDelay(pdMS_TO_TICKS(UART_SNAPSHOT_INTERVAL_MS));
    }
}

}  // namespace

void uartTxInit() {
    g_tx_mutex = xSemaphoreCreateMutex();
    g_uart.begin(UART_BRIDGE_BAUD, SERIAL_8N1, BLE_UART_RX_PIN, BLE_UART_TX_PIN);
    logPrintf("UART TX: Serial1 RX=%d TX=%d baud=%lu",
              BLE_UART_RX_PIN, BLE_UART_TX_PIN,
              static_cast<unsigned long>(UART_BRIDGE_BAUD));
}

void uartTxStart() {
    if (g_task != nullptr) {
        return;
    }
    xTaskCreatePinnedToCore(snapshotTask, "uartTx", 4096, nullptr, 1, &g_task, 1);
}

void uartTxSendRr(uint16_t rr_ms) {
    if (rr_ms < HRV_RR_MIN_MS || rr_ms > HRV_RR_MAX_MS) {
        return;
    }
    char buf[32];
    const size_t n = uartEncodeRr(buf, sizeof(buf), rr_ms);
    sendLine(buf, n, 0);
}
