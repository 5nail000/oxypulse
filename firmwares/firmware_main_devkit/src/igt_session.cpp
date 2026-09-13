#include "igt_session.h"

#include <Arduino.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "ao02.h"
#include "config.h"
#include "dps310.h"
#include "gpio_controller.h"
#include "logger.h"
#include "servo_controller.h"
#include "sfm3300.h"

namespace {

enum class IgtState : uint8_t {
    Idle = 0,
    CheckSensors,
    Ready,
    Hypoxia,
    Normoxia,
    Done,
    Aborted,
    Error,
};

enum class BufferPrepPhase : uint8_t {
    Idle = 0,
    Prime,
    PrimaryFill,
    Recirculate,
    Ready,
};

enum class PhaseKind : uint8_t {
    Hypoxia = 0,
    Normoxia = 1,
};

SemaphoreHandle_t g_mutex = nullptr;
IgtState g_state = IgtState::Idle;
BufferPrepPhase g_buffer_prep = BufferPrepPhase::Idle;
char g_name[IGT_NAME_MAX] = "";
char g_error[IGT_ERROR_MAX] = "";
float g_o2_target = 15.0f;
uint32_t g_hypoxia_sec = 0;
uint32_t g_normoxia_sec = 0;
uint8_t g_cycles = 0;
uint8_t g_cycle = 0;
uint32_t g_phase_start_ms = 0;
uint32_t g_buffer_prep_start_ms = 0;
uint32_t g_practice_started_ms = 0;
float g_pressure_start_hpa = 0.0f;
float g_o2_now = 0.0f;
float g_pressure_now = 0.0f;
bool g_sensor_warn = false;
bool g_waiting_buffer = false;
size_t g_phase_count = 0;
IgtPhaseMark g_phases[IGT_PHASE_MARK_MAX];

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

const char *stateName(IgtState state) {
    switch (state) {
        case IgtState::CheckSensors:
            return "check_sensors";
        case IgtState::Ready:
            return "ready";
        case IgtState::Hypoxia:
            return "hypoxia";
        case IgtState::Normoxia:
            return "normoxia";
        case IgtState::Done:
            return "done";
        case IgtState::Aborted:
            return "aborted";
        case IgtState::Error:
            return "error";
        case IgtState::Idle:
        default:
            return "idle";
    }
}

const char *bufferPrepName(BufferPrepPhase phase) {
    switch (phase) {
        case BufferPrepPhase::Prime:
            return "prime";
        case BufferPrepPhase::PrimaryFill:
            return "primary_fill";
        case BufferPrepPhase::Recirculate:
            return "recirculate";
        case BufferPrepPhase::Ready:
            return "ready";
        case BufferPrepPhase::Idle:
        default:
            return "idle";
    }
}

const char *phaseName(IgtState state, BufferPrepPhase buffer_prep) {
    if (state == IgtState::Hypoxia) {
        return "hypoxia";
    }
    if (state == IgtState::Normoxia) {
        return "normoxia";
    }
    if (state == IgtState::CheckSensors || buffer_prep != BufferPrepPhase::Idle) {
        return "prep";
    }
    return "";
}

const char *publicStateName(IgtState state, BufferPrepPhase buffer_prep) {
    if (state == IgtState::CheckSensors && buffer_prep == BufferPrepPhase::Prime) {
        return "prime";
    }
    if (state == IgtState::CheckSensors && buffer_prep == BufferPrepPhase::PrimaryFill) {
        return "primary_fill";
    }
    if (state == IgtState::CheckSensors && buffer_prep == BufferPrepPhase::Recirculate) {
        return "recirculate";
    }
    if (state == IgtState::Ready || buffer_prep == BufferPrepPhase::Ready) {
        if (state == IgtState::Ready) {
            return "ready";
        }
    }
    return stateName(state);
}

bool isTerminal(IgtState state) {
    return state == IgtState::Idle || state == IgtState::Done || state == IgtState::Aborted ||
           state == IgtState::Error;
}

bool isInitialPrep(IgtState state) {
    return state == IgtState::CheckSensors || state == IgtState::Ready;
}

bool bufferPrepActive(BufferPrepPhase phase) {
    return phase == BufferPrepPhase::Prime || phase == BufferPrepPhase::PrimaryFill ||
           phase == BufferPrepPhase::Recirculate;
}

void copyName(char *dst, const char *src) {
    if (dst == nullptr) {
        return;
    }
    dst[0] = '\0';
    if (src == nullptr) {
        return;
    }
    size_t n = 0;
    for (size_t i = 0; src[i] != '\0' && n + 1 < IGT_NAME_MAX; ++i) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        if (c < 32 || c == '"' || c == '\\') {
            continue;
        }
        dst[n] = static_cast<char>(c);
        n += 1;
    }
    dst[n] = '\0';
}

void setErrorLocked(const char *msg) {
    g_error[0] = '\0';
    if (msg != nullptr) {
        strncpy(g_error, msg, IGT_ERROR_MAX - 1);
        g_error[IGT_ERROR_MAX - 1] = '\0';
    }
}

void clearSessionLocked() {
    g_name[0] = '\0';
    g_error[0] = '\0';
    g_o2_target = 0.0f;
    g_hypoxia_sec = 0;
    g_normoxia_sec = 0;
    g_cycles = 0;
    g_cycle = 0;
    g_phase_start_ms = 0;
    g_buffer_prep_start_ms = 0;
    g_practice_started_ms = 0;
    g_pressure_start_hpa = 0.0f;
    g_sensor_warn = false;
    g_waiting_buffer = false;
    g_buffer_prep = BufferPrepPhase::Idle;
    g_phase_count = 0;
}

void addPhaseMark(PhaseKind kind, uint8_t cycle, uint32_t now) {
    if (g_phase_count >= IGT_PHASE_MARK_MAX) {
        return;
    }
    IgtPhaseMark &mark = g_phases[g_phase_count];
    mark.kind = static_cast<uint8_t>(kind);
    mark.cycle = cycle;
    mark.start_esp_ms = now;
    g_phase_count += 1;
}

void setV1(bool on) {
    if (on) {
        gpioSet(VALVE_IN_HYPER_PIN, false);
        gpioSet(VALVE_IN_WORK_PIN, false);
    }
    gpioSet(VALVE_IN_HYPO_PIN, on);
}

void setV4(bool on) {
    if (on) {
        gpioSet(VALVE_OUT_DEP_HYPER_PIN, false);
    }
    gpioSet(VALVE_OUT_DEP_HYPO_PIN, on);
}

void setMotor(bool on) {
    gpioSet(O2_MOTOR_PIN, on);
}

void setServo(uint8_t angle) {
    servoSetAngle(SERVO1_PIN, angle);
}

void safeActuatorsOff() {
    setMotor(false);
    setV1(false);
    setV4(false);
}

const char *requiredSensorsError(const Ao02Snapshot &o2, const Dps310Snapshot &pressure,
                                 const FlowSnapshot &flow) {
    if (!o2.ok) {
        return "нет O2 гипоксии";
    }
    if (!pressure.ok) {
        return "нет давления гипоксии";
    }
    if (!flow.ok) {
        return "нет потока";
    }
    return nullptr;
}

const char *requiredBufferSensorsError(const Ao02Snapshot &o2, const Dps310Snapshot &pressure) {
    if (!o2.ok) {
        return "нет O2 гипоксии";
    }
    if (!pressure.ok) {
        return "нет давления гипоксии";
    }
    return nullptr;
}

uint32_t remainSec(uint32_t elapsed_ms, uint32_t duration_ms) {
    if (elapsed_ms >= duration_ms) {
        return 0;
    }
    return (duration_ms - elapsed_ms + 999U) / 1000U;
}

void applyBufferPrepActuators(BufferPrepPhase phase) {
    switch (phase) {
        case BufferPrepPhase::Prime:
            setV1(true);
            setV4(false);
            setMotor(true);
            break;
        case BufferPrepPhase::PrimaryFill:
            setV1(false);
            setV4(true);
            setMotor(true);
            break;
        case BufferPrepPhase::Recirculate:
            setV1(true);
            setV4(true);
            setMotor(true);
            break;
        case BufferPrepPhase::Ready:
        case BufferPrepPhase::Idle:
        default:
            safeActuatorsOff();
            break;
    }
}

void bufferPrepBegin(uint32_t now, float pressure_hpa) {
    g_buffer_prep = BufferPrepPhase::Prime;
    g_buffer_prep_start_ms = now;
    g_pressure_start_hpa = pressure_hpa;
    applyBufferPrepActuators(BufferPrepPhase::Prime);
}

const char *bufferPrepTick(uint32_t now, const Ao02Snapshot &o2, const Dps310Snapshot &pressure,
                           BufferPrepPhase *prev_phase, bool parallel_normoxia) {
    if (!bufferPrepActive(g_buffer_prep)) {
        return nullptr;
    }

    const char *sensor_err = requiredBufferSensorsError(o2, pressure);
    if (sensor_err != nullptr) {
        return sensor_err;
    }

    *prev_phase = g_buffer_prep;
    const uint32_t elapsed = now - g_buffer_prep_start_ms;

    if (g_buffer_prep == BufferPrepPhase::Prime) {
        if (elapsed >= IGT_PRESSURE_WAIT_MS) {
            return "давление не упало";
        }
        if (pressure.ok && (g_pressure_start_hpa - pressure.pressure_hpa) >= IGT_PRESSURE_DROP_HPA_F) {
            g_buffer_prep = BufferPrepPhase::PrimaryFill;
            g_buffer_prep_start_ms = now;
        }
    } else if (g_buffer_prep == BufferPrepPhase::PrimaryFill) {
        if (elapsed >= IGT_PRIMARY_FILL_MS) {
            g_buffer_prep = BufferPrepPhase::Recirculate;
            g_buffer_prep_start_ms = now;
        }
    } else if (g_buffer_prep == BufferPrepPhase::Recirculate) {
        if (elapsed >= IGT_RECIRC_TIMEOUT_MS) {
            return "O2 не достиг цели";
        }
        if (o2.ok && o2.o2_percent <= g_o2_target) {
            g_buffer_prep = BufferPrepPhase::Ready;
            g_buffer_prep_start_ms = now;
        }
    }

    if (*prev_phase != g_buffer_prep) {
        applyBufferPrepActuators(g_buffer_prep);
        if (g_buffer_prep == BufferPrepPhase::Ready && !parallel_normoxia) {
            setServo(IGT_SERVO_HYPOXIA_ANGLE);
        }
    } else if (g_buffer_prep == BufferPrepPhase::Ready && *prev_phase == BufferPrepPhase::Ready) {
        // no-op
    }

    return nullptr;
}

uint32_t bufferPrepRemainSec(BufferPrepPhase phase, uint32_t now) {
    if (!bufferPrepActive(phase)) {
        return 0;
    }
    const uint32_t elapsed = now - g_buffer_prep_start_ms;
    if (phase == BufferPrepPhase::Prime) {
        return remainSec(elapsed, IGT_PRESSURE_WAIT_MS);
    }
    if (phase == BufferPrepPhase::PrimaryFill) {
        return remainSec(elapsed, IGT_PRIMARY_FILL_MS);
    }
    if (phase == BufferPrepPhase::Recirculate) {
        return remainSec(elapsed, IGT_RECIRC_TIMEOUT_MS);
    }
    return 0;
}

void transitionToHypoxia(uint8_t cycle, uint32_t now) {
    g_cycle = cycle;
    g_state = IgtState::Hypoxia;
    g_phase_start_ms = now;
    g_waiting_buffer = false;
    g_buffer_prep = BufferPrepPhase::Idle;
    addPhaseMark(PhaseKind::Hypoxia, cycle, now);
    setServo(IGT_SERVO_HYPOXIA_ANGLE);
}

}  // namespace

bool igtProtocolValid(float o2_target, uint32_t hypoxia_sec, uint32_t normoxia_sec, uint8_t cycles) {
    if (!std::isfinite(o2_target) || o2_target < IGT_O2_TARGET_MIN ||
        o2_target > IGT_O2_TARGET_MAX) {
        return false;
    }
    if (hypoxia_sec < IGT_PHASE_SEC_MIN || hypoxia_sec > IGT_PHASE_SEC_MAX) {
        return false;
    }
    if (normoxia_sec < IGT_PHASE_SEC_MIN || normoxia_sec > IGT_PHASE_SEC_MAX) {
        return false;
    }
    if (cycles < IGT_CYCLES_MIN || cycles > IGT_CYCLES_MAX) {
        return false;
    }
    return true;
}

void igtSessionInit() {
    if (g_mutex != nullptr) {
        return;
    }
    g_mutex = xSemaphoreCreateMutex();
    if (g_mutex == nullptr) {
        logPrintf("IGT: mutex failed");
        return;
    }
    g_state = IgtState::Idle;
    clearSessionLocked();
    logPrintf("IGT: session ready, drop=%.0f hPa fill=%u s",
              static_cast<double>(IGT_PRESSURE_DROP_HPA_F),
              static_cast<unsigned>(IGT_PRIMARY_FILL_SEC));
}

bool igtSessionIsIdle() {
    if (!takeLock(20)) {
        return false;
    }
    const bool idle = g_state == IgtState::Idle;
    giveLock();
    return idle;
}

bool igtSessionBlocksManual(uint8_t pin) {
    if (pin != VALVE_IN_HYPO_PIN && pin != VALVE_OUT_DEP_HYPO_PIN && pin != O2_MOTOR_PIN &&
        pin != SERVO1_PIN) {
        return false;
    }
    if (!takeLock(20)) {
        return true;
    }
    const bool block = g_state != IgtState::Idle;
    giveLock();
    return block;
}

bool igtSessionPrep(const char *name, float o2_target, uint32_t hypoxia_sec, uint32_t normoxia_sec,
                    uint8_t cycles) {
    if (!igtProtocolValid(o2_target, hypoxia_sec, normoxia_sec, cycles)) {
        logPrintf("IGT: prep rejected, invalid protocol");
        return false;
    }
    if (!takeLock(50)) {
        return false;
    }
    if (!isTerminal(g_state)) {
        giveLock();
        logPrintf("IGT: prep rejected, busy %s", stateName(g_state));
        return false;
    }
    const IgtState prev = g_state;
    clearSessionLocked();
    copyName(g_name, name);
    if (g_name[0] == '\0') {
        strncpy(g_name, "IGT", IGT_NAME_MAX - 1);
        g_name[IGT_NAME_MAX - 1] = '\0';
    }
    g_o2_target = o2_target;
    g_hypoxia_sec = hypoxia_sec;
    g_normoxia_sec = normoxia_sec;
    g_cycles = cycles;
    g_state = IgtState::CheckSensors;
    g_phase_start_ms = millis();
    char name_log[IGT_NAME_MAX];
    strncpy(name_log, g_name, IGT_NAME_MAX - 1);
    name_log[IGT_NAME_MAX - 1] = '\0';
    giveLock();
    logPrintf("IGT: prep '%s' O2=%.1f hyp=%us nor=%us cycles=%u (from %s)", name_log,
              static_cast<double>(o2_target), static_cast<unsigned>(hypoxia_sec),
              static_cast<unsigned>(normoxia_sec), static_cast<unsigned>(cycles), stateName(prev));
    return true;
}

bool igtSessionStart() {
    if (!takeLock(50)) {
        return false;
    }
    if (g_state != IgtState::Ready) {
        giveLock();
        logPrintf("IGT: start rejected, state=%s", stateName(g_state));
        return false;
    }
    const uint32_t now = millis();
    g_practice_started_ms = now;
    transitionToHypoxia(1, now);
    giveLock();
    logPrintf("IGT: start practice, cycle 1/%u hypoxia", static_cast<unsigned>(g_cycles));
    return true;
}

bool igtSessionAbort() {
    if (!takeLock(50)) {
        return false;
    }
    const IgtState prev = g_state;
    if (prev == IgtState::Idle) {
        giveLock();
        return true;
    }
    if (prev == IgtState::Done || prev == IgtState::Aborted || prev == IgtState::Error) {
        giveLock();
        return true;
    }
    g_state = IgtState::Aborted;
    g_buffer_prep = BufferPrepPhase::Idle;
    g_waiting_buffer = false;
    setErrorLocked("");
    giveLock();
    safeActuatorsOff();
    setServo(IGT_SERVO_NORMOXIA_ANGLE);
    logPrintf("IGT: abort from %s", stateName(prev));
    return true;
}

bool igtSessionAck() {
    if (!takeLock(50)) {
        return false;
    }
    if (g_state != IgtState::Done && g_state != IgtState::Aborted && g_state != IgtState::Error) {
        giveLock();
        logPrintf("IGT: ack rejected, state=%s", stateName(g_state));
        return false;
    }
    const IgtState prev = g_state;
    g_state = IgtState::Idle;
    clearSessionLocked();
    giveLock();
    logPrintf("IGT: ack from %s -> idle", stateName(prev));
    return true;
}

void igtSessionPoll() {
    const Ao02Snapshot o2 = ao02GetSnapshot();
    const Dps310Snapshot pressure = dps310GetSnapshot();
    const FlowSnapshot flow = sfm3300GetSnapshot();
    const uint32_t now = millis();
    const char *sensor_err = requiredSensorsError(o2, pressure, flow);

    if (!takeLock(20)) {
        return;
    }

    g_o2_now = o2.ok ? o2.o2_percent : 0.0f;
    g_pressure_now = pressure.ok ? pressure.pressure_hpa : 0.0f;
    g_sensor_warn = sensor_err != nullptr &&
                    (g_state == IgtState::Hypoxia || g_state == IgtState::Normoxia);

    const IgtState prev_state = g_state;
    const BufferPrepPhase prev_buffer = g_buffer_prep;
    const char *fail = nullptr;
    bool entered_initial_prime = false;
    bool entered_normoxia = false;
    bool entered_done = false;
    bool entered_next_hypoxia = false;
    bool entered_initial_ready = false;
    uint8_t next_cycle = g_cycle;
    const float pressure_for_prime = pressure.ok ? pressure.pressure_hpa : 0.0f;
    const float target_now = g_o2_target;
    const uint8_t cycles_now = g_cycles;

    if (g_state == IgtState::CheckSensors) {
        if (sensor_err != nullptr) {
            fail = sensor_err;
        } else if (g_buffer_prep == BufferPrepPhase::Idle) {
            bufferPrepBegin(now, pressure_for_prime);
            g_state = IgtState::CheckSensors;
            entered_initial_prime = true;
        } else {
            BufferPrepPhase step_prev = g_buffer_prep;
            fail = bufferPrepTick(now, o2, pressure, &step_prev, false);
            if (fail == nullptr && g_buffer_prep == BufferPrepPhase::Ready &&
                prev_buffer != BufferPrepPhase::Ready) {
                g_state = IgtState::Ready;
                entered_initial_ready = true;
            }
        }
    } else if (g_state == IgtState::Hypoxia) {
        if ((now - g_phase_start_ms) >= (g_hypoxia_sec * 1000U)) {
            g_state = IgtState::Normoxia;
            g_phase_start_ms = now;
            g_waiting_buffer = false;
            addPhaseMark(PhaseKind::Normoxia, g_cycle, now);
            entered_normoxia = true;
            if (g_cycle < g_cycles) {
                bufferPrepBegin(now, pressure_for_prime);
            }
        }
    } else if (g_state == IgtState::Normoxia) {
        const bool normoxia_elapsed = (now - g_phase_start_ms) >= (g_normoxia_sec * 1000U);
        if (bufferPrepActive(g_buffer_prep)) {
            BufferPrepPhase step_prev = g_buffer_prep;
            fail = bufferPrepTick(now, o2, pressure, &step_prev, true);
        }
        if (fail == nullptr && normoxia_elapsed) {
            if (g_cycle < g_cycles) {
                if (g_buffer_prep == BufferPrepPhase::Ready) {
                    next_cycle = static_cast<uint8_t>(g_cycle + 1);
                    transitionToHypoxia(next_cycle, now);
                    entered_next_hypoxia = true;
                } else {
                    g_waiting_buffer = true;
                }
            } else {
                g_state = IgtState::Done;
                g_phase_start_ms = now;
                g_waiting_buffer = false;
                g_buffer_prep = BufferPrepPhase::Idle;
                entered_done = true;
            }
        } else if (fail == nullptr && g_waiting_buffer &&
                   g_buffer_prep == BufferPrepPhase::Ready) {
            next_cycle = static_cast<uint8_t>(g_cycle + 1);
            transitionToHypoxia(next_cycle, now);
            entered_next_hypoxia = true;
        }
    }

    if (fail != nullptr) {
        g_state = IgtState::Error;
        g_buffer_prep = BufferPrepPhase::Idle;
        g_waiting_buffer = false;
        setErrorLocked(fail);
    }

    const BufferPrepPhase buffer_after = g_buffer_prep;
    const IgtState state_after = g_state;
    const bool buffer_changed = prev_buffer != buffer_after;

    giveLock();

    if (entered_initial_prime) {
        logPrintf("IGT: prime, V1+motor, P0=%.1f hPa", static_cast<double>(pressure_for_prime));
    }
    if (buffer_changed && buffer_after == BufferPrepPhase::PrimaryFill && !entered_normoxia &&
        state_after == IgtState::CheckSensors) {
        logPrintf("IGT: primary fill %u s, V4+motor", static_cast<unsigned>(IGT_PRIMARY_FILL_SEC));
    }
    if (buffer_changed && buffer_after == BufferPrepPhase::Recirculate &&
        state_after == IgtState::CheckSensors) {
        logPrintf("IGT: recirculate until O2<=%.1f", static_cast<double>(target_now));
    }
    if (entered_initial_ready) {
        logPrintf("IGT: ready, servo=%u, O2=%.1f", static_cast<unsigned>(IGT_SERVO_HYPOXIA_ANGLE),
                  static_cast<double>(o2.o2_percent));
    }
    if (entered_normoxia) {
        setServo(IGT_SERVO_NORMOXIA_ANGLE);
        if (g_cycle < cycles_now) {
            logPrintf("IGT: cycle %u normoxia + buffer prep, servo=%u",
                      static_cast<unsigned>(g_cycle),
                      static_cast<unsigned>(IGT_SERVO_NORMOXIA_ANGLE));
        } else {
            logPrintf("IGT: cycle %u final normoxia, servo=%u", static_cast<unsigned>(g_cycle),
                      static_cast<unsigned>(IGT_SERVO_NORMOXIA_ANGLE));
        }
    }
    if (buffer_changed && buffer_after == BufferPrepPhase::PrimaryFill &&
        state_after == IgtState::Normoxia) {
        logPrintf("IGT: normoxia buffer primary fill %u s",
                  static_cast<unsigned>(IGT_PRIMARY_FILL_SEC));
    }
    if (buffer_changed && buffer_after == BufferPrepPhase::Recirculate &&
        state_after == IgtState::Normoxia) {
        logPrintf("IGT: normoxia buffer recirculate until O2<=%.1f",
                  static_cast<double>(target_now));
    }
    if (buffer_changed && buffer_after == BufferPrepPhase::Ready &&
        prev_state == IgtState::Normoxia) {
        logPrintf("IGT: normoxia buffer ready, O2=%.1f", static_cast<double>(o2.o2_percent));
    }
    if (entered_next_hypoxia) {
        logPrintf("IGT: cycle %u hypoxia, servo=%u", static_cast<unsigned>(next_cycle),
                  static_cast<unsigned>(IGT_SERVO_HYPOXIA_ANGLE));
    }
    if (entered_done) {
        safeActuatorsOff();
        setServo(IGT_SERVO_NORMOXIA_ANGLE);
        logPrintf("IGT: done, %u cycles, servo=%u", static_cast<unsigned>(cycles_now),
                  static_cast<unsigned>(IGT_SERVO_NORMOXIA_ANGLE));
    }
    if (fail != nullptr) {
        safeActuatorsOff();
        setServo(IGT_SERVO_NORMOXIA_ANGLE);
        logPrintf("IGT: error from %s: %s", stateName(prev_state), fail);
    }
}

void igtSessionGetStatus(IgtPublicStatus *out) {
    if (out == nullptr) {
        return;
    }
    *out = IgtPublicStatus{};
    if (!takeLock(50)) {
        out->state = "idle";
        return;
    }
    const uint32_t now = millis();
    out->state = publicStateName(g_state, g_buffer_prep);
    out->phase = phaseName(g_state, g_buffer_prep);
    strncpy(out->name, g_name, IGT_NAME_MAX - 1);
    out->name[IGT_NAME_MAX - 1] = '\0';
    strncpy(out->error, g_error, IGT_ERROR_MAX - 1);
    out->error[IGT_ERROR_MAX - 1] = '\0';
    out->cycle = g_cycle;
    out->cycles = g_cycles;
    out->hypoxia_sec = g_hypoxia_sec;
    out->normoxia_sec = g_normoxia_sec;
    out->o2_target = g_o2_target;
    out->pressure_start_hpa = g_pressure_start_hpa;
    out->o2_now = g_o2_now;
    out->pressure_now = g_pressure_now;
    out->ready_to_start = g_state == IgtState::Ready;
    out->sensor_warn = g_sensor_warn;
    out->practice_started_esp_ms = g_practice_started_ms;
    out->buffer_prep = bufferPrepName(g_buffer_prep);
    out->buffer_prep_remain_sec = bufferPrepRemainSec(g_buffer_prep, now);
    out->waiting_buffer = g_waiting_buffer;
    const uint32_t elapsed = (g_phase_start_ms == 0) ? 0 : (now - g_phase_start_ms);
    if (g_state == IgtState::CheckSensors && bufferPrepActive(g_buffer_prep)) {
        out->phase_remain_sec = bufferPrepRemainSec(g_buffer_prep, now);
        out->fill_remain_sec =
            g_buffer_prep == BufferPrepPhase::PrimaryFill ? out->phase_remain_sec : 0;
    } else if (g_state == IgtState::Hypoxia) {
        out->phase_remain_sec = remainSec(elapsed, g_hypoxia_sec * 1000U);
    } else if (g_state == IgtState::Normoxia) {
        out->phase_remain_sec = g_waiting_buffer ? 0 : remainSec(elapsed, g_normoxia_sec * 1000U);
    } else {
        out->phase_remain_sec = 0;
    }
    out->phase_count = g_phase_count;
    for (size_t i = 0; i < g_phase_count; ++i) {
        out->phases[i] = g_phases[i];
    }
    giveLock();
}
