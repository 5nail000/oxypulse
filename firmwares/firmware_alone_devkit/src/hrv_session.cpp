#include "hrv_session.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config.h"
#include "logger.h"

namespace {

enum class HrvState : uint8_t {
    Idle = 0,
    Recording,
    Done,
    Error,
};

SemaphoreHandle_t g_mutex = nullptr;
HrvState g_state = HrvState::Idle;
uint16_t g_rr[HRV_RR_MAX];
size_t g_count = 0;
uint32_t g_duration_sec = 0;
uint32_t g_start_ms = 0;
uint32_t g_elapsed_ms = 0;
char g_error[HRV_ERROR_MAX] = "";
bool g_full_logged = false;
uint32_t g_disconnect_ms = 0;
bool g_grace_logged = false;

bool takeLock(uint32_t timeout_ms) {
    if (g_mutex == nullptr) {
        return false;
    }
    return xSemaphoreTake(g_mutex, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void giveLock() {
    if (g_mutex != nullptr) {
        xSemaphoreGive(g_mutex);
    }
}

const char *stateName(HrvState state) {
    switch (state) {
        case HrvState::Recording:
            return "recording";
        case HrvState::Done:
            return "done";
        case HrvState::Error:
            return "error";
        case HrvState::Idle:
        default:
            return "idle";
    }
}

void setErrorLocked(const char *msg) {
    g_state = HrvState::Error;
    g_error[0] = '\0';
    if (msg != nullptr) {
        strncpy(g_error, msg, HRV_ERROR_MAX - 1);
        g_error[HRV_ERROR_MAX - 1] = '\0';
    }
}

bool appendStr(char *buffer, size_t capacity, size_t *len, const char *s) {
    if (buffer == nullptr || len == nullptr || s == nullptr) {
        return false;
    }
    const size_t n = strlen(s);
    if (*len + n >= capacity) {
        return false;
    }
    memcpy(buffer + *len, s, n);
    *len += n;
    buffer[*len] = '\0';
    return true;
}

bool appendUint(char *buffer, size_t capacity, size_t *len, uint32_t v) {
    char tmp[12];
    snprintf(tmp, sizeof(tmp), "%lu", static_cast<unsigned long>(v));
    return appendStr(buffer, capacity, len, tmp);
}

}  // namespace

bool hrvDurationAllowed(uint32_t duration_sec) {
    return duration_sec == 30U || duration_sec == 120U || duration_sec == 180U ||
           duration_sec == 300U;
}

void hrvSessionInit() {
    if (g_mutex != nullptr) {
        return;
    }
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == nullptr) {
        logPrintf("HRV: mutex failed");
        return;
    }
    g_state = HrvState::Idle;
    g_count = 0;
    g_duration_sec = 0;
    g_elapsed_ms = 0;
    g_error[0] = '\0';
    g_disconnect_ms = 0;
    g_grace_logged = false;
    logPrintf("HRV: session ready, rr_max=%u", static_cast<unsigned>(HRV_RR_MAX));
}

bool hrvSessionStart(uint32_t duration_sec) {
    if (!hrvDurationAllowed(duration_sec)) {
        logPrintf("HRV: start rejected, duration=%u", static_cast<unsigned>(duration_sec));
        return false;
    }
    if (!takeLock(50)) {
        return false;
    }
    const HrvState prev = g_state;
    const size_t prev_count = g_count;
    g_count = 0;
    g_duration_sec = duration_sec;
    g_start_ms = millis();
    g_elapsed_ms = 0;
    g_error[0] = '\0';
    g_full_logged = false;
    g_disconnect_ms = 0;
    g_grace_logged = false;
    g_state = HrvState::Recording;
    giveLock();
    if (prev != HrvState::Idle) {
        logPrintf("HRV: start %u s, replaced %s rr=%u", static_cast<unsigned>(duration_sec),
                  stateName(prev), static_cast<unsigned>(prev_count));
    } else {
        logPrintf("HRV: start %u s", static_cast<unsigned>(duration_sec));
    }
    return true;
}

void hrvSessionStop() {
    if (!takeLock(50)) {
        return;
    }
    const HrvState prev = g_state;
    const size_t count = g_count;
    g_state = HrvState::Idle;
    g_count = 0;
    g_duration_sec = 0;
    g_elapsed_ms = 0;
    g_error[0] = '\0';
    g_full_logged = false;
    g_disconnect_ms = 0;
    g_grace_logged = false;
    giveLock();
    if (prev != HrvState::Idle) {
        logPrintf("HRV: stop from %s, rr=%u", stateName(prev), static_cast<unsigned>(count));
    }
}

void hrvSessionFeedRr(uint16_t rr_ms) {
    if (rr_ms < HRV_RR_MIN_MS || rr_ms > HRV_RR_MAX_MS) {
        return;
    }
    if (!takeLock(5)) {
        return;
    }
    if (g_state != HrvState::Recording) {
        giveLock();
        return;
    }
    if (g_count >= HRV_RR_MAX) {
        const bool log_full = !g_full_logged;
        g_full_logged = true;
        giveLock();
        if (log_full) {
            logPrintf("HRV: rr buffer full (%u)", static_cast<unsigned>(HRV_RR_MAX));
        }
        return;
    }
    g_rr[g_count] = rr_ms;
    g_count += 1;
    giveLock();
}

void hrvSessionPoll(bool belt_connected) {
    if (!takeLock(20)) {
        return;
    }
    if (g_state != HrvState::Recording) {
        giveLock();
        return;
    }
    const uint32_t now = millis();
    g_elapsed_ms = now - g_start_ms;
    const uint32_t duration_ms = g_duration_sec * 1000U;
    bool log_lost = false;
    bool log_back = false;
    bool log_error = false;
    uint32_t gone_ms = 0;
    const size_t count = g_count;
    const uint32_t duration_sec = g_duration_sec;

    if (belt_connected) {
        if (g_disconnect_ms != 0) {
            gone_ms = now - g_disconnect_ms;
            g_disconnect_ms = 0;
            g_grace_logged = false;
            log_back = true;
        }
    } else {
        if (g_disconnect_ms == 0) {
            g_disconnect_ms = now;
        }
        gone_ms = now - g_disconnect_ms;
        if (!g_grace_logged) {
            g_grace_logged = true;
            log_lost = true;
        }
        if (gone_ms >= HRV_BLE_GRACE_MS && g_elapsed_ms < duration_ms) {
            setErrorLocked("ремень отключён");
            log_error = true;
        }
    }

    bool log_done = false;
    if (!log_error && g_elapsed_ms >= duration_ms) {
        g_elapsed_ms = duration_ms;
        g_state = HrvState::Done;
        log_done = true;
    }
    giveLock();

    if (log_lost) {
        logPrintf("HRV: belt lost, grace %u ms, rr=%u",
                  static_cast<unsigned>(HRV_BLE_GRACE_MS), static_cast<unsigned>(count));
    }
    if (log_back) {
        logPrintf("HRV: belt back after %u ms", static_cast<unsigned>(gone_ms));
    }
    if (log_error) {
        logPrintf("HRV: error, belt gone %u ms, rr=%u", static_cast<unsigned>(gone_ms),
                  static_cast<unsigned>(count));
    }
    if (log_done) {
        logPrintf("HRV: done %u s, rr=%u", static_cast<unsigned>(duration_sec),
                  static_cast<unsigned>(count));
    }
}

bool hrvSessionStreamJson(HrvJsonSink sink) {
    if (sink == nullptr) {
        return false;
    }

    HrvState state = HrvState::Idle;
    uint32_t duration_sec = 0;
    uint32_t elapsed_ms = 0;
    size_t count = 0;
    char error[HRV_ERROR_MAX];
    error[0] = '\0';

    if (!takeLock(50)) {
        return false;
    }
    state = g_state;
    duration_sec = g_duration_sec;
    elapsed_ms = g_elapsed_ms;
    if (state == HrvState::Recording) {
        elapsed_ms = millis() - g_start_ms;
        if (elapsed_ms > duration_sec * 1000U) {
            elapsed_ms = duration_sec * 1000U;
        }
    }
    count = g_count;
    strncpy(error, g_error, HRV_ERROR_MAX - 1);
    error[HRV_ERROR_MAX - 1] = '\0';
    giveLock();

    char chunk[HRV_JSON_CHUNK];
    size_t len = 0;
    chunk[0] = '\0';
    if (!appendStr(chunk, sizeof(chunk), &len, "{\"state\":\"")) {
        return false;
    }
    if (!appendStr(chunk, sizeof(chunk), &len, stateName(state))) {
        return false;
    }
    if (!appendStr(chunk, sizeof(chunk), &len, "\",\"duration_sec\":")) {
        return false;
    }
    if (!appendUint(chunk, sizeof(chunk), &len, duration_sec)) {
        return false;
    }
    if (!appendStr(chunk, sizeof(chunk), &len, ",\"elapsed_ms\":")) {
        return false;
    }
    if (!appendUint(chunk, sizeof(chunk), &len, elapsed_ms)) {
        return false;
    }
    if (!appendStr(chunk, sizeof(chunk), &len, ",\"rr_count\":")) {
        return false;
    }
    if (!appendUint(chunk, sizeof(chunk), &len, static_cast<uint32_t>(count))) {
        return false;
    }
    if (state == HrvState::Error && error[0] != '\0') {
        if (!appendStr(chunk, sizeof(chunk), &len, ",\"error\":\"")) {
            return false;
        }
        if (!appendStr(chunk, sizeof(chunk), &len, error)) {
            return false;
        }
        if (!appendStr(chunk, sizeof(chunk), &len, "\"")) {
            return false;
        }
    }
    if (state != HrvState::Done || count == 0) {
        if (!appendStr(chunk, sizeof(chunk), &len, "}")) {
            return false;
        }
        return sink(chunk, len);
    }
    if (!appendStr(chunk, sizeof(chunk), &len, ",\"rr\":[")) {
        return false;
    }
    if (!sink(chunk, len)) {
        return false;
    }

    for (size_t i = 0; i < count; ) {
        len = 0;
        chunk[0] = '\0';
        const size_t batch_start = i;
        while (i < count) {
            if (i > batch_start && !appendStr(chunk, sizeof(chunk), &len, ",")) {
                break;
            }
            if (!takeLock(20)) {
                return false;
            }
            const uint16_t rr = g_rr[i];
            giveLock();
            if (!appendUint(chunk, sizeof(chunk), &len, rr)) {
                return false;
            }
            i += 1;
            if (len >= HRV_JSON_CHUNK - 24) {
                break;
            }
        }
        if (len == 0 || !sink(chunk, len)) {
            return false;
        }
    }

    len = 0;
    chunk[0] = '\0';
    if (!appendStr(chunk, sizeof(chunk), &len, "]}")) {
        return false;
    }
    return sink(chunk, len);
}
