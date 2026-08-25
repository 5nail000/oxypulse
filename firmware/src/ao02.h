#pragma once

struct Ao02Snapshot {
    float o2_percent = 0.0f;
    float voltage_mv = 0.0f;
    bool ok = false;
};

void ao02Init();
Ao02Snapshot ao02GetSnapshot();
