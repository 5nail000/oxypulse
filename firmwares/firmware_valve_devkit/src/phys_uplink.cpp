#include "phys_uplink.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {

SemaphoreHandle_t g_mutex = nullptr;
PhysUplink g_phys;

}  // namespace

void physUplinkInit() {
    if (g_mutex == nullptr) {
        g_mutex = xSemaphoreCreateMutex();
    }
}

void physUplinkSet(uint8_t spo2, uint8_t hr, bool belt, bool contact) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) != pdTRUE) {
        return;
    }
    g_phys.spo2 = spo2;
    g_phys.hr = hr;
    g_phys.belt = belt;
    g_phys.contact = contact;
    g_phys.last_ms = millis();
    g_phys.valid = true;
    xSemaphoreGive(g_mutex);
}

PhysUplink physUplinkGet() {
    PhysUplink out;
    if (g_mutex != nullptr && xSemaphoreTake(g_mutex, pdMS_TO_TICKS(20)) == pdTRUE) {
        out = g_phys;
        xSemaphoreGive(g_mutex);
    }
    return out;
}

bool physUplinkStale(uint32_t now_ms, uint32_t timeout_ms) {
    const PhysUplink snap = physUplinkGet();
    if (!snap.valid || snap.last_ms == 0) {
        return true;
    }
    return (now_ms - snap.last_ms) > timeout_ms;
}
