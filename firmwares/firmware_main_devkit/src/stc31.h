#pragma once

#include <cstdint>

// STC31-C (рабочий контур). Поля ppm/est сохранены для совместимости API со SCD41.
struct Stc31Snapshot {
    uint16_t co2_ppm = 0;       // percent * 10000 (для UI/трендов)
    uint16_t co2_ppm_raw = 0;
    uint16_t co2_ppm_est = 0;   // = raw (динамическая компенсация не нужна)
    float co2_percent = 0.0f;   // vol% с чипа
    uint16_t gas_ticks = 0;     // сырое слово концентрации STC31
    float temp_c = 0.0f;
    float temp_c_est = 0.0f;
    float rh_percent = 0.0f;    // RH, записанная в STC31 с SHT4x
    float rh_percent_est = 0.0f;
    float sht_temp_c = 0.0f;    // температура SHT4x, в чип не пишется
    bool warming_up = false;
    bool ok = false;
    bool dynamic_comp = false;
    bool asc_enabled = false;
};

void stc31Init();
Stc31Snapshot stc31GetSnapshot();
// target_ppm: 400…2000 (как в API co2_frc); внутри переводится в vol% STC31.
void stc31RequestForcedRecalibration(uint16_t target_ppm);
