#include "ble_cu.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "command_handler.h"
#include "config.h"
#include "igt_session.h"
#include "logger.h"

namespace {

NimBLECharacteristic *g_live_char = nullptr;
SemaphoreHandle_t g_mutex = nullptr;
uint32_t g_last_rx_ms = 0;
bool g_connected = false;
char g_live_buffer[LIVE_JSON_MAX];
uint32_t g_last_live_ms = 0;
bool g_link_stale_logged = false;

void lock() {
    if (g_mutex != nullptr) {
        xSemaphoreTake(g_mutex, portMAX_DELAY);
    }
}

void unlock() {
    if (g_mutex != nullptr) {
        xSemaphoreGive(g_mutex);
    }
}

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *server) override {
        (void)server;
        bleCuNoteRx();
        lock();
        g_connected = true;
        g_link_stale_logged = false;
        unlock();
        logPrintf("BLE: клиент подключился");
    }

    void onDisconnect(NimBLEServer *server) override {
        (void)server;
        lock();
        g_connected = false;
        unlock();
        if (!igtSessionIsIdle()) {
            logPrintf("CU: GATT drop, IGT continues");
        } else {
            logPrintf("BLE: клиент отключился");
        }
        NimBLEDevice::startAdvertising();
    }
};

class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *characteristic) override {
        const std::string &value = characteristic->getValue();
        bleCuNoteRx();
        if (value.empty()) {
            return;
        }
        parseCommandJson(value.c_str(), value.size());
    }
};

void sendLiveNotify() {
    if (g_live_char == nullptr || g_live_char->getSubscribedCount() == 0) {
        return;
    }
    const size_t len = buildLiveJson(g_live_buffer, sizeof(g_live_buffer));
    if (len == 0) {
        return;
    }
    g_live_char->setValue(reinterpret_cast<uint8_t *>(g_live_buffer), len);
    g_live_char->notify();
}

}  // namespace

void bleCuNoteRx() {
    lock();
    g_last_rx_ms = millis();
    g_link_stale_logged = false;
    unlock();
}

uint32_t bleCuLastRxMs() {
    lock();
    const uint32_t v = g_last_rx_ms;
    unlock();
    return v;
}

bool bleCuClientConnected() {
    lock();
    const bool v = g_connected;
    unlock();
    return v;
}

void bleCuInit() {
    if (g_mutex == nullptr) {
        g_mutex = xSemaphoreCreateMutex();
    }
    logPrintf("BLE: NimBLE init, heap=%u", ESP.getFreeHeap());
    NimBLEDevice::init(BLE_DEVICE_NAME);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setMTU(CU_PREFERRED_MTU);

    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());

    NimBLEService *service = server->createService(CU_SERVICE_UUID);
    g_live_char = service->createCharacteristic(
        CU_LIVE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *cmd_char = service->createCharacteristic(
        CU_CMD_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
    cmd_char->setCallbacks(new CommandCallbacks());
    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(CU_SERVICE_UUID);
    advertising->setName(BLE_DEVICE_NAME);
    advertising->start();
    logPrintf("BLE: advertising as %s, heap=%u", BLE_DEVICE_NAME, ESP.getFreeHeap());
}

void bleCuPoll() {
    const uint32_t now = millis();
    if ((now - g_last_live_ms) >= CU_LIVE_INTERVAL_MS) {
        g_last_live_ms = now;
        sendLiveNotify();
    }

    if (igtSessionIsIdle()) {
        return;
    }
    const uint32_t last_rx = bleCuLastRxMs();
    if (last_rx == 0) {
        return;
    }
    if ((now - last_rx) <= CU_LINK_TIMEOUT_MS) {
        return;
    }
    lock();
    const bool already = g_link_stale_logged;
    g_link_stale_logged = true;
    unlock();
    if (!already) {
        logPrintf("CU: link stale, IGT continues (no abort)");
    }
}
