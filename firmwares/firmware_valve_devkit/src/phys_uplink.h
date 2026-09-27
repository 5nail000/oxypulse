#pragma once

#include <cstdint>

struct PhysUplink {
    uint8_t spo2 = 0;
    uint8_t hr = 0;
    bool belt = false;
    bool contact = false;
    uint32_t last_ms = 0;
    bool valid = false;
};

void physUplinkInit();
void physUplinkSet(uint8_t spo2, uint8_t hr, bool belt, bool contact);
PhysUplink physUplinkGet();
bool physUplinkStale(uint32_t now_ms, uint32_t timeout_ms);
