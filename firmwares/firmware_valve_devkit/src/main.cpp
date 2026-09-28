#include <Arduino.h>
#include <WiFi.h>

#include "ao02.h"
#include "ble_cu.h"
#include "dps310.h"
#include "gpio_controller.h"
#include "i2c_bus.h"
#include "igt_session.h"
#include "logger.h"
#include "ocs3rl.h"
#include "phys_uplink.h"
#include "servo_controller.h"
#include "sfm3300.h"
#include "stc31.h"

void setup() {
    Serial.begin(115200);
    delay(500);

    WiFi.mode(WIFI_OFF);
    WiFi.disconnect(true, true);

    logPrintf("OxyPulse CU starting, heap=%u", ESP.getFreeHeap());
    valvesInit();
    o2MotorInit();

    i2cBusInit();
    i2cBusScan();
    ao02Init();
    workingO2Init();
    dps310Init();
    workingPressureInit();
    sfm3300Init();
    stc31Init();

    servoControllerInit();
    igtSessionInit();
    physUplinkInit();
    bleCuInit();

    logPrintf("Ready: I2C + valves + IGT + BLE CU, heap=%u", ESP.getFreeHeap());
}

void loop() {
    igtSessionPoll();
    bleCuPoll();
    delay(2);
}
