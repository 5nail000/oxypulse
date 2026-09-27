package com.oxypulse.esp32tester.api

import com.oxypulse.esp32tester.domain.BridgeStatus
import com.oxypulse.esp32tester.domain.Co2Status
import com.oxypulse.esp32tester.domain.CoospoStatus
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.FlowStatus
import com.oxypulse.esp32tester.domain.GpioState
import com.oxypulse.esp32tester.domain.HrvSnapshot
import com.oxypulse.esp32tester.domain.IgtStatus
import com.oxypulse.esp32tester.domain.O2Status
import com.oxypulse.esp32tester.domain.PressureStatus
import com.oxypulse.esp32tester.domain.Sensors
import com.oxypulse.esp32tester.domain.ServoState
import com.oxypulse.esp32tester.domain.TrendPage
import com.oxypulse.esp32tester.domain.TrendSources
import com.oxypulse.esp32tester.domain.WellueStatus
import com.oxypulse.esp32tester.domain.WifiNetwork
import com.oxypulse.esp32tester.domain.WifiScanResult
import com.oxypulse.esp32tester.domain.WifiStaStatus
import com.oxypulse.esp32tester.domain.WifiStatus
import org.json.JSONArray
import org.json.JSONObject

object StatusParser {
    fun parse(json: String): DeviceStatus = parseStatus(JSONObject(json))

    fun parseStatus(root: JSONObject): DeviceStatus {
        if (root.has("u") && !root.has("uptime_ms")) {
            return parseLive(root)
        }
        val gpio = mutableListOf<GpioState>()
        val gpioArr = root.optJSONArray("gpio") ?: JSONArray()
        for (i in 0 until gpioArr.length()) {
            val item = gpioArr.getJSONObject(i)
            gpio += GpioState(item.getInt("pin"), item.getInt("state"))
        }

        val servos = mutableListOf<ServoState>()
        val servoArr = root.optJSONArray("servos") ?: JSONArray()
        for (i in 0 until servoArr.length()) {
            val item = servoArr.getJSONObject(i)
            servos += ServoState(
                pin = item.getInt("pin"),
                angle = item.getInt("angle"),
                auto = item.optBoolean("auto", false),
            )
        }

        val whitelist = mutableListOf<Int>()
        val wl = root.optJSONArray("whitelist") ?: JSONArray()
        for (i in 0 until wl.length()) {
            whitelist += wl.getInt(i)
        }

        val sensors = root.optJSONObject("sensors")
        val hypoxia = sensors?.optJSONObject("hypoxia")
        val working = sensors?.optJSONObject("working")
        val trends = root.optJSONObject("trends")?.optJSONObject("sources")
        return DeviceStatus(
            uptimeMs = root.optLong("uptime_ms", 0),
            gpioTapActive = root.optBoolean("gpio_tap_active", false),
            gpio = gpio,
            servos = servos,
            whitelist = whitelist,
            sensors = Sensors(
                hypoxiaO2 = parseO2(hypoxia?.optJSONObject("o2") ?: sensors?.optJSONObject("o2")),
                hypoxiaPressure = parsePressure(
                    hypoxia?.optJSONObject("pressure") ?: sensors?.optJSONObject("pressure"),
                ),
                workO2 = parseO2(working?.optJSONObject("o2")),
                workPressure = parsePressure(working?.optJSONObject("pressure")),
                flow = parseFlow(working?.optJSONObject("flow") ?: sensors?.optJSONObject("flow")),
                co2 = parseCo2(working?.optJSONObject("co2") ?: sensors?.optJSONObject("co2")),
                wellue = parseWellue(sensors?.optJSONObject("wellue")),
                coospo = parseCoospo(sensors?.optJSONObject("coospo")),
                bridge = parseBridge(sensors?.optJSONObject("bridge")),
            ),
            wifi = parseWifi(root.optJSONObject("wifi")),
            trends = parseSources(trends),
            igt = parseIgt(root.optJSONObject("igt")),
        )
    }

    fun parseSources(obj: JSONObject?): TrendSources {
        if (obj == null) return TrendSources()
        return TrendSources(
            o2 = obj.optBoolean("o2", false),
            flow = obj.optBoolean("flow", false),
            pressure = obj.optBoolean("pressure", false),
            co2 = obj.optBoolean("co2", false),
            wellue = obj.optBoolean("wellue", false),
            hr = obj.optBoolean("hr", false),
            rr = obj.optBoolean("rr", false),
            workO2 = obj.optBoolean("work_o2", false),
            workPressure = obj.optBoolean("work_pressure", false),
        )
    }

    fun parseTrends(json: String): TrendPage {
        val root = JSONObject(json)
        return TrendPage(
            count = root.optInt("count", 0),
            total = root.optInt("total", 0),
            offset = root.optInt("offset", 0),
            intervalMs = root.optLong("interval_ms", 1000),
            nowMs = root.optLong("now_ms", 0),
            t0Ms = root.optLong("t0_ms", 0),
            sources = parseSources(root.optJSONObject("sources")),
            spo2 = intList(root.optJSONArray("spo2")),
            hrWellue = intList(root.optJSONArray("hr_wellue")),
            hrCoospo = intList(root.optJSONArray("hr_coospo")),
            co2 = intList(root.optJSONArray("co2")),
            co2Raw = intList(root.optJSONArray("co2_raw")),
            o2 = intList(root.optJSONArray("o2")),
            flow = intList(root.optJSONArray("flow")),
            pressure = intList(root.optJSONArray("pressure")),
            workO2 = intList(root.optJSONArray("work_o2")),
            workPressure = intList(root.optJSONArray("work_pressure")),
            rr = intList(root.optJSONArray("rr")),
        )
    }

    fun parseHrv(json: String): HrvSnapshot {
        val root = JSONObject(json)
        val rr = mutableListOf<Int>()
        val arr = root.optJSONArray("rr")
        if (arr != null) {
            for (i in 0 until arr.length()) rr += arr.optInt(i)
        }
        return HrvSnapshot(
            state = root.optString("state", "idle"),
            durationSec = root.optInt("duration_sec", 0),
            elapsedMs = root.optLong("elapsed_ms", 0),
            rrCount = root.optInt("rr_count", 0),
            error = root.optString("error", ""),
            rr = rr,
        )
    }

    fun parseWifiScan(json: String): WifiScanResult {
        val root = JSONObject(json)
        val networks = mutableListOf<WifiNetwork>()
        val arr = root.optJSONArray("networks") ?: JSONArray()
        for (i in 0 until arr.length()) {
            val n = arr.getJSONObject(i)
            networks += WifiNetwork(
                ssid = n.optString("ssid", ""),
                rssi = n.optInt("rssi", 0),
                secure = n.optBoolean("secure", false),
                channel = n.optInt("channel", 0),
            )
        }
        return WifiScanResult(
            scanning = root.optBoolean("scanning", false),
            networks = networks,
            error = root.optString("error", ""),
        )
    }

    fun errorFromBody(body: String?): String {
        if (body.isNullOrBlank()) return ""
        return try {
            JSONObject(body).optString("error", "")
        } catch (_: Exception) {
            ""
        }
    }

    private fun parseO2(obj: JSONObject?): O2Status {
        if (obj == null) return O2Status()
        return O2Status(
            ok = obj.optBoolean("ok", false),
            percent = obj.optDouble("percent", 0.0).toFloat(),
            mv = obj.optDouble("mv", 0.0).toFloat(),
        )
    }

    private fun parsePressure(obj: JSONObject?): PressureStatus {
        if (obj == null) return PressureStatus()
        return PressureStatus(
            ok = obj.optBoolean("ok", false),
            hpa = obj.optDouble("hpa", 0.0).toFloat(),
            tempC = obj.optDouble("temp_c", 0.0).toFloat(),
            baselineOk = obj.optBoolean("baseline_ok", false),
            baselineHpa = obj.optDouble("baseline_hpa", 0.0).toFloat(),
        )
    }

    private fun parseFlow(obj: JSONObject?): FlowStatus {
        if (obj == null) return FlowStatus()
        return FlowStatus(
            ok = obj.optBoolean("ok", false),
            slm = obj.optDouble("slm", obj.optDouble("lpm", 0.0)).toFloat(),
            inhaleL = obj.optDouble("inhale_l", 0.0).toFloat(),
            exhaleL = obj.optDouble("exhale_l", 0.0).toFloat(),
            veLpm = obj.optDouble("ve_lpm", 0.0).toFloat(),
            phase = obj.optString("phase", "idle"),
        )
    }

    private fun parseCo2(obj: JSONObject?): Co2Status {
        if (obj == null) return Co2Status()
        val tempEst = if (obj.has("temp_c_est") && !obj.isNull("temp_c_est")) {
            obj.optDouble("temp_c_est").toFloat()
        } else {
            null
        }
        val rhEst = if (obj.has("rh_est") && !obj.isNull("rh_est")) {
            obj.optDouble("rh_est").toFloat()
        } else {
            null
        }
        return Co2Status(
            ok = obj.optBoolean("ok", false),
            ppm = obj.optInt("ppm", 0),
            ppmRaw = obj.optInt("ppm_raw", 0),
            ppmEst = obj.optInt("ppm_est", 0),
            dynamicComp = obj.optBoolean("dynamic_comp", false),
            percent = obj.optDouble("percent", 0.0).toFloat(),
            tempC = obj.optDouble("temp_c", 0.0).toFloat(),
            rh = obj.optDouble("rh", 0.0).toFloat(),
            tempCEst = tempEst,
            rhEst = rhEst,
            warmingUp = obj.optBoolean("warming_up", false),
            ascEnabled = obj.optBoolean("asc_enabled", false),
        )
    }

    private fun parseWellue(obj: JSONObject?): WellueStatus {
        if (obj == null) return WellueStatus()
        return WellueStatus(
            ok = obj.optBoolean("ok", false),
            connected = obj.optBoolean("connected", false),
            name = obj.optString("name", ""),
            spo2 = obj.optInt("spo2", 0),
            hr = obj.optInt("hr", 0),
            battery = obj.optInt("battery", 0),
            motion = obj.optInt("motion", 0),
            contact = obj.optBoolean("contact", false),
            error = obj.optString("error", ""),
        )
    }

    private fun parseCoospo(obj: JSONObject?): CoospoStatus {
        if (obj == null) return CoospoStatus()
        return CoospoStatus(
            ok = obj.optBoolean("ok", false),
            connected = obj.optBoolean("connected", false),
            name = obj.optString("name", ""),
            bpm = obj.optInt("bpm", 0),
            rrMs = obj.optInt("rr_ms", 0),
            contact = obj.optBoolean("contact", false),
            error = obj.optString("error", ""),
        )
    }

    private fun parseBridge(obj: JSONObject?): BridgeStatus {
        if (obj == null) return BridgeStatus()
        return BridgeStatus(
            ok = obj.optBoolean("ok", false),
            error = obj.optString("error", ""),
        )
    }

    private fun parseWifi(obj: JSONObject?): WifiStatus {
        if (obj == null) return WifiStatus()
        val sta = obj.optJSONObject("sta")
        return WifiStatus(
            apSsid = obj.optString("ap_ssid", "ESP32-Tester"),
            apIp = obj.optString("ap_ip", "192.168.4.1"),
            sta = WifiStaStatus(
                configured = sta?.optBoolean("configured", false) ?: false,
                connected = sta?.optBoolean("connected", false) ?: false,
                ssid = sta?.optString("ssid", "") ?: "",
                ip = sta?.optString("ip", "") ?: "",
                rssi = sta?.optInt("rssi", 0) ?: 0,
                error = sta?.optString("error", "") ?: "",
            ),
        )
    }

    fun parseLive(root: JSONObject): DeviceStatus {
        val h = root.optJSONObject("h")
        val w = root.optJSONObject("w")
        val hypoO2 = optLiveFloat(h, "o2")
        val hypoP = optLiveFloat(h, "p")
        val workO2 = optLiveFloat(w, "o2")
        val workP = optLiveFloat(w, "p")
        val ve = optLiveFloat(w, "ve")
        val slm = optLiveFloat(w, "slm")
        val inhaleL = optLiveFloat(w, "il")
        val exhaleL = optLiveFloat(w, "el")
        val co2pct = optLiveFloat(w, "co2")
        val gpio = mutableListOf<GpioState>()
        val gp = root.optJSONArray("gp") ?: JSONArray()
        for (i in 0 until gp.length()) {
            val row = gp.optJSONArray(i) ?: continue
            gpio += GpioState(row.optInt(0), row.optInt(1))
        }
        val servos = mutableListOf<ServoState>()
        val sv = root.optJSONArray("sv") ?: JSONArray()
        for (i in 0 until sv.length()) {
            val row = sv.optJSONArray(i) ?: continue
            servos += ServoState(row.optInt(0), row.optInt(1), row.optInt(2) != 0)
        }
        val igt = root.optJSONObject("igt")
        val co2Ok = co2pct.first
        val co2Percent = co2pct.second
        return DeviceStatus(
            uptimeMs = root.optLong("u", 0),
            gpioTapActive = root.optInt("gt", 0) != 0,
            gpio = gpio,
            servos = servos,
            whitelist = PinWhitelist.DEFAULT,
            sensors = Sensors(
                hypoxiaO2 = O2Status(ok = hypoO2.first, percent = hypoO2.second),
                hypoxiaPressure = PressureStatus(ok = hypoP.first, hpa = hypoP.second),
                workO2 = O2Status(ok = workO2.first, percent = workO2.second),
                workPressure = PressureStatus(ok = workP.first, hpa = workP.second),
                flow = FlowStatus(
                    ok = ve.first || slm.first || inhaleL.first || exhaleL.first,
                    slm = slm.second,
                    inhaleL = inhaleL.second,
                    exhaleL = exhaleL.second,
                    veLpm = ve.second,
                ),
                co2 = Co2Status(
                    ok = co2Ok,
                    percent = co2Percent,
                    ppm = (co2Percent * 10000f).toInt(),
                    ppmRaw = (co2Percent * 10000f).toInt(),
                    ppmEst = (co2Percent * 10000f).toInt(),
                ),
            ),
            igt = parseLiveIgt(igt),
        )
    }

    private fun parseLiveIgt(obj: JSONObject?): IgtStatus {
        if (obj == null) return IgtStatus()
        return IgtStatus(
            state = obj.optString("st", "idle"),
            phase = obj.optString("ph", ""),
            name = obj.optString("nm", ""),
            cycle = obj.optInt("c", 0),
            cycles = obj.optInt("n", 0),
            hypoxiaSec = obj.optInt("hs", 0),
            normoxiaSec = obj.optInt("ns", 0),
            phaseRemainSec = obj.optInt("r", 0),
            o2Target = obj.optDouble("o2", 15.0).toFloat(),
            readyToStart = flag(obj, "rd"),
            sensorWarn = flag(obj, "sw"),
            bufferPrep = obj.optString("bp", "idle"),
            bufferPrepRemainSec = obj.optInt("br", 0),
            waitingBuffer = flag(obj, "wb"),
            fillTimeoutPending = flag(obj, "ftp"),
            practiceStartedEspMs = obj.optLong("p0", 0),
            error = obj.optString("err", ""),
            testAction = obj.optString("ta", ""),
            lastPhaseVolumeOk = flag(obj, "lv"),
            lastPhaseKind = livePhaseKind(obj.optString("lk", "")),
            lastPhaseCycle = obj.optInt("lc", 0),
            lastPhaseInhaleL = obj.optDouble("li", 0.0).toFloat(),
            lastPhaseExhaleL = obj.optDouble("le", 0.0).toFloat(),
        )
    }

    private fun livePhaseKind(raw: String): String = when (raw) {
        "h" -> "hypoxia"
        "n" -> "normoxia"
        else -> raw
    }

    private fun flag(obj: JSONObject, key: String): Boolean {
        if (!obj.has(key)) return false
        return when (val v = obj.opt(key)) {
            is Boolean -> v
            is Number -> v.toInt() != 0
            else -> obj.optInt(key, 0) != 0
        }
    }

    private fun optLiveFloat(obj: JSONObject?, key: String): Pair<Boolean, Float> {
        if (obj == null || !obj.has(key) || obj.isNull(key)) return false to 0f
        return true to obj.optDouble(key, 0.0).toFloat()
    }

    private fun parseIgt(obj: JSONObject?): IgtStatus {
        if (obj == null) return IgtStatus()
        return IgtStatus(
            state = obj.optString("state", "idle"),
            phase = obj.optString("phase", ""),
            name = obj.optString("name", ""),
            cycle = obj.optInt("cycle", 0),
            cycles = obj.optInt("cycles", 0),
            hypoxiaSec = obj.optInt("hypoxia_sec", 0),
            normoxiaSec = obj.optInt("normoxia_sec", 0),
            phaseRemainSec = obj.optInt("phase_remain_sec", 0),
            o2Target = obj.optDouble("o2_target", 15.0).toFloat(),
            readyToStart = obj.optBoolean("ready_to_start", false),
            sensorWarn = obj.optBoolean("sensor_warn", false),
            bufferPrep = obj.optString("buffer_prep", "idle"),
            bufferPrepRemainSec = obj.optInt("buffer_prep_remain_sec", 0),
            waitingBuffer = obj.optBoolean("waiting_buffer", false),
            fillTimeoutPending = obj.optBoolean("fill_timeout_pending", false),
            practiceStartedEspMs = obj.optLong("practice_started_esp_ms", 0),
            error = obj.optString("error", ""),
            testAction = obj.optString("test_action", ""),
            lastPhaseVolumeOk = obj.optBoolean("last_phase_volume_ok", false),
            lastPhaseKind = obj.optString("last_phase_kind", ""),
            lastPhaseCycle = obj.optInt("last_phase_cycle", 0),
            lastPhaseInhaleL = obj.optDouble("last_phase_inhale_l", 0.0).toFloat(),
            lastPhaseExhaleL = obj.optDouble("last_phase_exhale_l", 0.0).toFloat(),
        )
    }

    private fun intList(arr: JSONArray?): List<Int> {
        if (arr == null) return emptyList()
        return (0 until arr.length()).map { arr.optInt(it) }
    }
}
