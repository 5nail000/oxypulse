#include "command_handler.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "ao02.h"
#include "config.h"
#include "dps310.h"
#include "gpio_controller.h"
#include "igt_session.h"
#include "logger.h"
#include "ocs3rl.h"
#include "phys_uplink.h"
#include "servo_controller.h"
#include "sfm3300.h"
#include "stc31.h"

namespace {

bool handleGpioSet(JsonVariantConst doc) {
    if (!doc["pin"].is<uint8_t>() && !doc["pin"].is<int>()) {
        return false;
    }
    const uint8_t pin = doc["pin"].as<uint8_t>();
    if (!doc["state"].is<int>() && !doc["state"].is<bool>()) {
        return false;
    }
    const bool state = doc["state"].as<int>() != 0;
    if (igtSessionBlocksManual(pin)) {
        logPrintf("GPIO: pin %u занят ИГТ", pin);
        return false;
    }
    return gpioSet(pin, state);
}

bool handleGpioTap(JsonVariantConst doc) {
    if (!doc["pin"].is<uint8_t>() && !doc["pin"].is<int>()) {
        return false;
    }
    const uint8_t pin = doc["pin"].as<uint8_t>();
    if (igtSessionBlocksManual(pin)) {
        logPrintf("GPIO: tap pin %u занят ИГТ", pin);
        return false;
    }
    uint32_t pulse_ms = GPIO_TAP_MS;
    if (doc["pulse_ms"].is<uint32_t>() || doc["pulse_ms"].is<int>()) {
        pulse_ms = doc["pulse_ms"].as<uint32_t>();
    }
    return gpioTap(pin, pulse_ms);
}

bool handleServoSet(JsonVariantConst doc) {
    if (!doc["pin"].is<uint8_t>() && !doc["pin"].is<int>()) {
        return false;
    }
    if (!doc["angle"].is<uint8_t>() && !doc["angle"].is<int>()) {
        return false;
    }
    const uint8_t pin = doc["pin"].as<uint8_t>();
    const uint8_t angle = doc["angle"].as<uint8_t>();
    if (igtSessionBlocksManual(pin)) {
        logPrintf("servo: pin %u занят ИГТ", pin);
        return false;
    }
    return servoSetAngle(pin, angle);
}

bool handleServoAuto(JsonVariantConst doc) {
    if (!doc["pin"].is<uint8_t>() && !doc["pin"].is<int>()) {
        return false;
    }
    const uint8_t pin = doc["pin"].as<uint8_t>();
    if (igtSessionBlocksManual(pin)) {
        logPrintf("servo auto: pin %u занят ИГТ", pin);
        return false;
    }
    const bool enabled = doc["enabled"].is<bool>() ? doc["enabled"].as<bool>()
                                                     : doc["enabled"].as<int>() != 0;
    uint32_t pause_ms = SERVO_AUTO_PAUSE_MS;
    if (doc["pause_ms"].is<uint32_t>() || doc["pause_ms"].is<int>()) {
        pause_ms = doc["pause_ms"].as<uint32_t>();
    }
    return servoSetAuto(pin, enabled, pause_ms);
}

bool readProtocol(JsonVariantConst doc, const char **name, float *o2_target, uint32_t *hypoxia_sec,
                  uint32_t *normoxia_sec, uint8_t *cycles) {
    if (!doc["o2_target"].is<float>() && !doc["o2_target"].is<int>() &&
        !doc["o2_target"].is<double>()) {
        return false;
    }
    if (!doc["hypoxia_sec"].is<uint32_t>() && !doc["hypoxia_sec"].is<int>()) {
        return false;
    }
    if (!doc["normoxia_sec"].is<uint32_t>() && !doc["normoxia_sec"].is<int>()) {
        return false;
    }
    if (!doc["cycles"].is<uint8_t>() && !doc["cycles"].is<int>()) {
        return false;
    }
    *name = doc["name"] | "IGT";
    *o2_target = doc["o2_target"].as<float>();
    *hypoxia_sec = doc["hypoxia_sec"].as<uint32_t>();
    *normoxia_sec = doc["normoxia_sec"].as<uint32_t>();
    *cycles = doc["cycles"].as<uint8_t>();
    return igtProtocolValid(*o2_target, *hypoxia_sec, *normoxia_sec, *cycles);
}

void setOptFloat(JsonObject obj, const char *key, bool ok, float value) {
    if (ok) {
        obj[key] = value;
    } else {
        obj[key] = nullptr;
    }
}

}  // namespace

bool parseCommandJson(const char *json, size_t len) {
    if (json == nullptr || len == 0 || len >= CMD_JSON_MAX) {
        logPrintf("cmd: пустой или слишком длинный JSON");
        return false;
    }

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, json, len);
    if (err) {
        logPrintf("cmd: JSON parse error: %s", err.c_str());
        return false;
    }

    const char *cmd = doc["cmd"] | "";
    if (strcmp(cmd, "ping") == 0) {
        logPrintf("cmd: ping");
        return true;
    }
    if (strcmp(cmd, "phys") == 0) {
        const uint8_t spo2 = doc["spo2"] | 0;
        const uint8_t hr = doc["hr"] | 0;
        const bool belt = (doc["belt"] | 0) != 0;
        const bool contact = (doc["contact"] | 0) != 0;
        physUplinkSet(spo2, hr, belt, contact);
        return true;
    }
    if (strcmp(cmd, "gpio_set") == 0) {
        return handleGpioSet(doc.as<JsonVariantConst>());
    }
    if (strcmp(cmd, "gpio_tap") == 0) {
        return handleGpioTap(doc.as<JsonVariantConst>());
    }
    if (strcmp(cmd, "servo_set") == 0) {
        return handleServoSet(doc.as<JsonVariantConst>());
    }
    if (strcmp(cmd, "servo_auto") == 0) {
        return handleServoAuto(doc.as<JsonVariantConst>());
    }
    if (strcmp(cmd, "flow_reset") == 0) {
        sfm3300ResetVolume();
        return true;
    }
    if (strcmp(cmd, "co2_frc") == 0) {
        uint16_t target_ppm = 400;
        if (doc["target_ppm"].is<int>() || doc["target_ppm"].is<uint16_t>()) {
            target_ppm = doc["target_ppm"].as<uint16_t>();
        }
        if (target_ppm < 400 || target_ppm > 2000) {
            logPrintf("cmd: co2_frc target_ppm вне диапазона");
            return false;
        }
        stc31RequestForcedRecalibration(target_ppm);
        return true;
    }
    if (strcmp(cmd, "igt_prep") == 0) {
        const char *name = "IGT";
        float o2_target = 0;
        uint32_t hypoxia_sec = 0;
        uint32_t normoxia_sec = 0;
        uint8_t cycles = 0;
        if (!readProtocol(doc.as<JsonVariantConst>(), &name, &o2_target, &hypoxia_sec, &normoxia_sec,
                          &cycles)) {
            logPrintf("cmd: igt_prep invalid protocol");
            return false;
        }
        if (!igtSessionPrep(name, o2_target, hypoxia_sec, normoxia_sec, cycles)) {
            logPrintf("cmd: igt_prep busy");
            return false;
        }
        return true;
    }
    if (strcmp(cmd, "igt_start") == 0) {
        if (doc["o2_target"].is<float>() || doc["o2_target"].is<int>() ||
            doc["o2_target"].is<double>()) {
            const char *name = "IGT";
            float o2_target = 0;
            uint32_t hypoxia_sec = 0;
            uint32_t normoxia_sec = 0;
            uint8_t cycles = 0;
            if (!readProtocol(doc.as<JsonVariantConst>(), &name, &o2_target, &hypoxia_sec,
                              &normoxia_sec, &cycles)) {
                logPrintf("cmd: igt_start invalid protocol");
                return false;
            }
            if (!igtSessionStartDirect(name, o2_target, hypoxia_sec, normoxia_sec, cycles)) {
                logPrintf("cmd: igt_start busy");
                return false;
            }
            return true;
        }
        if (!igtSessionStart()) {
            logPrintf("cmd: igt_start not ready");
            return false;
        }
        return true;
    }
    if (strcmp(cmd, "igt_abort") == 0) {
        return igtSessionAbort();
    }
    if (strcmp(cmd, "igt_ack") == 0) {
        return igtSessionAck();
    }
    if (strcmp(cmd, "igt_skip") == 0) {
        return igtSessionBufferSkipToRecirculate();
    }
    if (strcmp(cmd, "igt_fill_continue") == 0) {
        return igtSessionBufferFillContinue();
    }
    if (strcmp(cmd, "igt_fill_abort") == 0) {
        return igtSessionBufferFillAbort();
    }
    if (strcmp(cmd, "igt_test") == 0) {
        const char *action = doc["action"] | "";
        float o2_target = 15.0f;
        if (doc["o2_target"].is<float>() || doc["o2_target"].is<int>() ||
            doc["o2_target"].is<double>()) {
            o2_target = doc["o2_target"].as<float>();
        }
        return igtSessionTestStart(action, o2_target);
    }
    if (strcmp(cmd, "igt_test_stop") == 0) {
        return igtSessionTestStop();
    }

    logPrintf("cmd: неизвестная команда '%s'", cmd);
    return false;
}

size_t buildLiveJson(char *buffer, size_t capacity) {
    if (buffer == nullptr || capacity == 0) {
        return 0;
    }

    JsonDocument doc;
    doc["u"] = millis();
    doc["gt"] = gpioTapInProgress() ? 1 : 0;
    doc["m"] = o2MotorIsOn() ? 1 : 0;

    const Ao02Snapshot o2 = ao02GetSnapshot();
    const Ao02Snapshot work_o2 = workingO2GetSnapshot();
    const Ocs3rlSnapshot ocs = ocs3rlGetSnapshot();
    const FlowSnapshot flow = sfm3300GetSnapshot();
    const Dps310Snapshot pressure = dps310GetSnapshot();
    const Dps310Snapshot work_pressure = workingPressureGetSnapshot();
    const Stc31Snapshot co2 = stc31GetSnapshot();

    JsonObject h = doc["h"].to<JsonObject>();
    setOptFloat(h, "o2", o2.ok, o2.o2_percent);
    setOptFloat(h, "p", pressure.ok, pressure.pressure_hpa);

    JsonObject w = doc["w"].to<JsonObject>();
    setOptFloat(w, "o2", work_o2.ok, work_o2.o2_percent);
#if WORKING_PRESSURE_ENABLED
    setOptFloat(w, "p", work_pressure.ok, work_pressure.pressure_hpa);
#else
    setOptFloat(w, "p", ocs.pressure_hpa > 0.0f, ocs.pressure_hpa);
    w["pi"] = ocs.p_in_kpa;
    w["po"] = ocs.p_out_kpa;
#endif
    setOptFloat(w, "ve", flow.ok, flow.ve_lpm);
    setOptFloat(w, "slm", flow.ok, flow.flow_slm);
    setOptFloat(w, "il", flow.ok, flow.inhale_l);
    setOptFloat(w, "el", flow.ok, flow.exhale_l);
    setOptFloat(w, "co2", co2.ok, co2.co2_percent);
    if (co2.ok) {
        w["ct"] = co2.temp_c;
        w["cr"] = co2.rh_percent;
        w["cs"] = co2.sht_temp_c;
        w["ck"] = co2.gas_ticks;
    }
    // Сырые поля OCS (ЦУ live) — иначе Android видит нули и «не определён».
    const bool ocs_frame =
        ocs.ok || ocs.field_a != 0.0f || ocs.field_b != 0.0f || ocs.field_c != 0.0f;
    if (ocs_frame) {
        w["oa"] = ocs.field_a;
        w["ob"] = ocs.field_b;
        w["oc"] = ocs.field_c;
        w["ot"] = ocs.temp_c;
        w["orh"] = ocs.rh_percent;
        w["of"] = ocs.o2_field_index;
        w["ost"] = ocs.status_raw;
        if (ocs.pressure_hpa > 0.0f) {
            w["op"] = ocs.pressure_hpa;
        }
        const float o2_raw = (work_o2.ok && work_o2.o2_percent > 0.0f) ? work_o2.o2_percent : ocs.o2_percent;
        w["o2c"] = ocs3rlO2Co2Compensated(o2_raw, co2.co2_percent, co2.ok);
    }

    JsonArray gp = doc["gp"].to<JsonArray>();
    uint8_t pins[MAX_GPIO_TRACKED];
    bool states[MAX_GPIO_TRACKED];
    const size_t gpio_count = gpioExportEntries(pins, states, MAX_GPIO_TRACKED);
    for (size_t i = 0; i < gpio_count; ++i) {
        JsonArray row = gp.add<JsonArray>();
        row.add(pins[i]);
        row.add(states[i] ? 1 : 0);
    }

    JsonArray sv = doc["sv"].to<JsonArray>();
    uint8_t servo_pins[MAX_SERVOS];
    uint8_t servo_angles[MAX_SERVOS];
    bool servo_auto[MAX_SERVOS];
    const size_t servo_count =
        servoExportEntries(servo_pins, servo_angles, servo_auto, MAX_SERVOS);
    for (size_t i = 0; i < servo_count; ++i) {
        JsonArray row = sv.add<JsonArray>();
        row.add(servo_pins[i]);
        row.add(servo_angles[i]);
        row.add(servo_auto[i] ? 1 : 0);
    }

    IgtPublicStatus igt{};
    igtSessionGetStatus(&igt);
    JsonObject igt_obj = doc["igt"].to<JsonObject>();
    igt_obj["st"] = igt.state;
    if (strcmp(igt.state, "idle") != 0) {
        if (igt.phase[0] != '\0') {
            igt_obj["ph"] = igt.phase;
        }
        char name_short[25];
        strncpy(name_short, igt.name, sizeof(name_short) - 1);
        name_short[sizeof(name_short) - 1] = '\0';
        if (name_short[0] != '\0') {
            igt_obj["nm"] = name_short;
        }
        igt_obj["c"] = igt.cycle;
        igt_obj["n"] = igt.cycles;
        igt_obj["r"] = igt.phase_remain_sec;
        igt_obj["o2"] = igt.o2_target;
        igt_obj["rd"] = igt.ready_to_start ? 1 : 0;
        const bool run = strcmp(igt.state, "hypoxia") == 0 || strcmp(igt.state, "normoxia") == 0;
        const bool phys_stale = physUplinkStale(millis(), CU_PHYS_STALE_MS);
        igt_obj["sw"] = (igt.sensor_warn || (run && phys_stale)) ? 1 : 0;
        igt_obj["bp"] = igt.buffer_prep;
        igt_obj["br"] = igt.buffer_prep_remain_sec;
        igt_obj["wb"] = igt.waiting_buffer ? 1 : 0;
        igt_obj["hs"] = igt.hypoxia_sec;
        igt_obj["ns"] = igt.normoxia_sec;
        igt_obj["fr"] = igt.fill_remain_sec;
        igt_obj["p0"] = igt.practice_started_esp_ms;
        if (igt.test_action[0] != '\0') {
            igt_obj["ta"] = igt.test_action;
        }
        if (igt.error[0] != '\0') {
            igt_obj["err"] = igt.error;
        }
        if (igt.fill_timeout_pending) {
            igt_obj["ftp"] = 1;
        }
        if (igt.last_phase_volume_ok) {
            igt_obj["lv"] = 1;
            igt_obj["lk"] = igt.last_phase_kind == 0 ? "h" : "n";
            igt_obj["lc"] = igt.last_phase_cycle;
            igt_obj["li"] = igt.last_phase_inhale_l;
            igt_obj["le"] = igt.last_phase_exhale_l;
        }
    }

    const size_t len = serializeJson(doc, buffer, capacity);
    if (len >= capacity) {
        logPrintf("LIVE: JSON truncated cap=%u", static_cast<unsigned>(capacity));
        return 0;
    }
    return len;
}
