package com.oxypulse.esp32tester.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.oxypulse.esp32tester.api.CommandBuilder
import com.oxypulse.esp32tester.api.PinWhitelist
import com.oxypulse.esp32tester.data.AppPrefs
import com.oxypulse.esp32tester.data.Esp32Repository
import com.oxypulse.esp32tester.domain.ChartPoint
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.Formatters
import com.oxypulse.esp32tester.domain.HrvMath
import com.oxypulse.esp32tester.domain.HrvMetrics
import com.oxypulse.esp32tester.domain.HrvStep
import com.oxypulse.esp32tester.domain.IgtLogic
import com.oxypulse.esp32tester.domain.IgtMonitor
import com.oxypulse.esp32tester.domain.IgtMonitorReport
import com.oxypulse.esp32tester.domain.IgtProtocol
import com.oxypulse.esp32tester.domain.IgtStatus
import com.oxypulse.esp32tester.domain.Sensors
import com.oxypulse.esp32tester.domain.IgtStep
import com.oxypulse.esp32tester.ui.charts.ChartMarker
import com.oxypulse.esp32tester.ui.theme.NormoxiaGreen
import com.oxypulse.esp32tester.ui.theme.Orange
import com.oxypulse.esp32tester.domain.LinkState
import com.oxypulse.esp32tester.domain.Overlay
import com.oxypulse.esp32tester.domain.ScannedDevice
import com.oxypulse.esp32tester.domain.SessionSources
import com.oxypulse.esp32tester.domain.SessionUi
import com.oxypulse.esp32tester.domain.TrendSources
import com.oxypulse.esp32tester.domain.TrendStore
import com.oxypulse.esp32tester.domain.TransportType
import com.oxypulse.esp32tester.domain.hasFullUi
import com.oxypulse.esp32tester.domain.WifiNetwork
import com.oxypulse.esp32tester.util.AppLogger
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

data class ChartSnapshot(
    val now: Long = 0L,
    val o2: List<ChartPoint> = emptyList(),
    val workO2: List<ChartPoint> = emptyList(),
    val flow: List<ChartPoint> = emptyList(),
    val slm: List<ChartPoint> = emptyList(),
    val pressure: List<ChartPoint> = emptyList(),
    val workPressure: List<ChartPoint> = emptyList(),
    val co2: List<ChartPoint> = emptyList(),
    val co2Raw: List<ChartPoint> = emptyList(),
    val spo2: List<ChartPoint> = emptyList(),
    val hrWellue: List<ChartPoint> = emptyList(),
    val hrCoospo: List<ChartPoint> = emptyList(),
    val rr: List<ChartPoint> = emptyList(),
)

data class HrvUi(
    val step: HrvStep = HrvStep.Setup,
    val durationSec: Int = 30,
    val countdown: Int = 5,
    val elapsedMs: Long = 0,
    val rrCount: Int = 0,
    val error: String = "",
    val metrics: HrvMetrics? = null,
    val stamp: String = "",
    val note: String = "",
    val comment: String = "",
)

data class SessionCharts(
    val startedAt: Long? = null,
    val stopAt: Long? = null,
    val durationSec: Int = 0,
    val windowSec: Int = 60,
    val spo2: List<ChartPoint> = emptyList(),
    val hrWellue: List<ChartPoint> = emptyList(),
    val hrCoospo: List<ChartPoint> = emptyList(),
    val hr: List<ChartPoint> = emptyList(),
    val rr: List<ChartPoint> = emptyList(),
    val ve: List<ChartPoint> = emptyList(),
    val slm: List<ChartPoint> = emptyList(),
    val o2: List<ChartPoint> = emptyList(),
    val co2: List<ChartPoint> = emptyList(),
    val co2Raw: List<ChartPoint> = emptyList(),
    val pressure: List<ChartPoint> = emptyList(),
    val workO2: List<ChartPoint> = emptyList(),
    val workPressure: List<ChartPoint> = emptyList(),
    val chartOrigin: Long? = null,
    val scrollLive: Boolean = false,
    val fullSession: Boolean = false,
    val phaseMarkers: List<ChartMarker> = emptyList(),
)

data class CpUi(
    val ui: SessionUi = SessionUi.Idle,
    val charts: SessionCharts = SessionCharts(windowSec = 60),
)

data class PracticeUi(
    val ui: SessionUi = SessionUi.Idle,
    val useBelt: Boolean = false,
    val charts: SessionCharts = SessionCharts(windowSec = 120),
)

data class IgtUi(
    val step: IgtStep = IgtStep.Setup,
    val selectedId: String = IgtLogic.BUILTIN.id,
    val protocols: List<IgtProtocol> = listOf(IgtLogic.BUILTIN),
    val editorOpen: Boolean = false,
    val editorNew: Boolean = false,
    val editName: String = "",
    val editO2: String = "15",
    val editHyp: String = "6",
    val editNor: String = "5",
    val editCycles: String = "6",
    val formError: String = "",
    val testOpen: Boolean = false,
    val confirmAbort: Boolean = false,
    val comment: String = "",
    val actionError: String = "",
    val windowSec: Int = 300,
    val charts: SessionCharts = SessionCharts(windowSec = 300),
    val confirmFillTimeout: Boolean = false,
    val monitorOpen: Boolean = false,
    val showFullSessionChart: Boolean = false,
)

data class WifiUi(
    val scanning: Boolean = false,
    val status: String = "",
    val networks: List<WifiNetwork> = emptyList(),
    val ssid: String = "",
    val password: String = "",
    val saving: Boolean = false,
)

@HiltViewModel
class TesterViewModel @Inject constructor(
    private val repository: Esp32Repository,
    private val prefs: AppPrefs,
) : ViewModel() {

    val transport = repository.transport
    val status = repository.status
    val connectionState = repository.connectionState
    val scannedDevices: StateFlow<List<ScannedDevice>> = repository.scannedDevices
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())
    val wearableNearby = repository.wearableNearby
    val wearableScan = repository.wearableScan

    private val _message = MutableStateFlow<String?>(null)
    val message: StateFlow<String?> = _message.asStateFlow()

    private val _overlay = MutableStateFlow(Overlay.None)
    val overlay: StateFlow<Overlay> = _overlay.asStateFlow()

    private val _host = MutableStateFlow(prefs.lastHost())
    val host: StateFlow<String> = _host.asStateFlow()

    private val _linkState = MutableStateFlow(LinkState.Idle)
    val linkState: StateFlow<LinkState> = _linkState.asStateFlow()

    private val _lastError = MutableStateFlow("")
    val lastError: StateFlow<String> = _lastError.asStateFlow()

    private val _lastOkAt = MutableStateFlow(0L)
    val lastOkAt: StateFlow<Long> = _lastOkAt.asStateFlow()

    private val _trendSources = MutableStateFlow(TrendSources())
    val trendSources: StateFlow<TrendSources> = _trendSources.asStateFlow()

    private val _chartWindowSec = MutableStateFlow(300)
    val chartWindowSec: StateFlow<Int> = _chartWindowSec.asStateFlow()

    private val _charts = MutableStateFlow(ChartSnapshot())
    val charts: StateFlow<ChartSnapshot> = _charts.asStateFlow()

    private val _flowDynamics = MutableStateFlow(prefs.flowDynamics())
    val flowDynamics: StateFlow<Boolean> = _flowDynamics.asStateFlow()

    private val _hrViewBelt = MutableStateFlow(prefs.hrViewBelt())
    val hrViewBelt: StateFlow<Boolean> = _hrViewBelt.asStateFlow()

    private val _hrViewWellue = MutableStateFlow(prefs.hrViewWellue())
    val hrViewWellue: StateFlow<Boolean> = _hrViewWellue.asStateFlow()

    private val _gpioTapBusy = MutableStateFlow(false)
    val gpioTapBusy: StateFlow<Boolean> = _gpioTapBusy.asStateFlow()

    private val _gpioTapLabel = MutableStateFlow("Тапнуть")
    val gpioTapLabel: StateFlow<String> = _gpioTapLabel.asStateFlow()

    private val _hrv = MutableStateFlow(HrvUi())
    val hrv: StateFlow<HrvUi> = _hrv.asStateFlow()

    private val _cp = MutableStateFlow(CpUi())
    val cp: StateFlow<CpUi> = _cp.asStateFlow()

    private val _practice = MutableStateFlow(PracticeUi())
    val practice: StateFlow<PracticeUi> = _practice.asStateFlow()

    private val _igt = MutableStateFlow(IgtUi(protocols = prefs.allProtocols()))
    val igt: StateFlow<IgtUi> = _igt.asStateFlow()

    private val _wifiUi = MutableStateFlow(WifiUi())
    val wifiUi: StateFlow<WifiUi> = _wifiUi.asStateFlow()

    private val history = TrendStore()
    private val igtHistory = TrendStore()
    private val igtMonitor = IgtMonitor()
    private val _igtMonitorReport = MutableStateFlow(IgtMonitorReport.empty())
    val igtMonitorReport: StateFlow<IgtMonitorReport> = _igtMonitorReport.asStateFlow()
    private var igtChartOrigin: Long? = null
    private var igtLastPhaseKey: String? = null
    private val igtPhaseMarkers = mutableListOf<ChartMarker>()
    private val cpStore = TrendStore()
    private val practiceStore = TrendStore()

    private var pollJob: Job? = null
    private var linkJob: Job? = null
    private var hrvJob: Job? = null
    private var wifiScanJob: Job? = null
    private var tapJob: Job? = null
    private var cpTickJob: Job? = null
    private var practiceTickJob: Job? = null
    private var statusPollPaused = false
    private var hydrateOnNextOk = true
    private var lastPollFailed = true
    private var sessionSourcesSaved: TrendSources? = null
    private var igtSourcesOn = false
    private var cuLinkWasUp = false
    private var igtGattDropOpen = false

    init {
        viewModelScope.launch {
            combine(repository.transport, repository.connectionState) { _, state -> state }
                .collect { onCuConnection(it) }
        }
    }

    fun setHost(value: String) {
        _host.value = value
    }

    fun connectWifi(host: String) {
        viewModelScope.launch {
            try {
                val cleaned = host.trim().removeSuffix("/")
                prefs.setLastHost(cleaned.removePrefix("http://").removePrefix("https://"))
                _host.value = cleaned
                repository.connectWifi(cleaned)
                repository.verifyWifiConnection()
                startPolling()
                _message.value = "WiFi подключено"
                AppLogger.i("WiFi connected: $cleaned")
            } catch (e: Exception) {
                repository.disconnect()
                _message.value = "WiFi ошибка: ${e.message}"
                AppLogger.e("WiFi error: ${e.message}")
            }
        }
    }

    fun startCuScan() = repository.connectCuStartScan()
    fun stopCuScan() = repository.stopCuScan()

    fun connectCu(address: String) {
        viewModelScope.launch {
            try {
                repository.connectCu(address)
                delay(400)
                startPolling()
                _message.value = "ЦУ подключено"
                AppLogger.i("CU connected: $address")
            } catch (e: Exception) {
                repository.disconnect()
                _message.value = "ЦУ: ${e.message}"
                AppLogger.e("CU error: ${e.message}")
            }
        }
    }

    fun startBleScan() = startCuScan()
    fun stopBleScan() = stopCuScan()

    fun connectBle(address: String) = connectCu(address)

    fun findWearables() {
        repository.findWearables()
        AppLogger.i("Wearable find requested")
    }

    fun openWearablesFinder(rescan: Boolean = true) {
        if (transport.value != TransportType.ValveCu) {
            _message.value = "Поиск кольца и пояса — только с ЦУ"
            return
        }
        if (rescan) {
            repository.findWearables()
        }
        closeSessions(keep = Overlay.Wearables)
        _overlay.value = Overlay.Wearables
        AppLogger.i("Wearable finder opened rescan=$rescan")
    }

    fun connectWearable(address: String, wellue: Boolean) {
        repository.connectWearable(address, wellue)
        _message.value = if (wellue) "Подключаю кольцо" else "Подключаю пояс"
        AppLogger.i("Wearable connect as ${if (wellue) "Wellue" else "COOSPO"} $address")
    }

    fun reconnectWearable(wellue: Boolean) {
        repository.reconnectWearable(wellue)
        _message.value = if (wellue) "Подключаю кольцо" else "Подключаю пояс"
        AppLogger.i("Wearable reconnect ${if (wellue) "Wellue" else "COOSPO"}")
    }

    fun lastWellueAddress(): String = repository.lastWellueAddress()

    fun lastCoospoAddress(): String = repository.lastCoospoAddress()

    fun disconnect() {
        pollJob?.cancel()
        linkJob?.cancel()
        statusPollPaused = false
        repository.disconnect()
        history.clear()
        _charts.value = ChartSnapshot()
        _linkState.value = LinkState.Idle
        _overlay.value = Overlay.None
        _message.value = "Отключено"
        AppLogger.i("Disconnected")
    }

    fun consumeMessage() {
        _message.value = null
    }

    fun availablePins(status: DeviceStatus): List<Int> =
        status.whitelist.ifEmpty { PinWhitelist.DEFAULT }

    fun applyGpio(pin: Int, on: Boolean) {
        viewModelScope.launch {
            try {
                repository.sendCommand(CommandBuilder.gpioSet(pin, on))
            } catch (e: Exception) {
                _message.value = "GPIO: ${e.message}"
            }
        }
    }

    fun tapGpio(pin: Int) {
        if (_gpioTapBusy.value) return
        tapJob?.cancel()
        tapJob = viewModelScope.launch {
            val motor = pin == CommandBuilder.O2_MOTOR_GPIO
            _gpioTapBusy.value = true
            try {
                repository.sendCommand(CommandBuilder.gpioTap(pin))
                if (motor) {
                    _gpioTapLabel.value = "Импульс…"
                    delay(CommandBuilder.GPIO_TAP_PULSE_MS.toLong())
                } else {
                    _gpioTapLabel.value = "Тап…"
                    delay(CommandBuilder.GPIO_TAP_PULSE_MS.toLong())
                    val end = System.currentTimeMillis() + 6000
                    while (isActive && System.currentTimeMillis() < end) {
                        val left = ((end - System.currentTimeMillis() + 999) / 1000).coerceAtLeast(1)
                        _gpioTapLabel.value = "$left с…"
                        delay(250)
                    }
                }
            } catch (e: Exception) {
                _message.value = "GPIO tap: ${e.message}"
            } finally {
                _gpioTapBusy.value = false
                _gpioTapLabel.value = if (pin == CommandBuilder.O2_MOTOR_GPIO) "Переключить мотор" else "Тапнуть"
            }
        }
    }

    fun applyServo(pin: Int, angle: Int, auto: Boolean) {
        viewModelScope.launch {
            try {
                val cmd = when {
                    auto -> CommandBuilder.servoAuto(pin, true)
                    status.value.servos.any { it.pin == pin && it.auto } -> CommandBuilder.servoAuto(pin, false)
                    else -> CommandBuilder.servoSet(pin, angle)
                }
                repository.sendCommand(cmd)
            } catch (e: Exception) {
                _message.value = "Servo: ${e.message}"
            }
        }
    }

    fun flowReset() {
        viewModelScope.launch {
            try {
                repository.sendCommand(CommandBuilder.flowReset())
            } catch (e: Exception) {
                _message.value = "Поток: ${e.message}"
            }
        }
    }

    fun co2Frc() {
        viewModelScope.launch {
            try {
                repository.sendCommand(CommandBuilder.co2Frc())
                _message.value = "FRC 400 ppm отправлен"
            } catch (e: Exception) {
                _message.value = "CO₂: ${e.message}"
            }
        }
    }

    fun setChartWindow(sec: Int) {
        _chartWindowSec.value = sec
        publishCharts()
    }

    fun toggleTrend(key: String) {
        if (!transport.value.hasFullUi()) {
            _message.value = "Тренды доступны на ЦУ и по WiFi"
            return
        }
        viewModelScope.launch {
            val next = _trendSources.value.toggle(key)
            try {
                if (transport.value == TransportType.Wifi) {
                    repository.postTrendSources(next)
                }
                _trendSources.value = next
                publishCharts()
            } catch (e: Exception) {
                _message.value = "Тренды: ${e.message}"
            }
        }
    }

    fun setFlowDynamics(on: Boolean) {
        _flowDynamics.value = on
        prefs.setFlowDynamics(on)
        publishCharts()
    }

    fun setHrViewBelt(on: Boolean) {
        _hrViewBelt.value = on
        prefs.setHrViewBelt(on)
    }

    fun setHrViewWellue(on: Boolean) {
        _hrViewWellue.value = on
        prefs.setHrViewWellue(on)
    }

    fun openOverlay(target: Overlay) {
        if (target == Overlay.Wifi && transport.value != TransportType.Wifi) {
            _message.value = "Настройка STA — только стенд WiFi"
            return
        }
        if (target != Overlay.None && !transport.value.hasFullUi()) {
            _message.value = "Действия доступны на ЦУ и по WiFi"
            return
        }
        closeSessions(keep = target)
        _overlay.value = target
        when (target) {
            Overlay.Hrv -> _hrv.value = HrvUi()
            Overlay.Cp -> {
                cpStore.clear()
                _cp.value = CpUi(ui = SessionUi.Ready)
            }
            Overlay.Practice -> {
                practiceStore.clear()
                _practice.value = PracticeUi(ui = SessionUi.Ready)
            }
            Overlay.Igt -> {
                _igt.value = _igt.value.copy(
                    protocols = prefs.allProtocols(),
                    step = IgtLogic.stepOf(status.value.igt),
                    actionError = "",
                    confirmAbort = false,
                    testOpen = false,
                    editorOpen = false,
                )
            }
            Overlay.Wifi -> {
                _wifiUi.value = WifiUi(status = Formatters.wifiModalStatus(status.value.wifi))
            }
            Overlay.Wearables -> Unit
            Overlay.None -> Unit
        }
    }

    fun closeOverlay() {
        when (_overlay.value) {
            Overlay.Hrv -> closeHrv()
            Overlay.Cp -> closeCp()
            Overlay.Practice -> closePractice()
            Overlay.Igt -> closeIgt()
            else -> _overlay.value = Overlay.None
        }
    }

    fun scanWifi() {
        wifiScanJob?.cancel()
        wifiScanJob = viewModelScope.launch {
            _wifiUi.value = _wifiUi.value.copy(scanning = true, status = "Запуск скана… AP может кратко моргнуть")
            try {
                repository.startWifiScan()
                repeat(24) { i ->
                    delay(800)
                    try {
                        val data = repository.getWifiScan()
                        if (data.scanning) {
                            _wifiUi.value = _wifiUi.value.copy(status = "Сканирование… ${i + 1}/24")
                            return@repeat
                        }
                        val msg = when {
                            data.error.isNotBlank() -> "Ошибка: ${data.error}"
                            data.networks.isNotEmpty() -> "Найдено: ${data.networks.size}"
                            else -> "Сети не найдены — введите SSID вручную"
                        }
                        _wifiUi.value = _wifiUi.value.copy(
                            scanning = false,
                            networks = data.networks,
                            status = msg,
                        )
                        return@launch
                    } catch (e: Exception) {
                        _wifiUi.value = _wifiUi.value.copy(
                            status = "Скан… (переподключитесь к AP при обрыве) · ${e.message}",
                        )
                    }
                }
                _wifiUi.value = _wifiUi.value.copy(
                    scanning = false,
                    status = "Таймаут скана — введите SSID вручную",
                )
            } catch (e: Exception) {
                _wifiUi.value = _wifiUi.value.copy(
                    scanning = false,
                    status = "Ошибка: ${e.message}. Введите SSID вручную.",
                )
            }
        }
    }

    fun setWifiSsid(v: String) {
        _wifiUi.value = _wifiUi.value.copy(ssid = v)
    }

    fun setWifiPassword(v: String) {
        _wifiUi.value = _wifiUi.value.copy(password = v)
    }

    fun saveWifi() {
        val ssid = _wifiUi.value.ssid.trim()
        if (ssid.isBlank()) return
        viewModelScope.launch {
            _wifiUi.value = _wifiUi.value.copy(saving = true, status = "Сохранение…")
            try {
                repository.saveWifi(ssid, _wifiUi.value.password)
                _message.value = "ESP32 перезагружается. Подключитесь снова."
                AppLogger.i("WiFi config saved, device rebooting")
                disconnect()
            } catch (e: Exception) {
                _wifiUi.value = _wifiUi.value.copy(saving = false, status = "WiFi: ${e.message}")
            }
        }
    }

    fun forgetWifi() {
        viewModelScope.launch {
            try {
                repository.forgetWifi()
                _message.value = "Сеть забыта, ESP32 перезагружается"
                disconnect()
            } catch (e: Exception) {
                _wifiUi.value = _wifiUi.value.copy(status = "WiFi: ${e.message}")
            }
        }
    }

    fun setHrvDuration(sec: Int) {
        _hrv.value = _hrv.value.copy(durationSec = sec)
    }

    fun startHrvCountdown() {
        hrvJob?.cancel()
        hrvJob = viewModelScope.launch {
            statusPollPaused = true
            var left = 5
            _hrv.value = _hrv.value.copy(step = HrvStep.Countdown, countdown = left, error = "")
            while (left > 0) {
                delay(1000)
                left -= 1
                _hrv.value = _hrv.value.copy(countdown = left)
            }
            startHrvRecording()
        }
    }

    fun cancelHrv() {
        hrvJob?.cancel()
        viewModelScope.launch {
            statusPollPaused = false
            runCatching { repository.stopHrv() }
            _hrv.value = HrvUi(durationSec = _hrv.value.durationSec)
        }
    }

    fun setHrvComment(v: String) {
        _hrv.value = _hrv.value.copy(comment = v)
    }

    fun startCp() {
        cpStore.clear()
        _cp.value = CpUi(
            ui = SessionUi.Recording,
            charts = SessionCharts(startedAt = System.currentTimeMillis(), windowSec = 60),
        )
        enableSessionSources(SessionSources.CP)
        cpTickJob?.cancel()
        cpTickJob = viewModelScope.launch {
            while (isActive && (_cp.value.ui == SessionUi.Recording || _cp.value.ui == SessionUi.Afterglow)) {
                tickCp()
                delay(1000)
            }
        }
    }

    fun finishCp() {
        val cur = _cp.value
        if (cur.ui != SessionUi.Recording) return
        val dur = elapsedSec(cur.charts.startedAt)
        val hasSpo2 = cpStore.spo2.any { it != null }
        _cp.value = cur.copy(
            ui = if (hasSpo2) SessionUi.Afterglow else SessionUi.Done,
            charts = cur.charts.copy(stopAt = System.currentTimeMillis(), durationSec = dur),
        )
        if (!hasSpo2) cpTickJob?.cancel()
    }

    fun startPractice() {
        practiceStore.clear()
        val belt = status.value.coospo.connected && status.value.coospo.ok
        _practice.value = PracticeUi(
            ui = SessionUi.Recording,
            useBelt = belt,
            charts = SessionCharts(startedAt = System.currentTimeMillis(), windowSec = 120),
        )
        enableSessionSources(SessionSources.PRACTICE)
        practiceTickJob?.cancel()
        practiceTickJob = viewModelScope.launch {
            while (isActive && (_practice.value.ui == SessionUi.Recording || _practice.value.ui == SessionUi.Afterglow)) {
                tickPractice()
                delay(1000)
            }
        }
    }

    fun finishPractice() {
        val cur = _practice.value
        if (cur.ui != SessionUi.Recording) return
        _practice.value = cur.copy(
            ui = SessionUi.Afterglow,
            charts = cur.charts.copy(
                stopAt = System.currentTimeMillis(),
                durationSec = elapsedSec(cur.charts.startedAt),
            ),
        )
    }

    fun repeatPractice() {
        practiceStore.clear()
        practiceTickJob?.cancel()
        _practice.value = PracticeUi(ui = SessionUi.Ready)
        restoreSessionSources()
    }

    fun selectIgtProtocol(id: String) {
        _igt.value = _igt.value.copy(selectedId = id, actionError = "")
    }

    fun openIgtEditor(new: Boolean) {
        val p = selectedProtocol()
        _igt.value = _igt.value.copy(
            editorOpen = true,
            editorNew = new,
            editName = if (new || p.builtin) "" else p.name,
            editO2 = p.o2Target.toString(),
            editHyp = p.hypoxiaMin.toString(),
            editNor = p.normoxiaMin.toString(),
            editCycles = p.cycles.toString(),
            formError = "",
        )
    }

    fun closeIgtEditor() {
        _igt.value = _igt.value.copy(editorOpen = false)
    }

    fun setIgtEdit(name: String? = null, o2: String? = null, hyp: String? = null, nor: String? = null, cycles: String? = null) {
        _igt.value = _igt.value.copy(
            editName = name ?: _igt.value.editName,
            editO2 = o2 ?: _igt.value.editO2,
            editHyp = hyp ?: _igt.value.editHyp,
            editNor = nor ?: _igt.value.editNor,
            editCycles = cycles ?: _igt.value.editCycles,
        )
    }

    fun saveIgtProtocol() {
        val ui = _igt.value
        val name = ui.editName.trim()
        val o2 = ui.editO2.toFloatOrNull() ?: 0f
        val hyp = ui.editHyp.toIntOrNull() ?: 0
        val nor = ui.editNor.toIntOrNull() ?: 0
        val cycles = ui.editCycles.toIntOrNull() ?: 0
        val err = IgtLogic.validateCustom(name, o2, hyp, nor, cycles)
        if (err != null) {
            _igt.value = ui.copy(formError = err)
            return
        }
        val customs = prefs.loadCustomProtocols().toMutableList()
        val id = if (ui.editorNew || selectedProtocol().builtin) "p_${System.currentTimeMillis()}" else ui.selectedId
        val proto = IgtProtocol(id, name, o2, hyp, nor, cycles)
        val idx = customs.indexOfFirst { it.id == id }
        if (idx >= 0) customs[idx] = proto else customs += proto
        prefs.saveCustomProtocols(customs)
        _igt.value = ui.copy(
            protocols = prefs.allProtocols(),
            selectedId = id,
            editorOpen = false,
            formError = "",
        )
    }

    fun deleteIgtProtocol() {
        val id = _igt.value.selectedId
        prefs.saveCustomProtocols(prefs.loadCustomProtocols().filter { it.id != id })
        _igt.value = _igt.value.copy(
            protocols = prefs.allProtocols(),
            selectedId = IgtLogic.BUILTIN.id,
            editorOpen = false,
        )
    }

    fun openIgtTest() {
        _igt.value = _igt.value.copy(testOpen = true)
    }

    fun closeIgtTest() {
        _igt.value = _igt.value.copy(testOpen = false)
    }

    fun igtPrepare() {
        val proto = selectedProtocol()
        val blocks = IgtLogic.blockers(status.value.sensors)
        if (blocks.isNotEmpty()) {
            _igt.value = _igt.value.copy(actionError = "Нельзя начать подготовку: ${blocks.joinToString(", ")}")
            return
        }
        viewModelScope.launch {
            try {
                resetIgtSessionTracking()
                ensureIgtSources()
                repository.igtPost(
                    "/api/igt/prep",
                    CommandBuilder.igtProtocolJson(
                        proto.name,
                        proto.o2Target,
                        proto.hypoxiaMin * 60,
                        proto.normoxiaMin * 60,
                        proto.cycles,
                    ),
                )
                _igt.value = _igt.value.copy(step = IgtStep.Prep, actionError = "")
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Подготовка: ${e.message}")
            }
        }
    }

    fun igtStart() {
        val proto = selectedProtocol()
        val direct = _igt.value.step == IgtStep.Setup || IgtLogic.isIdle(status.value.igt)
        viewModelScope.launch {
            try {
                ensureIgtSources()
                val body = if (direct) {
                    CommandBuilder.igtProtocolJson(
                        proto.name,
                        proto.o2Target,
                        proto.hypoxiaMin * 60,
                        proto.normoxiaMin * 60,
                        proto.cycles,
                    )
                } else {
                    null
                }
                repository.igtPost("/api/igt/start", body)
                if (direct) resetIgtSessionTracking()
                _igt.value = _igt.value.copy(
                    step = IgtStep.Run,
                    actionError = "",
                    windowSec = IgtLogic.liveChartWindowSec(proto, status.value.igt),
                    showFullSessionChart = false,
                )
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Старт: ${e.message}")
            }
        }
    }

    fun igtSkipBuffer() {
        viewModelScope.launch {
            try {
                repository.igtPost("/api/igt/buffer/skip-recirculate", "{}")
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Пропуск: ${e.message}")
            }
        }
    }

    fun igtFillContinue() {
        viewModelScope.launch {
            try {
                repository.igtPost("/api/igt/buffer/fill-continue", "{}")
                _igt.value = _igt.value.copy(confirmFillTimeout = false, actionError = "")
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Продолжение: ${e.message}")
            }
        }
    }

    fun igtFillAbort() {
        viewModelScope.launch {
            try {
                repository.igtPost("/api/igt/buffer/fill-abort", "{}")
                _igt.value = _igt.value.copy(confirmFillTimeout = false, actionError = "")
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Отмена: ${e.message}")
            }
        }
    }

    fun dismissFillTimeoutPrompt() {
        _igt.value = _igt.value.copy(confirmFillTimeout = false)
    }

    fun openIgtMonitorReport() {
        _igt.value = _igt.value.copy(monitorOpen = true)
    }

    fun closeIgtMonitorReport() {
        _igt.value = _igt.value.copy(monitorOpen = false)
    }

    fun toggleIgtFullSessionChart() {
        _igt.value = _igt.value.copy(showFullSessionChart = !_igt.value.showFullSessionChart)
        publishIgtCharts(_igt.value, status.value.igt)
    }

    fun requestIgtAbort() {
        _igt.value = _igt.value.copy(confirmAbort = true)
    }

    fun cancelIgtAbort() {
        _igt.value = _igt.value.copy(confirmAbort = false)
    }

    fun confirmIgtAbort() {
        viewModelScope.launch {
            try {
                repository.igtPost("/api/igt/abort", "{}")
                _igt.value = _igt.value.copy(confirmAbort = false, testOpen = false)
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Прерывание: ${e.message}", confirmAbort = false)
            }
        }
    }

    fun igtAck() {
        viewModelScope.launch {
            runCatching { repository.igtPost("/api/igt/ack", "{}") }
            igtHistory.clear()
            igtChartOrigin = null
            igtPhaseMarkers.clear()
            igtLastPhaseKey = null
            igtSourcesOn = false
            restoreSessionSources()
            _igt.value = _igt.value.copy(
                step = IgtStep.Setup,
                comment = "",
                actionError = "",
                confirmAbort = false,
                confirmFillTimeout = false,
                showFullSessionChart = false,
                charts = SessionCharts(),
            )
        }
    }

    fun igtTestStart(action: String) {
        viewModelScope.launch {
            try {
                val body = CommandBuilder.igtTestJson(
                    action,
                    if (action == "hypo_recirculate") 15f else null,
                )
                repository.igtPost("/api/igt/test/start", body)
                resetIgtSessionTracking("Тест-драйв")
                _igt.value = _igt.value.copy(
                    step = IgtStep.Test,
                    actionError = "",
                    windowSec = IgtLogic.liveChartWindowSec(selectedProtocol(), status.value.igt),
                    showFullSessionChart = false,
                )
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Тест: ${e.message}")
            }
        }
    }

    fun igtTestStop() {
        viewModelScope.launch {
            try {
                repository.igtPost("/api/igt/test/stop", "{}")
            } catch (e: Exception) {
                _igt.value = _igt.value.copy(actionError = "Стоп: ${e.message}")
            }
        }
    }

    fun setIgtComment(v: String) {
        _igt.value = _igt.value.copy(comment = v)
    }

    fun selectedProtocol(): IgtProtocol =
        _igt.value.protocols.find { it.id == _igt.value.selectedId } ?: IgtLogic.BUILTIN

    private suspend fun startHrvRecording() {
        try {
            repository.startHrv(_hrv.value.durationSec)
        } catch (e: Exception) {
            statusPollPaused = false
            _hrv.value = _hrv.value.copy(step = HrvStep.Error, error = HrvMath.mapApiError(e.message ?: ""))
            return
        }
        _hrv.value = _hrv.value.copy(step = HrvStep.Recording, elapsedMs = 0, rrCount = 0)
        var fails = 0
        while (kotlin.coroutines.coroutineContext.isActive && _hrv.value.step == HrvStep.Recording) {
            try {
                val snap = repository.getHrv()
                fails = 0
                when (snap.state) {
                    "recording" -> _hrv.value = _hrv.value.copy(
                        elapsedMs = snap.elapsedMs,
                        rrCount = snap.rrCount,
                        durationSec = snap.durationSec.takeIf { it > 0 } ?: _hrv.value.durationSec,
                    )
                    "done" -> {
                        val metrics = HrvMath.compute(snap.rr)
                        statusPollPaused = false
                        if (!metrics.ok) {
                            _hrv.value = _hrv.value.copy(
                                step = HrvStep.Error,
                                error = "Мало данных для HRV: ${metrics.filteredCount} интервалов после фильтрации (нужно ≥ ${HrvMath.MIN_RR}). Исходных R-R: ${metrics.rawCount}.",
                            )
                        } else {
                            _hrv.value = _hrv.value.copy(
                                step = HrvStep.Result,
                                metrics = metrics,
                                stamp = java.text.SimpleDateFormat("yyyy-MM-dd HH:mm:ss", java.util.Locale.US)
                                    .format(java.util.Date()),
                                note = "Интервалов: ${metrics.rawCount} (после фильтрации ${metrics.filteredCount})",
                            )
                        }
                        return
                    }
                    "error" -> {
                        statusPollPaused = false
                        _hrv.value = _hrv.value.copy(step = HrvStep.Error, error = snap.error.ifBlank { "Ошибка замера HRV" })
                        return
                    }
                    else -> {
                        statusPollPaused = false
                        _hrv.value = _hrv.value.copy(step = HrvStep.Error, error = "Замер прерван")
                        return
                    }
                }
            } catch (e: Exception) {
                fails += 1
                if (fails >= 5) {
                    statusPollPaused = false
                    _hrv.value = _hrv.value.copy(step = HrvStep.Error, error = e.message ?: "Нет связи с ESP32")
                    return
                }
            }
            delay(1000)
        }
    }

    private fun closeHrv() {
        hrvJob?.cancel()
        viewModelScope.launch {
            statusPollPaused = false
            runCatching { repository.stopHrv() }
            _overlay.value = Overlay.None
            _hrv.value = HrvUi()
        }
    }

    private fun closeCp() {
        cpTickJob?.cancel()
        cpStore.clear()
        _cp.value = CpUi()
        _overlay.value = Overlay.None
        restoreSessionSources()
    }

    private fun closePractice() {
        practiceTickJob?.cancel()
        practiceStore.clear()
        _practice.value = PracticeUi()
        _overlay.value = Overlay.None
        restoreSessionSources()
    }

    private fun closeIgt() {
        if (IgtLogic.isLive(status.value.igt)) {
            _overlay.value = Overlay.None
            return
        }
        igtHistory.clear()
        if (igtSourcesOn) {
            igtSourcesOn = false
            restoreSessionSources()
        }
        _igt.value = _igt.value.copy(editorOpen = false, testOpen = false, confirmAbort = false)
        _overlay.value = Overlay.None
    }

    private fun closeSessions(keep: Overlay) {
        if (keep != Overlay.Hrv && _overlay.value == Overlay.Hrv) closeHrv()
        if (keep != Overlay.Cp && _cp.value.ui != SessionUi.Idle) closeCp()
        if (keep != Overlay.Practice && _practice.value.ui != SessionUi.Idle) closePractice()
    }

    private fun startPolling() {
        hydrateOnNextOk = true
        lastPollFailed = true
        _lastOkAt.value = 0
        _lastError.value = ""
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                if (!statusPollPaused) {
                    try {
                        repository.refreshStatus()
                        onStatus(repository.status.value)
                    } catch (e: Exception) {
                        lastPollFailed = true
                        _lastError.value = e.message ?: "ошибка"
                        if (transport.value == TransportType.Wifi) {
                            AppLogger.w("Polling: ${e.message}")
                        }
                    }
                }
                refreshLink()
                delay(1000)
            }
        }
        linkJob?.cancel()
        linkJob = viewModelScope.launch {
            while (isActive) {
                refreshLink()
                delay(250)
            }
        }
    }

    private suspend fun onStatus(data: DeviceStatus) {
        lastPollFailed = false
        _lastOkAt.value = System.currentTimeMillis()
        _lastError.value = ""
        if (transport.value == TransportType.Wifi) {
            _trendSources.value = data.trends
        }
        if (hydrateOnNextOk) {
            hydrateOnNextOk = false
            if (transport.value == TransportType.Wifi) {
                hydrateTrends()
            } else if (transport.value == TransportType.ValveCu && !_trendSources.value.any()) {
                _trendSources.value = TrendSources.ALL
            }
        }
        if (_trendSources.value.any()) history.pushLive(data, _trendSources.value)
        feedCp(data)
        feedPractice(data)
        feedIgt(data)
        publishCharts()
    }

    private fun feedCp(data: DeviceStatus) {
        val ui = _cp.value
        if (ui.ui != SessionUi.Recording && ui.ui != SessionUi.Afterglow) return
        val s = data.sensors
        val wellueLive = s.wellue.ok && s.wellue.contact
        val coospoLive = s.coospo.ok && s.coospo.contact
        cpStore.t += System.currentTimeMillis()
        cpStore.spo2 += if (wellueLive) TrendStore.saneSpo2(s.wellue.spo2) else null
        cpStore.hrWellue += if (wellueLive) TrendStore.saneHr(s.wellue.hr) else null
        cpStore.hrCoospo += if (coospoLive) TrendStore.saneHr(s.coospo.bpm) else null
        tickCp()
    }

    private fun tickCp() {
        val ui = _cp.value
        if (ui.ui == SessionUi.Idle || ui.ui == SessionUi.Ready) return
        var next = ui.ui
        var frozen = ui.charts.stopAt
        val started = ui.charts.startedAt ?: return
        if (ui.ui == SessionUi.Afterglow) {
            val stop = ui.charts.stopAt ?: System.currentTimeMillis()
            if (System.currentTimeMillis() - stop >= 45_000) {
                next = SessionUi.Done
                frozen = System.currentTimeMillis()
                cpTickJob?.cancel()
            }
        }
        val now = frozen ?: System.currentTimeMillis()
        val elapsed = ((now - started) / 1000).toInt()
        var window = ui.charts.windowSec
        if (elapsed + 5 >= window) window += 15
        _cp.value = ui.copy(
            ui = next,
            charts = ui.charts.copy(
                windowSec = window,
                durationSec = if (ui.ui == SessionUi.Recording) elapsed else ui.charts.durationSec,
                spo2 = zip(cpStore.t, cpStore.spo2),
                hrWellue = zip(cpStore.t, cpStore.hrWellue),
                hrCoospo = zip(cpStore.t, cpStore.hrCoospo),
            ),
        )
    }

    private fun feedPractice(data: DeviceStatus) {
        val ui = _practice.value
        if (ui.ui != SessionUi.Recording && ui.ui != SessionUi.Afterglow) return
        val s = data.sensors
        val wellueLive = s.wellue.ok && s.wellue.contact
        val coospoLive = s.coospo.ok && s.coospo.contact
        val useBelt = ui.useBelt || coospoLive
        practiceStore.t += System.currentTimeMillis()
        practiceStore.spo2 += if (wellueLive) TrendStore.saneSpo2(s.wellue.spo2) else null
        practiceStore.hrCoospo += if (useBelt && coospoLive) TrendStore.saneHr(s.coospo.bpm) else null
        practiceStore.hrWellue += if (!useBelt && wellueLive) TrendStore.saneHr(s.wellue.hr) else null
        practiceStore.rr += if (coospoLive) TrendStore.saneRr(s.coospo.rrMs) else null
        practiceStore.flow += TrendStore.finiteOrNull(s.flow.ok, s.flow.veLpm)
        practiceStore.slm += TrendStore.finiteOrNull(s.flow.ok, s.flow.slm)
        practiceStore.workO2 += TrendStore.finiteOrNull(s.workO2.ok, s.workO2.percent)
        practiceStore.co2 += TrendStore.finiteOrNull(s.co2.ok, Formatters.co2Percent(s.co2))
        practiceStore.co2Raw += if (s.co2.dynamicComp) TrendStore.finiteOrNull(s.co2.ok, s.co2.ppmRaw / 10000f) else null
        tickPractice()
    }

    private fun tickPractice() {
        val ui = _practice.value
        if (ui.ui == SessionUi.Idle || ui.ui == SessionUi.Ready) return
        var next = ui.ui
        val started = ui.charts.startedAt ?: return
        var frozen = ui.charts.stopAt
        if (ui.ui == SessionUi.Afterglow) {
            val stop = ui.charts.stopAt ?: System.currentTimeMillis()
            if (System.currentTimeMillis() - stop >= 60_000) {
                next = SessionUi.Done
                frozen = System.currentTimeMillis()
                practiceTickJob?.cancel()
            }
        }
        val now = frozen ?: System.currentTimeMillis()
        val elapsed = ((now - started) / 1000).toInt()
        var window = ui.charts.windowSec
        if (elapsed >= window * 0.75f) window = (window * 1.5f).toInt()
        val hr = if (ui.useBelt) zip(practiceStore.t, practiceStore.hrCoospo) else zip(practiceStore.t, practiceStore.hrWellue)
        _practice.value = ui.copy(
            ui = next,
            charts = ui.charts.copy(
                windowSec = window,
                durationSec = if (ui.ui == SessionUi.Recording) elapsed else ui.charts.durationSec,
                spo2 = zip(practiceStore.t, practiceStore.spo2),
                hr = hr,
                rr = zip(practiceStore.t, practiceStore.rr),
                ve = zip(practiceStore.t, practiceStore.flow),
                slm = zip(practiceStore.t, practiceStore.slm),
                o2 = zip(practiceStore.t, practiceStore.workO2),
                co2 = zip(practiceStore.t, practiceStore.co2),
                co2Raw = zip(practiceStore.t, practiceStore.co2Raw),
            ),
        )
    }

    private fun feedIgt(data: DeviceStatus) {
        val igtStatus = data.igt
        val now = System.currentTimeMillis()
        val step = IgtLogic.stepOf(igtStatus)
        val proto = selectedProtocol()
        val live = IgtLogic.isPrep(igtStatus) || IgtLogic.isRun(igtStatus) || IgtLogic.isTest(igtStatus)
        val review = IgtLogic.isReview(igtStatus)

        if (_overlay.value == Overlay.Igt || IgtLogic.isBusy(igtStatus)) {
            val prevStep = _igt.value.step
            val nextStep = if (_igt.value.editorOpen || _igt.value.testOpen) {
                if (step != IgtStep.Setup) step else _igt.value.step
            } else {
                step
            }
            val enteredReview = prevStep != IgtStep.Review && nextStep == IgtStep.Review
            _igt.value = _igt.value.copy(
                step = nextStep,
                windowSec = if (review || _igt.value.showFullSessionChart) {
                    IgtLogic.protocolWindowSec(proto, igtStatus)
                } else {
                    IgtLogic.liveChartWindowSec(proto, igtStatus)
                },
                confirmFillTimeout = igtStatus.fillTimeoutPending,
                showFullSessionChart = _igt.value.showFullSessionChart || enteredReview,
            )
        }

        if (live || review) {
            trackIgtMonitor(igtStatus, data.sensors, now)
        }
        if (!live && !review) return

        if (igtChartOrigin == null) {
            igtChartOrigin = now
        }
        trackIgtPhaseMarkers(igtStatus, now)

        val s = data.sensors
        igtHistory.t += now
        igtHistory.o2 += TrendStore.finiteOrNull(s.hypoxiaO2.ok, s.hypoxiaO2.percent)
        igtHistory.pressure += TrendStore.finiteOrNull(s.hypoxiaPressure.ok, s.hypoxiaPressure.hpa)
        igtHistory.flow += TrendStore.finiteOrNull(s.flow.ok, s.flow.veLpm)
        igtHistory.slm += TrendStore.finiteOrNull(s.flow.ok, s.flow.slm)
        igtHistory.workO2 += TrendStore.finiteOrNull(s.workO2.ok, s.workO2.percent)
        igtHistory.workPressure += TrendStore.finiteOrNull(s.workPressure.ok, s.workPressure.hpa)
        igtHistory.co2 += TrendStore.finiteOrNull(s.co2.ok, Formatters.co2Percent(s.co2))
        igtHistory.spo2 += if (s.wellue.ok && s.wellue.contact) TrendStore.saneSpo2(s.wellue.spo2) else null
        igtHistory.hrCoospo += if (s.coospo.ok && s.coospo.contact) TrendStore.saneHr(s.coospo.bpm)
        else if (s.wellue.ok && s.wellue.contact) TrendStore.saneHr(s.wellue.hr) else null
        igtHistory.rr += if (s.coospo.ok && s.coospo.contact) TrendStore.saneRr(s.coospo.rrMs) else null
        publishIgtCharts(_igt.value, igtStatus)
    }

    private fun resetIgtSessionTracking(sessionName: String? = null) {
        igtHistory.clear()
        igtChartOrigin = null
        igtPhaseMarkers.clear()
        igtLastPhaseKey = null
        igtMonitor.reset()
        val proto = selectedProtocol()
        igtMonitor.beginSession(
            sessionName ?: proto.name,
            proto.hypoxiaMin * 60,
            proto.normoxiaMin * 60,
            proto.cycles,
            System.currentTimeMillis(),
        )
        _igtMonitorReport.value = igtMonitor.snapshot()
    }

    private fun trackIgtMonitor(igt: IgtStatus, sensors: Sensors, now: Long) {
        igtMonitor.onStatus(igt, sensors, now)
        _igtMonitorReport.value = igtMonitor.snapshot()
    }

    private fun trackIgtPhaseMarkers(igt: IgtStatus, now: Long) {
        if (igt.state != "hypoxia" && igt.state != "normoxia") return
        val key = IgtMonitor.phaseKey(igt)
        if (key == igtLastPhaseKey) return
        igtLastPhaseKey = key
        val color = if (igt.state == "normoxia") NormoxiaGreen else Orange
        igtPhaseMarkers += ChartMarker(now, color)
    }

    private fun publishIgtCharts(ui: IgtUi, igtStatus: IgtStatus) {
        val origin = igtChartOrigin ?: return
        val now = System.currentTimeMillis()
        val full = ui.showFullSessionChart || IgtLogic.isReview(igtStatus)
        val win = if (full) IgtLogic.protocolWindowSec(selectedProtocol(), igtStatus) else ui.windowSec
        fun slice(values: List<Float?>) =
            igtHistory.windowScroll(values, origin, win, now, fullSession = full)
        _igt.value = ui.copy(
            charts = SessionCharts(
                startedAt = origin,
                windowSec = win,
                chartOrigin = origin,
                scrollLive = !full,
                fullSession = full,
                phaseMarkers = igtPhaseMarkers.toList(),
                o2 = slice(igtHistory.o2),
                pressure = slice(igtHistory.pressure),
                ve = slice(igtHistory.flow),
                slm = slice(igtHistory.slm),
                workO2 = slice(igtHistory.workO2),
                workPressure = slice(igtHistory.workPressure),
                co2 = slice(igtHistory.co2),
                spo2 = slice(igtHistory.spo2),
                hr = slice(igtHistory.hrCoospo),
                rr = slice(igtHistory.rr),
            ),
        )
    }

    private suspend fun hydrateTrends() {
        try {
            var offset = 0
            var replace = true
            for (i in 0 until 32) {
                val page = repository.getTrends(offset, 240)
                if (page.sources != TrendSources()) _trendSources.value = page.sources
                history.applyPage(page, replace)
                replace = false
                offset += page.count
                if (page.count == 0 || offset >= page.total) break
            }
            publishCharts()
        } catch (e: Exception) {
            AppLogger.w("Trends hydrate: ${e.message}")
        }
    }

    private fun publishCharts() {
        val win = _chartWindowSec.value
        _charts.value = ChartSnapshot(
            now = System.currentTimeMillis(),
            o2 = history.window(history.o2, win),
            workO2 = history.window(history.workO2, win),
            flow = history.window(history.flow, win),
            slm = history.window(history.slm, win),
            pressure = history.window(history.pressure, win),
            workPressure = history.window(history.workPressure, win),
            co2 = history.window(history.co2, win),
            co2Raw = history.window(history.co2Raw, win),
            spo2 = history.window(history.spo2, win),
            hrWellue = history.window(history.hrWellue, win),
            hrCoospo = history.window(history.hrCoospo, win),
            rr = history.window(history.rr, win),
        )
    }

    private fun enableSessionSources(extra: TrendSources) {
        viewModelScope.launch {
            if (sessionSourcesSaved == null) sessionSourcesSaved = _trendSources.value
            val next = _trendSources.value.merged(extra)
            if (next == _trendSources.value) return@launch
            if (transport.value == TransportType.Wifi) {
                runCatching { repository.postTrendSources(next) }
            }
            _trendSources.value = next
        }
    }

    private fun restoreSessionSources() {
        val saved = sessionSourcesSaved ?: return
        sessionSourcesSaved = null
        viewModelScope.launch {
            if (transport.value == TransportType.Wifi) {
                runCatching { repository.postTrendSources(saved) }
            }
            _trendSources.value = saved
        }
    }

    private fun ensureIgtSources() {
        if (igtSourcesOn) return
        igtSourcesOn = true
        enableSessionSources(SessionSources.IGT.merged(_trendSources.value))
    }

    private fun onCuConnection(state: ConnectionState) {
        if (transport.value != TransportType.ValveCu) {
            cuLinkWasUp = false
            igtGattDropOpen = false
            return
        }
        when (state) {
            ConnectionState.Connected -> {
                if (igtGattDropOpen) {
                    igtMonitor.noteLinkRecovered(System.currentTimeMillis())
                    _igtMonitorReport.value = igtMonitor.snapshot()
                    igtGattDropOpen = false
                    AppLogger.i("CU GATT restored, IGT continues")
                }
                cuLinkWasUp = true
                if (_lastError.value.startsWith("восстановление BLE")) {
                    _lastError.value = ""
                }
            }
            ConnectionState.Connecting, ConnectionState.Disconnected, ConnectionState.Scanning -> {
                if (cuLinkWasUp && !igtGattDropOpen) {
                    val igt = status.value.igt
                    if (IgtLogic.isBusy(igt)) {
                        igtMonitor.noteLinkDrop(System.currentTimeMillis(), IgtMonitor.phaseLabel(igt))
                        _igtMonitorReport.value = igtMonitor.snapshot()
                        igtGattDropOpen = true
                        _lastError.value = "восстановление BLE…"
                        AppLogger.w("CU GATT drop recorded, IGT not aborted")
                    }
                }
                cuLinkWasUp = false
            }
        }
    }

    private fun refreshLink() {
        val polling = pollJob?.isActive == true && transport.value != TransportType.None
        val now = System.currentTimeMillis()
        val cuRestore = transport.value == TransportType.ValveCu &&
            connectionState.value == ConnectionState.Connecting &&
            _lastOkAt.value > 0L
        _linkState.value = when {
            !polling -> LinkState.Idle
            cuRestore -> LinkState.Stale
            _lastOkAt.value == 0L -> LinkState.Offline
            now - _lastOkAt.value > 7000 -> LinkState.Offline
            now - _lastOkAt.value > 2500 -> LinkState.Stale
            else -> LinkState.Live
        }
    }

    private fun elapsedSec(startedAt: Long?): Int {
        if (startedAt == null) return 0
        return ((System.currentTimeMillis() - startedAt) / 1000).toInt()
    }

    private fun zip(t: List<Long>, values: List<Float?>): List<ChartPoint> {
        val n = minOf(t.size, values.size)
        return (0 until n).map { ChartPoint(t[it], values[it]) }
    }
}
