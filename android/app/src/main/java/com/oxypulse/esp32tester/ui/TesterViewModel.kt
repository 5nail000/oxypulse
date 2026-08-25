package com.oxypulse.esp32tester.ui

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.oxypulse.esp32tester.api.CommandBuilder
import com.oxypulse.esp32tester.api.PinWhitelist
import com.oxypulse.esp32tester.data.Esp32Repository
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.ScannedDevice
import com.oxypulse.esp32tester.domain.TransportType
import com.oxypulse.esp32tester.util.AppLogger
import dagger.hilt.android.lifecycle.HiltViewModel
import javax.inject.Inject
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

@HiltViewModel
class TesterViewModel @Inject constructor(
    private val repository: Esp32Repository,
) : ViewModel() {

    val transport = repository.transport
    val status = repository.status
    val connectionState = repository.connectionState
    val scannedDevices: StateFlow<List<ScannedDevice>> = repository.scannedDevices
        .stateIn(viewModelScope, SharingStarted.WhileSubscribed(5000), emptyList())

    private val _message = MutableStateFlow<String?>(null)
    val message: StateFlow<String?> = _message.asStateFlow()

    private var pollJob: Job? = null

    fun connectWifi(host: String) {
        viewModelScope.launch {
            try {
                repository.connectWifi(host)
                repository.verifyWifiConnection()
                startPolling()
                _message.value = "WiFi подключено"
                AppLogger.i("WiFi connected: $host")
            } catch (e: Exception) {
                repository.disconnect()
                _message.value = "WiFi ошибка: ${e.message}"
                AppLogger.e("WiFi error: ${e.message}")
            }
        }
    }

    fun startBleScan() {
        repository.connectBleStartScan()
    }

    fun stopBleScan() {
        repository.stopBleScan()
    }

    fun connectBle(address: String) {
        viewModelScope.launch {
            try {
                repository.connectBle(address)
                delay(500)
                startPolling()
                _message.value = "BLE подключено"
            } catch (e: Exception) {
                _message.value = "BLE ошибка: ${e.message}"
            }
        }
    }

    fun disconnect() {
        pollJob?.cancel()
        repository.disconnect()
        _message.value = "Отключено"
    }

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
        viewModelScope.launch {
            try {
                repository.sendCommand(CommandBuilder.gpioTap(pin))
            } catch (e: Exception) {
                _message.value = "GPIO tap: ${e.message}"
            }
        }
    }

    fun applyServo(pin: Int, angle: Int, auto: Boolean) {
        viewModelScope.launch {
            try {
                val cmd = when {
                    auto -> CommandBuilder.servoAuto(pin, true)
                    status.value.servos.any { it.pin == pin && it.auto } ->
                        CommandBuilder.servoAuto(pin, false)
                    else -> CommandBuilder.servoSet(pin, angle)
                }
                repository.sendCommand(cmd)
            } catch (e: Exception) {
                _message.value = "Servo: ${e.message}"
            }
        }
    }

    fun availablePins(status: DeviceStatus): List<Int> =
        status.whitelist.ifEmpty { PinWhitelist.DEFAULT }

    private fun startPolling() {
        pollJob?.cancel()
        pollJob = viewModelScope.launch {
            while (isActive) {
                try {
                    repository.refreshStatus()
                } catch (e: Exception) {
                    if (transport.value == TransportType.Wifi) {
                        _message.value = "Polling: ${e.message}"
                    }
                }
                delay(1000)
            }
        }
    }
}
