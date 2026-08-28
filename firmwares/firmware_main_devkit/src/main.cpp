#include <Arduino.h>

#include "ao02.h"
#include "ble_sensors.h"
#include "dps310.h"
#include "i2c_bus.h"
#include "logger.h"
#include "servo_controller.h"
#include "scd41.h"
#include "sfm3300.h"
#include "trend_buffer.h"
#include "wifi_server.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    logPrintf("OxyPulse main starting, heap=%u", ESP.getFreeHeap());
    trendBufferInit();

    i2cBusInit();
    i2cBusScan();
    ao02Init();
    workingO2Init();
    dps310Init();
    workingPressureInit();
    sfm3300Init();
    scd41Init();

    servoControllerInit();
    bleSensorsInit();
    wifiServerInit();

    logPrintf("Ready: sensors + WiFi AP + HTTP + UART BLE bridge, heap=%u", ESP.getFreeHeap());
}

void loop() {
    wifiServerLoop();
    delay(2);
}
