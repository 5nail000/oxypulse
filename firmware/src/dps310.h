#pragma once

struct Dps310Snapshot {
    float pressure_hpa = 0.0f;
    float temp_c = 0.0f;
    bool ok = false;
};

void dps310Init();
Dps310Snapshot dps310GetSnapshot();
