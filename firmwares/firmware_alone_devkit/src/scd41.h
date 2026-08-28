#pragma once

#include <cstdint>

struct Scd41Snapshot {
    uint16_t co2_ppm = 0;
    float temp_c = 0.0f;
    float rh_percent = 0.0f;
    bool warming_up = true;
    bool ok = false;
};

void scd41Init();
Scd41Snapshot scd41GetSnapshot();
