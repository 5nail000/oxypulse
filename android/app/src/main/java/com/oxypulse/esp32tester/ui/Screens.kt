package com.oxypulse.esp32tester.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.Card
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
import com.oxypulse.esp32tester.domain.ScannedDevice
import com.oxypulse.esp32tester.ui.components.Muted
import com.oxypulse.esp32tester.ui.components.PrimaryButton
import com.oxypulse.esp32tester.ui.components.SectionTitle

@Composable
fun ConnectScreen(viewModel: TesterViewModel) {
    var tab by remember { mutableIntStateOf(0) }
    val host by viewModel.host.collectAsStateWithLifecycle()
    var hostText by remember(host) { mutableStateOf(host) }
    val scanned by viewModel.scannedDevices.collectAsStateWithLifecycle()
    val message by viewModel.message.collectAsStateWithLifecycle()
    val snackbar = remember { SnackbarHostState() }

    LaunchedEffect(message) {
        message?.let {
            snackbar.showSnackbar(it)
            viewModel.consumeMessage()
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
            SectionTitle("Подключение к OxyPulse")
            Muted("ЦУ клапанов — BLE OxyPulse-CU. Стенд WiFi — откат на firmware_main_devkit.")

            TabRow(selectedTabIndex = tab) {
                Tab(selected = tab == 0, onClick = { tab = 0 }, text = { Text("ЦУ") })
                Tab(selected = tab == 1, onClick = { tab = 1 }, text = { Text("Стенд WiFi") })
            }

            if (tab == 0) {
                Muted("Имя рекламы OxyPulse-CU. Кольцо и ремень телефон берёт сам после connect.")
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    PrimaryButton("Сканировать", onClick = { viewModel.startCuScan() })
                    PrimaryButton("Стоп", onClick = { viewModel.stopCuScan() })
                }
                LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(scanned) { device ->
                        DeviceRow(device) { viewModel.connectCu(device.address) }
                    }
                }
            } else {
                OutlinedTextField(
                    value = hostText,
                    onValueChange = {
                        hostText = it
                        viewModel.setHost(it)
                    },
                    label = { Text("IP ESP32 (main)") },
                    modifier = Modifier.fillMaxWidth(),
                )
                PrimaryButton(
                    "Подключиться по WiFi",
                    onClick = { viewModel.connectWifi(hostText) },
                    modifier = Modifier.fillMaxWidth(),
                )
                Muted("AP 192.168.4.1 или STA IP. Wellue/COOSPO тогда с S3, не с телефона.")
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
