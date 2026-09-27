package com.oxypulse.esp32tester.ble

import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.Handler
import android.os.Looper
import com.oxypulse.esp32tester.util.AppLogger
import com.welie.blessed.BluetoothCentralManager
import com.welie.blessed.BluetoothCentralManagerCallback
import com.welie.blessed.BluetoothPeripheral
import com.welie.blessed.BluetoothPeripheralCallback
import com.welie.blessed.HciStatus
import com.welie.blessed.ScanFailure
import com.welie.blessed.ScanMode
import dagger.hilt.android.qualifiers.ApplicationContext
import java.util.concurrent.CopyOnWriteArrayList
import javax.inject.Inject
import javax.inject.Singleton

interface BleDiscoveryListener {
    fun onDiscovered(peripheral: BluetoothPeripheral, scanResult: ScanResult)
}

interface BleConnectionListener {
    fun onConnected(peripheral: BluetoothPeripheral)
    fun onConnectionFailed(peripheral: BluetoothPeripheral, status: HciStatus)
    fun onDisconnected(peripheral: BluetoothPeripheral, status: HciStatus)
}

@Singleton
class BleHub @Inject constructor(
    @ApplicationContext context: Context,
) {
    private val handler = Handler(Looper.getMainLooper())
    private val discovery = CopyOnWriteArrayList<BleDiscoveryListener>()
    private val connections = CopyOnWriteArrayList<BleConnectionListener>()

    private val centralCallback = object : BluetoothCentralManagerCallback() {
        override fun onDiscovered(peripheral: BluetoothPeripheral, scanResult: ScanResult) {
            discovery.forEach { it.onDiscovered(peripheral, scanResult) }
        }

        override fun onConnected(peripheral: BluetoothPeripheral) {
            AppLogger.i("BLE hub connected: ${peripheral.name} ${peripheral.address}")
            connections.forEach { it.onConnected(peripheral) }
        }

        override fun onConnectionFailed(peripheral: BluetoothPeripheral, status: HciStatus) {
            AppLogger.e("BLE hub connect failed ${peripheral.address}: $status")
            connections.forEach { it.onConnectionFailed(peripheral, status) }
        }

        override fun onDisconnected(peripheral: BluetoothPeripheral, status: HciStatus) {
            AppLogger.w("BLE hub disconnected ${peripheral.address}: $status")
            connections.forEach { it.onDisconnected(peripheral, status) }
        }

        override fun onScanFailed(scanFailure: ScanFailure) {
            scanning = false
            AppLogger.e("BLE hub scan failed: $scanFailure")
        }
    }

    val central = BluetoothCentralManager(context, centralCallback, handler)
    private var scanning = false

    val isScanning: Boolean
        get() = scanning

    init {
        runCatching { central.setScanMode(ScanMode.LOW_LATENCY) }
    }

    fun addDiscoveryListener(listener: BleDiscoveryListener) {
        if (!discovery.contains(listener)) discovery += listener
    }

    fun removeDiscoveryListener(listener: BleDiscoveryListener) {
        discovery.remove(listener)
    }

    fun addConnectionListener(listener: BleConnectionListener) {
        if (!connections.contains(listener)) connections += listener
    }

    fun removeConnectionListener(listener: BleConnectionListener) {
        connections.remove(listener)
    }

    fun getPeripheral(address: String): BluetoothPeripheral = central.getPeripheral(address)

    fun connect(peripheral: BluetoothPeripheral, callback: BluetoothPeripheralCallback) {
        central.connect(peripheral, callback)
    }

    fun autoConnect(peripheral: BluetoothPeripheral, callback: BluetoothPeripheralCallback) {
        central.autoConnect(peripheral, callback)
    }

    fun cancel(peripheral: BluetoothPeripheral) {
        central.cancelConnection(peripheral)
    }

    fun startScan(filters: List<ScanFilter>? = null) {
        if (scanning) return
        scanning = true
        if (filters.isNullOrEmpty()) {
            central.scanForPeripherals()
        } else {
            central.scanForPeripheralsUsingFilters(filters)
        }
        AppLogger.i("BLE hub scan started filters=${filters?.size ?: 0}")
    }

    fun restartScan(filters: List<ScanFilter>? = null) {
        if (scanning) {
            scanning = false
            central.stopScan()
        }
        startScan(filters)
    }

    fun stopScan() {
        if (!scanning) return
        scanning = false
        central.stopScan()
    }
}
