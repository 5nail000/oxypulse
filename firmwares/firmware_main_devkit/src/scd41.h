#pragma once

#include <cstdint>

struct Scd41Snapshot {
    uint16_t co2_ppm = 0;       // отображаемое (est при comp, иначе raw)
    uint16_t co2_ppm_raw = 0;   // с чипа
    uint16_t co2_ppm_est = 0;   // оценка с компенсацией инерции
    float temp_c = 0.0f;
    float temp_c_est = 0.0f;
    float rh_percent = 0.0f;
    float rh_percent_est = 0.0f;
    bool warming_up = true;
    bool ok = false;
    bool dynamic_comp = false;
    bool asc_enabled = true;
};

void scd41Init();
Scd41Snapshot scd41GetSnapshot();
void scd41RequestForcedRecalibration(uint16_t target_ppm);
