package com.oxypulse.esp32tester.ble

import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.le.ScanResult
import android.os.Handler
import android.os.Looper
import com.oxypulse.esp32tester.api.Esp32Api
import com.oxypulse.esp32tester.api.StatusParser
import com.oxypulse.esp32tester.api.ValveCuConstants
import com.oxypulse.esp32tester.api.CommandBuilder
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.ScannedDevice
import com.oxypulse.esp32tester.util.AppLogger
import com.welie.blessed.BluetoothPeripheral
import com.welie.blessed.BluetoothPeripheralCallback
import com.welie.blessed.GattStatus
import com.welie.blessed.HciStatus
import com.welie.blessed.WriteType
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import javax.inject.Inject
import javax.inject.Singleton
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout

@Singleton
class CuBleClient @Inject constructor(
    private val hub: BleHub,
) : Esp32Api, BleDiscoveryListener, BleConnectionListener {

    private val handler = Handler(Looper.getMainLooper())
    private val discovered = ConcurrentHashMap<String, ScannedDevice>()
    private var peripheral: BluetoothPeripheral? = null
    private var lastAddress: String = ""
    private var lastLiveJson: String = ""
    private var connectAck: CompletableDeferred<Boolean>? = null
    private var scanning = false
    private var wantSession = false

    private val reconnectRunnable = Runnable { attemptReconnect() }

    private val _connectionState = MutableStateFlow(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _scannedDevices = MutableStateFlow<List<ScannedDevice>>(emptyList())
    val scannedDevices: StateFlow<List<ScannedDevice>> = _scannedDevices.asStateFlow()

    private val serviceUuid = UUID.fromString(ValveCuConstants.SERVICE_UUID)
    private val liveUuid = UUID.fromString(ValveCuConstants.LIVE_CHAR_UUID)
    private val cmdUuid = UUID.fromString(ValveCuConstants.CMD_CHAR_UUID)

    private val peripheralCallback = object : BluetoothPeripheralCallback() {
        override fun onServicesDiscovered(peripheral: BluetoothPeripheral) {
            runCatching { peripheral.requestMtu(ValveCuConstants.PREFERRED_MTU) }
            peripheral.startNotify(serviceUuid, liveUuid)
            peripheral.readCharacteristic(serviceUuid, liveUuid)
            AppLogger.i("CU services discovered ${peripheral.address}")
        }

        override fun onMtuChanged(peripheral: BluetoothPeripheral, mtu: Int, status: GattStatus) {
            AppLogger.i("CU MTU=$mtu status=$status")
        }

        override fun onCharacteristicUpdate(
            peripheral: BluetoothPeripheral,
            value: ByteArray,
            characteristic: BluetoothGattCharacteristic,
            status: GattStatus,
        ) {
            if (status != GattStatus.SUCCESS) return
            if (characteristic.uuid == liveUuid) {
                lastLiveJson = value.toString(Charsets.UTF_8)
            }
        }
    }

    init {
        hub.addDiscoveryListener(this)
        hub.addConnectionListener(this)
    }

    override fun onDiscovered(peripheral: BluetoothPeripheral, scanResult: ScanResult) {
        if (!scanning) return
        val rawName = peripheral.name ?: scanResult.scanRecord?.deviceName
        val hasService = scanResult.scanRecord?.serviceUuids?.any {
            it.uuid == serviceUuid
        } == true
        if (rawName?.startsWith(ValveCuConstants.DEVICE_NAME) != true && !hasService) return
        val name = rawName?.takeIf { it.isNotBlank() } ?: ValveCuConstants.DEVICE_NAME
        discovered[peripheral.address] = ScannedDevice(name, peripheral.address)
        _scannedDevices.value = discovered.values.sortedBy { it.name }
    }

    override fun onConnected(peripheral: BluetoothPeripheral) {
        if (this.peripheral?.address != peripheral.address) return
        _connectionState.value = ConnectionState.Connected
        connectAck?.takeIf { !it.isCompleted }?.complete(true)
    }

    override fun onConnectionFailed(peripheral: BluetoothPeripheral, status: HciStatus) {
        if (this.peripheral?.address != peripheral.address &&
            (lastAddress.isBlank() || !peripheral.address.equals(lastAddress, true))
        ) {
            return
        }
        AppLogger.e("CU connect failed: $status")
        if (!wantSession) {
            resetConnection()
            connectAck?.takeIf { !it.isCompleted }?.complete(false)
            return
        }
        _connectionState.value = ConnectionState.Connecting
        scheduleReconnect()
    }

    override fun onDisconnected(peripheral: BluetoothPeripheral, status: HciStatus) {
        if (this.peripheral?.address != peripheral.address &&
            (lastAddress.isBlank() || !peripheral.address.equals(lastAddress, true))
        ) {
            return
        }
        if (!wantSession) {
            AppLogger.i("CU disconnected (session closed)")
            resetConnection()
            return
        }
        AppLogger.w("CU GATT drop, restoring: $status")
        _connectionState.value = ConnectionState.Connecting
        scheduleReconnect()
    }

    fun startScan() {
        discovered.clear()
        _scannedDevices.value = emptyList()
        scanning = true
        _connectionState.value = ConnectionState.Scanning
        hub.restartScan(null)
        AppLogger.i("CU scan started")
    }

    fun stopScan() {
        if (!scanning) return
        scanning = false
        hub.stopScan()
        if (_connectionState.value == ConnectionState.Scanning) {
            _connectionState.value = ConnectionState.Disconnected
        }
    }

    suspend fun connect(address: String) {
        stopScan()
        withContext(Dispatchers.Main) {
            wantSession = true
            lastAddress = address
            handler.removeCallbacks(reconnectRunnable)
            _connectionState.value = ConnectionState.Connecting
            lastLiveJson = ""
            val p = hub.getPeripheral(address)
            peripheral = p
            val ack = CompletableDeferred<Boolean>()
            connectAck = ack
            hub.connect(p, peripheralCallback)
        }
        val ok = try {
            withTimeout(20_000) { connectAck?.await() == true }
        } catch (_: Exception) {
            false
        }
        if (!ok) {
            disconnect()
            throw IllegalStateException("ЦУ не ответил")
        }
        handler.postDelayed({
            peripheral?.readCharacteristic(serviceUuid, liveUuid)
        }, 400)
    }

    fun disconnect() {
        wantSession = false
        handler.removeCallbacks(reconnectRunnable)
        peripheral?.let { hub.cancel(it) }
        resetConnection()
    }

    private fun resetConnection() {
        handler.removeCallbacks(reconnectRunnable)
        peripheral = null
        lastLiveJson = ""
        _connectionState.value = ConnectionState.Disconnected
    }

    private fun scheduleReconnect() {
        if (!wantSession || lastAddress.isBlank()) return
        handler.removeCallbacks(reconnectRunnable)
        handler.postDelayed(reconnectRunnable, 800)
    }

    private fun attemptReconnect() {
        if (!wantSession || lastAddress.isBlank()) return
        val p = hub.getPeripheral(lastAddress)
        peripheral = p
        _connectionState.value = ConnectionState.Connecting
        AppLogger.i("CU restoring GATT $lastAddress")
        runCatching { hub.autoConnect(p, peripheralCallback) }
            .onFailure {
                AppLogger.e("CU restore: ${it.message}")
                handler.postDelayed(reconnectRunnable, 2000)
            }
    }

    override suspend fun getStatus(): DeviceStatus = withContext(Dispatchers.IO) {
        if (lastLiveJson.isBlank()) {
            withContext(Dispatchers.Main) {
                peripheral?.readCharacteristic(serviceUuid, liveUuid)
            }
            kotlinx.coroutines.delay(400)
        }
        if (lastLiveJson.isBlank()) {
            throw IllegalStateException("Нет LIVE с ЦУ")
        }
        StatusParser.parse(lastLiveJson)
    }

    override suspend fun sendCommand(json: String) {
        withContext(Dispatchers.Main) {
            val p = peripheral ?: throw IllegalStateException("ЦУ не подключён")
            AppLogger.d("CU cmd: $json")
            val ok = p.writeCharacteristic(
                serviceUuid,
                cmdUuid,
                json.toByteArray(Charsets.UTF_8),
                WriteType.WITHOUT_RESPONSE,
            )
            if (!ok) throw IllegalStateException("CU write failed")
        }
    }

    override suspend fun igtPost(path: String, body: String?) {
        val json = when {
            path.contains("/test/start") -> CommandBuilder.igtCuCommand("igt_test", body)
            path.contains("/test/stop") -> CommandBuilder.igtCuCommand("igt_test_stop")
            path.contains("skip-recirculate") -> CommandBuilder.igtCuCommand("igt_skip")
            path.contains("fill-continue") -> CommandBuilder.igtCuCommand("igt_fill_continue")
            path.contains("fill-abort") -> CommandBuilder.igtCuCommand("igt_fill_abort")
            path.endsWith("/prep") -> CommandBuilder.igtCuCommand("igt_prep", body)
            path.endsWith("/start") -> CommandBuilder.igtCuCommand("igt_start", body)
            path.endsWith("/abort") -> CommandBuilder.igtCuCommand("igt_abort")
            path.endsWith("/ack") -> CommandBuilder.igtCuCommand("igt_ack")
            else -> throw IllegalStateException("Неизвестный путь ИГТ: $path")
        }
        sendCommand(json)
    }
}
