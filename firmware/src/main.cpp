#include <Arduino.h>
#include <esp_bt.h>

#include "ao02.h"
#include "ble_sensors.h"
#include "ble_server.h"
#include "dps310.h"
#include "i2c_bus.h"
#include "logger.h"
#include "servo_controller.h"
#include "scd41.h"
#include "sfm3300.h"
#include "wifi_server.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    logPrintf("ESP32-Tester starting, heap=%u", ESP.getFreeHeap());

    // Classic BT RAM must be released before WiFi, and BLE controller
    // must start while heap is still large — otherwise NimBLE abort() reboot-loop.
    const esp_err_t bt_rel = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    logPrintf("BT: classic mem release %s, heap=%u", esp_err_to_name(bt_rel), ESP.getFreeHeap());
    bleServerInit();

    i2cBusInit();
    i2cBusScan();
    ao02Init();
    sfm3300Init();
    dps310Init();
    scd41Init();

    servoControllerInit();
    wifiServerInit();
    bleSensorsInit();

    logPrintf("Ready: sensors + WiFi AP + BLE + HTTP, heap=%u", ESP.getFreeHeap());
}

void loop() {
    wifiServerLoop();
    delay(2);
}
