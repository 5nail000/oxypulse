package com.oxypulse.esp32tester.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import kotlinx.coroutines.delay
import kotlinx.coroutines.launchCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.oxypulse.esp32tester.domain.DeviceStatus

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GpioDialog(
    pins: List<Int>,
    status: DeviceStatus,
    onDismiss: () -> Unit,
    onApply: (pin: Int, on: Boolean) -> Unit,
    onTap: (pin: Int) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    var selectedPin by remember(pins) { mutableIntStateOf(pins.firstOrNull() ?: 18) }
    val current = status.gpio.find { it.pin == selectedPin }
    var on by remember(selectedPin, current) { mutableStateOf(current?.state == 1) }
    var tapBusy by remember { mutableStateOf(false) }
    val scope = rememberCoroutineScope()

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("GPIO") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
                    OutlinedTextField(
                        value = "GPIO $selectedPin",
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Pin") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .menuAnchor(),
                    )
                    ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                        pins.forEach { pin ->
                            DropdownMenuItem(
                                text = { Text("GPIO $pin") },
                                onClick = {
                                    selectedPin = pin
                                    expanded = false
                                    on = status.gpio.find { it.pin == pin }?.state == 1
                                },
                            )
                        }
                    }
                }
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text(if (on) "ON" else "OFF")
                    Switch(checked = on, onCheckedChange = { on = it })
                }
            }
        },
        confirmButton = {
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                TextButton(
                    enabled = !tapBusy,
                    onClick = {
                        if (tapBusy) return@TextButton
                        tapBusy = true
                        scope.launch {
                            val started = System.currentTimeMillis()
                            try {
                                onTap(selectedPin)
                            } finally {
                                val remaining = 3000L - (System.currentTimeMillis() - started)
                                if (remaining > 0) delay(remaining)
                                tapBusy = false
                            }
                        }
                    },
                ) { Text(if (tapBusy) "Тап…" else "Тапнуть") }
                Button(onClick = { onApply(selectedPin, on) }) { Text("Применить") }
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Отмена") }
        },
    )
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun ServoDialog(
    pins: List<Int>,
    status: DeviceStatus,
    onDismiss: () -> Unit,
    onApply: (pin: Int, angle: Int, auto: Boolean) -> Unit,
) {
    var expanded by remember { mutableStateOf(false) }
    var selectedPin by remember(pins) { mutableIntStateOf(pins.firstOrNull() ?: 18) }
    val current = status.servos.find { it.pin == selectedPin }
    var angle by remember(selectedPin, current) { mutableIntStateOf(current?.angle ?: 90) }
    var auto by remember(selectedPin, current) { mutableStateOf(current?.auto ?: false) }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Servo") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                ExposedDropdownMenuBox(expanded = expanded, onExpandedChange = { expanded = it }) {
                    OutlinedTextField(
                        value = "GPIO $selectedPin",
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Pin") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(expanded) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .menuAnchor(),
                    )
                    ExposedDropdownMenu(expanded = expanded, onDismissRequest = { expanded = false }) {
                        pins.forEach { pin ->
                            DropdownMenuItem(
                                text = { Text("GPIO $pin") },
                                onClick = {
                                    selectedPin = pin
                                    expanded = false
                                    val s = status.servos.find { it.pin == pin }
                                    angle = s?.angle ?: 90
                                    auto = s?.auto ?: false
                                },
                            )
                        }
                    }
                }
                Text("Угол: $angle°")
                androidx.compose.material3.Slider(
                    value = angle.toFloat(),
                    onValueChange = { angle = it.toInt() },
                    valueRange = 0f..180f,
                    steps = 179,
                    enabled = !auto,
                )
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Text("Авто 0↔180 (5 сек)")
                    Switch(checked = auto, onCheckedChange = { auto = it })
                }
            }
        },
        confirmButton = {
            Button(onClick = { onApply(selectedPin, angle, auto) }) { Text("Применить") }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) { Text("Отмена") }
        },
    )
}
