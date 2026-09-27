package com.oxypulse.esp32tester.domain

enum class TransportType {
    None,
    Wifi,
    ValveCu,
    Ble,
}

fun TransportType.hasFullUi(): Boolean = this == TransportType.Wifi || this == TransportType.ValveCu

enum class ConnectionState { Disconnected, Scanning, Connecting, Connected }

enum class LinkState { Idle, Live, Stale, Offline }

enum class Overlay { None, Wifi, Hrv, Cp, Practice, Igt, Wearables }

enum class HrvStep { Setup, Countdown, Recording, Result, Error }

enum class SessionUi { Idle, Ready, Recording, Afterglow, Done }

enum class IgtStep { Setup, Prep, Run, Test, Review }

data class GpioState(val pin: Int, val state: Int)

data class ServoState(val pin: Int, val angle: Int, val auto: Boolean)

data class O2Status(
    val ok: Boolean = false,
    val percent: Float = 0f,
    val mv: Float = 0f,
)

data class PressureStatus(
    val ok: Boolean = false,
    val hpa: Float = 0f,
    val tempC: Float = 0f,
    val baselineOk: Boolean = false,
    val baselineHpa: Float = 0f,
)

data class FlowStatus(
    val ok: Boolean = false,
    val slm: Float = 0f,
    val inhaleL: Float = 0f,
    val exhaleL: Float = 0f,
    val veLpm: Float = 0f,
    val phase: String = "idle",
)

data class Co2Status(
    val ok: Boolean = false,
    val ppm: Int = 0,
    val ppmRaw: Int = 0,
    val ppmEst: Int = 0,
    val dynamicComp: Boolean = false,
    val percent: Float = 0f,
    val tempC: Float = 0f,
    val rh: Float = 0f,
    val tempCEst: Float? = null,
    val rhEst: Float? = null,
    val warmingUp: Boolean = false,
    val ascEnabled: Boolean = false,
)

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
    val rrMs: Int = 0,
    val contact: Boolean = false,
    val error: String = "",
)

data class NearbyBleDevice(
    val name: String,
    val address: String,
    val rssi: Int,
    val likelyWellue: Boolean = false,
    val likelyBelt: Boolean = false,
)

data class WearableScanUi(
    val scanning: Boolean = false,
    val remainSec: Int = 0,
    val hint: String = "",
)

data class BridgeStatus(
    val ok: Boolean = false,
    val error: String = "",
)

data class Sensors(
    val hypoxiaO2: O2Status = O2Status(),
    val hypoxiaPressure: PressureStatus = PressureStatus(),
    val workO2: O2Status = O2Status(),
    val workPressure: PressureStatus = PressureStatus(),
    val flow: FlowStatus = FlowStatus(),
    val co2: Co2Status = Co2Status(),
    val wellue: WellueStatus = WellueStatus(),
    val coospo: CoospoStatus = CoospoStatus(),
    val bridge: BridgeStatus = BridgeStatus(),
)

data class WifiStaStatus(
    val configured: Boolean = false,
    val connected: Boolean = false,
    val ssid: String = "",
    val ip: String = "",
    val rssi: Int = 0,
    val error: String = "",
)

data class WifiStatus(
    val apSsid: String = "ESP32-Tester",
    val apIp: String = "192.168.4.1",
    val sta: WifiStaStatus = WifiStaStatus(),
)

data class TrendSources(
    val o2: Boolean = false,
    val flow: Boolean = false,
    val pressure: Boolean = false,
    val co2: Boolean = false,
    val wellue: Boolean = false,
    val hr: Boolean = false,
    val rr: Boolean = false,
    val workO2: Boolean = false,
    val workPressure: Boolean = false,
) {
    fun any(): Boolean =
        o2 || flow || pressure || co2 || wellue || hr || rr || workO2 || workPressure

    fun merged(extra: TrendSources): TrendSources = copy(
        o2 = o2 || extra.o2,
        flow = flow || extra.flow,
        pressure = pressure || extra.pressure,
        co2 = co2 || extra.co2,
        wellue = wellue || extra.wellue,
        hr = hr || extra.hr,
        rr = rr || extra.rr,
        workO2 = workO2 || extra.workO2,
        workPressure = workPressure || extra.workPressure,
    )

    fun toggle(key: String): TrendSources = when (key) {
        "o2" -> copy(o2 = !o2)
        "flow" -> copy(flow = !flow)
        "pressure" -> copy(pressure = !pressure)
        "co2" -> copy(co2 = !co2)
        "wellue" -> copy(wellue = !wellue)
        "hr" -> copy(hr = !hr)
        "rr" -> copy(rr = !rr)
        "work_o2" -> copy(workO2 = !workO2)
        "work_pressure" -> copy(workPressure = !workPressure)
        else -> this
    }

    fun isOn(key: String): Boolean = when (key) {
        "o2" -> o2
        "flow" -> flow
        "pressure" -> pressure
        "co2" -> co2
        "wellue" -> wellue
        "hr" -> hr
        "rr" -> rr
        "work_o2" -> workO2
        "work_pressure" -> workPressure
        else -> false
    }

    companion object {
        val ALL = TrendSources(
            o2 = true,
            flow = true,
            pressure = true,
            co2 = true,
            wellue = true,
            hr = true,
            rr = true,
            workO2 = true,
            workPressure = true,
        )
    }
}

data class IgtStatus(
    val state: String = "idle",
    val phase: String = "",
    val name: String = "",
    val cycle: Int = 0,
    val cycles: Int = 0,
    val hypoxiaSec: Int = 0,
    val normoxiaSec: Int = 0,
    val phaseRemainSec: Int = 0,
    val o2Target: Float = 15f,
    val readyToStart: Boolean = false,
    val sensorWarn: Boolean = false,
    val bufferPrep: String = "idle",
    val bufferPrepRemainSec: Int = 0,
    val waitingBuffer: Boolean = false,
    val fillTimeoutPending: Boolean = false,
    val practiceStartedEspMs: Long = 0,
    val error: String = "",
    val testAction: String = "",
    val lastPhaseVolumeOk: Boolean = false,
    val lastPhaseKind: String = "",
    val lastPhaseCycle: Int = 0,
    val lastPhaseInhaleL: Float = 0f,
    val lastPhaseExhaleL: Float = 0f,
)

data class DeviceStatus(
    val uptimeMs: Long = 0,
    val gpioTapActive: Boolean = false,
    val gpio: List<GpioState> = emptyList(),
    val servos: List<ServoState> = emptyList(),
    val whitelist: List<Int> = emptyList(),
    val sensors: Sensors = Sensors(),
    val wifi: WifiStatus = WifiStatus(),
    val trends: TrendSources = TrendSources(),
    val igt: IgtStatus = IgtStatus(),
) {
    val wellue: WellueStatus get() = sensors.wellue
    val coospo: CoospoStatus get() = sensors.coospo
}

data class ScannedDevice(val name: String, val address: String)

data class WifiNetwork(
    val ssid: String,
    val rssi: Int,
    val secure: Boolean,
    val channel: Int,
)

data class WifiScanResult(
    val scanning: Boolean = false,
    val networks: List<WifiNetwork> = emptyList(),
    val error: String = "",
)

data class ChartPoint(val t: Long, val v: Float?)

data class TrendPage(
    val count: Int = 0,
    val total: Int = 0,
    val offset: Int = 0,
    val intervalMs: Long = 1000,
    val nowMs: Long = 0,
    val t0Ms: Long = 0,
    val sources: TrendSources = TrendSources(),
    val spo2: List<Int> = emptyList(),
    val hrWellue: List<Int> = emptyList(),
    val hrCoospo: List<Int> = emptyList(),
    val co2: List<Int> = emptyList(),
    val co2Raw: List<Int> = emptyList(),
    val o2: List<Int> = emptyList(),
    val flow: List<Int> = emptyList(),
    val pressure: List<Int> = emptyList(),
    val workO2: List<Int> = emptyList(),
    val workPressure: List<Int> = emptyList(),
    val rr: List<Int> = emptyList(),
)

data class HrvSnapshot(
    val state: String = "idle",
    val durationSec: Int = 0,
    val elapsedMs: Long = 0,
    val rrCount: Int = 0,
    val error: String = "",
    val rr: List<Int> = emptyList(),
)

data class PoincarePair(val x: Float, val y: Float)

data class HrvMetrics(
    val ok: Boolean,
    val rawCount: Int = 0,
    val filteredCount: Int = 0,
    val meanRr: Float = 0f,
    val hr: Float = 0f,
    val sdnn: Float = 0f,
    val rmssd: Float = 0f,
    val pnn50: Float = 0f,
    val sd1: Float = 0f,
    val sd2: Float = 0f,
    val pairs: List<PoincarePair> = emptyList(),
)

data class IgtProtocol(
    val id: String,
    val name: String,
    val o2Target: Float,
    val hypoxiaMin: Int,
    val normoxiaMin: Int,
    val cycles: Int,
    val builtin: Boolean = false,
)

object SessionSources {
    val CP = TrendSources(wellue = true, hr = true)
    val PRACTICE = TrendSources(
        wellue = true,
        hr = true,
        rr = true,
        flow = true,
        workO2 = true,
        co2 = true,
    )
    val IGT = TrendSources(o2 = true, pressure = true, flow = true)
}
