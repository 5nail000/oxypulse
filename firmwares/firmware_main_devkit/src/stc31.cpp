#include "stc31.h"

#include <Arduino.h>
#include <cmath>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "logger.h"
#include "sensirion_i2c.h"
#include "sht4x.h"

namespace {

// Sensirion STC31-C command set (datasheet §3.3).
constexpr uint16_t CMD_SET_BINARY_GAS = 0x3615;
constexpr uint16_t CMD_SET_RH = 0x3624;
constexpr uint16_t CMD_SET_TEMP = 0x361E;
constexpr uint16_t CMD_SET_PRESSURE = 0x362F;
constexpr uint16_t CMD_MEASURE = 0x3639;
constexpr uint16_t CMD_FRC = 0x3661;
constexpr uint16_t CMD_ENABLE_ASC = 0x3FEF;
constexpr uint16_t CMD_DISABLE_ASC = 0x3F6E;

// Low-cross-sensitivity, CO₂ in air, 0…40 vol% (меньше чувствительность к ошибкам T/RH/P).
constexpr uint16_t BINARY_GAS_CO2_AIR_40 = 0x0013;

SemaphoreHandle_t g_mutex = nullptr;
SemaphoreHandle_t g_pending_mutex = nullptr;
TaskHandle_t g_task = nullptr;

Stc31Snapshot g_snap;
bool g_ready = false;
bool g_asc_enabled = STC31_ASC_ENABLED != 0;
uint32_t g_start_ms = 0;
uint32_t g_last_ok_ms = 0;
float g_last_rh = -1.0f;
float g_sent_rh = -1.0f;
float g_last_sht_t = 0.0f;

struct PendingCommands {
    bool frc = false;
    uint16_t frc_target_ppm = 400;
};

PendingCommands g_pending;

uint16_t percentToRaw(float vol_percent) {
    if (vol_percent < 0.0f) {
        vol_percent = 0.0f;
    }
    if (vol_percent > 100.0f) {
        vol_percent = 100.0f;
    }
    // Datasheet STC31-C §3.5 / Sensirion driver:
    // raw = vol% / 100 * 2^15 + 2^14
    const float raw = vol_percent * 327.68f + 16384.0f;
    if (raw <= 0.0f) {
        return 0;
    }
    if (raw >= 65535.0f) {
        return 65535;
    }
    return static_cast<uint16_t>(raw + 0.5f);
}

float rawToPercent(uint16_t raw) {
    // C[%] = (S - 2^14) / 2^15 * 100. Не обрезаем: воздух ~0.04%, минус = сдвиг нуля.
    return (static_cast<float>(raw) - 16384.0f) / 32768.0f * 100.0f;
}

uint16_t percentToPpm(float vol_percent) {
    const float ppm = vol_percent * 10000.0f;
    if (ppm <= 0.0f) {
        return 0;
    }
    if (ppm >= 65535.0f) {
        return 65535;
    }
    return static_cast<uint16_t>(ppm + 0.5f);
}

uint16_t ppmToFrcRaw(uint16_t target_ppm) {
    // vol% = ppm / 10000; FRC ticks = vol%·327.68 + 16384
    const float vol = static_cast<float>(target_ppm) / 10000.0f;
    return percentToRaw(vol);
}

uint16_t rhToRaw(float rh_percent) {
    if (rh_percent < 0.0f) {
        rh_percent = 0.0f;
    }
    if (rh_percent > 100.0f) {
        rh_percent = 100.0f;
    }
    return static_cast<uint16_t>(rh_percent / 100.0f * 65535.0f + 0.5f);
}

int16_t tempToRaw(float temp_c) {
    return static_cast<int16_t>(temp_c * 200.0f);
}

uint16_t pressureToRaw(float hpa) {
    if (hpa < 0.0f) {
        hpa = 0.0f;
    }
    if (hpa > 65535.0f) {
        hpa = 65535.0f;
    }
    return static_cast<uint16_t>(hpa + 0.5f);
}

void setSnapshot(const Stc31Snapshot &snap) {
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

bool configureSensor() {
    // Без general-call soft reset: на рабочей шине также SFM3300/DPS.
    if (!sensirionSendCommandArg(ADDR_STC31, CMD_SET_BINARY_GAS, BINARY_GAS_CO2_AIR_40)) {
        logPrintf("STC31: set binary gas failed");
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    const uint16_t asc_cmd = (STC31_ASC_ENABLED != 0) ? CMD_ENABLE_ASC : CMD_DISABLE_ASC;
    if (sensirionSendCommand(ADDR_STC31, asc_cmd)) {
        g_asc_enabled = STC31_ASC_ENABLED != 0;
        logPrintf("STC31: ASC %s", g_asc_enabled ? "on" : "off");
    } else {
        logPrintf("STC31: ASC command failed");
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    g_start_ms = millis();
    return true;
}

bool applyCompensation() {
    // RH только с SHT4x на той же шине. Без команды STC31 считает воздух сухим.
    // T чипа не подменяем. Давление не пишем: дефолт 1013 мбар.
    float sht_t = 0.0f;
    float rh = 0.0f;
    if (!sht4xRead(sht_t, rh)) {
        return true;
    }
    g_last_rh = rh;
    g_last_sht_t = sht_t;
    if (g_sent_rh >= 0.0f && std::fabs(rh - g_sent_rh) < 1.0f) {
        return true;
    }
    if (!sensirionSendCommandArg(ADDR_STC31, CMD_SET_RH, rhToRaw(rh))) {
        logPrintf("STC31: set RH %.1f %% не записалась", static_cast<double>(rh));
        return true;
    }
    g_sent_rh = rh;
    logPrintf("STC31: RH %.1f %%  T=%.1f C с SHT4x",
              static_cast<double>(rh),
              static_cast<double>(sht_t));
    return true;
}

bool measureOnce(float &co2_percent, float &temp_c, uint16_t &ticks) {
    if (!applyCompensation()) {
        return false;
    }
    uint16_t words[2] = {0, 0};
    // Measurement ~110 ms in low-cross-sensitivity mode.
    if (!sensirionReadAfterCommand(ADDR_STC31, CMD_MEASURE, words, 2, 120)) {
        return false;
    }
    ticks = words[0];
    co2_percent = rawToPercent(words[0]);
    temp_c = static_cast<float>(static_cast<int16_t>(words[1])) / 200.0f;
    static uint32_t last_tick_log_ms = 0;
    const uint32_t now = millis();
    if ((now - last_tick_log_ms) >= 2000) {
        last_tick_log_ms = now;
        logPrintf("STC31: ticks=%u → %.3f vol%%  T_raw=%d → %.2f C",
                  static_cast<unsigned>(words[0]),
                  static_cast<double>(co2_percent),
                  static_cast<int>(static_cast<int16_t>(words[1])),
                  static_cast<double>(temp_c));
    }
    return true;
}

void applyPending(const PendingCommands &pending) {
    if (!pending.frc) {
        return;
    }
    const uint16_t frc_raw = ppmToFrcRaw(pending.frc_target_ppm);
    applyCompensation();
    if (sensirionSendCommandArg(ADDR_STC31, CMD_FRC, frc_raw)) {
        vTaskDelay(pdMS_TO_TICKS(120));
        logPrintf("STC31: FRC на %u ppm (raw=%u, ~%.3f vol%%)",
                  static_cast<unsigned>(pending.frc_target_ppm),
                  static_cast<unsigned>(frc_raw),
                  static_cast<double>(static_cast<float>(pending.frc_target_ppm) / 10000.0f));
    } else {
        logPrintf("STC31: FRC не удалась");
    }
}

void stc31Task(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;

    for (;;) {
        if (!g_ready) {
            g_ready = configureSensor();
            if (!g_ready) {
                setSnapshot(Stc31Snapshot{});
                logPrintf("STC31: not found (0x%02X), retry", ADDR_STC31);
                vTaskDelay(pdMS_TO_TICKS(2000));
                continue;
            }
            logPrintf("STC31: ready, CO2-in-air 0..40 vol%%, poll=%u ms",
                      static_cast<unsigned>(STC31_POLL_INTERVAL_MS));
        }

        applyPending(takePending());

        float co2_percent = 0.0f;
        float temp_c = 0.0f;
        uint16_t ticks = 0;
        const uint32_t now = millis();
        if (measureOnce(co2_percent, temp_c, ticks)) {
            g_last_ok_ms = now;
            Stc31Snapshot snap;
            snap.co2_percent = co2_percent;
            snap.gas_ticks = ticks;
            snap.co2_ppm = percentToPpm(co2_percent);
            snap.co2_ppm_raw = snap.co2_ppm;
            snap.co2_ppm_est = snap.co2_ppm;
            snap.temp_c = temp_c;
            snap.temp_c_est = temp_c;
            snap.rh_percent = g_last_rh >= 0.0f ? g_last_rh : 0.0f;
            snap.rh_percent_est = snap.rh_percent;
            snap.sht_temp_c = g_last_sht_t;
            snap.warming_up = (now - g_start_ms) < STC31_WARMUP_MS;
            snap.ok = true;
            snap.dynamic_comp = false;
            snap.asc_enabled = g_asc_enabled;
            setSnapshot(snap);
        } else if (g_last_ok_ms != 0 && (now - g_last_ok_ms) > 5000) {
            g_ready = false;
            g_last_ok_ms = 0;
            setSnapshot(Stc31Snapshot{});
            logPrintf("STC31: measurement lost, reinit");
        }

        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const Stc31Snapshot snap = stc31GetSnapshot();
            if (snap.ok) {
                logPrintf("STC31: CO2=%.2f %% (%u ppm)  T=%.2f C%s",
                          static_cast<double>(snap.co2_percent),
                          static_cast<unsigned>(snap.co2_ppm),
                          static_cast<double>(snap.temp_c),
                          snap.warming_up ? "  (warmup)" : "");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(STC31_POLL_INTERVAL_MS));
    }
}

}  // namespace

void stc31Init() {
    g_mutex = xSemaphoreCreateMutex();
    g_pending_mutex = xSemaphoreCreateMutex();
    g_ready = configureSensor();
    if (g_ready) {
        logPrintf("STC31: init OK (0x%02X)", ADDR_STC31);
    } else {
        logPrintf("STC31: init failed");
    }
    xTaskCreatePinnedToCore(stc31Task, "stc31Task", 4096, nullptr, 1, &g_task, 1);
}

Stc31Snapshot stc31GetSnapshot() {
    Stc31Snapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap = g_snap;
    xSemaphoreGive(g_mutex);
    return snap;
}

void stc31RequestForcedRecalibration(uint16_t target_ppm) {
    if (g_pending_mutex == nullptr ||
        xSemaphoreTake(g_pending_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        return;
    }
    g_pending.frc = true;
    g_pending.frc_target_ppm = target_ppm;
    xSemaphoreGive(g_pending_mutex);
    logPrintf("STC31: запрошена FRC на %u ppm", static_cast<unsigned>(target_ppm));
}
