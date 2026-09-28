#include "ocs3rl.h"

#include <cmath>
#include <cstring>
#include <cstdio>
#include <HardwareSerial.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config.h"
#include "logger.h"

namespace {

// Winpower OCS-3RL2.0, UART 9600 8N1. Руководство OCS_3RL User Manual:
// 16 09 MM | u16 O2 | u16 RH | u16 T | u16 P | CS, каждое слово /10.
//   MM=0x01 — PSA+воздух (заводской режим, шкала 21–95.6%)
//   MM=0x02 — чистый O2+воздух (команда 11 01 02 EC, шкала 0–100%)
// Потока (LPM) в этом кадре нет: второе слово — влажность.
constexpr size_t FRAME_SHORT_LEN = 12;
constexpr size_t FRAME_EXT_LEN = 15;
constexpr size_t RX_CAP = FRAME_EXT_LEN * 3;
constexpr uint8_t HDR0 = 0x16;
// 11 01 02 EC — режим «чистый O2 + воздух» и одновременно запрос кадра.
// После первого запроса автовыдача 2 Гц выключается, датчик отвечает только на команду.
constexpr uint8_t CMD_MODE_AIR[4] = {0x11, 0x01, 0x02, 0xEC};
constexpr uint32_t OCS_AIR_QUERY_MS = 500;

HardwareSerial OcsSerial(1);
SemaphoreHandle_t g_mutex = nullptr;
TaskHandle_t g_task = nullptr;
Ocs3rlSnapshot g_snap;
uint32_t g_last_ok_ms = 0;
uint32_t g_last_hex_ms = 0;
uint8_t g_rx[RX_CAP];
size_t g_rx_len = 0;
float g_range_o2 = -1.0f;
uint32_t g_range_o2_ms = 0;

bool looksLikeO2(float v);

void keepRangeOxygen(Ocs3rlSnapshot &snap) {
    // Автокадр 16 09 01 — шкала PSA/концентратора. 0–100% живёт в ответе 16 0C.
    if (snap.full_range_frame || g_range_o2 < 0.0f) {
        return;
    }
    if ((millis() - g_range_o2_ms) > 3000) {
        return;
    }
    snap.o2_percent = g_range_o2;
    snap.o2_field_index = 7;
    snap.ok = looksLikeO2(g_range_o2);
    snap.full_range_frame = true;
}

void setSnapshot(const Ocs3rlSnapshot &snap) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return;
    }
    g_snap = snap;
    xSemaphoreGive(g_mutex);
}

bool checksumOk(const uint8_t *frame, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum = static_cast<uint8_t>(sum + frame[i]);
    }
    return sum == 0;
}

uint16_t be16(const uint8_t *p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

float u16_div10(uint16_t v) {
    return static_cast<float>(v) / 10.0f;
}

void logHexThrottled(const char *tag, const uint8_t *frame, size_t len) {
    const uint32_t now = millis();
    if ((now - g_last_hex_ms) < 2000) {
        return;
    }
    g_last_hex_ms = now;
    char hex[64];
    size_t n = 0;
    for (size_t i = 0; i < len && n + 3 < sizeof(hex); ++i) {
        n += static_cast<size_t>(
            snprintf(hex + n, sizeof(hex) - n, "%02X%s", frame[i], (i + 1 < len) ? " " : ""));
    }
    logPrintf("OCS-3RL: %s [%s]", tag, hex);
}

bool looksLikeO2(float v) {
    return v >= 0.0f && v <= 100.0f;
}

bool looksLikeTempC(float v) {
    return v >= -10.0f && v <= 80.0f;
}

bool looksLikeAmbientO2(float v) {
    // Воздух ~20.9%; верх < комнатной T (~24–28), чтобы не принять TEMP за O₂.
    return v >= 15.0f && v <= 23.0f;
}

void fillStatusPressure(uint16_t st, Ocs3rlSnapshot &out) {
    out.status_raw = st;
    if (st >= 300 && st <= 1500) {
        out.pressure_hpa = static_cast<float>(st);
    } else if (st >= 3000 && st <= 15000) {
        out.pressure_hpa = static_cast<float>(st) / 10.0f;
    } else {
        out.pressure_hpa = 0.0f;
    }
}

void assignShortFields(float a, float b, float c, const uint8_t *frame, Ocs3rlSnapshot &out) {
    // OCS-3RL2.0: a = O₂ %, b = влажность %RH, c = температура °C. Все /10.
    out.field_a = a;
    out.field_b = b;
    out.field_c = c;
    out.o2_percent = a;
    out.o2_field_index = 0;
    out.rh_percent = b;
    out.temp_c = c;
    out.aux_raw = b;

    // OCS-3F100P: байты 9–10 — одно давление, (ST1<<8|ST2)/10 кПа.
    // 0x03 0xF3 = 1011 → 101.1 кПа, не «вход 3 / выход 243».
    const uint16_t p_raw = be16(frame + 9);
    out.p_in_kpa = frame[9];
    out.p_out_kpa = frame[10];
    out.status_raw = p_raw;
    out.pressure_hpa = static_cast<float>(p_raw);
    out.ok = looksLikeO2(a);
}

bool parseShortFrame(const uint8_t *frame, Ocs3rlSnapshot &out) {
    // 16 09 01 | u16_a | u16_b | u16_c | ST1 ST2 | CS
    if (frame[1] != 0x09 || (frame[2] != 0x01 && frame[2] != 0x02)) {
        return false;
    }
    if (!checksumOk(frame, FRAME_SHORT_LEN)) {
        return false;
    }
    const float a = u16_div10(be16(frame + 3));
    const float b = u16_div10(be16(frame + 5));
    const float c = u16_div10(be16(frame + 7));

    assignShortFields(a, b, c, frame, out);
    out.full_range_frame = false;

    const bool air_mode = frame[2] == 0x02;
    logPrintf("OCS-3RL: режим %s  O2=%.1f %%  RH=%.1f %%  T=%.1f C  P=%.1f kPa",
              air_mode ? "02 воздух" : "01 PSA",
              static_cast<double>(out.o2_percent),
              static_cast<double>(out.rh_percent),
              static_cast<double>(out.temp_c),
              static_cast<double>(out.pressure_hpa) / 10.0);
    return true;
}

bool parseExtFrame(const uint8_t *frame, Ocs3rlSnapshot &out) {
    // Gasboard/Cubic: 11 02 02 00 EB → 16 0C 02 00 DF1..DF10 CS
    // O₂ 0–100% = (DF7<<8|DF8)/10, T = (DF5<<8|DF6)/10. Это не переключение режима.
    if (frame[1] != 0x0C || frame[2] != 0x02) {
        return false;
    }
    if (!checksumOk(frame, FRAME_EXT_LEN)) {
        return false;
    }
    const float df12 = u16_div10(be16(frame + 4));
    const float df56 = u16_div10(be16(frame + 8));
    const float df78 = u16_div10(be16(frame + 10));
    const float df910 = u16_div10(be16(frame + 12));

    out.field_a = df12;
    out.field_b = df56;
    out.field_c = df78;
    out.o2_percent = df78;
    out.temp_c = df56;
    out.aux_raw = df910;
    out.rh_percent = 0.0f;
    out.pressure_hpa = 0.0f;
    out.status_raw = 0;
    out.o2_field_index = 7;
    out.full_range_frame = true;
    out.ok = looksLikeO2(df78);
    logPrintf("OCS-3RL: 0-100 O2=%.1f %%  T=%.1f C  DF1-2=%.1f DF9-10=%.1f",
              static_cast<double>(df78),
              static_cast<double>(df56),
              static_cast<double>(df12),
              static_cast<double>(df910));
    return true;
}

void consumeFrame(size_t len) {
    if (len >= g_rx_len) {
        g_rx_len = 0;
        return;
    }
    memmove(g_rx, g_rx + len, g_rx_len - len);
    g_rx_len -= len;
}

void tryParse() {
    while (g_rx_len >= FRAME_SHORT_LEN) {
        size_t start = g_rx_len;
        for (size_t i = 0; i < g_rx_len; ++i) {
            if (g_rx[i] == HDR0) {
                start = i;
                break;
            }
        }
        if (start >= g_rx_len) {
            g_rx_len = 0;
            return;
        }
        if (start > 0) {
            memmove(g_rx, g_rx + start, g_rx_len - start);
            g_rx_len -= start;
        }
        if (g_rx_len < FRAME_SHORT_LEN) {
            return;
        }

        Ocs3rlSnapshot snap;
        if (g_rx_len >= FRAME_EXT_LEN && g_rx[1] == 0x0C && parseExtFrame(g_rx, snap)) {
            g_last_ok_ms = millis();
            g_range_o2 = snap.o2_percent;
            g_range_o2_ms = g_last_ok_ms;
            const Ocs3rlSnapshot prev = g_snap;
            if (snap.pressure_hpa == 0.0f && prev.pressure_hpa > 0.0f) {
                snap.pressure_hpa = prev.pressure_hpa;
                snap.p_in_kpa = prev.p_in_kpa;
                snap.p_out_kpa = prev.p_out_kpa;
                snap.status_raw = prev.status_raw;
            }
            if (snap.rh_percent == 0.0f && prev.rh_percent > 0.0f) {
                snap.rh_percent = prev.rh_percent;
            }
            logHexThrottled("ext", g_rx, FRAME_EXT_LEN);
            setSnapshot(snap);
            consumeFrame(FRAME_EXT_LEN);
            continue;
        }
        if (parseShortFrame(g_rx, snap)) {
            g_last_ok_ms = millis();
            keepRangeOxygen(snap);
            logHexThrottled("short", g_rx, FRAME_SHORT_LEN);
            setSnapshot(snap);
            consumeFrame(FRAME_SHORT_LEN);
            continue;
        }
        consumeFrame(1);
    }
}

void feedByte(uint8_t b) {
    if (g_rx_len >= sizeof(g_rx)) {
        memmove(g_rx, g_rx + 1, sizeof(g_rx) - 1);
        g_rx_len = sizeof(g_rx) - 1;
    }
    g_rx[g_rx_len++] = b;
    tryParse();
}

void sendAirMode() {
    OcsSerial.write(CMD_MODE_AIR, sizeof(CMD_MODE_AIR));
}

void requestAirMode() {
    sendAirMode();
    vTaskDelay(pdMS_TO_TICKS(80));
    while (OcsSerial.available() > 0) {
        feedByte(static_cast<uint8_t>(OcsSerial.read()));
    }
    logPrintf("OCS-3RL: команда 11 01 02 EC — режим воздух, дальше опрос каждые %u мс",
              static_cast<unsigned>(OCS_AIR_QUERY_MS));
}

void ocsTask(void *param) {
    (void)param;
    uint32_t last_log_ms = 0;
    uint32_t last_query_ms = 0;

    requestAirMode();

    for (;;) {
        while (OcsSerial.available() > 0) {
            feedByte(static_cast<uint8_t>(OcsSerial.read()));
        }

        const uint32_t now = millis();
        // Режим защёлкивается одной командой, но запрос гасит автокадр: без повтора данные пропадают.
        if ((now - last_query_ms) >= OCS_AIR_QUERY_MS) {
            last_query_ms = now;
            sendAirMode();
        }

        if (g_last_ok_ms != 0 && (now - g_last_ok_ms) > OCS_STALE_MS) {
            setSnapshot(Ocs3rlSnapshot{});
            g_last_ok_ms = 0;
            logPrintf("OCS-3RL: нет данных >%u ms", static_cast<unsigned>(OCS_STALE_MS));
        }

        if ((now - last_log_ms) >= 2000) {
            last_log_ms = now;
            const Ocs3rlSnapshot snap = ocs3rlGetSnapshot();
            if (snap.field_a != 0.0f || snap.field_b != 0.0f || snap.field_c != 0.0f || snap.ok) {
                logPrintf(
                    "OCS-3RL: O2=%.1f %% (field %d)  T=%.1f C  RH=%.1f %%  P=%.0f  a=%.1f b=%.1f c=%.1f  %s",
                    static_cast<double>(snap.o2_percent),
                    snap.o2_field_index,
                    static_cast<double>(snap.temp_c),
                    static_cast<double>(snap.rh_percent),
                    static_cast<double>(snap.pressure_hpa),
                    static_cast<double>(snap.field_a),
                    static_cast<double>(snap.field_b),
                    static_cast<double>(snap.field_c),
                    snap.full_range_frame ? "ext" : "short");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(OCS_POLL_INTERVAL_MS));
    }
}

}  // namespace

void ocs3rlInit() {
    g_mutex = xSemaphoreCreateMutex();
    OcsSerial.begin(OCS_UART_BAUD, SERIAL_8N1, OCS_UART_RX_PIN, OCS_UART_TX_PIN);
    OcsSerial.setTimeout(20);
    logPrintf("OCS-3RL: UART %lu baud RX=%d TX=%d (O2@bytes3-4, без потока)",
              static_cast<unsigned long>(OCS_UART_BAUD),
              OCS_UART_RX_PIN,
              OCS_UART_TX_PIN);
    xTaskCreatePinnedToCore(ocsTask, "ocs3rlTask", 3072, nullptr, 1, &g_task, 1);
}

Ocs3rlSnapshot ocs3rlGetSnapshot() {
    Ocs3rlSnapshot snap{};
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, portMAX_DELAY) != pdTRUE) {
        return snap;
    }
    snap = g_snap;
    xSemaphoreGive(g_mutex);
    return snap;
}

float ocs3rlO2Co2Compensated(float o2_raw_percent, float co2_vol_percent, bool co2_ok) {
    if (!co2_ok) {
        return o2_raw_percent;
    }
    float excess = co2_vol_percent - OCS_O2_CO2_AMBIENT_VOL;
    if (excess < 0.0f) {
        excess = 0.0f;
    }
    float out = o2_raw_percent - excess * OCS_O2_CO2_PER_VOL;
    if (out < 0.0f) {
        out = 0.0f;
    }
    if (out > 100.0f) {
        out = 100.0f;
    }
    return out;
}

void workingO2Init() {
    ocs3rlInit();
}

Ao02Snapshot workingO2GetSnapshot() {
    const Ocs3rlSnapshot ocs = ocs3rlGetSnapshot();
    Ao02Snapshot snap{};
    snap.ok = ocs.ok;
    snap.o2_percent = ocs.o2_percent;
    snap.voltage_mv = 0.0f;
    return snap;
}
