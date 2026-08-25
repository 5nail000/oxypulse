package com.oxypulse.esp32tester.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.oxypulse.esp32tester.domain.CoospoStatus
import com.oxypulse.esp32tester.domain.ConnectionState
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.ScannedDevice
import com.oxypulse.esp32tester.domain.TransportType
import com.oxypulse.esp32tester.domain.WellueStatus

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ConnectScreen(
    viewModel: TesterViewModel,
    onConnected: () -> Unit,
) {
    var tab by remember { mutableIntStateOf(0) }
    var host by remember { mutableStateOf("192.168.4.1") }
    val connectionState by viewModel.connectionState.collectAsStateWithLifecycle()
    val transport by viewModel.transport.collectAsStateWithLifecycle()
    val scanned by viewModel.scannedDevices.collectAsStateWithLifecycle()
    val message by viewModel.message.collectAsStateWithLifecycle()
    val snackbar = remember { SnackbarHostState() }

    LaunchedEffect(message) {
        message?.let {
            snackbar.showSnackbar(it)
        }
    }

    LaunchedEffect(transport, connectionState) {
        if (transport == TransportType.Wifi ||
            (transport == TransportType.Ble && connectionState == ConnectionState.Connected)
        ) {
            onConnected()
        }
    }

    Scaffold(snackbarHost = { SnackbarHost(snackbar) }) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            Text("Подключение к ESP32-Tester", style = MaterialTheme.typography.headlineSmall)

            TabRow(selectedTabIndex = tab) {
                Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("WiFi") })
                Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("BLE") })
            }

            if (tab == 0) {
                OutlinedTextField(
                    value = host,
                    onValueChange = { host = it },
                    label = { Text("IP ESP32") },
                    modifier = Modifier.fillMaxWidth(),
                )
                Button(onClick = { viewModel.connectWifi(host) }, modifier = Modifier.fillMaxWidth()) {
                    Text("Подключиться по WiFi")
                }
            } else {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    Button(onClick = { viewModel.startBleScan() }) { Text("Сканировать") }
                    Button(onClick = { viewModel.stopBleScan() }) { Text("Стоп") }
                }
                LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(scanned) { device ->
                        DeviceRow(device) { viewModel.connectBle(device.address) }
                    }
                }
            }
        }
    }
}

@Composable
private fun DeviceRow(device: ScannedDevice, onClick: () -> Unit) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Row(
            modifier = Modifier
                .fillMaxWidth()
                .padding(12.dp),
            horizontalArrangement = Arrangement.SpaceBetween,
        ) {
            Column {
                Text(device.name)
                Text(device.address, style = MaterialTheme.typography.bodySmall)
            }
            TextButton(onClick = onClick) { Text("Connect") }
        }
    }
}

@Composable
fun TesterScreen(
    viewModel: TesterViewModel,
    onDisconnect: () -> Unit,
) {
    val status by viewModel.status.collectAsStateWithLifecycle()
    val transport by viewModel.transport.collectAsStateWithLifecycle()
    var showGpio by remember { mutableStateOf(false) }
    var showServo by remember { mutableStateOf(false) }
    val pins = viewModel.availablePins(status)

    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        Text("ESP32 Tester", style = MaterialTheme.typography.headlineSmall)
        Text("Transport: ${transport.name}")
        Text("Uptime: ${status.uptimeMs / 1000.0} s")

        Button(onClick = { showGpio = true }, modifier = Modifier.fillMaxWidth()) {
            Text("GPIO тест")
        }
        Button(onClick = { showServo = true }, modifier = Modifier.fillMaxWidth()) {
            Text("Servo тест")
        }

        StatusList(status)

        Spacer(modifier = Modifier.height(8.dp))
        Button(onClick = {
            viewModel.disconnect()
            onDisconnect()
        }, modifier = Modifier.fillMaxWidth()) {
            Text("Отключиться")
        }
    }

    if (showGpio) {
        GpioDialog(
            pins = pins,
            status = status,
            onDismiss = { showGpio = false },
            onApply = { pin, on ->
                viewModel.applyGpio(pin, on)
                showGpio = false
            },
            onTap = { pin ->
                viewModel.tapGpio(pin)
            },
        )
    }

    if (showServo) {
        ServoDialog(
            pins = pins,
            status = status,
            onDismiss = { showServo = false },
            onApply = { pin, angle, auto ->
                viewModel.applyServo(pin, angle, auto)
                showServo = false
            },
        )
    }
}

@Composable
private fun StatusList(status: DeviceStatus) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(modifier = Modifier.padding(12.dp)) {
            Text("GPIO:", style = MaterialTheme.typography.titleMedium)
            if (status.gpio.isEmpty()) Text("—")
            status.gpio.forEach { Text("Pin ${it.pin}: ${if (it.state == 1) "ON" else "OFF"}") }

            Spacer(modifier = Modifier.height(8.dp))
            Text("Servos:", style = MaterialTheme.typography.titleMedium)
            if (status.servos.isEmpty()) Text("—")
            status.servos.forEach {
                Text("Pin ${it.pin}: ${it.angle}°, auto=${it.auto}")
            }

            Spacer(modifier = Modifier.height(8.dp))
            Text("Wellue Ring O2 S:", style = MaterialTheme.typography.titleMedium)
            Text(formatWellue(status.wellue))

            Spacer(modifier = Modifier.height(8.dp))
            Text("COOSPO H6M:", style = MaterialTheme.typography.titleMedium)
            Text(formatCoospo(status.coospo))
        }
    }
}

private fun formatWellue(s: WellueStatus): String {
    val error = s.error.takeIf { it.isNotBlank() }
    if (!s.connected) {
        return if (error != null) "поиск… ($error)" else "поиск…"
    }
    if (!s.ok) {
        return if (error != null) "подключено — нет данных ($error)" else "подключено — нет данных"
    }
    val name = s.name.ifBlank { "Wellue" }
    return "$name  SpO₂ ${s.spo2}%  HR ${s.hr} bpm  bat ${s.battery}%  контакт=${if (s.contact) "да" else "нет"}  motion=${s.motion}"
}

private fun formatCoospo(s: CoospoStatus): String {
    val error = s.error.takeIf { it.isNotBlank() }
    if (!s.connected) {
        return if (error != null) "поиск… ($error)" else "поиск…"
    }
    if (!s.ok) {
        return if (error != null) "подключено — нет данных ($error)" else "подключено — нет данных"
    }
    val name = s.name.ifBlank { "COOSPO" }
    return "$name  HR ${s.bpm} bpm  контакт=${if (s.contact) "да" else "нет"}"
}
