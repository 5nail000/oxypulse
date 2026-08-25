package com.oxypulse.esp32tester.domain

enum class TransportType { None, Wifi, Ble }

enum class ConnectionState { Disconnected, Scanning, Connecting, Connected }

data class GpioState(val pin: Int, val state: Int)

data class ServoState(val pin: Int, val angle: Int, val auto: Boolean)

data class WellueStatus(
    val ok: Boolean = false,
    val connected: Boolean = false,
    val name: String = "",
    val spo2: Int = 0,
    val hr: Int = 0,
    val battery: Int = 0,
    val motion: Int = 0,
    val contact: Boolean = false,
    val error: String = "",
)

data class CoospoStatus(
    val ok: Boolean = false,
    val connected: Boolean = false,
    val name: String = "",
    val bpm: Int = 0,
    val contact: Boolean = false,
    val error: String = "",
)

data class DeviceStatus(
    val uptimeMs: Long = 0,
    val gpio: List<GpioState> = emptyList(),
    val servos: List<ServoState> = emptyList(),
    val whitelist: List<Int> = emptyList(),
    val wellue: WellueStatus = WellueStatus(),
    val coospo: CoospoStatus = CoospoStatus(),
)

data class ScannedDevice(val name: String, val address: String)
