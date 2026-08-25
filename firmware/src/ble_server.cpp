#include "ble_server.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "command_handler.h"
#include "config.h"
#include "logger.h"

namespace {

NimBLECharacteristic *g_status_char = nullptr;
TaskHandle_t g_ble_task = nullptr;
char g_status_buffer[STATUS_JSON_MAX];

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server) override {
        (void)server;
        logPrintf("BLE: клиент подключился");
    }

    void onDisconnect(NimBLEServer *server) override {
        (void)server;
        logPrintf("BLE: клиент отключился");
        NimBLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic) override {
        const std::string &value = characteristic->getValue();
        if (value.empty()) {
            return;
        }
        parseCommandJson(value.c_str(), value.size());
    }
};

void sendStatusNotify() {
    if (g_status_char == nullptr || g_status_char->getSubscribedCount() == 0) {
        return;
    }
    const size_t len = buildStatusJson(g_status_buffer, sizeof(g_status_buffer));
    if (len == 0) {
        return;
    }
    g_status_char->setValue(reinterpret_cast<uint8_t *>(g_status_buffer), len);
    g_status_char->notify();
}

void bleTask(void *param) {
    (void)param;
    uint32_t last_ms = 0;
    for (;;) {
        const uint32_t now = millis();
        if ((now - last_ms) >= 1000) {
            last_ms = now;
            sendStatusNotify();
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

}  // namespace

void bleServerInit() {
    logPrintf("BLE: NimBLE init, heap=%u", ESP.getFreeHeap());
    NimBLEDevice::init(BLE_DEVICE_NAME);
    // P9 fights WiFi AP+STA on the same radio; P3 is enough for nearby phone/sensors.
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);
    logPrintf("BLE: stack ready, heap=%u", ESP.getFreeHeap());

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService *service = server->createService(SERVICE_UUID);

    g_status_char = service->createCharacteristic(
        STATUS_CHAR_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic *cmd_char = service->createCharacteristic(
        CMD_CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    cmd_char->setCallbacks(new CommandCallbacks());

    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setName(BLE_DEVICE_NAME);
    advertising->start();

    xTaskCreatePinnedToCore(bleTask, "bleTask", 4096, nullptr, 1, &g_ble_task, 0);
    logPrintf("BLE: advertising as %s, heap=%u", BLE_DEVICE_NAME, ESP.getFreeHeap());
}

void bleServerLoop() {
    // notify task handles BLE updates
}
