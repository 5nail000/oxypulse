#pragma once

#include <cstdint>

#include "ao02.h"

// Winpower OCS-3RL2.0 — ультразвуковой O₂, UART 9600 3.3 V TTL (рабочий контур).
// Кадр: O₂, влажность, температура, давление. Потока LPM в протоколе нет.
struct Ocs3rlSnapshot {
    float o2_percent = 0.0f;
    float temp_c = 0.0f;
    float rh_percent = 0.0f;
    float pressure_hpa = 0.0f;
    uint8_t p_in_kpa = 0;
    uint8_t p_out_kpa = 0;
    float aux_raw = 0.0f;
    // Сырые u16/10 из короткого кадра (и аналоги для ext).
    float field_a = 0.0f;
    float field_b = 0.0f;
    float field_c = 0.0f;
    uint16_t status_raw = 0;
    int o2_field_index = -1;  // 0=a,1=b,2=c
    bool ok = false;
    bool full_range_frame = false;
};

void ocs3rlInit();
Ocs3rlSnapshot ocs3rlGetSnapshot();
// Сырой O₂ с OCS минус поправка на избыток CO₂ (vol%) относительно комнатного фона.
float ocs3rlO2Co2Compensated(float o2_raw_percent, float co2_vol_percent, bool co2_ok);

void workingO2Init();
Ao02Snapshot workingO2GetSnapshot();
