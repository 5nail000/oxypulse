#include "ble_sensors.h"

#include <Arduino.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "hrv_session.h"
#include "logger.h"
#include "uart_codec.h"
#include "uart_protocol.h"

namespace {

HardwareSerial &g_uart = Serial2;
SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;

WellueSnapshot g_wellue;
CoospoSnapshot g_coospo;
BridgeSnapshot g_bridge;
uint32_t g_last_rx_ms = 0;
bool g_got_line = false;

char g_line[UART_LINE_MAX];
size_t g_line_len = 0;

void markUartDown() {
    g_bridge.ok = false;
    uartCopyCstr(g_bridge.error, sizeof(g_bridge.error), "uart");

    g_wellue.connected = false;
    g_wellue.ok = false;
    g_wellue.spo2 = 0;
    g_wellue.hr = 0;
    g_wellue.contact = false;
    uartCopyCstr(g_wellue.error, sizeof(g_wellue.error), "uart");

    g_coospo.connected = false;
    g_coospo.ok = false;
    g_coospo.bpm = 0;
    g_coospo.rr_ms = 0;
    g_coospo.contact = false;
    uartCopyCstr(g_coospo.error, sizeof(g_coospo.error), "uart");
}

void applyLine(const char *line) {
    UartIncoming msg;
    if (!uartParseLine(line, &msg)) {
        logPrintf("UART bridge: bad JSON");
        return;
    }

    if (g_mutex != nullptr) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
    }
    g_last_rx_ms = millis();
    g_got_line = true;
    g_bridge.ok = true;
    g_bridge.error[0] = '\0';

    if (msg.type == UartMsgType::Hello) {
        if (g_mutex != nullptr) {
            xSemaphoreGive(g_mutex);
        }
        logPrintf("UART bridge: hello from BLE node");
        return;
    }
    if (msg.type == UartMsgType::Wellue) {
        g_wellue = msg.wellue;
        if (g_mutex != nullptr) {
            xSemaphoreGive(g_mutex);
        }
        return;
    }
    if (msg.type == UartMsgType::Coospo) {
        g_coospo = msg.coospo;
        if (g_mutex != nullptr) {
            xSemaphoreGive(g_mutex);
        }
        return;
    }
    if (msg.type == UartMsgType::Rr) {
        if (g_mutex != nullptr) {
            xSemaphoreGive(g_mutex);
        }
        hrvSessionFeedRr(msg.rr_ms);
        return;
    }
    if (g_mutex != nullptr) {
        xSemaphoreGive(g_mutex);
    }
}

void feedChar(char c) {
    if (c == '\r') {
        return;
    }
    if (c == '\n') {
        if (g_line_len > 0) {
            g_line[g_line_len] = '\0';
            applyLine(g_line);
            g_line_len = 0;
        }
        return;
    }
    if (g_line_len + 1 >= sizeof(g_line)) {
        g_line_len = 0;
        logPrintf("UART bridge: line overflow");
        return;
    }
    g_line[g_line_len++] = c;
}

void uartBridgeTask(void *param) {
    (void)param;
    for (;;) {
        while (g_uart.available() > 0) {
            feedChar(static_cast<char>(g_uart.read()));
        }

        if (g_mutex != nullptr) {
            xSemaphoreTake(g_mutex, portMAX_DELAY);
        }
        const uint32_t now = millis();
        const bool stale = !g_got_line || (now - g_last_rx_ms) >= UART_BRIDGE_STALE_MS;
        if (stale && g_bridge.ok) {
            markUartDown();
            if (g_mutex != nullptr) {
                xSemaphoreGive(g_mutex);
            }
            logPrintf("UART bridge: stale, waiting for BLE node");
        } else if (g_mutex != nullptr) {
            xSemaphoreGive(g_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

}  // namespace

void bleSensorsInit() {
    g_mutex = xSemaphoreCreateMutex();
    markUartDown();

    g_uart.begin(UART_BRIDGE_BAUD, SERIAL_8N1, MAIN_UART_RX_PIN, MAIN_UART_TX_PIN);
    xTaskCreatePinnedToCore(uartBridgeTask, "uartRx", 4096, nullptr, 1, &g_task, 1);
    logPrintf("UART bridge: Serial2 RX=%d TX=%d baud=%lu",
              MAIN_UART_RX_PIN, MAIN_UART_TX_PIN,
              static_cast<unsigned long>(UART_BRIDGE_BAUD));
}

WellueSnapshot bleSensorsGetWellue() {
    WellueSnapshot snap;
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return snap;
    }
    snap = g_wellue;
    xSemaphoreGive(g_mutex);
    return snap;
}

CoospoSnapshot bleSensorsGetCoospo() {
    CoospoSnapshot snap;
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return snap;
    }
    snap = g_coospo;
    xSemaphoreGive(g_mutex);
    return snap;
}

BridgeSnapshot bleSensorsGetBridge() {
    BridgeSnapshot snap;
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        uartCopyCstr(snap.error, sizeof(snap.error), "uart");
        return snap;
    }
    snap = g_bridge;
    xSemaphoreGive(g_mutex);
    return snap;
}
