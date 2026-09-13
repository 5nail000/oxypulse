#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"

struct IgtPhaseMark {
    uint8_t kind = 0;  // 0 = hypoxia, 1 = normoxia
    uint8_t cycle = 0;
    uint32_t start_esp_ms = 0;
};

struct IgtPublicStatus {
    const char *state = "idle";
    const char *phase = "";
    char name[IGT_NAME_MAX] = "";
    char error[IGT_ERROR_MAX] = "";
    uint8_t cycle = 0;
    uint8_t cycles = 0;
    uint32_t hypoxia_sec = 0;
    uint32_t normoxia_sec = 0;
    uint32_t phase_remain_sec = 0;
    uint32_t fill_remain_sec = 0;
    uint32_t practice_started_esp_ms = 0;
    float o2_target = 0.0f;
    float pressure_start_hpa = 0.0f;
    float o2_now = 0.0f;
    float pressure_now = 0.0f;
    bool ready_to_start = false;
    bool sensor_warn = false;
    const char *buffer_prep = "idle";
    uint32_t buffer_prep_remain_sec = 0;
    bool waiting_buffer = false;
    size_t phase_count = 0;
    IgtPhaseMark phases[IGT_PHASE_MARK_MAX];
};

void igtSessionInit();
bool igtSessionPrep(const char *name, float o2_target, uint32_t hypoxia_sec, uint32_t normoxia_sec,
                    uint8_t cycles);
bool igtSessionStart();
bool igtSessionAbort();
bool igtSessionAck();
void igtSessionPoll();
bool igtSessionIsIdle();
bool igtSessionBlocksManual(uint8_t pin);
bool igtProtocolValid(float o2_target, uint32_t hypoxia_sec, uint32_t normoxia_sec, uint8_t cycles);
void igtSessionGetStatus(IgtPublicStatus *out);
