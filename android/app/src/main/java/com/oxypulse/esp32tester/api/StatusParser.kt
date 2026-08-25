package com.oxypulse.esp32tester.api

import com.oxypulse.esp32tester.domain.CoospoStatus
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.GpioState
import com.oxypulse.esp32tester.domain.ServoState
import com.oxypulse.esp32tester.domain.WellueStatus
import org.json.JSONArray
import org.json.JSONObject

object StatusParser {
    fun parse(json: String): DeviceStatus {
        val root = JSONObject(json)
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
        return DeviceStatus(
            uptimeMs = root.optLong("uptime_ms", 0),
            gpio = gpio,
            servos = servos,
            whitelist = whitelist,
            wellue = parseWellue(sensors?.optJSONObject("wellue")),
            coospo = parseCoospo(sensors?.optJSONObject("coospo")),
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
            contact = obj.optBoolean("contact", false),
            error = obj.optString("error", ""),
        )
    }
}
