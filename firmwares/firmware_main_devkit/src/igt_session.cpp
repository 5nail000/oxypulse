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
    Test,
    Done,
    Aborted,
    Error,
};

enum class TestAction : uint8_t {
    None = 0,
    HypoEvac,
    HyperEvac,
    WorkFlush,
    HypoFill,
    HypoRecirculate,
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
float g_pressure_fill_start_hpa = 0.0f;
bool g_fill_timeout_pending = false;
float g_o2_now = 0.0f;
float g_pressure_now = 0.0f;
float g_work_pressure_now = 0.0f;
bool g_sensor_warn = false;
bool g_waiting_buffer = false;
size_t g_phase_count = 0;
IgtPhaseMark g_phases[IGT_PHASE_MARK_MAX];
TestAction g_test_action = TestAction::None;
float g_last_phase_inhale_l = 0.0f;
float g_last_phase_exhale_l = 0.0f;
uint8_t g_last_phase_kind = 0;
uint8_t g_last_phase_cycle = 0;
bool g_last_phase_volume_ok = false;
bool g_phase_volume_tracking = false;

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
        case IgtState::Test:
            return "test";
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

const char *testActionName(TestAction action) {
    switch (action) {
        case TestAction::HypoEvac:
            return "hypo_evac";
        case TestAction::HyperEvac:
            return "hyper_evac";
        case TestAction::WorkFlush:
            return "work_flush";
        case TestAction::HypoFill:
            return "hypo_fill";
        case TestAction::HypoRecirculate:
            return "hypo_recirculate";
        case TestAction::None:
        default:
            return "";
    }
}

TestAction parseTestAction(const char *action) {
    if (action == nullptr) {
        return TestAction::None;
    }
    if (strcmp(action, "hypo_evac") == 0) {
        return TestAction::HypoEvac;
    }
    if (strcmp(action, "hyper_evac") == 0) {
        return TestAction::HyperEvac;
    }
    if (strcmp(action, "work_flush") == 0) {
        return TestAction::WorkFlush;
    }
    if (strcmp(action, "hypo_fill") == 0) {
        return TestAction::HypoFill;
    }
    if (strcmp(action, "hypo_recirculate") == 0) {
        return TestAction::HypoRecirculate;
    }
    return TestAction::None;
}

uint32_t testDurationMs(TestAction action) {
    switch (action) {
        case TestAction::HypoEvac:
            return IGT_PRESSURE_WAIT_MS;
        case TestAction::HypoRecirculate:
            return IGT_RECIRC_TIMEOUT_MS;
        case TestAction::HyperEvac:
        case TestAction::WorkFlush:
        case TestAction::HypoFill:
            return IGT_PRIMARY_FILL_MS;
        case TestAction::None:
        default:
            return 0;
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
    if (state == IgtState::Test) {
        return "test";
    }
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
    g_pressure_fill_start_hpa = 0.0f;
    g_fill_timeout_pending = false;
    g_sensor_warn = false;
    g_waiting_buffer = false;
    g_buffer_prep = BufferPrepPhase::Idle;
    g_test_action = TestAction::None;
    g_phase_count = 0;
    g_last_phase_inhale_l = 0.0f;
    g_last_phase_exhale_l = 0.0f;
    g_last_phase_kind = 0;
    g_last_phase_cycle = 0;
    g_last_phase_volume_ok = false;
    g_phase_volume_tracking = false;
}

void beginPhaseVolumeTracking(bool flow_ok) {
    g_phase_volume_tracking = flow_ok;
    if (flow_ok) {
        sfm3300ResetVolume();
    }
}

void commitPhaseVolume(PhaseKind kind, const FlowSnapshot &flow) {
    if (!g_phase_volume_tracking || !flow.ok) {
        return;
    }
    g_last_phase_inhale_l = flow.inhale_l;
    g_last_phase_exhale_l = flow.exhale_l;
    g_last_phase_kind = static_cast<uint8_t>(kind);
    g_last_phase_cycle = g_cycle;
    g_last_phase_volume_ok = true;
    logPrintf("IGT: phase %s cycle %u volumes inh=%.3f L exh=%.3f L",
              kind == PhaseKind::Hypoxia ? "hypoxia" : "normoxia",
              static_cast<unsigned>(g_last_phase_cycle),
              static_cast<double>(g_last_phase_inhale_l),
              static_cast<double>(g_last_phase_exhale_l));
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

void setV2(bool on) {
    if (on) {
        gpioSet(VALVE_IN_HYPO_PIN, false);
        gpioSet(VALVE_IN_WORK_PIN, false);
    }
    gpioSet(VALVE_IN_HYPER_PIN, on);
}

void setV3(bool on) {
    if (on) {
        gpioSet(VALVE_IN_HYPO_PIN, false);
        gpioSet(VALVE_IN_HYPER_PIN, false);
    }
    gpioSet(VALVE_IN_WORK_PIN, on);
}

void setV4(bool on) {
    if (on) {
        gpioSet(VALVE_OUT_DEP_HYPER_PIN, false);
    }
    gpioSet(VALVE_OUT_DEP_HYPO_PIN, on);
}

void setMotor(bool on) {
    if (on) {
        o2MotorEnsureOn();
    } else {
        o2MotorForceOff();
    }
}

void setServo(uint8_t angle) {
    servoSetAngle(SERVO1_PIN, angle);
}

void allActuatorsOff() {
    gpioSet(VALVE_IN_HYPO_PIN, false);
    gpioSet(VALVE_IN_HYPER_PIN, false);
    gpioSet(VALVE_IN_WORK_PIN, false);
    gpioSet(VALVE_OUT_DEP_HYPO_PIN, false);
    gpioSet(VALVE_OUT_DEP_HYPER_PIN, false);
    gpioSet(VALVE_OUT_RICH_HYPER_PIN, false);
    o2MotorForceOff();
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

float baselineDropHpa(const Dps310Snapshot &snap);
const char *bufferPrepBaselineEmergency(const Dps310Snapshot &pressure,
                                        const Dps310Snapshot &work_pressure);
bool primeHypoBaselineDropDone(const Dps310Snapshot &pressure);
bool primeWorkDeltaDone(const Dps310Snapshot &pressure, const Dps310Snapshot &work_pressure);
float pressureRiseFromFillStart(const Dps310Snapshot &pressure);
bool primaryFillRiseDone(const Dps310Snapshot &pressure);
bool primaryFillBaselineDone(const Dps310Snapshot &pressure);
const char *primaryFillDoneReason(const Dps310Snapshot &pressure);
void beginPrimaryFill(uint32_t now, float pressure_hpa);

void holdRecirculateActuators() {
    setV1(true);
    setV4(true);
    setMotor(true);
}

void applyTestActuators(TestAction action) {
    switch (action) {
        case TestAction::HypoEvac:
            setV1(true);
            setV4(false);
            setMotor(true);
            break;
        case TestAction::HyperEvac:
            setV2(true);
            setV4(false);
            setMotor(true);
            break;
        case TestAction::WorkFlush:
            setV3(true);
            setV4(false);
            setMotor(true);
            break;
        case TestAction::HypoFill:
            setV1(false);
            setV4(true);
            setMotor(true);
            break;
        case TestAction::HypoRecirculate:
            holdRecirculateActuators();
            break;
        case TestAction::None:
        default:
            allActuatorsOff();
            break;
    }
}

void finishTestSuccessLocked() {
    g_state = IgtState::Idle;
    g_test_action = TestAction::None;
    setErrorLocked("");
}

const char *testSessionTick(uint32_t now, const Ao02Snapshot &o2, const Dps310Snapshot &pressure,
                            const Dps310Snapshot &work_pressure, bool *done) {
    if (g_test_action == TestAction::None) {
        return "нет действия";
    }

    const uint32_t elapsed = now - g_phase_start_ms;
    const uint32_t duration_ms = testDurationMs(g_test_action);

    if (g_test_action == TestAction::HypoEvac) {
        const char *sensor_err = requiredBufferSensorsError(o2, pressure);
        if (sensor_err != nullptr) {
            return sensor_err;
        }
        if (primeHypoBaselineDropDone(pressure) || primeWorkDeltaDone(pressure, work_pressure)) {
            *done = true;
            return nullptr;
        }
        if (elapsed >= duration_ms) {
            return "выкачка не завершилась";
        }
        return bufferPrepBaselineEmergency(pressure, work_pressure);
    }

    if (g_test_action == TestAction::HypoFill) {
        const char *sensor_err = requiredBufferSensorsError(o2, pressure);
        if (sensor_err != nullptr) {
            return sensor_err;
        }
        if (g_pressure_fill_start_hpa <= 0.0f && pressure.ok) {
            g_pressure_fill_start_hpa = pressure.pressure_hpa;
        }
        if (g_fill_timeout_pending) {
            return nullptr;
        }
        const char *fill_reason = primaryFillDoneReason(pressure);
        if (fill_reason != nullptr) {
            logPrintf("IGT: test hypo_fill done (%s), rise=%.1f hPa",
                      fill_reason,
                      static_cast<double>(pressureRiseFromFillStart(pressure)));
            *done = true;
            return nullptr;
        }
        if (elapsed >= duration_ms) {
            g_fill_timeout_pending = true;
            setErrorLocked("наполнение: таймаут");
            return nullptr;
        }
        return bufferPrepBaselineEmergency(pressure, work_pressure);
    }

    if (g_test_action == TestAction::HypoRecirculate) {
        holdRecirculateActuators();
        const char *sensor_err = requiredBufferSensorsError(o2, pressure);
        if (sensor_err != nullptr) {
            return sensor_err;
        }
        const char *baseline_emergency = bufferPrepBaselineEmergency(pressure, work_pressure);
        if (baseline_emergency != nullptr) {
            return baseline_emergency;
        }
        if (o2.ok && o2.o2_percent <= g_o2_target) {
            *done = true;
            return nullptr;
        }
        if (elapsed >= duration_ms) {
            return "O2 не достиг цели";
        }
        return nullptr;
    }

    const char *baseline_emergency = bufferPrepBaselineEmergency(pressure, work_pressure);
    if (baseline_emergency != nullptr) {
        return baseline_emergency;
    }
    if (elapsed >= duration_ms) {
        *done = true;
    }
    return nullptr;
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
            holdRecirculateActuators();
            break;
        case BufferPrepPhase::Ready:
        case BufferPrepPhase::Idle:
        default:
            allActuatorsOff();
            break;
    }
}

void bufferPrepBegin(uint32_t now, float pressure_hpa) {
    g_buffer_prep = BufferPrepPhase::Prime;
    g_buffer_prep_start_ms = now;
    g_pressure_start_hpa = pressure_hpa;
    applyBufferPrepActuators(BufferPrepPhase::Prime);
}

float baselineDropHpa(const Dps310Snapshot &snap) {
    if (!snap.ok || !snap.baseline_ok) {
        return 0.0f;
    }
    return snap.baseline_hpa - snap.pressure_hpa;
}

const char *bufferPrepBaselineEmergency(const Dps310Snapshot &pressure,
                                        const Dps310Snapshot &work_pressure) {
    const float hypo_drop = baselineDropHpa(pressure);
    if (hypo_drop >= IGT_PRESSURE_BASELINE_LIMIT_HPA_F) {
        return "гипо: давление ниже baseline";
    }
    const float work_drop = baselineDropHpa(work_pressure);
    if (work_drop >= IGT_PRESSURE_BASELINE_LIMIT_HPA_F) {
        return "раб.: давление ниже baseline";
    }
    return nullptr;
}

bool primeHypoBaselineDropDone(const Dps310Snapshot &pressure) {
    return baselineDropHpa(pressure) >= IGT_PRESSURE_DELTA_HPA_F;
}

float pressureRiseFromFillStart(const Dps310Snapshot &pressure) {
    if (!pressure.ok) {
        return 0.0f;
    }
    return pressure.pressure_hpa - g_pressure_fill_start_hpa;
}

bool primaryFillRiseDone(const Dps310Snapshot &pressure) {
    return pressureRiseFromFillStart(pressure) >= IGT_PRESSURE_DELTA_HPA_F;
}

bool primaryFillBaselineDone(const Dps310Snapshot &pressure) {
    if (!pressure.ok || !pressure.baseline_ok) {
        return false;
    }
    const float drop = baselineDropHpa(pressure);
    return drop <= IGT_PRESSURE_DELTA_HPA_F ||
           drop <= (IGT_PRESSURE_BASELINE_LIMIT_HPA_F - IGT_PRESSURE_DELTA_HPA_F);
}

const char *primaryFillDoneReason(const Dps310Snapshot &pressure) {
    if (primaryFillRiseDone(pressure)) {
        return "pressure_rise";
    }
    if (primaryFillBaselineDone(pressure)) {
        return "baseline_recovered";
    }
    return nullptr;
}

void beginPrimaryFill(uint32_t now, float pressure_hpa) {
    g_buffer_prep = BufferPrepPhase::PrimaryFill;
    g_buffer_prep_start_ms = now;
    g_pressure_fill_start_hpa = pressure_hpa;
    g_fill_timeout_pending = false;
}

void bufferPrepBeginFill(uint32_t now, float pressure_hpa) {
    beginPrimaryFill(now, pressure_hpa);
    applyBufferPrepActuators(BufferPrepPhase::PrimaryFill);
}

bool primeWorkDeltaDone(const Dps310Snapshot &pressure, const Dps310Snapshot &work_pressure) {
    return pressure.ok && work_pressure.ok &&
           (work_pressure.pressure_hpa - pressure.pressure_hpa) >= IGT_WORK_PRESSURE_DELTA_HPA_F;
}

const char *bufferPrepTick(uint32_t now, const Ao02Snapshot &o2, const Dps310Snapshot &pressure,
                           const Dps310Snapshot &work_pressure, BufferPrepPhase *prev_phase,
                           bool parallel_normoxia) {
    (void)parallel_normoxia;
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
            return "выкачка не завершилась";
        }
        if (primeHypoBaselineDropDone(pressure) ||
            primeWorkDeltaDone(pressure, work_pressure)) {
            beginPrimaryFill(now, pressure.ok ? pressure.pressure_hpa : g_pressure_start_hpa);
        } else {
            const char *baseline_emergency =
                bufferPrepBaselineEmergency(pressure, work_pressure);
            if (baseline_emergency != nullptr) {
                return baseline_emergency;
            }
        }
    } else if (g_buffer_prep == BufferPrepPhase::PrimaryFill) {
        if (g_fill_timeout_pending) {
            return nullptr;
        }
        const char *baseline_emergency = bufferPrepBaselineEmergency(pressure, work_pressure);
        if (baseline_emergency != nullptr) {
            return baseline_emergency;
        }
        const char *fill_reason = primaryFillDoneReason(pressure);
        if (fill_reason != nullptr) {
            g_buffer_prep = BufferPrepPhase::Recirculate;
            g_buffer_prep_start_ms = now;
            logPrintf("IGT: primary fill done (%s), rise=%.1f hPa P0=%.1f now=%.1f",
                      fill_reason,
                      static_cast<double>(pressureRiseFromFillStart(pressure)),
                      static_cast<double>(g_pressure_fill_start_hpa),
                      static_cast<double>(pressure.pressure_hpa));
        } else if (elapsed >= IGT_PRIMARY_FILL_MS) {
            g_fill_timeout_pending = true;
            setErrorLocked("наполнение: таймаут");
        }
    } else if (g_buffer_prep == BufferPrepPhase::Recirculate) {
        holdRecirculateActuators();
        const char *baseline_emergency = bufferPrepBaselineEmergency(pressure, work_pressure);
        if (baseline_emergency != nullptr) {
            return baseline_emergency;
        }
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

void transitionToHypoxia(uint8_t cycle, uint32_t now, bool flow_ok) {
    g_cycle = cycle;
    g_state = IgtState::Hypoxia;
    g_phase_start_ms = now;
    g_waiting_buffer = false;
    g_buffer_prep = BufferPrepPhase::Idle;
    addPhaseMark(PhaseKind::Hypoxia, cycle, now);
    setServo(IGT_SERVO_HYPOXIA_ANGLE);
    beginPhaseVolumeTracking(flow_ok);
}

bool bufferPrepSkipToRecirculateLocked(uint32_t now) {
    if (g_state != IgtState::Normoxia) {
        return false;
    }
    if (g_cycle >= g_cycles) {
        return false;
    }
    if (g_buffer_prep != BufferPrepPhase::Prime &&
        g_buffer_prep != BufferPrepPhase::PrimaryFill) {
        return false;
    }
    const BufferPrepPhase prev = g_buffer_prep;
    g_buffer_prep = BufferPrepPhase::Recirculate;
    g_buffer_prep_start_ms = now;
    applyBufferPrepActuators(g_buffer_prep);
    logPrintf("IGT: buffer prep skip %s -> recirculate, O2 target %.1f",
              bufferPrepName(prev),
              static_cast<double>(g_o2_target));
    return true;
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
    logPrintf("IGT: session ready, delta=%.0f hPa limit=%.0f work_delta=%.0f fill=%u s",
              static_cast<double>(IGT_PRESSURE_DELTA_HPA_F),
              static_cast<double>(IGT_PRESSURE_BASELINE_LIMIT_HPA_F),
              static_cast<double>(IGT_WORK_PRESSURE_DELTA_HPA_F),
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
    if (pin != VALVE_IN_HYPO_PIN && pin != VALVE_IN_HYPER_PIN && pin != VALVE_IN_WORK_PIN &&
        pin != VALVE_OUT_DEP_HYPO_PIN && pin != VALVE_OUT_DEP_HYPER_PIN &&
        pin != VALVE_OUT_RICH_HYPER_PIN && pin != O2_MOTOR_PIN && pin != SERVO1_PIN) {
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
    o2MotorForceOff();
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
    const bool flow_ok = sfm3300GetSnapshot().ok;
    transitionToHypoxia(1, now, flow_ok);
    giveLock();
    allActuatorsOff();
    logPrintf("IGT: start practice, cycle 1/%u hypoxia", static_cast<unsigned>(g_cycles));
    return true;
}

bool igtSessionStartDirect(const char *name, float o2_target, uint32_t hypoxia_sec,
                           uint32_t normoxia_sec, uint8_t cycles) {
    if (!igtProtocolValid(o2_target, hypoxia_sec, normoxia_sec, cycles)) {
        logPrintf("IGT: direct start rejected, invalid protocol");
        return false;
    }
    if (!takeLock(50)) {
        return false;
    }
    if (!isTerminal(g_state)) {
        giveLock();
        logPrintf("IGT: direct start rejected, busy %s", stateName(g_state));
        return false;
    }
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
    const uint32_t now = millis();
    g_practice_started_ms = now;
    const bool flow_ok = sfm3300GetSnapshot().ok;
    transitionToHypoxia(1, now, flow_ok);
    char name_log[IGT_NAME_MAX];
    strncpy(name_log, g_name, IGT_NAME_MAX - 1);
    name_log[IGT_NAME_MAX - 1] = '\0';
    const uint8_t cycles_now = g_cycles;
    giveLock();
    allActuatorsOff();
    logPrintf("IGT: direct start '%s' skip prep, cycle 1/%u hypoxia",
              name_log, static_cast<unsigned>(cycles_now));
    return true;
}

bool igtSessionAbort(const char *reason) {
    if (!takeLock(50)) {
        return false;
    }
    const IgtState prev = g_state;
    const BufferPrepPhase prev_prep = g_buffer_prep;
    const uint8_t cycle = g_cycle;
    const uint8_t cycles = g_cycles;
    if (prev == IgtState::Idle) {
        giveLock();
        return true;
    }
    if (prev == IgtState::Done || prev == IgtState::Aborted || prev == IgtState::Error) {
        giveLock();
        return true;
    }
    if (prev == IgtState::Test) {
        const TestAction stopped = g_test_action;
        g_test_action = TestAction::None;
        g_state = IgtState::Idle;
        setErrorLocked("");
        giveLock();
        allActuatorsOff();
        logPrintf("IGT: test stop (%s)", testActionName(stopped));
        return true;
    }
    g_state = IgtState::Aborted;
    g_buffer_prep = BufferPrepPhase::Idle;
    g_waiting_buffer = false;
    char msg[IGT_ERROR_MAX];
    if (reason != nullptr && reason[0] != '\0') {
        snprintf(msg, sizeof(msg), "%s (%s %u/%u)", reason, publicStateName(prev, prev_prep),
                 static_cast<unsigned>(cycle), static_cast<unsigned>(cycles));
    } else {
        snprintf(msg, sizeof(msg), "прервано: %s %u/%u", publicStateName(prev, prev_prep),
                 static_cast<unsigned>(cycle), static_cast<unsigned>(cycles));
    }
    setErrorLocked(msg);
    giveLock();
    allActuatorsOff();
    setServo(IGT_SERVO_NORMOXIA_ANGLE);
    logPrintf("IGT: abort from %s: %s", stateName(prev), msg);
    return true;
}

bool igtSessionTestStart(const char *action, float o2_target) {
    const TestAction ta = parseTestAction(action);
    if (ta == TestAction::None) {
        logPrintf("IGT: test rejected, unknown action");
        return false;
    }
    if (!std::isfinite(o2_target) || o2_target < IGT_O2_TARGET_MIN || o2_target > IGT_O2_TARGET_MAX) {
        o2_target = 15.0f;
    }
    if (!takeLock(50)) {
        return false;
    }
    if (g_state != IgtState::Idle) {
        giveLock();
        logPrintf("IGT: test rejected, busy %s", stateName(g_state));
        return false;
    }
    clearSessionLocked();
    g_test_action = ta;
    g_o2_target = o2_target;
    strncpy(g_name, "test", IGT_NAME_MAX - 1);
    g_name[IGT_NAME_MAX - 1] = '\0';
    g_state = IgtState::Test;
    g_phase_start_ms = millis();
    applyTestActuators(ta);
    const TestAction started = g_test_action;
    giveLock();
    logPrintf("IGT: test start %s O2=%.1f", testActionName(started), static_cast<double>(o2_target));
    return true;
}

bool igtSessionTestStop() {
    return igtSessionAbort();
}

bool igtSessionBufferSkipToRecirculate() {
    if (!takeLock(50)) {
        return false;
    }
    const bool ok = bufferPrepSkipToRecirculateLocked(millis());
    if (!ok) {
        logPrintf("IGT: skip buffer prep rejected, state=%s prep=%s cycle=%u/%u",
                  stateName(g_state),
                  bufferPrepName(g_buffer_prep),
                  static_cast<unsigned>(g_cycle),
                  static_cast<unsigned>(g_cycles));
    }
    giveLock();
    return ok;
}

bool igtSessionBufferFillContinue() {
    if (!takeLock(50)) {
        return false;
    }
    if (!g_fill_timeout_pending) {
        giveLock();
        logPrintf("IGT: fill continue rejected, no timeout pending");
        return false;
    }
    const uint32_t now = millis();
    const IgtState prev_state = g_state;
    const TestAction prev_test = g_test_action;
    g_fill_timeout_pending = false;
    setErrorLocked("");
    if (g_state == IgtState::Test && g_test_action == TestAction::HypoFill) {
        g_test_action = TestAction::None;
        g_state = IgtState::Idle;
        giveLock();
        allActuatorsOff();
        logPrintf("IGT: test hypo_fill continue after timeout");
        return true;
    }
    if (g_buffer_prep != BufferPrepPhase::PrimaryFill) {
        giveLock();
        logPrintf("IGT: fill continue rejected, state=%s prep=%s",
                  stateName(prev_state),
                  bufferPrepName(g_buffer_prep));
        return false;
    }
    g_buffer_prep = BufferPrepPhase::Recirculate;
    g_buffer_prep_start_ms = now;
    applyBufferPrepActuators(g_buffer_prep);
    giveLock();
    logPrintf("IGT: primary fill continue after timeout -> recirculate (from %s)",
              stateName(prev_state));
    (void)prev_test;
    return true;
}

bool igtSessionBufferFillAbort() {
    if (!takeLock(50)) {
        return false;
    }
    if (!g_fill_timeout_pending) {
        giveLock();
        return false;
    }
    const IgtState prev = g_state;
    g_fill_timeout_pending = false;
    g_state = IgtState::Error;
    g_buffer_prep = BufferPrepPhase::Idle;
    g_waiting_buffer = false;
    g_test_action = TestAction::None;
    if (g_error[0] == '\0') {
        setErrorLocked("наполнение: таймаут (отменено)");
    }
    giveLock();
    allActuatorsOff();
    setServo(IGT_SERVO_NORMOXIA_ANGLE);
    logPrintf("IGT: fill timeout declined from %s", stateName(prev));
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
    allActuatorsOff();
    logPrintf("IGT: ack from %s -> idle", stateName(prev));
    return true;
}

void igtSessionPoll() {
    const Ao02Snapshot o2 = ao02GetSnapshot();
    const Dps310Snapshot pressure = dps310GetSnapshot();
    const Dps310Snapshot work_pressure = workingPressureGetSnapshot();
    const FlowSnapshot flow = sfm3300GetSnapshot();
    const uint32_t now = millis();
    const char *sensor_err = requiredBufferSensorsError(o2, pressure);

    if (!takeLock(20)) {
        return;
    }

    g_o2_now = o2.ok ? o2.o2_percent : 0.0f;
    g_pressure_now = pressure.ok ? pressure.pressure_hpa : 0.0f;
    g_work_pressure_now = work_pressure.ok ? work_pressure.pressure_hpa : 0.0f;
    const char *run_sensor_err = requiredBufferSensorsError(o2, pressure);
    g_sensor_warn = run_sensor_err != nullptr &&
                    (g_state == IgtState::Hypoxia || g_state == IgtState::Normoxia);

    const IgtState prev_state = g_state;
    const BufferPrepPhase prev_buffer = g_buffer_prep;
    const char *fail = nullptr;
    bool entered_initial_prime = false;
    bool entered_normoxia = false;
    bool entered_done = false;
    bool entered_next_hypoxia = false;
    bool entered_initial_ready = false;
    bool test_done = false;
    bool entered_fill_timeout = false;
    const TestAction test_action_before = g_test_action;
    const bool fill_timeout_before = g_fill_timeout_pending;
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
            fail = bufferPrepTick(now, o2, pressure, work_pressure, &step_prev, false);
            if (fail == nullptr && g_buffer_prep == BufferPrepPhase::Ready &&
                prev_buffer != BufferPrepPhase::Ready) {
                g_state = IgtState::Ready;
                entered_initial_ready = true;
            }
        }
    } else if (g_state == IgtState::Hypoxia) {
        if ((now - g_phase_start_ms) >= (g_hypoxia_sec * 1000U)) {
            commitPhaseVolume(PhaseKind::Hypoxia, flow);
            g_state = IgtState::Normoxia;
            g_phase_start_ms = now;
            g_waiting_buffer = false;
            addPhaseMark(PhaseKind::Normoxia, g_cycle, now);
            beginPhaseVolumeTracking(flow.ok);
            entered_normoxia = true;
            if (g_cycle < g_cycles) {
                bufferPrepBeginFill(now, pressure_for_prime);
            }
        }
    } else if (g_state == IgtState::Normoxia) {
        const bool normoxia_elapsed = (now - g_phase_start_ms) >= (g_normoxia_sec * 1000U);
        if (bufferPrepActive(g_buffer_prep)) {
            BufferPrepPhase step_prev = g_buffer_prep;
            fail = bufferPrepTick(now, o2, pressure, work_pressure, &step_prev, true);
        }
        if (fail == nullptr && normoxia_elapsed) {
            if (g_cycle < g_cycles) {
                if (g_buffer_prep == BufferPrepPhase::Ready) {
                    commitPhaseVolume(PhaseKind::Normoxia, flow);
                    next_cycle = static_cast<uint8_t>(g_cycle + 1);
                    transitionToHypoxia(next_cycle, now, flow.ok);
                    entered_next_hypoxia = true;
                } else {
                    g_waiting_buffer = true;
                }
            } else {
                commitPhaseVolume(PhaseKind::Normoxia, flow);
                g_state = IgtState::Done;
                g_phase_start_ms = now;
                g_waiting_buffer = false;
                g_buffer_prep = BufferPrepPhase::Idle;
                entered_done = true;
                setErrorLocked("протокол завершён");
            }
        } else if (fail == nullptr && g_waiting_buffer &&
                   g_buffer_prep == BufferPrepPhase::Ready) {
            commitPhaseVolume(PhaseKind::Normoxia, flow);
            next_cycle = static_cast<uint8_t>(g_cycle + 1);
            transitionToHypoxia(next_cycle, now, flow.ok);
            entered_next_hypoxia = true;
        }
    } else if (g_state == IgtState::Test) {
        fail = testSessionTick(now, o2, pressure, work_pressure, &test_done);
        if (fail == nullptr && test_done) {
            finishTestSuccessLocked();
        }
    }

    if (fail != nullptr && !g_fill_timeout_pending) {
        g_state = IgtState::Error;
        g_buffer_prep = BufferPrepPhase::Idle;
        g_waiting_buffer = false;
        g_test_action = TestAction::None;
        setErrorLocked(fail);
    }
    entered_fill_timeout = g_fill_timeout_pending && !fill_timeout_before;

    const BufferPrepPhase buffer_after = g_buffer_prep;
    const IgtState state_after = g_state;
    const bool buffer_changed = prev_buffer != buffer_after;

    giveLock();

    if (entered_fill_timeout) {
        allActuatorsOff();
        logPrintf("IGT: fill timeout pending, awaiting continue/abort");
    }
    if (test_done) {
        allActuatorsOff();
        logPrintf("IGT: test done %s", testActionName(test_action_before));
    }
    if (entered_initial_prime) {
        logPrintf("IGT: prime, V1+motor, P0=%.1f hPa", static_cast<double>(pressure_for_prime));
    }
    if (buffer_changed && prev_buffer == BufferPrepPhase::Prime &&
        buffer_after == BufferPrepPhase::PrimaryFill) {
        if (primeHypoBaselineDropDone(pressure)) {
            logPrintf("IGT: prime done, hypo baseline drop %.1f hPa (base=%.1f now=%.1f)",
                      static_cast<double>(baselineDropHpa(pressure)),
                      static_cast<double>(pressure.baseline_hpa),
                      static_cast<double>(pressure.pressure_hpa));
        } else if (primeWorkDeltaDone(pressure, work_pressure)) {
            logPrintf("IGT: prime done, work-hypo delta %.1f hPa (work=%.1f hypo=%.1f)",
                      static_cast<double>(work_pressure.pressure_hpa - pressure.pressure_hpa),
                      static_cast<double>(work_pressure.pressure_hpa),
                      static_cast<double>(pressure.pressure_hpa));
        }
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
        logPrintf("IGT: ready, O2=%.1f (servo at practice start)",
                  static_cast<double>(o2.o2_percent));
    }
    if (entered_normoxia) {
        setServo(IGT_SERVO_NORMOXIA_ANGLE);
        if (g_cycle < cycles_now) {
            logPrintf("IGT: cycle %u normoxia + buffer fill (no prime), servo=%u",
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
        allActuatorsOff();
        setServo(IGT_SERVO_NORMOXIA_ANGLE);
        logPrintf("IGT: done, %u cycles, servo=%u", static_cast<unsigned>(cycles_now),
                  static_cast<unsigned>(IGT_SERVO_NORMOXIA_ANGLE));
    }
    if (fail != nullptr) {
        allActuatorsOff();
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
    out->work_pressure_now = g_work_pressure_now;
    out->ready_to_start = g_state == IgtState::Ready;
    out->sensor_warn = g_sensor_warn;
    out->practice_started_esp_ms = g_practice_started_ms;
    out->buffer_prep = bufferPrepName(g_buffer_prep);
    out->buffer_prep_remain_sec = bufferPrepRemainSec(g_buffer_prep, now);
    out->waiting_buffer = g_waiting_buffer;
    out->fill_timeout_pending = g_fill_timeout_pending;
    out->last_phase_volume_ok = g_last_phase_volume_ok;
    out->last_phase_kind = g_last_phase_kind;
    out->last_phase_cycle = g_last_phase_cycle;
    out->last_phase_inhale_l = g_last_phase_inhale_l;
    out->last_phase_exhale_l = g_last_phase_exhale_l;
    out->test_action = testActionName(g_test_action);
    const uint32_t elapsed = (g_phase_start_ms == 0) ? 0 : (now - g_phase_start_ms);
    if (g_state == IgtState::CheckSensors && bufferPrepActive(g_buffer_prep)) {
        out->phase_remain_sec = bufferPrepRemainSec(g_buffer_prep, now);
        out->fill_remain_sec =
            g_buffer_prep == BufferPrepPhase::PrimaryFill ? out->phase_remain_sec : 0;
    } else if (g_state == IgtState::Hypoxia) {
        out->phase_remain_sec = remainSec(elapsed, g_hypoxia_sec * 1000U);
    } else if (g_state == IgtState::Normoxia) {
        out->phase_remain_sec = g_waiting_buffer ? 0 : remainSec(elapsed, g_normoxia_sec * 1000U);
    } else if (g_state == IgtState::Test) {
        out->phase_remain_sec = remainSec(elapsed, testDurationMs(g_test_action));
    } else {
        out->phase_remain_sec = 0;
    }
    out->phase_count = g_phase_count;
    for (size_t i = 0; i < g_phase_count; ++i) {
        out->phases[i] = g_phases[i];
    }
    giveLock();
}
