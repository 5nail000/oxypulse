package com.oxypulse.esp32tester.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Checkbox
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExposedDropdownMenuBox
import androidx.compose.material3.ExposedDropdownMenuDefaults
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Slider
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.oxypulse.esp32tester.api.CommandBuilder
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.WearableScanUi
import com.oxypulse.esp32tester.domain.Formatters
import com.oxypulse.esp32tester.domain.IgtLogic
import com.oxypulse.esp32tester.domain.LinkState
import com.oxypulse.esp32tester.domain.Overlay
import com.oxypulse.esp32tester.domain.TransportType
import com.oxypulse.esp32tester.domain.hasFullUi
import com.oxypulse.esp32tester.ui.charts.ChartSeries
import com.oxypulse.esp32tester.ui.charts.TimeSeriesChart
import com.oxypulse.esp32tester.ui.charts.lastNowText
import com.oxypulse.esp32tester.ui.components.LinkBar
import com.oxypulse.esp32tester.ui.components.Muted
import com.oxypulse.esp32tester.ui.components.OxyCard
import com.oxypulse.esp32tester.ui.components.Pill
import com.oxypulse.esp32tester.ui.components.PrimaryButton
import com.oxypulse.esp32tester.ui.components.SecondaryButton
import com.oxypulse.esp32tester.ui.components.SectionTitle
import com.oxypulse.esp32tester.ui.components.SensorTile
import com.oxypulse.esp32tester.ui.theme.BeltRed
import com.oxypulse.esp32tester.ui.theme.Co2Blue
import com.oxypulse.esp32tester.ui.theme.Co2Raw
import com.oxypulse.esp32tester.ui.theme.FlowViolet
import com.oxypulse.esp32tester.ui.theme.OkGreen
import com.oxypulse.esp32tester.ui.theme.Orange
import com.oxypulse.esp32tester.ui.theme.TextMuted

@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun DashboardScreen(viewModel: TesterViewModel) {
    val status by viewModel.status.collectAsStateWithLifecycle()
    val transport by viewModel.transport.collectAsStateWithLifecycle()
    val link by viewModel.linkState.collectAsStateWithLifecycle()
    val lastOk by viewModel.lastOkAt.collectAsStateWithLifecycle()
    val lastError by viewModel.lastError.collectAsStateWithLifecycle()
    val overlay by viewModel.overlay.collectAsStateWithLifecycle()
    val sources by viewModel.trendSources.collectAsStateWithLifecycle()
    val windowSec by viewModel.chartWindowSec.collectAsStateWithLifecycle()
    val charts by viewModel.charts.collectAsStateWithLifecycle()
    val flowDyn by viewModel.flowDynamics.collectAsStateWithLifecycle()
    val hrBelt by viewModel.hrViewBelt.collectAsStateWithLifecycle()
    val hrWellue by viewModel.hrViewWellue.collectAsStateWithLifecycle()
    val tapBusy by viewModel.gpioTapBusy.collectAsStateWithLifecycle()
    val tapLabel by viewModel.gpioTapLabel.collectAsStateWithLifecycle()
    val message by viewModel.message.collectAsStateWithLifecycle()
    val igtMonitor by viewModel.igtMonitorReport.collectAsStateWithLifecycle()
    val igtUi by viewModel.igt.collectAsStateWithLifecycle()
    val wearableScan by viewModel.wearableScan.collectAsStateWithLifecycle()
    val wearableNearby by viewModel.wearableNearby.collectAsStateWithLifecycle()
    val snackbar = remember { SnackbarHostState() }
    val wifi = transport == TransportType.Wifi
    val fullUi = transport.hasFullUi()
    val cmdsOff = link == LinkState.Offline || link == LinkState.Idle
    val igtBusy = IgtLogic.isBusy(status.igt)
    val pins = viewModel.availablePins(status)
    var gpioPin by remember { mutableIntStateOf(25) }
    var servoPin by remember { mutableIntStateOf(4) }
    var gpioOn by remember { mutableStateOf(false) }
    var servoAngle by remember { mutableIntStateOf(90) }
    var servoAuto by remember { mutableStateOf(false) }
    var gpioMenu by remember { mutableStateOf(false) }
    var servoMenu by remember { mutableStateOf(false) }
    val motor = gpioPin == CommandBuilder.O2_MOTOR_GPIO
    val host = viewModel.host.collectAsStateWithLifecycle().value
    val age = if (lastOk > 0) Formatters.formatAge(System.currentTimeMillis() - lastOk) else ""
    val (linkText, linkMeta) = when (link) {
        LinkState.Live -> "Онлайн" to "${host.ifBlank { "" }} · обновлено $age"
        LinkState.Stale -> "Задержка" to "$age без ответа"
        LinkState.Offline -> "Нет связи" to (if (lastOk > 0) "данные устарели · $age" else lastError.ifBlank { "ESP32 недоступен" })
        LinkState.Idle -> "Не подключено" to host
    }

    LaunchedEffect(message) {
        message?.let {
            snackbar.showSnackbar(it)
            viewModel.consumeMessage()
        }
    }
    LaunchedEffect(gpioPin, status.gpio, status.gpioTapActive) {
        if (!status.gpioTapActive && !tapBusy) {
            gpioOn = status.gpio.find { it.pin == gpioPin }?.state == 1
        }
    }
    LaunchedEffect(servoPin, status.servos) {
        status.servos.find { it.pin == servoPin }?.let {
            servoAngle = it.angle
            servoAuto = it.auto
        }
    }

    Box(Modifier.fillMaxSize()) {
    Scaffold(
        snackbarHost = { SnackbarHost(snackbar) },
        topBar = { LinkBar(link, linkText, linkMeta) },
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(rememberScrollState())
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            ConnectionCard(
                status = status,
                wifi = wifi,
                cu = transport == TransportType.ValveCu,
                host = host,
                cmdsOff = cmdsOff,
                onWifi = { viewModel.openOverlay(Overlay.Wifi) },
            )

            OxyCard {
                SectionTitle("GPIO")
                ExposedDropdownMenuBox(expanded = gpioMenu, onExpandedChange = { gpioMenu = it }) {
                    OutlinedTextField(
                        value = Formatters.pinLabel(gpioPin),
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Pin") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(gpioMenu) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .menuAnchor(),
                    )
                    ExposedDropdownMenu(expanded = gpioMenu, onDismissRequest = { gpioMenu = false }) {
                        pins.forEach { pin ->
                            DropdownMenuItem(
                                text = { Text(Formatters.pinLabel(pin)) },
                                onClick = {
                                    gpioPin = pin
                                    gpioMenu = false
                                },
                            )
                        }
                    }
                }
                if (!motor) {
                    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
                        Text(if (gpioOn) "ON" else "OFF", modifier = Modifier.weight(1f))
                        Switch(checked = gpioOn, onCheckedChange = { gpioOn = it }, enabled = !cmdsOff && !igtBusy)
                    }
                } else {
                    Muted(if (gpioOn) "ON (лог.)" else "OFF (лог.)")
                }
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                    if (!motor) {
                        PrimaryButton(
                            "Применить",
                            onClick = { viewModel.applyGpio(gpioPin, gpioOn) },
                            modifier = Modifier.weight(1f),
                            enabled = !cmdsOff && !igtBusy,
                        )
                    }
                    SecondaryButton(
                        if (motor && !tapBusy) "Переключить мотор" else tapLabel,
                        onClick = { viewModel.tapGpio(gpioPin) },
                        modifier = Modifier.weight(1f),
                        enabled = !cmdsOff && !igtBusy && !tapBusy,
                    )
                }
            }

            OxyCard {
                SectionTitle("Servo")
                ExposedDropdownMenuBox(expanded = servoMenu, onExpandedChange = { servoMenu = it }) {
                    OutlinedTextField(
                        value = Formatters.pinLabel(servoPin),
                        onValueChange = {},
                        readOnly = true,
                        label = { Text("Pin") },
                        trailingIcon = { ExposedDropdownMenuDefaults.TrailingIcon(servoMenu) },
                        modifier = Modifier
                            .fillMaxWidth()
                            .menuAnchor(),
                    )
                    ExposedDropdownMenu(expanded = servoMenu, onDismissRequest = { servoMenu = false }) {
                        pins.forEach { pin ->
                            DropdownMenuItem(
                                text = { Text(Formatters.pinLabel(pin)) },
                                onClick = {
                                    servoPin = pin
                                    servoMenu = false
                                },
                            )
                        }
                    }
                }
                Text("Угол: $servoAngle°")
                Slider(
                    value = servoAngle.toFloat(),
                    onValueChange = { servoAngle = it.toInt() },
                    valueRange = 0f..180f,
                    enabled = !servoAuto && !cmdsOff && !igtBusy,
                )
                Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
                    Text("Авто 0↔180 (5 сек)", modifier = Modifier.weight(1f))
                    Switch(
                        checked = servoAuto,
                        onCheckedChange = { servoAuto = it },
                        enabled = !cmdsOff && !igtBusy,
                    )
                }
                PrimaryButton(
                    "Применить",
                    onClick = { viewModel.applyServo(servoPin, servoAngle, servoAuto) },
                    modifier = Modifier.fillMaxWidth(),
                    enabled = !cmdsOff && !igtBusy,
                )
                val servoItem = status.servos.find { it.pin == servoPin }
                Muted(
                    if (servoItem != null) {
                        "На устройстве: ${servoItem.angle}°, auto=${servoItem.auto}"
                    } else {
                        "Серва на этом pin ещё не настроена"
                    },
                )
            }

            SensorsCard(
                status = status,
                sources = sources,
                wifi = fullUi,
                phoneBle = transport == TransportType.ValveCu,
                cmdsOff = cmdsOff,
                flowDyn = flowDyn,
                wearableScan = wearableScan,
                lastWellue = viewModel.lastWellueAddress(),
                lastCoospo = viewModel.lastCoospoAddress(),
                onTrend = { viewModel.toggleTrend(it) },
                onFlowDyn = { viewModel.setFlowDynamics(!flowDyn) },
                onFlowReset = { viewModel.flowReset() },
                onCo2Frc = { viewModel.co2Frc() },
                onFindWearables = {
                    viewModel.openWearablesFinder(
                        rescan = wearableNearby.isEmpty() && !wearableScan.scanning,
                    )
                },
                onReconnectWearable = { wellue -> viewModel.reconnectWearable(wellue) },
            )

            OxyCard {
                SectionTitle("Действия")
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    PrimaryButton(
                        "Замерить HRV",
                        onClick = { viewModel.openOverlay(Overlay.Hrv) },
                        enabled = fullUi && status.coospo.connected,
                    )
                    PrimaryButton("Замер КП Бутейко", onClick = { viewModel.openOverlay(Overlay.Cp) }, enabled = fullUi)
                    PrimaryButton("Практика Бутейко", onClick = { viewModel.openOverlay(Overlay.Practice) }, enabled = fullUi)
                    PrimaryButton("ИГТ", onClick = { viewModel.openOverlay(Overlay.Igt) }, enabled = fullUi)
                    if (igtMonitor.isRecent()) {
                        SecondaryButton("Отчёт ИГТ", onClick = { viewModel.openIgtMonitorReport() })
                    }
                }
                if (fullUi && !status.coospo.connected) Muted("HRV: подключите нагрудный ремень")
            }

            if (overlay == Overlay.None) {
                TrendsCard(
                    sources = sources,
                    windowSec = windowSec,
                    charts = charts,
                    flowDyn = flowDyn,
                    hrBelt = hrBelt,
                    hrWellue = hrWellue,
                    onWindow = { viewModel.setChartWindow(it) },
                    onHrBelt = { viewModel.setHrViewBelt(it) },
                    onHrWellue = { viewModel.setHrViewWellue(it) },
                )
            }

            SecondaryButton("Отключиться", onClick = { viewModel.disconnect() }, modifier = Modifier.fillMaxWidth())
        }
    }

    when (overlay) {
        Overlay.Wifi -> WifiDialog(viewModel)
        Overlay.Hrv -> HrvDialog(viewModel)
        Overlay.Cp -> CpDialog(viewModel)
        Overlay.Practice -> PracticeDialog(viewModel)
        Overlay.Igt -> IgtDialog(viewModel)
        Overlay.Wearables -> WearablesDialog(viewModel)
        Overlay.None -> Unit
    }
    if (igtUi.monitorOpen && overlay != Overlay.Igt) {
        IgtMonitorReportDialog(viewModel)
    }
    }
}

@Composable
private fun ConnectionCard(
    status: DeviceStatus,
    wifi: Boolean,
    cu: Boolean,
    host: String,
    cmdsOff: Boolean,
    onWifi: () -> Unit,
) {
    OxyCard {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            SectionTitle("Подключение")
            androidx.compose.foundation.layout.Spacer(Modifier.weight(1f))
            if (wifi) SecondaryButton("WiFi", onClick = onWifi, enabled = !cmdsOff)
        }
        Muted(
            when {
                wifi -> "Стенд WiFi · $host"
                cu -> "ЦУ OxyPulse-CU · BLE"
                else -> "BLE"
            },
        )
        if (status.uptimeMs > 0) Muted("Uptime ESP32: ${"%.1f".format(status.uptimeMs / 1000.0)} s")
        Formatters.wifiLanHint(status.wifi, host)?.let { Muted(it) }
        if (IgtLogic.isBusy(status.igt)) Muted("ИГТ: ${status.igt.state} — ручные V1/V4/мотор/Servo1 заблокированы")
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun SensorsCard(
    status: DeviceStatus,
    sources: com.oxypulse.esp32tester.domain.TrendSources,
    wifi: Boolean,
    phoneBle: Boolean,
    cmdsOff: Boolean,
    flowDyn: Boolean,
    wearableScan: WearableScanUi,
    lastWellue: String,
    lastCoospo: String,
    onTrend: (String) -> Unit,
    onFlowDyn: () -> Unit,
    onFlowReset: () -> Unit,
    onCo2Frc: () -> Unit,
    onFindWearables: () -> Unit,
    onReconnectWearable: (Boolean) -> Unit,
) {
    val s = status.sensors
    OxyCard {
        SectionTitle("Датчики")
        Muted("Буфер гипоксии · I²C 22/23")
        SensorTile("O₂ (ADS1115)", Formatters.o2Text(s.hypoxiaO2), s.hypoxiaO2.ok, trendOn = sources.o2, onTrend = { onTrend("o2") }.takeIf { wifi })
        SensorTile("Давление DPS310", Formatters.pressureText(s.hypoxiaPressure), s.hypoxiaPressure.ok, trendOn = sources.pressure, onTrend = { onTrend("pressure") }.takeIf { wifi })
        Muted("Рабочий контур · I²C 18/19")
        SensorTile("O₂ (ADS1115)", Formatters.o2Text(s.workO2), s.workO2.ok, trendOn = sources.workO2, onTrend = { onTrend("work_o2") }.takeIf { wifi })
        SensorTile("Давление DPS310", Formatters.pressureText(s.workPressure), s.workPressure.ok, trendOn = sources.workPressure, onTrend = { onTrend("work_pressure") }.takeIf { wifi })
        SensorTile(
            title = "Поток SFM3300",
            value = Formatters.flowText(s.flow),
            ok = s.flow.ok,
            trendOn = sources.flow,
            onTrend = { onTrend("flow") }.takeIf { wifi },
        ) {
            Muted("Фаза: ${Formatters.flowPhase(s.flow.phase)}")
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                SecondaryButton("Сбросить объём", onClick = onFlowReset, enabled = !cmdsOff)
                if (wifi) {
                    SecondaryButton(
                        if (flowDyn) "Динамика вкл" else "Динамика",
                        onClick = onFlowDyn,
                        enabled = !cmdsOff,
                    )
                }
            }
        }
        SensorTile("CO₂ SCD41", Formatters.co2Text(s.co2), s.co2.ok, trendOn = sources.co2, onTrend = { onTrend("co2") }.takeIf { wifi }) {
            SecondaryButton("Калибровка 400 ppm", onClick = onCo2Frc, enabled = !cmdsOff)
        }
        Muted(if (phoneBle) "BLE (телефон)" else "BLE (ESP32 central)")
        if (phoneBle) {
            PrimaryButton(
                text = when {
                    wearableScan.scanning -> "Поиск ${wearableScan.remainSec} с…"
                    wearableScan.hint.isNotBlank() -> "Кольцо и пояс"
                    else -> "Найти кольцо и пояс"
                },
                onClick = onFindWearables,
                modifier = Modifier.fillMaxWidth(),
            )
            if (wearableScan.hint.isNotBlank()) Muted(wearableScan.hint)
        }
        SensorTile("Wellue Ring O2 S", Formatters.wellueText(s.wellue), Formatters.bleOk(s.wellue), trendOn = sources.wellue, onTrend = { onTrend("wellue") }.takeIf { wifi }) {
            if (phoneBle && !s.wellue.connected && lastWellue.isNotBlank()) {
                SecondaryButton("Подключить", onClick = { onReconnectWearable(true) })
            }
        }
        SensorTile(
            title = s.coospo.name.ifBlank { "Нагрудный HR" },
            value = Formatters.coospoText(s.coospo),
            ok = Formatters.bleOk(s.coospo),
            trendLabel = null,
            onTrend = null,
        ) {
            if (phoneBle && !s.coospo.connected && lastCoospo.isNotBlank()) {
                SecondaryButton("Подключить", onClick = { onReconnectWearable(false) })
            }
            if (wifi) {
                Row {
                    Text("ЧСС", color = if (sources.hr) OkGreen else TextMuted, modifier = Modifier.padding(end = 12.dp))
                    androidx.compose.material3.TextButton(onClick = { onTrend("hr") }) {
                        Text(if (sources.hr) "ЧСС вкл" else "ЧСС", color = if (sources.hr) OkGreen else TextMuted)
                    }
                    androidx.compose.material3.TextButton(onClick = { onTrend("rr") }) {
                        Text(if (sources.rr) "R-R вкл" else "R-R", color = if (sources.rr) OkGreen else TextMuted)
                    }
                }
            }
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun TrendsCard(
    sources: com.oxypulse.esp32tester.domain.TrendSources,
    windowSec: Int,
    charts: ChartSnapshot,
    flowDyn: Boolean,
    hrBelt: Boolean,
    hrWellue: Boolean,
    onWindow: (Int) -> Unit,
    onHrBelt: (Boolean) -> Unit,
    onHrWellue: (Boolean) -> Unit,
) {
    OxyCard {
        SectionTitle("Тренды")
        FlowRow(horizontalArrangement = Arrangement.spacedBy(4.dp)) {
            listOf(120 to "2 мин", 300 to "5 мин", 600 to "10 мин", 3600 to "60 мин", 4500 to "75 мин").forEach { (sec, label) ->
                Pill(label, selected = windowSec == sec, onClick = { onWindow(sec) })
            }
        }
        if (!sources.any() && !flowDyn) {
            Muted("Включите «Тренд» на плитке датчика — набор хранится на ESP32")
            return@OxyCard
        }
        ChartBlock("O₂ · гипоксия", sources.o2, lastNowText(charts.o2) { "%.1f %%".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.o2, OkGreen)), windowSec, { "%.1f".format(it) }, 15f, 21f)
        }
        ChartBlock("O₂ · рабочий", sources.workO2, lastNowText(charts.workO2) { "%.1f %%".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.workO2, OkGreen)), windowSec, { "%.1f".format(it) }, 15f, 21f)
        }
        ChartBlock("Минутная вентиляция · рабочий", sources.flow, lastNowText(charts.flow) { "%.1f л/мин".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.flow, FlowViolet)), windowSec, { "%.1f".format(it) }, 0f, 20f)
        }
        ChartBlock("Поток вдох/выдох · рабочий", flowDyn, lastNowText(charts.slm) { "%.2f л/мин".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.slm, FlowViolet, maxGapMs = 1000)), windowSec, { "%.1f".format(it) }, -20f, 20f)
        }
        ChartBlock("Давление · гипоксия", sources.pressure, lastNowText(charts.pressure) { "%.1f hPa".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.pressure, Orange)), windowSec, { "%.1f".format(it) }, 950f, 1050f)
        }
        ChartBlock("Давление · рабочий", sources.workPressure, lastNowText(charts.workPressure) { "%.1f hPa".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.workPressure, Orange)), windowSec, { "%.1f".format(it) }, 950f, 1050f)
        }
        ChartBlock("CO₂ · рабочий", sources.co2, lastNowText(charts.co2) { "%.2f %%".format(it) }) {
            val series = mutableListOf(ChartSeries(charts.co2, Co2Blue))
            if (charts.co2Raw.any { it.v != null }) series += ChartSeries(charts.co2Raw, Co2Raw, dashed = true)
            TimeSeriesChart(series, windowSec, { "%.2f".format(it) }, 0f, 8f)
        }
        ChartBlock("SpO₂", sources.wellue, lastNowText(charts.spo2) { "%.0f %%".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.spo2, OkGreen)), windowSec, { "%.0f".format(it) }, 85f, 100f)
        }
        ChartBlock("ЧСС", sources.hr || sources.wellue, lastNowText(if (hrBelt) charts.hrCoospo else charts.hrWellue) { "%.0f bpm".format(it) }) {
            if (sources.hr && sources.wellue) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Checkbox(checked = hrBelt, onCheckedChange = onHrBelt)
                    Text("Нагрудный", color = BeltRed)
                    Checkbox(checked = hrWellue, onCheckedChange = onHrWellue)
                    Text("Wellue", color = Orange)
                }
            }
            val series = mutableListOf<ChartSeries>()
            if (hrBelt && sources.hr) series += ChartSeries(charts.hrCoospo, BeltRed)
            if (hrWellue && sources.wellue) series += ChartSeries(charts.hrWellue, Orange, dashed = true)
            if (series.isEmpty() && sources.hr) series += ChartSeries(charts.hrCoospo, BeltRed)
            TimeSeriesChart(series, windowSec, { "%.0f".format(it) }, 40f, 180f)
        }
        ChartBlock("R-R", sources.rr, lastNowText(charts.rr) { "%.0f мс".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(charts.rr, BeltRed)), windowSec, { "%.0f".format(it) }, 400f, 1200f)
        }
    }
}

@Composable
private fun ChartBlock(title: String, visible: Boolean, now: String, content: @Composable () -> Unit) {
    if (!visible) return
    Column(verticalArrangement = Arrangement.spacedBy(4.dp), modifier = Modifier.fillMaxWidth()) {
        Row(modifier = Modifier.fillMaxWidth()) {
            Text(title, modifier = Modifier.weight(1f))
            Text(now, color = TextMuted)
        }
        content()
    }
}
