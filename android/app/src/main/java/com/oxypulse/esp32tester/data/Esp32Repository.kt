package com.oxypulse.esp32tester.data

import android.content.Context
import com.oxypulse.esp32tester.api.CommandBuilder
import com.oxypulse.esp32tester.api.Esp32Api
import com.oxypulse.esp32tester.api.HttpEsp32Api
import com.oxypulse.esp32tester.ble.CuBleClient
import com.oxypulse.esp32tester.ble.WearableBleManager
import com.oxypulse.esp32tester.ble.WearableBleService
import com.oxypulse.esp32tester.domain.BridgeStatus
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.HrvSnapshot
import com.oxypulse.esp32tester.domain.TrendPage
import com.oxypulse.esp32tester.domain.TrendSources
import com.oxypulse.esp32tester.domain.TransportType
import com.oxypulse.esp32tester.domain.WifiScanResult
import com.oxypulse.esp32tester.util.AppLogger
import dagger.hilt.android.qualifiers.ApplicationContext
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

@Singleton
class Esp32Repository @Inject constructor(
    @ApplicationContext private val context: Context,
    private val cu: CuBleClient,
    private val wearables: WearableBleManager,
) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private var httpApi: HttpEsp32Api? = null
    private var keepAliveJob: Job? = null

    private val _transport = MutableStateFlow(TransportType.None)
    val transport: StateFlow<TransportType> = _transport.asStateFlow()

    private val _status = MutableStateFlow(DeviceStatus())
    val status: StateFlow<DeviceStatus> = _status.asStateFlow()

    val connectionState: StateFlow<ConnectionState> = cu.connectionState
    val scannedDevices = cu.scannedDevices
    val wearableNearby = wearables.nearby
    val wearableScan = wearables.scanUi

    val httpHost: String?
        get() = httpApi?.host

    fun connectWifi(host: String) {
        stopCuSession()
        val url = if (host.startsWith("http")) host else "http://$host"
        httpApi = HttpEsp32Api(url)
        _transport.value = TransportType.Wifi
    }

    suspend fun verifyWifiConnection() {
        _status.value = requireApi().getStatus()
    }

    fun connectCuStartScan() {
        wearables.start(allowConnect = false)
        cu.startScan()
    }

    fun stopCuScan() = cu.stopScan()

    suspend fun connectCu(address: String) {
        cu.connect(address)
        _transport.value = TransportType.ValveCu
        wearables.start(allowConnect = true)
        runCatching { WearableBleService.start(context) }
            .onFailure { AppLogger.e("FGS: ${it.message}") }
        startKeepAlive()
        AppLogger.i("CU session started $address")
    }

    fun findWearables() {
        runCatching { WearableBleService.start(context) }
            .onFailure { AppLogger.e("FGS: ${it.message}") }
        wearables.findWearables()
    }

    fun connectWearable(address: String, wellue: Boolean) = wearables.connectAs(address, wellue)

    fun reconnectWearable(wellue: Boolean) = wearables.reconnectLast(wellue)

    fun lastWellueAddress(): String = wearables.lastWellueAddress()

    fun lastCoospoAddress(): String = wearables.lastCoospoAddress()

    fun disconnect() {
        when (_transport.value) {
            TransportType.ValveCu, TransportType.Ble -> stopCuSession()
            TransportType.Wifi -> httpApi = null
            TransportType.None -> Unit
        }
        _transport.value = TransportType.None
        _status.value = DeviceStatus()
    }

    suspend fun refreshStatus() {
        val raw = requireApi().getStatus()
        _status.value = if (_transport.value == TransportType.ValveCu) mergeWearables(raw) else raw
    }

    suspend fun sendCommand(json: String) {
        requireApi().sendCommand(json)
        refreshStatus()
    }

    suspend fun getTrends(offset: Int, limit: Int): TrendPage =
        requireApi().getTrends(offset, limit)

    suspend fun postTrendSources(sources: TrendSources) {
        if (_transport.value == TransportType.ValveCu) return
        requireApi().postTrendSources(sources)
    }

    suspend fun startHrv(durationSec: Int) {
        if (_transport.value == TransportType.ValveCu) {
            wearables.startHrv(durationSec)
        } else {
            requireApi().startHrv(durationSec)
        }
    }

    suspend fun stopHrv() {
        if (_transport.value == TransportType.ValveCu) {
            wearables.stopHrv()
        } else {
            requireApi().stopHrv()
        }
    }

    suspend fun getHrv(): HrvSnapshot {
        return if (_transport.value == TransportType.ValveCu) {
            wearables.getHrv()
        } else {
            requireApi().getHrv()
        }
    }

    suspend fun igtPost(path: String, body: String? = null) = requireApi().igtPost(path, body)

    suspend fun startWifiScan() = requireApi().startWifiScan()

    suspend fun getWifiScan(): WifiScanResult = requireApi().getWifiScan()

    suspend fun saveWifi(ssid: String, password: String) = requireApi().saveWifi(ssid, password)

    suspend fun forgetWifi() = requireApi().forgetWifi()

    private fun requireApi(): Esp32Api = when (_transport.value) {
        TransportType.Wifi -> httpApi ?: throw IllegalStateException("WiFi не настроен")
        TransportType.ValveCu, TransportType.Ble -> cu
        TransportType.None -> throw IllegalStateException("Нет подключения")
    }

    private fun mergeWearables(raw: DeviceStatus): DeviceStatus {
        return raw.copy(
            sensors = raw.sensors.copy(
                wellue = wearables.wellue.value,
                coospo = wearables.coospo.value,
                bridge = BridgeStatus(ok = true),
            ),
        )
    }

    private fun startKeepAlive() {
        keepAliveJob?.cancel()
        keepAliveJob = scope.launch {
            while (isActive && _transport.value == TransportType.ValveCu) {
                val w = wearables.wellue.value
                val c = wearables.coospo.value
                runCatching {
                    cu.sendCommand(
                        CommandBuilder.phys(
                            spo2 = w.spo2,
                            hr = if (c.ok && c.bpm > 0) c.bpm else w.hr,
                            belt = c.connected,
                            contact = w.contact || c.contact,
                        ),
                    )
                }.onFailure { AppLogger.w("CU phys: ${it.message}") }
                delay(1000)
            }
        }
    }

    private fun stopCuSession() {
        keepAliveJob?.cancel()
        keepAliveJob = null
        cu.stopScan()
        cu.disconnect()
        WearableBleService.stop(context)
        httpApi = null
    }
}
