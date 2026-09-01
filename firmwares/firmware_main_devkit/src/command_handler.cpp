#include "command_handler.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "ao02.h"
#include "ble_sensors.h"
#include "config.h"
#include "dps310.h"
#include "gpio_controller.h"
#include "logger.h"
#include "servo_controller.h"
#include "scd41.h"
#include "sfm3300.h"
#include "trend_buffer.h"
#include "wifi_sta.h"

#include <WiFi.h>

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
    return gpioSet(pin, state);
}

bool handleGpioTap(JsonVariantConst doc) {
    if (!doc["pin"].is<uint8_t>() && !doc["pin"].is<int>()) {
        return false;
    }
    const uint8_t pin = doc["pin"].as<uint8_t>();
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
    return servoSetAngle(pin, angle);
}

bool handleServoAuto(JsonVariantConst doc) {
    if (!doc["pin"].is<uint8_t>() && !doc["pin"].is<int>()) {
        return false;
    }
    const uint8_t pin = doc["pin"].as<uint8_t>();
    const bool enabled = doc["enabled"].is<bool>() ? doc["enabled"].as<bool>()
                                                     : doc["enabled"].as<int>() != 0;
    uint32_t pause_ms = SERVO_AUTO_PAUSE_MS;
    if (doc["pause_ms"].is<uint32_t>() || doc["pause_ms"].is<int>()) {
        pause_ms = doc["pause_ms"].as<uint32_t>();
    }
    return servoSetAuto(pin, enabled, pause_ms);
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
        scd41RequestForcedRecalibration(target_ppm);
        return true;
    }

    logPrintf("cmd: неизвестная команда '%s'", cmd);
    return false;
}

size_t buildStatusJson(char *buffer, size_t capacity) {
    if (buffer == nullptr || capacity == 0) {
        return 0;
    }

    JsonDocument doc;
    doc["uptime_ms"] = millis();
    doc["gpio_tap_active"] = gpioTapInProgress();

    JsonArray gpio_arr = doc["gpio"].to<JsonArray>();
    uint8_t pins[MAX_GPIO_TRACKED];
    bool states[MAX_GPIO_TRACKED];
    const size_t gpio_count = gpioExportEntries(pins, states, MAX_GPIO_TRACKED);
    for (size_t i = 0; i < gpio_count; ++i) {
        JsonObject item = gpio_arr.add<JsonObject>();
        item["pin"] = pins[i];
        item["state"] = states[i] ? 1 : 0;
    }

    JsonArray servo_arr = doc["servos"].to<JsonArray>();
    uint8_t servo_pins[MAX_SERVOS];
    uint8_t servo_angles[MAX_SERVOS];
    bool servo_auto[MAX_SERVOS];
    const size_t servo_count =
        servoExportEntries(servo_pins, servo_angles, servo_auto, MAX_SERVOS);
    for (size_t i = 0; i < servo_count; ++i) {
        JsonObject item = servo_arr.add<JsonObject>();
        item["pin"] = servo_pins[i];
        item["angle"] = servo_angles[i];
        item["auto"] = servo_auto[i];
    }

    JsonArray whitelist = doc["whitelist"].to<JsonArray>();
    for (size_t i = 0; i < GPIO_WHITELIST_SIZE; ++i) {
        whitelist.add(GPIO_WHITELIST[i]);
    }

    const Ao02Snapshot o2 = ao02GetSnapshot();
    const Ao02Snapshot work_o2 = workingO2GetSnapshot();
    const FlowSnapshot flow = sfm3300GetSnapshot();
    const Dps310Snapshot pressure = dps310GetSnapshot();
    const Dps310Snapshot work_pressure = workingPressureGetSnapshot();
    const Scd41Snapshot co2 = scd41GetSnapshot();

    JsonObject sensors = doc["sensors"].to<JsonObject>();

    auto fillO2 = [](JsonObject obj, const Ao02Snapshot &snap) {
        obj["ok"] = snap.ok;
        obj["percent"] = snap.o2_percent;
        obj["mv"] = snap.voltage_mv;
    };
    auto fillPressure = [](JsonObject obj, const Dps310Snapshot &snap) {
        obj["ok"] = snap.ok;
        obj["hpa"] = snap.pressure_hpa;
        obj["temp_c"] = snap.temp_c;
        obj["baseline_hpa"] = snap.baseline_hpa;
        obj["baseline_ok"] = snap.baseline_ok;
    };
    auto fillFlow = [](JsonObject obj, const FlowSnapshot &snap) {
        obj["ok"] = snap.ok;
        obj["slm"] = snap.flow_slm;
        obj["inhale_l"] = snap.inhale_l;
        obj["exhale_l"] = snap.exhale_l;
        obj["ve_lpm"] = snap.ve_lpm;
        obj["phase"] =
            snap.phase == FlowPhase::Inhale ? "inhale" :
            snap.phase == FlowPhase::Exhale ? "exhale" : "idle";
    };
    auto fillCo2 = [](JsonObject obj, const Scd41Snapshot &snap) {
        obj["ok"] = snap.ok;
        obj["ppm"] = snap.co2_ppm;
        obj["ppm_raw"] = snap.co2_ppm_raw;
        obj["ppm_est"] = snap.co2_ppm_est;
        obj["dynamic_comp"] = snap.dynamic_comp;
        obj["percent"] = static_cast<float>(snap.co2_ppm) / 10000.0f;
        obj["temp_c"] = snap.temp_c;
        obj["temp_c_est"] = snap.temp_c_est;
        obj["rh"] = snap.rh_percent;
        obj["rh_est"] = snap.rh_percent_est;
        obj["warming_up"] = snap.warming_up;
        obj["asc_enabled"] = snap.asc_enabled;
    };

    fillO2(sensors["o2"].to<JsonObject>(), o2);
    fillFlow(sensors["flow"].to<JsonObject>(), flow);
    fillPressure(sensors["pressure"].to<JsonObject>(), pressure);
    fillCo2(sensors["co2"].to<JsonObject>(), co2);

    JsonObject hypoxia = sensors["hypoxia"].to<JsonObject>();
    fillO2(hypoxia["o2"].to<JsonObject>(), o2);
    fillPressure(hypoxia["pressure"].to<JsonObject>(), pressure);

    JsonObject working = sensors["working"].to<JsonObject>();
    fillO2(working["o2"].to<JsonObject>(), work_o2);
    fillPressure(working["pressure"].to<JsonObject>(), work_pressure);
    fillFlow(working["flow"].to<JsonObject>(), flow);
    fillCo2(working["co2"].to<JsonObject>(), co2);

    const WellueSnapshot wellue = bleSensorsGetWellue();
    JsonObject wellue_obj = sensors["wellue"].to<JsonObject>();
    wellue_obj["ok"] = wellue.ok;
    wellue_obj["connected"] = wellue.connected;
    wellue_obj["name"] = wellue.name;
    wellue_obj["spo2"] = wellue.spo2;
    wellue_obj["hr"] = wellue.hr;
    wellue_obj["battery"] = wellue.battery;
    wellue_obj["motion"] = wellue.motion;
    wellue_obj["contact"] = wellue.contact;
    wellue_obj["error"] = wellue.error;

    const CoospoSnapshot coospo = bleSensorsGetCoospo();
    JsonObject coospo_obj = sensors["coospo"].to<JsonObject>();
    coospo_obj["ok"] = coospo.ok;
    coospo_obj["connected"] = coospo.connected;
    coospo_obj["name"] = coospo.name;
    coospo_obj["bpm"] = coospo.bpm;
    coospo_obj["rr_ms"] = coospo.rr_ms;
    coospo_obj["contact"] = coospo.contact;
    coospo_obj["error"] = coospo.error;

    const BridgeSnapshot bridge = bleSensorsGetBridge();
    JsonObject bridge_obj = sensors["bridge"].to<JsonObject>();
    bridge_obj["ok"] = bridge.ok;
    bridge_obj["error"] = bridge.error;

    const WifiStaStatus sta = wifiStaGetStatus();
    JsonObject wifi_obj = doc["wifi"].to<JsonObject>();
    wifi_obj["ap_ssid"] = WIFI_AP_SSID;
    wifi_obj["ap_ip"] = WiFi.softAPIP().toString();
    JsonObject sta_obj = wifi_obj["sta"].to<JsonObject>();
    sta_obj["configured"] = sta.configured;
    sta_obj["connected"] = sta.connected;
    sta_obj["ssid"] = sta.ssid;
    sta_obj["ip"] = sta.ip;
    sta_obj["rssi"] = sta.rssi;
    sta_obj["error"] = sta.error;

    const uint32_t trend_mask = trendBufferGetMask();
    JsonObject trends = doc["trends"].to<JsonObject>();
    JsonObject sources = trends["sources"].to<JsonObject>();
    sources["o2"] = (trend_mask & TREND_SRC_O2) != 0;
    sources["flow"] = (trend_mask & TREND_SRC_FLOW) != 0;
    sources["pressure"] = (trend_mask & TREND_SRC_PRESSURE) != 0;
    sources["co2"] = (trend_mask & TREND_SRC_CO2) != 0;
    sources["wellue"] = (trend_mask & TREND_SRC_WELLUE) != 0;
    sources["hr"] = (trend_mask & TREND_SRC_HR) != 0;
    sources["rr"] = (trend_mask & TREND_SRC_RR) != 0;
    sources["work_o2"] = (trend_mask & TREND_SRC_WORK_O2) != 0;
    sources["work_pressure"] = (trend_mask & TREND_SRC_WORK_PRESSURE) != 0;

    const size_t len = serializeJson(doc, buffer, capacity);
    return len;
}
