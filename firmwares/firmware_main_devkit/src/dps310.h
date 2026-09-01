#pragma once

struct Dps310Snapshot {
    float pressure_hpa = 0.0f;
    float temp_c = 0.0f;
    float baseline_hpa = 0.0f;
    bool ok = false;
    bool baseline_ok = false;
};

void dps310Init();
void workingPressureInit();
Dps310Snapshot dps310GetSnapshot();
Dps310Snapshot workingPressureGetSnapshot();
