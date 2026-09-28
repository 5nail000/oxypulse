#pragma once

struct Ao02Snapshot {
    float o2_percent = 0.0f;
    float voltage_mv = 0.0f;
    bool ok = false;
};

// Буфер гипоксии: ADS1115 + AO-02. Рабочий O₂ — ocs3rl (UART).
void ao02Init();
Ao02Snapshot ao02GetSnapshot();
