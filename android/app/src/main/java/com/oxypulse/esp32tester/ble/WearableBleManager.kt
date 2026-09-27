package com.oxypulse.esp32tester.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.content.Context
import android.os.ParcelUuid
import com.oxypulse.esp32tester.api.WearableBleConstants
import com.oxypulse.esp32tester.data.AppPrefs
import com.oxypulse.esp32tester.domain.CoospoStatus
import com.oxypulse.esp32tester.domain.HrvSnapshot
import com.oxypulse.esp32tester.domain.NearbyBleDevice
import com.oxypulse.esp32tester.domain.WearableScanUi
import com.oxypulse.esp32tester.domain.WellueStatus
import com.oxypulse.esp32tester.util.AppLogger
import com.welie.blessed.BluetoothPeripheral
import com.welie.blessed.BluetoothPeripheralCallback
import com.welie.blessed.GattStatus
import com.welie.blessed.HciStatus
import com.welie.blessed.WriteType
import dagger.hilt.android.qualifiers.ApplicationContext
import java.util.UUID
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
class WearableBleManager @Inject constructor(
    @ApplicationContext private val context: Context,
    private val hub: BleHub,
    private val prefs: AppPrefs,
) : BleDiscoveryListener, BleConnectionListener {

    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main)
    private val oxyiiService = UUID.fromString(WearableBleConstants.OXYII_SERVICE)
    private val oxyiiWrite = UUID.fromString(WearableBleConstants.OXYII_WRITE)
    private val oxyiiNotify = UUID.fromString(WearableBleConstants.OXYII_NOTIFY)
    private val hrService = UUID.fromString(WearableBleConstants.HR_SERVICE)
    private val hrMeas = UUID.fromString(WearableBleConstants.HR_MEAS)

    private val _wellue = MutableStateFlow(WellueStatus())
    val wellue: StateFlow<WellueStatus> = _wellue.asStateFlow()

    private val _coospo = MutableStateFlow(CoospoStatus())
    val coospo: StateFlow<CoospoStatus> = _coospo.asStateFlow()

    private val _nearby = MutableStateFlow<List<NearbyBleDevice>>(emptyList())
    val nearby: StateFlow<List<NearbyBleDevice>> = _nearby.asStateFlow()

    private val _scanUi = MutableStateFlow(WearableScanUi())
    val scanUi: StateFlow<WearableScanUi> = _scanUi.asStateFlow()

    fun lastWellueAddress(): String = prefs.lastWellueAddress()
    fun lastCoospoAddress(): String = prefs.lastCoospoAddress()

    private var running = false
    private var welluePeripheral: BluetoothPeripheral? = null
    private var coospoPeripheral: BluetoothPeripheral? = null
    private var wellueSeq = 0
    private val wellueRx = ByteArray(WellueOxyii.RX_MAX)
    private val wellueRxLen = intArrayOf(0)
    private var wellueSampleAt = 0L
    private var coospoSampleAt = 0L
    private var pollJob: Job? = null
    private var pendingWellue: BluetoothPeripheral? = null
    private var pendingCoospo: BluetoothPeripheral? = null
    private var connecting = false
    private var allowConnect = false
    private var connectStartedAt = 0L
    private var lastScanKickAt = 0L
    private var openScan = false
    private var manualScanUntil = 0L
    private val seenAdv = HashSet<String>()
    private val nearbyByAddr = LinkedHashMap<String, NearbyBleDevice>()

    private val hrvLock = Any()
    private var hrvState = "idle"
    private var hrvDurationSec = 0
    private var hrvStartedAt = 0L
    private val hrvRr = mutableListOf<Int>()
    private var hrvError = ""

    private val wellueCallback = object : BluetoothPeripheralCallback() {
        override fun onServicesDiscovered(peripheral: BluetoothPeripheral) {
            val ok = peripheral.startNotify(oxyiiService, oxyiiNotify)
            AppLogger.i("Wellue notify=$ok ${peripheral.name}")
            if (!ok) {
                _wellue.value = _wellue.value.copy(connected = true, error = "нет OxyII характеристик")
            }
        }

        override fun onCharacteristicUpdate(
            peripheral: BluetoothPeripheral,
            value: ByteArray,
            characteristic: BluetoothGattCharacteristic,
            status: GattStatus,
        ) {
            if (status != GattStatus.SUCCESS || characteristic.uuid != oxyiiNotify) return
            val metrics = WellueOxyii.feedNotify(wellueRx, wellueRxLen, value) ?: return
            val contact = metrics.contact
            val spo2 = if (contact && metrics.spo2 in 50..100) metrics.spo2 else 0
            val hr = if (contact && metrics.hr in 25..240) metrics.hr else 0
            wellueSampleAt = System.currentTimeMillis()
            _wellue.value = _wellue.value.copy(
                connected = true,
                ok = contact && spo2 > 0,
                spo2 = spo2,
                hr = hr,
                battery = metrics.battery,
                motion = metrics.motion,
                contact = contact,
                error = "",
            )
        }
    }

    private val coospoCallback = object : BluetoothPeripheralCallback() {
        override fun onServicesDiscovered(peripheral: BluetoothPeripheral) {
            val ok = peripheral.startNotify(hrService, hrMeas)
            AppLogger.i("COOSPO notify=$ok ${peripheral.name}")
            if (!ok) {
                _coospo.value = _coospo.value.copy(connected = true, error = "нет Heart Rate service")
            }
        }

        override fun onCharacteristicUpdate(
            peripheral: BluetoothPeripheral,
            value: ByteArray,
            characteristic: BluetoothGattCharacteristic,
            status: GattStatus,
        ) {
            if (status != GattStatus.SUCCESS || characteristic.uuid != hrMeas) return
            val parsed = parseHrMeasurement(value) ?: return
            coospoSampleAt = System.currentTimeMillis()
            _coospo.value = _coospo.value.copy(
                connected = true,
                ok = parsed.bpm > 0,
                bpm = parsed.bpm,
                rrMs = parsed.rr.lastOrNull() ?: 0,
                contact = parsed.contact,
                error = "",
            )
            parsed.rr.forEach { feedHrv(it) }
        }
    }

    init {
        hub.addDiscoveryListener(this)
        hub.addConnectionListener(this)
    }

    fun start(allowConnect: Boolean = true) {
        this.allowConnect = allowConnect
        if (!running) {
            running = true
            hub.addDiscoveryListener(this)
            pollJob?.cancel()
            pollJob = scope.launch {
                while (isActive && running) {
                    tick()
                    delay(WearableBleConstants.WELLUE_POLL_MS)
                }
            }
            seenAdv.clear()
            openScan = false
            lastScanKickAt = 0L
            AppLogger.i("Wearables started connect=$allowConnect")
        } else {
            AppLogger.i("Wearables already running connect=$allowConnect")
        }
        if (allowConnect) {
            rememberKnownDevices()
            maybeConnectPending()
        }
        maybeScan()
    }

    fun stop() {
        running = false
        pollJob?.cancel()
        pollJob = null
        hub.stopScan()
        welluePeripheral?.let { hub.cancel(it) }
        coospoPeripheral?.let { hub.cancel(it) }
        welluePeripheral = null
        coospoPeripheral = null
        pendingWellue = null
        pendingCoospo = null
        connecting = false
        allowConnect = false
        openScan = false
        manualScanUntil = 0L
        connectStartedAt = 0L
        nearbyByAddr.clear()
        _nearby.value = emptyList()
        _scanUi.value = WearableScanUi()
        _wellue.value = WellueStatus()
        _coospo.value = CoospoStatus()
        stopHrv()
        AppLogger.i("Wearables stopped")
    }

    fun findWearables() {
        start(allowConnect = true)
        connecting = false
        connectStartedAt = 0L
        pendingWellue = null
        pendingCoospo = null
        seenAdv.clear()
        nearbyByAddr.clear()
        _nearby.value = emptyList()
        openScan = true
        val now = System.currentTimeMillis()
        lastScanKickAt = now
        manualScanUntil = now + WearableBleConstants.MANUAL_SCAN_MS
        _scanUi.value = WearableScanUi(
            scanning = true,
            remainSec = (WearableBleConstants.MANUAL_SCAN_MS / 1000).toInt(),
            hint = "скан BLE…",
        )
        if (_wellue.value.connected.not()) {
            _wellue.value = _wellue.value.copy(error = "поиск…")
        }
        if (_coospo.value.connected.not()) {
            _coospo.value = _coospo.value.copy(error = "поиск…")
        }
        rememberKnownDevices()
        hub.restartScan(null)
        AppLogger.i("Wearable manual scan ${WearableBleConstants.MANUAL_SCAN_MS}ms")
    }

    fun reconnectLast(wellue: Boolean) {
        start(allowConnect = true)
        val address = if (wellue) prefs.lastWellueAddress() else prefs.lastCoospoAddress()
        if (address.isBlank()) {
            AppLogger.w("No last ${if (wellue) "Wellue" else "COOSPO"} address")
            return
        }
        connectAs(address, wellue)
    }

    fun connectAs(address: String, wellue: Boolean) {
        if (address.isBlank()) return
        start(allowConnect = true)
        val p = hub.getPeripheral(address)
        connecting = false
        connectStartedAt = 0L
        if (wellue) {
            welluePeripheral?.let { hub.cancel(it) }
            welluePeripheral = null
            pendingWellue = p
            _wellue.value = WellueStatus(name = p.name ?: _wellue.value.name, error = "connect…")
            AppLogger.i("Wellue manual connect ${p.name} $address")
        } else {
            coospoPeripheral?.let { hub.cancel(it) }
            coospoPeripheral = null
            pendingCoospo = p
            _coospo.value = CoospoStatus(name = p.name ?: _coospo.value.name, error = "connect…")
            AppLogger.i("COOSPO manual connect ${p.name} $address")
        }
        maybeConnectPending()
    }

    override fun onDiscovered(peripheral: BluetoothPeripheral, scanResult: ScanResult) {
        if (!running) return
        val name = (peripheral.name ?: scanResult.scanRecord?.deviceName ?: "").trim()
        val lc = name.lowercase()
        if (lc.startsWith("oxypulse") || lc.startsWith("esp32-tester")) return
        val wellue = isWellue(scanResult, lc)
        val coospo = isCoospo(scanResult, lc, wellue)
        rememberNearby(
            NearbyBleDevice(
                name = name.ifBlank { "?" },
                address = peripheral.address,
                rssi = scanResult.rssi,
                likelyWellue = wellue,
                likelyBelt = coospo,
            ),
        )
        if (seenAdv.add(peripheral.address) && seenAdv.size <= 24) {
            AppLogger.i("BLE adv: '${name.ifBlank { "?" }}' ${peripheral.address} rssi=${scanResult.rssi}")
        }
        if (wellue && welluePeripheral == null && pendingWellue == null) {
            pendingWellue = peripheral
            AppLogger.i("Wellue found: $name ${peripheral.address}")
            if (allowConnect) maybeConnectPending()
        } else if (coospo && coospoPeripheral == null && pendingCoospo == null) {
            pendingCoospo = peripheral
            AppLogger.i("HR belt found: $name ${peripheral.address}")
            if (allowConnect) maybeConnectPending()
        }
    }

    override fun onConnected(peripheral: BluetoothPeripheral) {
        connecting = false
        connectStartedAt = 0L
        val name = (peripheral.name ?: "").trim()
        val lc = name.lowercase()
        when {
            isWellueTarget(peripheral, lc) -> {
                welluePeripheral = peripheral
                pendingWellue = null
                val shown = name.ifBlank { _wellue.value.name }
                _wellue.value = WellueStatus(connected = true, name = shown)
                wellueRxLen[0] = 0
                wellueSeq = 0
                prefs.setLastWellueAddress(peripheral.address)
                AppLogger.i("Wellue connected $shown")
            }
            isCoospoTarget(peripheral, lc) -> {
                coospoPeripheral = peripheral
                pendingCoospo = null
                val shown = name.ifBlank { _coospo.value.name }
                _coospo.value = CoospoStatus(connected = true, name = shown)
                prefs.setLastCoospoAddress(peripheral.address)
                AppLogger.i("COOSPO connected $shown")
            }
        }
        maybeScan()
    }

    private fun isWellueTarget(peripheral: BluetoothPeripheral, nameLc: String): Boolean {
        if (peripheral.address == pendingWellue?.address || peripheral.address == welluePeripheral?.address) return true
        if (peripheral.address.equals(prefs.lastWellueAddress(), true)) return true
        return isWellueName(nameLc)
    }

    private fun isCoospoTarget(peripheral: BluetoothPeripheral, nameLc: String): Boolean {
        if (peripheral.address == pendingCoospo?.address || peripheral.address == coospoPeripheral?.address) return true
        if (peripheral.address.equals(prefs.lastCoospoAddress(), true)) return true
        return isCoospoName(nameLc)
    }

    override fun onConnectionFailed(peripheral: BluetoothPeripheral, status: HciStatus) {
        connecting = false
        connectStartedAt = 0L
        if (peripheral.address == pendingWellue?.address) {
            pendingWellue = null
            _wellue.value = WellueStatus(error = "connect failed")
        }
        if (peripheral.address == pendingCoospo?.address) {
            pendingCoospo = null
            _coospo.value = CoospoStatus(error = "connect failed")
        }
        maybeScan()
    }

    override fun onDisconnected(peripheral: BluetoothPeripheral, status: HciStatus) {
        connecting = false
        connectStartedAt = 0L
        if (peripheral.address == welluePeripheral?.address) {
            welluePeripheral = null
            _wellue.value = WellueStatus(error = "disconnected")
            AppLogger.w("Wellue disconnected: $status")
        }
        if (peripheral.address == coospoPeripheral?.address) {
            coospoPeripheral = null
            _coospo.value = CoospoStatus(error = "disconnected")
            AppLogger.w("COOSPO disconnected: $status")
        }
        maybeScan()
    }

    private suspend fun tick() {
        val now = System.currentTimeMillis()
        if (welluePeripheral != null && now - wellueSampleAt > WearableBleConstants.SAMPLE_STALE_MS) {
            val cur = _wellue.value
            if (cur.connected) {
                _wellue.value = cur.copy(ok = false, spo2 = 0, hr = 0)
            }
        }
        if (coospoPeripheral != null && now - coospoSampleAt > WearableBleConstants.SAMPLE_STALE_MS) {
            val cur = _coospo.value
            if (cur.connected) {
                _coospo.value = cur.copy(ok = false, bpm = 0, rrMs = 0)
            }
        }
        welluePeripheral?.let { p ->
            val frame = WellueOxyii.buildRequest(WellueOxyii.CMD_LIVE_B, wellueSeq and 0xFF)
            wellueSeq += 1
            runCatching {
                p.writeCharacteristic(oxyiiService, oxyiiWrite, frame, WriteType.WITHOUT_RESPONSE)
            }
        }
        if (connecting && connectStartedAt > 0L &&
            now - connectStartedAt > WearableBleConstants.CONNECT_TIMEOUT_MS
        ) {
            AppLogger.w("Wearable connect timeout")
            pendingWellue?.let { hub.cancel(it) }
            pendingCoospo?.let { hub.cancel(it) }
            if (pendingWellue != null) {
                _wellue.value = _wellue.value.copy(error = "timeout")
                pendingWellue = null
            }
            if (pendingCoospo != null) {
                _coospo.value = _coospo.value.copy(error = "timeout")
                pendingCoospo = null
            }
            connecting = false
            connectStartedAt = 0L
        }
        maybeConnectPending()
        maybeScan()
        tickManualScan(now)
        tickHrv()
    }

    private fun rememberNearby(device: NearbyBleDevice) {
        nearbyByAddr[device.address] = device
        _nearby.value = nearbyByAddr.values.sortedByDescending { it.rssi }.take(16)
    }

    private fun tickManualScan(now: Long) {
        if (manualScanUntil <= 0L) return
        if (now < manualScanUntil) {
            val left = ((manualScanUntil - now + 999L) / 1000L).toInt().coerceAtLeast(1)
            _scanUi.value = WearableScanUi(scanning = true, remainSec = left, hint = "скан BLE… ${left} с")
            return
        }
        manualScanUntil = 0L
        val empty = _nearby.value.isEmpty()
        val hint = when {
            welluePeripheral != null && coospoPeripheral != null -> "кольцо и пояс подключены"
            empty -> "не видно в эфире"
            else -> "есть устройства — откройте список"
        }
        if (welluePeripheral == null && pendingWellue == null) {
            _wellue.value = _wellue.value.copy(error = if (empty) "не видно в эфире" else "есть в списке")
        }
        if (coospoPeripheral == null && pendingCoospo == null) {
            _coospo.value = _coospo.value.copy(error = if (empty) "не видно в эфире" else "есть в списке")
        }
        _scanUi.value = WearableScanUi(scanning = false, remainSec = 0, hint = hint)
        AppLogger.i("Wearable manual scan done nearby=${_nearby.value.size} $hint")
    }

    private fun maybeConnectPending() {
        if (!allowConnect || connecting) return
        val w = pendingWellue
        if (w != null && welluePeripheral == null) {
            connecting = true
            connectStartedAt = System.currentTimeMillis()
            hub.stopScan()
            runCatching { hub.connect(w, wellueCallback) }
                .onFailure {
                    connecting = false
                    pendingWellue = null
                    AppLogger.e("Wellue connect: ${it.message}")
                }
            return
        }
        val c = pendingCoospo
        if (c != null && coospoPeripheral == null) {
            connecting = true
            connectStartedAt = System.currentTimeMillis()
            hub.stopScan()
            runCatching { hub.connect(c, coospoCallback) }
                .onFailure {
                    connecting = false
                    pendingCoospo = null
                    AppLogger.e("COOSPO connect: ${it.message}")
                }
        }
    }

    private fun maybeScan() {
        if (!running || connecting || !allowConnect) return
        val manual = manualScanUntil > System.currentTimeMillis()
        val need = manual ||
            (welluePeripheral == null && pendingWellue == null) ||
            (coospoPeripheral == null && pendingCoospo == null)
        if (!need) {
            if (hub.isScanning) hub.stopScan()
            return
        }
        if (manual) {
            openScan = true
        }
        val now = System.currentTimeMillis()
        if (!openScan && lastScanKickAt > 0L &&
            now - lastScanKickAt > WearableBleConstants.OPEN_SCAN_AFTER_MS
        ) {
            openScan = true
            lastScanKickAt = now
            hub.restartScan(null)
            AppLogger.i("Wearable scan fallback open")
            return
        }
        if (!hub.isScanning || now - lastScanKickAt > WearableBleConstants.SCAN_RESTART_MS) {
            lastScanKickAt = now
            hub.restartScan(if (openScan) null else wearableFilters())
        }
    }

    private fun wearableFilters(): List<ScanFilter> = listOf(
        ScanFilter.Builder().setServiceUuid(ParcelUuid(oxyiiService)).build(),
        ScanFilter.Builder().setServiceUuid(ParcelUuid(hrService)).build(),
        ScanFilter.Builder()
            .setManufacturerData(WearableBleConstants.WELLUE_MFG_OXYII, byteArrayOf())
            .build(),
        ScanFilter.Builder()
            .setManufacturerData(WearableBleConstants.WELLUE_MFG_VIATOM, byteArrayOf())
            .build(),
    )

    @SuppressLint("MissingPermission")
    private fun rememberKnownDevices() {
        fun auto(address: String, wellue: Boolean) {
            if (address.isBlank()) return
            val p = hub.getPeripheral(address)
            AppLogger.i("${if (wellue) "Wellue" else "HR belt"} autoConnect $address")
            runCatching { hub.autoConnect(p, if (wellue) wellueCallback else coospoCallback) }
        }
        auto(prefs.lastWellueAddress(), true)
        auto(prefs.lastCoospoAddress(), false)
        val adapter = context.getSystemService(BluetoothManager::class.java)?.adapter ?: return
        adapter.bondedDevices.orEmpty().forEach { device ->
            val lc = (device.name ?: "").lowercase()
            if (isWellueName(lc)) auto(device.address, true)
            else if (isCoospoName(lc)) auto(device.address, false)
        }
    }

    private fun isWellueName(nameLc: String): Boolean {
        if (nameLc.startsWith("s8-aw") || nameLc.startsWith("t8520_")) return true
        return nameLc.contains("o2ring") || nameLc.contains("oxylink") ||
            nameLc.contains("wellue") || nameLc.contains("viatom")
    }

    private fun isCoospoName(nameLc: String): Boolean {
        return nameLc.contains("coospo") || nameLc.contains("h6m") || nameLc.contains("hw9")
    }

    private fun isWellue(scan: ScanResult, nameLc: String): Boolean {
        if (isWellueName(nameLc)) return true
        val rec = scan.scanRecord ?: return false
        if (rec.serviceUuids?.any { it.uuid == oxyiiService } == true) return true
        rec.getManufacturerSpecificData(WearableBleConstants.WELLUE_MFG_OXYII)?.let { return true }
        rec.getManufacturerSpecificData(WearableBleConstants.WELLUE_MFG_VIATOM)?.let { return true }
        return false
    }

    private fun isCoospo(scan: ScanResult, nameLc: String, wellue: Boolean): Boolean {
        if (wellue) return false
        if (isCoospoName(nameLc)) return true
        val rec = scan.scanRecord ?: return false
        return rec.serviceUuids?.any { it.uuid == hrService } == true
    }

    data class HrParsed(val bpm: Int, val contact: Boolean, val rr: List<Int>)

    fun parseHrMeasurement(data: ByteArray): HrParsed? {
        if (data.size < 2) return null
        val flags = data[0].toInt() and 0xFF
        val wide = flags and 0x01 != 0
        val contactBits = (flags shr 1) and 0x03
        val contact = when (contactBits) {
            0x02 -> false
            else -> true
        }
        var idx = 1
        val bpm: Int
        if (wide) {
            if (data.size < 3) return null
            bpm = (data[1].toInt() and 0xFF) or ((data[2].toInt() and 0xFF) shl 8)
            idx = 3
        } else {
            bpm = data[1].toInt() and 0xFF
            idx = 2
        }
        if (flags and 0x08 != 0) {
            if (data.size < idx + 2) return null
            idx += 2
        }
        val rr = mutableListOf<Int>()
        if (flags and 0x10 != 0) {
            while (idx + 1 < data.size) {
                val raw = (data[idx].toInt() and 0xFF) or ((data[idx + 1].toInt() and 0xFF) shl 8)
                idx += 2
                rr += ((raw * 1000L) / 1024L).toInt()
            }
        }
        return HrParsed(bpm, contact, rr)
    }

    fun startHrv(durationSec: Int) {
        synchronized(hrvLock) {
            hrvState = "recording"
            hrvDurationSec = durationSec.coerceIn(30, 300)
            hrvStartedAt = System.currentTimeMillis()
            hrvRr.clear()
            hrvError = ""
        }
        AppLogger.i("HRV local start ${durationSec}s")
    }

    fun stopHrv() {
        synchronized(hrvLock) {
            if (hrvState == "recording") hrvState = "idle"
            hrvRr.clear()
            hrvError = ""
            hrvStartedAt = 0L
        }
    }

    fun getHrv(): HrvSnapshot {
        synchronized(hrvLock) {
            tickHrvLocked()
            return HrvSnapshot(
                state = hrvState,
                durationSec = hrvDurationSec,
                elapsedMs = if (hrvStartedAt == 0L) 0L else System.currentTimeMillis() - hrvStartedAt,
                rrCount = hrvRr.size,
                error = hrvError,
                rr = if (hrvState == "done") hrvRr.toList() else emptyList(),
            )
        }
    }

    private fun feedHrv(rrMs: Int) {
        if (rrMs < 250 || rrMs > 2000) return
        synchronized(hrvLock) {
            if (hrvState != "recording") return
            hrvRr += rrMs
        }
    }

    private fun tickHrv() {
        synchronized(hrvLock) { tickHrvLocked() }
    }

    private fun tickHrvLocked() {
        if (hrvState != "recording") return
        val elapsed = System.currentTimeMillis() - hrvStartedAt
        if (elapsed >= hrvDurationSec * 1000L) {
            hrvState = "done"
            AppLogger.i("HRV local done rr=${hrvRr.size}")
        }
    }
}
