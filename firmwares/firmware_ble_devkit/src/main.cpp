#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include "ble_sensors.h"
#include "logger.h"
#include "uart_tx.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    logPrintf("OxyPulse BLE node starting, heap=%u", ESP.getFreeHeap());

    WiFi.mode(WIFI_OFF);
    const esp_err_t wifi_stop = esp_wifi_stop();
    logPrintf("WiFi: off (%s)", esp_err_to_name(wifi_stop));

    uartTxInit();
    bleSensorsInit();
    uartTxStart();

    logPrintf("Ready: BLE central, WiFi off, UART TX, heap=%u", ESP.getFreeHeap());
}

void loop() {
    delay(100);
}
