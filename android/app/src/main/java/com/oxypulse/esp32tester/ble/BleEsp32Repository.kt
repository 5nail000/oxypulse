package com.oxypulse.esp32tester.ble

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Handler
import android.os.Looper
import com.oxypulse.esp32tester.api.BleConstants
import com.oxypulse.esp32tester.api.Esp32Api
import com.oxypulse.esp32tester.api.StatusParser
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.ScannedDevice
import com.oxypulse.esp32tester.util.AppLogger
import com.welie.blessed.BluetoothCentralManager
import com.welie.blessed.BluetoothCentralManagerCallback
import com.welie.blessed.BluetoothPeripheral
import com.welie.blessed.BluetoothPeripheralCallback
import com.welie.blessed.GattStatus
import com.welie.blessed.HciStatus
import com.welie.blessed.WriteType
import dagger.hilt.android.qualifiers.ApplicationContext
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext

@Singleton
class BleEsp32Repository @Inject constructor(
    @ApplicationContext context: Context,
) : Esp32Api {

    private val handler = Handler(Looper.getMainLooper())
    private val discovered = ConcurrentHashMap<String, ScannedDevice>()
    private var connectedPeripheral: BluetoothPeripheral? = null
    private var lastStatusJson: String = ""
    private var notifyReady = false

    private val _connectionState = MutableStateFlow(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _scannedDevices = MutableStateFlow<List<ScannedDevice>>(emptyList())
    val scannedDevices: StateFlow<List<ScannedDevice>> = _scannedDevices.asStateFlow()

    private val serviceUuid = UUID.fromString(BleConstants.SERVICE_UUID)
    private val statusUuid = UUID.fromString(BleConstants.STATUS_CHAR_UUID)
    private val cmdUuid = UUID.fromString(BleConstants.CMD_CHAR_UUID)

    private val centralCallback = object : BluetoothCentralManagerCallback() {
        override fun onDiscovered(peripheral: BluetoothPeripheral, scanResult: ScanResult) {
            val name = peripheral.name ?: scanResult.scanRecord?.deviceName ?: return
            if (!name.startsWith(BleConstants.DEVICE_NAME_PREFIX)) return
            discovered[peripheral.address] = ScannedDevice(name, peripheral.address)
            _scannedDevices.value = discovered.values.sortedBy { it.name }
        }

        override fun onConnected(peripheral: BluetoothPeripheral) {
            AppLogger.i("BLE connected: ${peripheral.name}")
            connectedPeripheral = peripheral
            _connectionState.value = ConnectionState.Connected
        }

        override fun onConnectionFailed(peripheral: BluetoothPeripheral, status: HciStatus) {
            AppLogger.e("BLE connect failed: $status")
            resetConnection()
        }

        override fun onDisconnected(peripheral: BluetoothPeripheral, status: HciStatus) {
            AppLogger.w("BLE disconnected: $status")
            resetConnection()
        }
    }

    private val peripheralCallback = object : BluetoothPeripheralCallback() {
        override fun onServicesDiscovered(peripheral: BluetoothPeripheral) {
            notifyReady = peripheral.startNotify(serviceUuid, statusUuid)
            peripheral.readCharacteristic(serviceUuid, statusUuid)
            AppLogger.i("BLE services discovered, notify=$notifyReady")
        }

        override fun onCharacteristicUpdate(
            peripheral: BluetoothPeripheral,
            value: ByteArray,
            characteristic: BluetoothGattCharacteristic,
            status: GattStatus,
        ) {
            if (status != GattStatus.SUCCESS) return
            if (characteristic.uuid == statusUuid) {
                lastStatusJson = value.toString(Charsets.UTF_8)
            }
        }
    }

    private val centralManager = BluetoothCentralManager(context, centralCallback, handler)

    fun startScan() {
        discovered.clear()
        _scannedDevices.value = emptyList()
        _connectionState.value = ConnectionState.Scanning
        centralManager.scanForPeripheralsWithServices(setOf(serviceUuid))
        AppLogger.i("BLE scan started")
    }

    fun stopScan() {
        centralManager.stopScan()
        if (_connectionState.value == ConnectionState.Scanning) {
            _connectionState.value = ConnectionState.Disconnected
        }
    }

    fun connect(address: String) {
        stopScan()
        _connectionState.value = ConnectionState.Connecting
        val peripheral = centralManager.getPeripheral(address)
        connectedPeripheral = peripheral
        centralManager.connect(peripheral, peripheralCallback)
    }

    fun disconnect() {
        connectedPeripheral?.let { centralManager.cancelConnection(it) }
        resetConnection()
    }

    private fun resetConnection() {
        connectedPeripheral = null
        lastStatusJson = ""
        notifyReady = false
        _connectionState.value = ConnectionState.Disconnected
    }

    override suspend fun getStatus(): DeviceStatus = withContext(Dispatchers.IO) {
        if (lastStatusJson.isBlank()) {
            withContext(Dispatchers.Main) {
                connectedPeripheral?.readCharacteristic(serviceUuid, statusUuid)
            }
            delay(400)
        }
        if (lastStatusJson.isBlank()) {
            throw IllegalStateException("Нет BLE status")
        }
        StatusParser.parse(lastStatusJson)
    }

    override suspend fun sendCommand(json: String) {
        withContext(Dispatchers.Main) {
            val peripheral = connectedPeripheral
                ?: throw IllegalStateException("BLE не подключён")
            AppLogger.d("BLE cmd: $json")
            val ok = peripheral.writeCharacteristic(
                serviceUuid,
                cmdUuid,
                json.toByteArray(Charsets.UTF_8),
                WriteType.WITHOUT_RESPONSE,
            )
            if (!ok) {
                throw IllegalStateException("BLE write failed")
            }
        }
    }
}
