package com.oxypulse.esp32tester.api

object BleConstants {
    const val DEVICE_NAME_PREFIX = "ESP32-Tester"
    const val SERVICE_UUID = "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
    const val STATUS_CHAR_UUID = "a1b2c3d4-e5f6-7890-abcd-ef1234567891"
    const val CMD_CHAR_UUID = "a1b2c3d4-e5f6-7890-abcd-ef1234567892"
}

object CommandBuilder {
    fun gpioSet(pin: Int, on: Boolean): String =
        """{"cmd":"gpio_set","pin":$pin,"state":${if (on) 1 else 0}}"""

    fun gpioTap(pin: Int, pulseMs: Int = 3000): String =
        """{"cmd":"gpio_tap","pin":$pin,"pulse_ms":$pulseMs}"""

    fun servoSet(pin: Int, angle: Int): String =
        """{"cmd":"servo_set","pin":$pin,"angle":$angle}"""

    fun servoAuto(pin: Int, enabled: Boolean, pauseMs: Long = 5000): String =
        """{"cmd":"servo_auto","pin":$pin,"enabled":${enabled},"pause_ms":$pauseMs}"""

    fun ping(): String = """{"cmd":"ping"}"""
}

object PinWhitelist {
    val DEFAULT = listOf(13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33)
}
