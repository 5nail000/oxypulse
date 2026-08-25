package com.oxypulse.esp32tester.data

import com.oxypulse.esp32tester.api.Esp32Api
import com.oxypulse.esp32tester.api.HttpEsp32Api
import com.oxypulse.esp32tester.ble.BleEsp32Repository
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.TransportType
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

@Singleton
class Esp32Repository @Inject constructor(
    private val bleRepository: BleEsp32Repository,
) {
    private var httpApi: HttpEsp32Api? = null

    private val _transport = MutableStateFlow(TransportType.None)
    val transport: StateFlow<TransportType> = _transport.asStateFlow()

    private val _status = MutableStateFlow(DeviceStatus())
    val status: StateFlow<DeviceStatus> = _status.asStateFlow()

    val connectionState: StateFlow<ConnectionState>
        get() = bleRepository.connectionState

    val scannedDevices = bleRepository.scannedDevices

    fun connectWifi(host: String) {
        val url = if (host.startsWith("http")) host else "http://$host"
        httpApi = HttpEsp32Api(url)
        _transport.value = TransportType.Wifi
    }

    suspend fun verifyWifiConnection() {
        val api = requireApi()
        _status.value = api.getStatus()
    }

    fun connectBleStartScan() {
        _transport.value = TransportType.Ble
        bleRepository.startScan()
    }

    fun stopBleScan() = bleRepository.stopScan()

    suspend fun connectBle(address: String) {
        _transport.value = TransportType.Ble
        bleRepository.connect(address)
    }

    fun disconnect() {
        when (_transport.value) {
            TransportType.Ble -> bleRepository.disconnect()
            TransportType.Wifi -> httpApi = null
            TransportType.None -> Unit
        }
        _transport.value = TransportType.None
        _status.value = DeviceStatus()
    }

    suspend fun refreshStatus() {
        _status.value = requireApi().getStatus()
    }

    suspend fun sendCommand(json: String) {
        requireApi().sendCommand(json)
        refreshStatus()
    }

    private fun requireApi(): Esp32Api = when (_transport.value) {
        TransportType.Wifi -> httpApi ?: throw IllegalStateException("WiFi не настроен")
        TransportType.Ble -> bleRepository
        TransportType.None -> throw IllegalStateException("Нет подключения")
    }
}
