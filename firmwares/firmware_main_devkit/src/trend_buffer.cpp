#include "trend_buffer.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <math.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ao02.h"
#include "ble_sensors.h"
#include "config.h"
#include "dps310.h"
#include "logger.h"
#include "scd41.h"
#include "sfm3300.h"

namespace {

struct Sample {
    int16_t spo2;
    int16_t hr_wellue;
    int16_t hr_coospo;
    int16_t co2;
    int16_t o2;
    int16_t flow;
    int16_t pressure;
    int16_t work_o2;
    int16_t work_pressure;
    int16_t rr;
};

Sample *g_samples = nullptr;
size_t g_head = 0;
size_t g_count = 0;
uint32_t g_t0_ms = 0;
uint32_t g_last_sample_ms = 0;
uint32_t g_samples_since_log = 0;
uint32_t g_mask = 0;
Preferences g_prefs;

bool bitOn(uint32_t bit) {
    return (g_mask & bit) != 0;
}

int16_t clampI16Pos(int32_t v) {
    if (v < 0) {
        return TREND_MISSING;
    }
    if (v > 32767) {
        return 32767;
    }
    return static_cast<int16_t>(v);
}

int16_t scale10(float v) {
    const int32_t scaled = static_cast<int32_t>(lroundf(v * 10.0f));
    if (scaled <= static_cast<int32_t>(TREND_MISSING)) {
        return static_cast<int16_t>(TREND_MISSING + 1);
    }
    if (scaled > 32767) {
        return 32767;
    }
    return static_cast<int16_t>(scaled);
}

int16_t sampleAt(size_t logical, size_t field) {
    if (g_samples == nullptr || logical >= g_count) {
        return TREND_MISSING;
    }
    const size_t start = (g_head + TREND_CAPACITY - g_count) % TREND_CAPACITY;
    const Sample &s = g_samples[(start + logical) % TREND_CAPACITY];
    switch (field) {
        case 0:
            return s.spo2;
        case 1:
            return s.hr_wellue;
        case 2:
            return s.hr_coospo;
        case 3:
            return s.co2;
        case 4:
            return s.o2;
        case 5:
            return s.flow;
        case 6:
            return s.pressure;
        case 7:
            return s.rr;
        case 8:
            return s.work_o2;
        default:
            return s.work_pressure;
    }
}

struct ChunkBuf {
    char data[TREND_JSON_CHUNK];
    size_t len;
    TrendJsonSink sink;
    bool ok;
};

void chunkFlush(ChunkBuf *b) {
    if (b->len == 0 || !b->ok) {
        b->len = 0;
        return;
    }
    if (!b->sink(b->data, b->len)) {
        b->ok = false;
        logPrintf("Trends: JSON chunk send failed, heap=%u", ESP.getFreeHeap());
    }
    b->len = 0;
}

void chunkAppend(ChunkBuf *b, const char *s, size_t n) {
    if (!b->ok) {
        return;
    }
    while (n > 0 && b->ok) {
        const size_t room = sizeof(b->data) - b->len;
        const size_t take = n < room ? n : room;
        memcpy(b->data + b->len, s, take);
        b->len += take;
        s += take;
        n -= take;
        if (b->len == sizeof(b->data)) {
            chunkFlush(b);
        }
    }
}

void chunkAppendCstr(ChunkBuf *b, const char *s) {
    chunkAppend(b, s, strlen(s));
}

void chunkAppendU32(ChunkBuf *b, uint32_t v) {
    char tmp[12];
    const int n = snprintf(tmp, sizeof(tmp), "%lu", static_cast<unsigned long>(v));
    if (n > 0) {
        chunkAppend(b, tmp, static_cast<size_t>(n));
    }
}

void chunkAppendI16(ChunkBuf *b, int16_t v) {
    char tmp[8];
    const int n = snprintf(tmp, sizeof(tmp), "%d", static_cast<int>(v));
    if (n > 0) {
        chunkAppend(b, tmp, static_cast<size_t>(n));
    }
}

void chunkAppendBool(ChunkBuf *b, bool v) {
    chunkAppendCstr(b, v ? "true" : "false");
}

void writeArray(ChunkBuf *b, size_t field, size_t start, size_t n, uint32_t bit) {
    chunkAppendCstr(b, "[");
    if (bitOn(bit) && n > 0) {
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) {
                chunkAppendCstr(b, ",");
            }
            chunkAppendI16(b, sampleAt(start + i, field));
        }
    }
    chunkAppendCstr(b, "]");
}

void writeSources(ChunkBuf *b) {
    chunkAppendCstr(b, "\"sources\":{\"o2\":");
    chunkAppendBool(b, bitOn(TREND_SRC_O2));
    chunkAppendCstr(b, ",\"flow\":");
    chunkAppendBool(b, bitOn(TREND_SRC_FLOW));
    chunkAppendCstr(b, ",\"pressure\":");
    chunkAppendBool(b, bitOn(TREND_SRC_PRESSURE));
    chunkAppendCstr(b, ",\"co2\":");
    chunkAppendBool(b, bitOn(TREND_SRC_CO2));
    chunkAppendCstr(b, ",\"wellue\":");
    chunkAppendBool(b, bitOn(TREND_SRC_WELLUE));
    chunkAppendCstr(b, ",\"hr\":");
    chunkAppendBool(b, bitOn(TREND_SRC_HR));
    chunkAppendCstr(b, ",\"rr\":");
    chunkAppendBool(b, bitOn(TREND_SRC_RR));
    chunkAppendCstr(b, ",\"work_o2\":");
    chunkAppendBool(b, bitOn(TREND_SRC_WORK_O2));
    chunkAppendCstr(b, ",\"work_pressure\":");
    chunkAppendBool(b, bitOn(TREND_SRC_WORK_PRESSURE));
    chunkAppendCstr(b, "}");
}

void loadMask() {
    g_prefs.begin(TREND_NVS_NAMESPACE, true);
    g_mask = g_prefs.getUInt(TREND_NVS_KEY_MASK, 0);
    g_prefs.end();
}

void saveMask() {
    g_prefs.begin(TREND_NVS_NAMESPACE, false);
    g_prefs.putUInt(TREND_NVS_KEY_MASK, g_mask);
    g_prefs.end();
}

}  // namespace

void trendBufferInit() {
    g_head = 0;
    g_count = 0;
    g_t0_ms = 0;
    g_last_sample_ms = 0;
    g_samples_since_log = 0;
    loadMask();
    if (g_samples == nullptr) {
        const size_t bytes = sizeof(Sample) * TREND_CAPACITY;
        g_samples = static_cast<Sample *>(malloc(bytes));
        if (g_samples == nullptr) {
            logPrintf("Trends: malloc %u failed, heap=%u",
                      static_cast<unsigned>(bytes),
                      ESP.getFreeHeap());
        } else {
            memset(g_samples, 0, bytes);
            logPrintf("Trends: heap ring %u bytes, heap=%u",
                      static_cast<unsigned>(bytes),
                      ESP.getFreeHeap());
        }
    }
    const unsigned ring_min = static_cast<unsigned>(
        (static_cast<uint64_t>(TREND_CAPACITY) * TREND_INTERVAL_MS) / 60000ULL);
    logPrintf("Trends: RAM ring %u x 1s (~%u мин), mask=0x%lx, heap=%u",
              static_cast<unsigned>(TREND_CAPACITY),
              ring_min,
              static_cast<unsigned long>(g_mask),
              ESP.getFreeHeap());
}

uint32_t trendBufferGetMask() {
    return g_mask;
}

bool trendBufferApplySourcesJson(const char *json, size_t len) {
    if (json == nullptr || len == 0 || len >= TREND_SRC_JSON_MAX) {
        return false;
    }
    JsonDocument doc;
    if (deserializeJson(doc, json, len)) {
        return false;
    }
    uint32_t mask = 0;
    if (doc["o2"].is<bool>() && doc["o2"].as<bool>()) {
        mask |= TREND_SRC_O2;
    }
    if (doc["flow"].is<bool>() && doc["flow"].as<bool>()) {
        mask |= TREND_SRC_FLOW;
    }
    if (doc["pressure"].is<bool>() && doc["pressure"].as<bool>()) {
        mask |= TREND_SRC_PRESSURE;
    }
    if (doc["co2"].is<bool>() && doc["co2"].as<bool>()) {
        mask |= TREND_SRC_CO2;
    }
    if (doc["wellue"].is<bool>() && doc["wellue"].as<bool>()) {
        mask |= TREND_SRC_WELLUE;
    }
    if (doc["hr"].is<bool>() && doc["hr"].as<bool>()) {
        mask |= TREND_SRC_HR;
    }
    if (doc["rr"].is<bool>() && doc["rr"].as<bool>()) {
        mask |= TREND_SRC_RR;
    }
    if (doc["work_o2"].is<bool>() && doc["work_o2"].as<bool>()) {
        mask |= TREND_SRC_WORK_O2;
    }
    if (doc["work_pressure"].is<bool>() && doc["work_pressure"].as<bool>()) {
        mask |= TREND_SRC_WORK_PRESSURE;
    }
    g_mask = mask;
    saveMask();
    logPrintf("Trends: sources saved mask=0x%lx", static_cast<unsigned long>(g_mask));
    return true;
}

void trendBufferPoll() {
    if (g_samples == nullptr || g_mask == 0) {
        return;
    }
    const uint32_t now = millis();
    if (g_last_sample_ms != 0 && (now - g_last_sample_ms) < TREND_INTERVAL_MS) {
        return;
    }
    g_last_sample_ms = now;

    const WellueSnapshot wellue = bleSensorsGetWellue();
    const CoospoSnapshot coospo = bleSensorsGetCoospo();
    const Scd41Snapshot co2 = scd41GetSnapshot();
    const Ao02Snapshot o2 = ao02GetSnapshot();
    const Ao02Snapshot work_o2 = workingO2GetSnapshot();
    const FlowSnapshot flow = sfm3300GetSnapshot();
    const Dps310Snapshot pressure = dps310GetSnapshot();
    const Dps310Snapshot work_pressure = workingPressureGetSnapshot();

    Sample s;
    s.spo2 = TREND_MISSING;
    s.hr_wellue = TREND_MISSING;
    s.hr_coospo = TREND_MISSING;
    s.co2 = TREND_MISSING;
    s.o2 = TREND_MISSING;
    s.flow = TREND_MISSING;
    s.pressure = TREND_MISSING;
    s.work_o2 = TREND_MISSING;
    s.work_pressure = TREND_MISSING;
    s.rr = TREND_MISSING;

    if (bitOn(TREND_SRC_WELLUE) && wellue.ok && wellue.contact) {
        if (wellue.spo2 >= 50 && wellue.spo2 <= 100) {
            s.spo2 = clampI16Pos(wellue.spo2);
        }
        if (wellue.hr >= 25 && wellue.hr <= 240) {
            s.hr_wellue = clampI16Pos(wellue.hr);
        }
    }
    if (bitOn(TREND_SRC_HR) && coospo.ok && coospo.contact) {
        if (coospo.bpm >= 25 && coospo.bpm <= 240) {
            s.hr_coospo = clampI16Pos(coospo.bpm);
        }
    }
    if (bitOn(TREND_SRC_RR) && coospo.ok && coospo.contact) {
        if (coospo.rr_ms >= 250 && coospo.rr_ms <= 2000) {
            s.rr = clampI16Pos(coospo.rr_ms);
        }
    }
    if (bitOn(TREND_SRC_CO2) && co2.ok) {
        s.co2 = clampI16Pos(co2.co2_ppm);
    }
    if (bitOn(TREND_SRC_O2) && o2.ok) {
        s.o2 = scale10(o2.o2_percent);
    }
    if (bitOn(TREND_SRC_FLOW) && flow.ok) {
        s.flow = scale10(flow.ve_lpm);
    }
    if (bitOn(TREND_SRC_PRESSURE) && pressure.ok) {
        s.pressure = scale10(pressure.pressure_hpa);
    }
    if (bitOn(TREND_SRC_WORK_O2) && work_o2.ok) {
        s.work_o2 = scale10(work_o2.o2_percent);
    }
    if (bitOn(TREND_SRC_WORK_PRESSURE) && work_pressure.ok) {
        s.work_pressure = scale10(work_pressure.pressure_hpa);
    }

    if (g_count == 0) {
        g_t0_ms = now;
    } else if (g_count == TREND_CAPACITY) {
        g_t0_ms += TREND_INTERVAL_MS;
    }

    g_samples[g_head] = s;
    g_head = (g_head + 1) % TREND_CAPACITY;
    if (g_count < TREND_CAPACITY) {
        g_count += 1;
    }

    g_samples_since_log += 1;
    if (g_samples_since_log >= TREND_LOG_EVERY_SAMPLES) {
        g_samples_since_log = 0;
        logPrintf("Trends: count=%u/%u mask=0x%lx heap=%u",
                  static_cast<unsigned>(g_count),
                  static_cast<unsigned>(TREND_CAPACITY),
                  static_cast<unsigned long>(g_mask),
                  ESP.getFreeHeap());
    }
}

void trendBufferWriteJson(TrendJsonSink sink, size_t offset, size_t limit) {
    if (sink == nullptr) {
        return;
    }
    ChunkBuf b;
    b.len = 0;
    b.sink = sink;
    b.ok = true;

    const size_t total = (g_samples == nullptr || g_mask == 0) ? 0 : g_count;
    size_t start = offset;
    if (start > total) {
        start = total;
    }
    size_t n = total - start;
    if (n > limit) {
        n = limit;
    }

    chunkAppendCstr(&b, "{\"interval_ms\":");
    chunkAppendU32(&b, TREND_INTERVAL_MS);
    chunkAppendCstr(&b, ",\"now_ms\":");
    chunkAppendU32(&b, millis());
    chunkAppendCstr(&b, ",\"t0_ms\":");
    chunkAppendU32(&b, g_t0_ms);
    chunkAppendCstr(&b, ",\"total\":");
    chunkAppendU32(&b, static_cast<uint32_t>(total));
    chunkAppendCstr(&b, ",\"offset\":");
    chunkAppendU32(&b, static_cast<uint32_t>(start));
    chunkAppendCstr(&b, ",\"limit\":");
    chunkAppendU32(&b, static_cast<uint32_t>(limit));
    chunkAppendCstr(&b, ",\"count\":");
    chunkAppendU32(&b, static_cast<uint32_t>(n));
    chunkAppendCstr(&b, ",");
    writeSources(&b);
    chunkAppendCstr(&b, ",\"spo2\":");
    writeArray(&b, 0, start, n, TREND_SRC_WELLUE);
    chunkAppendCstr(&b, ",\"hr_wellue\":");
    writeArray(&b, 1, start, n, TREND_SRC_WELLUE);
    chunkAppendCstr(&b, ",\"hr_coospo\":");
    writeArray(&b, 2, start, n, TREND_SRC_HR);
    chunkAppendCstr(&b, ",\"co2\":");
    writeArray(&b, 3, start, n, TREND_SRC_CO2);
    chunkAppendCstr(&b, ",\"o2\":");
    writeArray(&b, 4, start, n, TREND_SRC_O2);
    chunkAppendCstr(&b, ",\"flow\":");
    writeArray(&b, 5, start, n, TREND_SRC_FLOW);
    chunkAppendCstr(&b, ",\"pressure\":");
    writeArray(&b, 6, start, n, TREND_SRC_PRESSURE);
    chunkAppendCstr(&b, ",\"rr\":");
    writeArray(&b, 7, start, n, TREND_SRC_RR);
    chunkAppendCstr(&b, ",\"work_o2\":");
    writeArray(&b, 8, start, n, TREND_SRC_WORK_O2);
    chunkAppendCstr(&b, ",\"work_pressure\":");
    writeArray(&b, 9, start, n, TREND_SRC_WORK_PRESSURE);
    chunkAppendCstr(&b, "}");
    chunkFlush(&b);
}
