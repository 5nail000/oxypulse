#pragma once

#include <cstdint>

enum class FlowPhase : int8_t {
    Idle = 0,
    Inhale = 1,
    Exhale = -1,
};

struct FlowSnapshot {
    float flow_slm = 0.0f;
    float inhale_l = 0.0f;
    float exhale_l = 0.0f;
    float ve_lpm = 0.0f;
    FlowPhase phase = FlowPhase::Idle;
    bool ok = false;
};

void sfm3300Init();
FlowSnapshot sfm3300GetSnapshot();
void sfm3300ResetVolume();
