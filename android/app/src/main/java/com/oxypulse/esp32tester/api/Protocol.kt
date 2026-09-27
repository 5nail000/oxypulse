package com.oxypulse.esp32tester.api

import com.oxypulse.esp32tester.domain.TrendSources
import org.json.JSONObject

object BleConstants {
    const val DEVICE_NAME_PREFIX = "ESP32-Tester"
    const val SERVICE_UUID = "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
    const val STATUS_CHAR_UUID = "a1b2c3d4-e5f6-7890-abcd-ef1234567891"
    const val CMD_CHAR_UUID = "a1b2c3d4-e5f6-7890-abcd-ef1234567892"
}

object ValveCuConstants {
    const val DEVICE_NAME = "OxyPulse-CU"
    const val SERVICE_UUID = "c0a10000-4f78-11ef-b200-0800200c9a66"
    const val LIVE_CHAR_UUID = "c0a10001-4f78-11ef-b200-0800200c9a66"
    const val CMD_CHAR_UUID = "c0a10002-4f78-11ef-b200-0800200c9a66"
    const val PREFERRED_MTU = 512
}

object WearableBleConstants {
    const val OXYII_SERVICE = "e8fb0001-a14b-98f9-831b-4e2941d01248"
    const val OXYII_WRITE = "e8fb0002-a14b-98f9-831b-4e2941d01248"
    const val OXYII_NOTIFY = "e8fb0003-a14b-98f9-831b-4e2941d01248"
    const val HR_SERVICE = "0000180d-0000-1000-8000-00805f9b34fb"
    const val HR_MEAS = "00002a37-0000-1000-8000-00805f9b34fb"
    const val WELLUE_MFG_OXYII = 0xF34E
    const val WELLUE_MFG_VIATOM = 0x036F
    const val SAMPLE_STALE_MS = 3000L
    const val WELLUE_POLL_MS = 1000L
    const val CONNECT_TIMEOUT_MS = 15_000L
    const val OPEN_SCAN_AFTER_MS = 8_000L
    const val SCAN_RESTART_MS = 12_000L
    const val MANUAL_SCAN_MS = 20_000L
}

object CommandBuilder {
    const val GPIO_TAP_PULSE_MS = 250
    const val O2_MOTOR_GPIO = 33
    const val SERVO1_GPIO = 4

    fun gpioSet(pin: Int, on: Boolean): String =
        """{"cmd":"gpio_set","pin":$pin,"state":${if (on) 1 else 0}}"""

    fun gpioTap(pin: Int, pulseMs: Int = GPIO_TAP_PULSE_MS): String =
        """{"cmd":"gpio_tap","pin":$pin,"pulse_ms":$pulseMs}"""

    fun servoSet(pin: Int, angle: Int): String =
        """{"cmd":"servo_set","pin":$pin,"angle":$angle}"""

    fun servoAuto(pin: Int, enabled: Boolean, pauseMs: Long = 5000): String =
        """{"cmd":"servo_auto","pin":$pin,"enabled":$enabled,"pause_ms":$pauseMs}"""

    fun flowReset(): String = """{"cmd":"flow_reset"}"""

    fun co2Frc(targetPpm: Int = 400): String =
        """{"cmd":"co2_frc","target_ppm":$targetPpm}"""

    fun ping(): String = """{"cmd":"ping"}"""

    fun phys(spo2: Int, hr: Int, belt: Boolean, contact: Boolean): String =
        JSONObject().apply {
            put("cmd", "phys")
            put("spo2", spo2)
            put("hr", hr)
            put("belt", if (belt) 1 else 0)
            put("contact", if (contact) 1 else 0)
        }.toString()

    fun igtCuCommand(cmd: String, body: String? = null): String {
        val obj = if (body.isNullOrBlank()) JSONObject() else JSONObject(body)
        obj.put("cmd", cmd)
        return obj.toString()
    }

    fun trendSourcesJson(s: TrendSources): String = JSONObject().apply {
        put("o2", s.o2)
        put("flow", s.flow)
        put("pressure", s.pressure)
        put("co2", s.co2)
        put("wellue", s.wellue)
        put("hr", s.hr)
        put("rr", s.rr)
        put("work_o2", s.workO2)
        put("work_pressure", s.workPressure)
    }.toString()

    fun igtProtocolJson(
        name: String,
        o2Target: Float,
        hypoxiaSec: Int,
        normoxiaSec: Int,
        cycles: Int,
    ): String = JSONObject().apply {
        put("name", name)
        put("o2_target", o2Target.toDouble())
        put("hypoxia_sec", hypoxiaSec)
        put("normoxia_sec", normoxiaSec)
        put("cycles", cycles)
    }.toString()

    fun hrvStartJson(durationSec: Int): String =
        JSONObject().put("duration_sec", durationSec).toString()

    fun wifiConfigJson(ssid: String, password: String): String =
        JSONObject().put("ssid", ssid).put("password", password).toString()

    fun wifiForgetJson(): String = """{"clear":true}"""

    fun igtTestJson(action: String, o2Target: Float? = null): String = JSONObject().apply {
        put("action", action)
        if (o2Target != null) put("o2_target", o2Target.toDouble())
    }.toString()
}

object PinWhitelist {
    val DEFAULT = listOf(4, 13, 14, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33)
}
