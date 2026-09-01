#include "sfm3300.h"

#include <Arduino.h>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "logger.h"
#include "sensirion_i2c.h"

namespace {

constexpr uint16_t CMD_START_CONTINUOUS = 0x1000;
constexpr uint16_t CMD_SOFT_RESET = 0x2000;
constexpr uint16_t CMD_READ_SCALE_FACTOR = 0x30DE;
constexpr UBaseType_t FLOW_TASK_PRIORITY = 3;

SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;
float g_flow_slm = 0.0f;
float g_inhale_l = 0.0f;
float g_exhale_l = 0.0f;
float g_ve_lpm = 0.0f;
FlowPhase g_phase = FlowPhase::Idle;
bool g_ok = false;
bool g_ready = false;
bool g_volume_primed = false;
uint32_t g_last_sample_ms = 0;

float g_bucket_inh[FLOW_VE_BUCKETS] = {};
float g_bucket_exh[FLOW_VE_BUCKETS] = {};
size_t g_bucket_head = 0;
size_t g_bucket_count = 0;
float g_cur_bucket_inh = 0.0f;
float g_cur_bucket_exh = 0.0f;
uint32_t g_bucket_period_ms = 0;
float g_window_inh = 0.0f;
float g_window_exh = 0.0f;
uint32_t g_window_start_ms = 0;

float g_scale = SFM_FLOW_SCALE;
float g_offset = SFM_FLOW_OFFSET;

void setSnapshot(float flow_slm, bool ok) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_flow_slm = flow_slm;
    g_ok = ok;
    xSemaphoreGive(g_mutex);
}

void resetVeWindowLocked(uint32_t now_ms) {
    for (size_t i = 0; i < FLOW_VE_BUCKETS; ++i) {
        g_bucket_inh[i] = 0.0f;
        g_bucket_exh[i] = 0.0f;
    }
    g_bucket_head = 0;
    g_bucket_count = 0;
    g_cur_bucket_inh = 0.0f;
    g_cur_bucket_exh = 0.0f;
    g_bucket_period_ms = now_ms;
    g_window_inh = 0.0f;
    g_window_exh = 0.0f;
    g_window_start_ms = now_ms;
    g_ve_lpm = 0.0f;
}

void resetVolumeLocked() {
    g_inhale_l = 0.0f;
    g_exhale_l = 0.0f;
    g_phase = FlowPhase::Idle;
    g_volume_primed = false;
    g_last_sample_ms = 0;
    resetVeWindowLocked(millis());
}

void commitBucket() {
    g_window_inh -= g_bucket_inh[g_bucket_head];
    g_window_exh -= g_bucket_exh[g_bucket_head];

    g_bucket_inh[g_bucket_head] = g_cur_bucket_inh;
    g_bucket_exh[g_bucket_head] = g_cur_bucket_exh;
    g_window_inh += g_cur_bucket_inh;
    g_window_exh += g_cur_bucket_exh;

    g_cur_bucket_inh = 0.0f;
    g_cur_bucket_exh = 0.0f;
    g_bucket_head = (g_bucket_head + 1) % FLOW_VE_BUCKETS;
    if (g_bucket_count < FLOW_VE_BUCKETS) {
        ++g_bucket_count;
    }
}

void updateVeLpm(uint32_t now_ms) {
    uint32_t span_ms = now_ms - g_window_start_ms;
    if (span_ms < FLOW_VE_BUCKET_MS) {
        g_ve_lpm = 0.0f;
        return;
    }
    if (span_ms > FLOW_VE_WINDOW_MS) {
        span_ms = FLOW_VE_WINDOW_MS;
    }
    const float span_min = static_cast<float>(span_ms) / 60000.0f;
    g_ve_lpm = ((g_window_inh + g_window_exh + g_cur_bucket_inh + g_cur_bucket_exh) * 0.5f) /
               span_min;
}

void updatePhase(float effective_slm) {
    if (effective_slm > FLOW_PHASE_THRESHOLD_SLM) {
        g_phase = FlowPhase::Inhale;
    } else if (effective_slm < -FLOW_PHASE_THRESHOLD_SLM) {
        g_phase = FlowPhase::Exhale;
    } else if (std::fabs(effective_slm) < FLOW_DEADBAND_SLM) {
        g_phase = FlowPhase::Idle;
    }
}

void integrateFlow(float flow_slm, uint32_t now_ms) {
    if (!g_volume_primed) {
        g_volume_primed = true;
        g_last_sample_ms = now_ms;
        resetVeWindowLocked(now_ms);
        return;
    }

    const uint32_t dt_ms = now_ms - g_last_sample_ms;
    g_last_sample_ms = now_ms;
    if (dt_ms == 0 || dt_ms > 1000) {
        return;
    }

    const float effective =
        (std::fabs(flow_slm) < FLOW_DEADBAND_SLM) ? 0.0f : flow_slm;
    const float delta_l = effective * static_cast<float>(dt_ms) / 60000.0f;

    if (delta_l > 0.0f) {
        g_inhale_l += delta_l;
        g_cur_bucket_inh += delta_l;
    } else if (delta_l < 0.0f) {
        g_exhale_l += -delta_l;
        g_cur_bucket_exh += -delta_l;
    }

    updatePhase(effective);

    if ((now_ms - g_bucket_period_ms) >= FLOW_VE_BUCKET_MS) {
        commitBucket();
        g_bucket_period_ms = now_ms;
    }
    updateVeLpm(now_ms);
}

bool startContinuous() {
    uint16_t discard = 0;
    if (!sensirionStartContinuousMassFlow(
            ADDR_SFM3300, CMD_START_CONTINUOUS, &discard, 80, 5)) {
        logPrintf("SFM3300: discard read after 0x1000 failed");
        return false;
    }
    return true;
}

bool startSensor() {
    if (!sensirionSendCommand(ADDR_SFM3300, CMD_SOFT_RESET)) {
        logPrintf("SFM3300: soft reset failed");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    g_scale = SFM_FLOW_SCALE;
    g_offset = SFM_FLOW_OFFSET;
    if (!startContinuous()) {
        return false;
    }

    uint16_t scale_raw = 0;
    if (sensirionReadAfterCommandMassFlow(ADDR_SFM3300, CMD_READ_SCALE_FACTOR, &scale_raw, 1, 5) &&
        scale_raw != 0) {
        g_scale = static_cast<float>(scale_raw);
        logPrintf("SFM3300: scale factor = %u", static_cast<unsigned>(scale_raw));
        return startContinuous();
    }

    logPrintf("SFM3300: scale factor from config = %.1f", static_cast<double>(g_scale));
    return true;
}

void flowTask(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;
    uint32_t fail_streak = 0;

    for (;;) {
        if (!g_ready) {
            g_ready = startSensor();
            if (!g_ready) {
                setSnapshot(0.0f, false);
                logPrintf("SFM3300: not found (0x%02X), retry", ADDR_SFM3300);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }
            logPrintf("SFM3300: ready, poll %u Hz",
                      static_cast<unsigned>(1000 / SFM_POLL_INTERVAL_MS));
        }

        uint16_t raw = 0;
        if (!sensirionReadWordsMassFlow(ADDR_SFM3300, &raw, 1)) {
            if (++fail_streak >= 10) {
                fail_streak = 0;
                g_ready = false;
                setSnapshot(0.0f, false);
                logPrintf("SFM3300: 10 read failures, re-init");
            }
            vTaskDelay(pdMS_TO_TICKS(SFM_POLL_INTERVAL_MS));
            continue;
        }
        fail_streak = 0;

        // Инверсия знака: на стенде «+» сенсора = выдох в маску, «−» = вдох.
        const float flow_slm =
            -(static_cast<float>(raw) - g_offset) / g_scale;
        const uint32_t now = millis();
        if (g_mutex != nullptr && xSemaphoreTake(g_mutex, portMAX_DELAY) == pdTRUE) {
            g_flow_slm = flow_slm;
            g_ok = true;
            integrateFlow(flow_slm, now);
            xSemaphoreGive(g_mutex);
        }

        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const char *phase =
                g_phase == FlowPhase::Inhale ? "IN" :
                g_phase == FlowPhase::Exhale ? "OUT" : "—";
            logPrintf("SFM3300: flow=%.2f slm [%s]  in=%.3f л  ex=%.3f л  VE=%.1f л/мин",
                      static_cast<double>(flow_slm),
                      phase,
                      static_cast<double>(g_inhale_l),
                      static_cast<double>(g_exhale_l),
                      static_cast<double>(g_ve_lpm));
        }

        vTaskDelay(pdMS_TO_TICKS(SFM_POLL_INTERVAL_MS));
    }
}

}  // namespace

void sfm3300Init() {
    g_mutex = xSemaphoreCreateMutex();
    g_ready = startSensor();
    if (g_ready) {
        logPrintf("SFM3300: init OK (0x%02X)", ADDR_SFM3300);
    } else {
        logPrintf("SFM3300: init failed");
    }
    xTaskCreatePinnedToCore(flowTask, "flowTask", 4096, nullptr, FLOW_TASK_PRIORITY, &g_task, 1);
}

FlowSnapshot sfm3300GetSnapshot() {
    FlowSnapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap.flow_slm = g_flow_slm;
    snap.inhale_l = g_inhale_l;
    snap.exhale_l = g_exhale_l;
    snap.ve_lpm = g_ve_lpm;
    snap.phase = g_phase;
    snap.ok = g_ok;
    xSemaphoreGive(g_mutex);
    return snap;
}

void sfm3300ResetVolume() {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    resetVolumeLocked();
    xSemaphoreGive(g_mutex);
    logPrintf("SFM3300: объёмы сброшены");
}
