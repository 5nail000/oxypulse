#include "scd41.h"

#include <Arduino.h>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "logger.h"
#include "sensirion_i2c.h"

namespace {

constexpr uint16_t CMD_START_PERIODIC = 0x21B1;
constexpr uint16_t CMD_STOP_PERIODIC = 0x3F86;
constexpr uint16_t CMD_READ_MEASUREMENT = 0xEC05;
constexpr uint16_t CMD_GET_DATA_READY = 0xE4B8;
constexpr uint16_t CMD_FORCED_RECALIBRATION = 0x362F;
constexpr uint16_t CMD_SET_ASC = 0x2416;
constexpr uint16_t CMD_PERSIST_SETTINGS = 0x3615;

SemaphoreHandle_t g_mutex = nullptr;
SemaphoreHandle_t g_pending_mutex = nullptr;
TaskHandle_t g_task = nullptr;

Scd41Snapshot g_snap;
uint32_t g_last_measurement_ms = 0;
uint32_t g_start_ms = 0;
bool g_ready = false;
bool g_asc_enabled = SCD41_ASC_ENABLED;

struct PendingCommands {
    bool frc = false;
    uint16_t frc_target_ppm = 400;
};

PendingCommands g_pending;

#if SCD41_CO2_DYNAMIC_COMP
uint16_t g_co2_history[SCD41_SLOPE_WINDOW] = {};
size_t g_co2_history_count = 0;
size_t g_co2_history_head = 0;

float g_temp_history[SCD41_SLOPE_WINDOW] = {};
size_t g_temp_history_count = 0;
size_t g_temp_history_head = 0;

float g_rh_history[SCD41_SLOPE_WINDOW] = {};
size_t g_rh_history_count = 0;
size_t g_rh_history_head = 0;

struct BaselineState {
    float co2_ppm = 400.0f;
    float temp_c = 25.0f;
    float rh_percent = 50.0f;
    bool initialized = false;
};

BaselineState g_baseline;

float clampf(float value, float min_v, float max_v) {
    if (value < min_v) {
        return min_v;
    }
    if (value > max_v) {
        return max_v;
    }
    return value;
}

void resetDynamicComp() {
    g_baseline = BaselineState{};
    g_co2_history_count = 0;
    g_co2_history_head = 0;
    g_temp_history_count = 0;
    g_temp_history_head = 0;
    g_rh_history_count = 0;
    g_rh_history_head = 0;
}

bool isBreathEvent(uint16_t co2_ppm, float temp_c, float rh_percent) {
    if (!g_baseline.initialized) {
        return false;
    }
    return co2_ppm > (static_cast<uint16_t>(g_baseline.co2_ppm) + SCD41_EVENT_CO2_PPM) ||
           temp_c > (g_baseline.temp_c + SCD41_EVENT_TEMP_C) ||
           rh_percent > (g_baseline.rh_percent + SCD41_EVENT_RH);
}

void updateBaseline(uint16_t co2_ppm, float temp_c, float rh_percent, bool warming_up) {
    if (warming_up) {
        return;
    }
    if (!g_baseline.initialized) {
        g_baseline.co2_ppm = static_cast<float>(co2_ppm);
        g_baseline.temp_c = temp_c;
        g_baseline.rh_percent = rh_percent;
        g_baseline.initialized = true;
        return;
    }
    if (isBreathEvent(co2_ppm, temp_c, rh_percent)) {
        return;
    }

    const float alpha = SCD41_BASELINE_ALPHA;
    g_baseline.co2_ppm += alpha * (static_cast<float>(co2_ppm) - g_baseline.co2_ppm);
    g_baseline.temp_c += alpha * (temp_c - g_baseline.temp_c);
    g_baseline.rh_percent += alpha * (rh_percent - g_baseline.rh_percent);
}

template <typename T>
void pushHistory(T *history, size_t &head, size_t &count, T value) {
    history[head] = value;
    head = (head + 1) % SCD41_SLOPE_WINDOW;
    if (count < SCD41_SLOPE_WINDOW) {
        ++count;
    }
}

template <typename T>
float estimateSlopePerSec(const T *history, size_t count, size_t head) {
    if (count < 2) {
        return 0.0f;
    }

    const float mean_i = static_cast<float>(count - 1) / 2.0f;
    float num = 0.0f;
    float den = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        const size_t idx = (head + SCD41_SLOPE_WINDOW - count + i) % SCD41_SLOPE_WINDOW;
        const float di = static_cast<float>(i) - mean_i;
        num += di * static_cast<float>(history[idx]);
        den += di * di;
    }
    if (den <= 0.0f) {
        return 0.0f;
    }
    return (num / den) / SCD41_SAMPLE_INTERVAL_S;
}

template <typename T>
float compensateChannel(float raw,
                        float tau_s,
                        float trust,
                        float max_corr,
                        float decay_damping,
                        const T *history,
                        size_t count,
                        size_t head,
                        float baseline,
                        float margin_below,
                        float margin_above,
                        float min_v,
                        float max_v) {
    const float slope = estimateSlopePerSec(history, count, head);
    float correction = tau_s * trust * slope;
    if (slope < 0.0f) {
        correction *= decay_damping;
    }
    correction = clampf(correction, -max_corr, max_corr);

    float result = raw + correction;
    if (g_baseline.initialized) {
        result = clampf(result, baseline - margin_below, baseline + margin_above);
    }
    return clampf(result, min_v, max_v);
}

uint16_t compensateCo2(uint16_t raw_ppm) {
    const float slope =
        estimateSlopePerSec(g_co2_history, g_co2_history_count, g_co2_history_head);
    float correction = SCD41_TAU_CO2_S * SCD41_TAU_TRUST_CO2 * slope;
    correction = clampf(correction, -SCD41_MAX_CORR_PPM, SCD41_MAX_CORR_PPM);
    const float compensated = static_cast<float>(raw_ppm) + correction;
    return static_cast<uint16_t>(clampf(compensated, 0.0f, 65535.0f) + 0.5f);
}

float compensateTemp(float raw_c) {
    return compensateChannel(
        raw_c,
        SCD41_TAU_TEMP_S,
        SCD41_TAU_TRUST_TEMP,
        SCD41_MAX_CORR_TEMP_C,
        SCD41_DECAY_DAMPING_TEMP,
        g_temp_history,
        g_temp_history_count,
        g_temp_history_head,
        g_baseline.temp_c,
        SCD41_CLAMP_TEMP_BELOW_C,
        SCD41_CLAMP_TEMP_ABOVE_C,
        -10.0f,
        60.0f);
}

float compensateRh(float raw_rh) {
    return compensateChannel(
        raw_rh,
        SCD41_TAU_RH_S,
        SCD41_TAU_TRUST_RH,
        SCD41_MAX_CORR_RH,
        SCD41_DECAY_DAMPING_RH,
        g_rh_history,
        g_rh_history_count,
        g_rh_history_head,
        g_baseline.rh_percent,
        SCD41_CLAMP_RH_BELOW,
        SCD41_CLAMP_RH_ABOVE,
        0.0f,
        100.0f);
}
#endif

Scd41Snapshot emptySnapshot() {
    Scd41Snapshot snap{};
    snap.asc_enabled = g_asc_enabled;
    return snap;
}

void setSnapshot(const Scd41Snapshot &snap) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_snap = snap;
    xSemaphoreGive(g_mutex);
}

PendingCommands takePending() {
    PendingCommands taken{};
    if (g_pending_mutex == nullptr ||
        xSemaphoreTake(g_pending_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return taken;
    }
    taken = g_pending;
    g_pending = PendingCommands{};
    xSemaphoreGive(g_pending_mutex);
    return taken;
}

bool startPeriodic() {
    sensirionSendCommand(ADDR_SCD41, CMD_STOP_PERIODIC);
    vTaskDelay(pdMS_TO_TICKS(500));
    if (!sensirionSendCommand(ADDR_SCD41, CMD_START_PERIODIC)) {
        return false;
    }
    g_start_ms = millis();
#if SCD41_CO2_DYNAMIC_COMP
    resetDynamicComp();
#endif
    return true;
}

bool applyAscFromBuildFlag() {
    sensirionSendCommand(ADDR_SCD41, CMD_STOP_PERIODIC);
    vTaskDelay(pdMS_TO_TICKS(500));

    const bool want = SCD41_ASC_ENABLED != 0;
    if (sensirionSendCommandArg(ADDR_SCD41, CMD_SET_ASC, want ? 1 : 0)) {
        g_asc_enabled = want;
        logPrintf("SCD41: ASC %s (SCD41_ASC_ENABLED=%d)",
                  want ? "включена" : "выключена",
                  SCD41_ASC_ENABLED);
    } else {
        logPrintf("SCD41: не удалось задать ASC");
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    sensirionSendCommand(ADDR_SCD41, CMD_PERSIST_SETTINGS);
    vTaskDelay(pdMS_TO_TICKS(800));
    return startPeriodic();
}

bool isDataReady() {
    uint16_t status = 0;
    if (!sensirionReadAfterCommand(ADDR_SCD41, CMD_GET_DATA_READY, &status, 1, 2)) {
        return false;
    }
    return (status & 0x07FF) != 0;
}

bool readMeasurement(uint16_t &co2_ppm, float &temp_c, float &rh_percent) {
    uint16_t words[3] = {0, 0, 0};
    if (!sensirionReadAfterCommand(ADDR_SCD41, CMD_READ_MEASUREMENT, words, 3, 2)) {
        return false;
    }
    co2_ppm = words[0];
    temp_c = -45.0f + 175.0f * static_cast<float>(words[1]) / 65535.0f;
    rh_percent = 100.0f * static_cast<float>(words[2]) / 65535.0f;
    return true;
}

void applyPending(const PendingCommands &pending) {
    if (!pending.frc) {
        return;
    }

    sensirionSendCommand(ADDR_SCD41, CMD_STOP_PERIODIC);
    vTaskDelay(pdMS_TO_TICKS(500));

    if (pending.frc) {
        uint16_t correction = 0;
        if (sensirionSendCommandArg(
                ADDR_SCD41, CMD_FORCED_RECALIBRATION, pending.frc_target_ppm)) {
            vTaskDelay(pdMS_TO_TICKS(400));
            if (sensirionReadWords(ADDR_SCD41, &correction, 1) && correction != 0xFFFF) {
                logPrintf("SCD41: FRC на %u ppm выполнена, коррекция = %d",
                          static_cast<unsigned>(pending.frc_target_ppm),
                          static_cast<int>(correction) - 0x8000);
            } else {
                logPrintf("SCD41: FRC отклонена датчиком (мало времени прогрева?)");
            }
        } else {
            logPrintf("SCD41: не удалось запустить FRC");
        }
    }

    sensirionSendCommand(ADDR_SCD41, CMD_PERSIST_SETTINGS);
    vTaskDelay(pdMS_TO_TICKS(800));

    if (!startPeriodic()) {
        g_ready = false;
    }
}

void scd41Task(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;

    for (;;) {
        if (!g_ready) {
            g_ready = startPeriodic();
            if (!g_ready) {
#if SCD41_CO2_DYNAMIC_COMP
                resetDynamicComp();
#endif
                setSnapshot(emptySnapshot());
                logPrintf("SCD41: not found (0x%02X), retry", ADDR_SCD41);
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            logPrintf("SCD41: periodic measurement started (5 s interval)");
        }

        const PendingCommands pending = takePending();
        applyPending(pending);

        const uint32_t now = millis();
        if (isDataReady()) {
            uint16_t co2 = 0;
            float temp = 0.0f;
            float rh = 0.0f;
            if (readMeasurement(co2, temp, rh) && co2 != 0) {
                g_last_measurement_ms = now;
                Scd41Snapshot snap;
                snap.co2_ppm_raw = co2;
                snap.temp_c = temp;
                snap.rh_percent = rh;
                snap.warming_up = (now - g_start_ms) < SCD41_WARMUP_MS;
                snap.ok = true;
                snap.asc_enabled = g_asc_enabled;
#if SCD41_CO2_DYNAMIC_COMP
                snap.dynamic_comp = true;
                updateBaseline(co2, temp, rh, snap.warming_up);
                pushHistory(g_co2_history, g_co2_history_head, g_co2_history_count, co2);
                pushHistory(g_temp_history, g_temp_history_head, g_temp_history_count, temp);
                pushHistory(g_rh_history, g_rh_history_head, g_rh_history_count, rh);
                if (snap.warming_up) {
                    snap.co2_ppm_est = co2;
                    snap.temp_c_est = temp;
                    snap.rh_percent_est = rh;
                } else {
                    snap.co2_ppm_est = compensateCo2(co2);
                    snap.temp_c_est = compensateTemp(temp);
                    snap.rh_percent_est = compensateRh(rh);
                }
                snap.co2_ppm = snap.co2_ppm_est;
#else
                snap.dynamic_comp = false;
                snap.co2_ppm_est = co2;
                snap.temp_c_est = temp;
                snap.rh_percent_est = rh;
                snap.co2_ppm = co2;
#endif
                setSnapshot(snap);
            }
        } else if (g_last_measurement_ms != 0) {
            Scd41Snapshot snap = scd41GetSnapshot();
            if ((now - g_last_measurement_ms) > 15000) {
                g_ready = false;
                g_last_measurement_ms = 0;
#if SCD41_CO2_DYNAMIC_COMP
                resetDynamicComp();
#endif
                snap = emptySnapshot();
            }
            setSnapshot(snap);
        }

        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const Scd41Snapshot snap = scd41GetSnapshot();
            if (snap.ok) {
#if SCD41_CO2_DYNAMIC_COMP
                logPrintf(
                    "SCD41: CO2 raw=%u est=%u ppm  T=%.2f (est=%.2f) C  RH=%.1f (est=%.1f) %%%s",
                    static_cast<unsigned>(snap.co2_ppm_raw),
                    static_cast<unsigned>(snap.co2_ppm_est),
                    static_cast<double>(snap.temp_c),
                    static_cast<double>(snap.temp_c_est),
                    static_cast<double>(snap.rh_percent),
                    static_cast<double>(snap.rh_percent_est),
                    snap.warming_up ? "  (warmup)" : "");
#else
                logPrintf("SCD41: CO2=%u ppm  T=%.2f C  RH=%.1f %%%s",
                          static_cast<unsigned>(snap.co2_ppm),
                          static_cast<double>(snap.temp_c),
                          static_cast<double>(snap.rh_percent),
                          snap.warming_up ? "  (warmup)" : "");
#endif
            }
        }

        vTaskDelay(pdMS_TO_TICKS(SCD41_POLL_INTERVAL_MS));
    }
}

}  // namespace

void scd41Init() {
    g_mutex = xSemaphoreCreateMutex();
    g_pending_mutex = xSemaphoreCreateMutex();
    g_ready = applyAscFromBuildFlag();
    if (g_ready) {
        logPrintf("SCD41: init OK (0x%02X), ASC=%s",
                  ADDR_SCD41,
                  g_asc_enabled ? "on" : "off");
#if SCD41_CO2_DYNAMIC_COMP
        logPrintf("SCD41: dynamic comp ON, tau=%.0f s, trust=%.2f, max_delta=%.0f ppm, window=%u",
                  static_cast<double>(SCD41_TAU_CO2_S),
                  static_cast<double>(SCD41_TAU_TRUST_CO2),
                  static_cast<double>(SCD41_MAX_CORR_PPM),
                  static_cast<unsigned>(SCD41_SLOPE_WINDOW));
#endif
    } else {
        logPrintf("SCD41: init failed");
    }
    xTaskCreatePinnedToCore(scd41Task, "scd41Task", 4096, nullptr, 1, &g_task, 1);
}

Scd41Snapshot scd41GetSnapshot() {
    Scd41Snapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap = g_snap;
    xSemaphoreGive(g_mutex);
    return snap;
}

void scd41RequestForcedRecalibration(uint16_t target_ppm) {
    if (g_pending_mutex == nullptr ||
        xSemaphoreTake(g_pending_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }
    g_pending.frc = true;
    g_pending.frc_target_ppm = target_ppm;
    xSemaphoreGive(g_pending_mutex);
    logPrintf("SCD41: запрошена FRC на %u ppm", static_cast<unsigned>(target_ppm));
}
