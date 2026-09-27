package com.oxypulse.esp32tester.ui

import android.graphics.Bitmap
import android.graphics.Canvas as AndroidCanvas
import android.graphics.Paint
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.oxypulse.esp32tester.domain.DeviceStatus
import com.oxypulse.esp32tester.domain.Formatters
import com.oxypulse.esp32tester.domain.NearbyBleDevice
import com.oxypulse.esp32tester.domain.HrvMath
import com.oxypulse.esp32tester.domain.HrvStep
import com.oxypulse.esp32tester.domain.IgtLogic
import com.oxypulse.esp32tester.domain.IgtProtocol
import com.oxypulse.esp32tester.domain.IgtStatus
import com.oxypulse.esp32tester.domain.IgtMonitor
import com.oxypulse.esp32tester.domain.IgtMonitorReport
import com.oxypulse.esp32tester.domain.IgtStep
import com.oxypulse.esp32tester.domain.SessionUi
import com.oxypulse.esp32tester.ui.charts.ChartSeries
import com.oxypulse.esp32tester.ui.charts.PoincareChart
import com.oxypulse.esp32tester.ui.charts.TimeSeriesChart
import com.oxypulse.esp32tester.ui.charts.lastNowText
import com.oxypulse.esp32tester.ui.components.DialogHead
import com.oxypulse.esp32tester.ui.components.FullDialog
import com.oxypulse.esp32tester.ui.components.Muted
import com.oxypulse.esp32tester.ui.components.OxyCard
import com.oxypulse.esp32tester.ui.components.Pill
import com.oxypulse.esp32tester.ui.components.PrimaryButton
import com.oxypulse.esp32tester.ui.components.SecondaryButton
import com.oxypulse.esp32tester.ui.theme.BeltRed
import com.oxypulse.esp32tester.ui.theme.Co2Blue
import com.oxypulse.esp32tester.ui.theme.Co2Raw
import com.oxypulse.esp32tester.ui.theme.ErrRed
import com.oxypulse.esp32tester.ui.theme.FlowViolet
import com.oxypulse.esp32tester.ui.theme.MarkerBlue
import com.oxypulse.esp32tester.ui.theme.NormoxiaGreen
import com.oxypulse.esp32tester.ui.theme.OkGreen
import com.oxypulse.esp32tester.ui.theme.Orange
import com.oxypulse.esp32tester.ui.theme.TextMain
import com.oxypulse.esp32tester.ui.theme.TextMuted

@Composable
fun IgtMonitorReportDialog(viewModel: TesterViewModel) {
    val report by viewModel.igtMonitorReport.collectAsStateWithLifecycle()
    FullDialog(onDismiss = { viewModel.closeIgtMonitorReport() }) {
        IgtMonitorReportContent(report, onClose = { viewModel.closeIgtMonitorReport() })
    }
}

@Composable
private fun IgtMonitorReportContent(report: IgtMonitorReport, onClose: () -> Unit) {
    DialogHead("Мониторинг ИГТ", onClose = onClose)
    Text(
        IgtMonitor.formatReport(report),
        modifier = Modifier.fillMaxWidth(),
        fontSize = 13.sp,
    )
}

@Composable
fun WifiDialog(viewModel: TesterViewModel) {
    val ui by viewModel.wifiUi.collectAsStateWithLifecycle()
    val status by viewModel.status.collectAsStateWithLifecycle()
    FullDialog(onDismiss = { viewModel.closeOverlay() }) {
        DialogHead("WiFi", onClose = { viewModel.closeOverlay() })
        Muted(ui.status.ifBlank { Formatters.wifiModalStatus(status.wifi) })
        Muted("SSID можно ввести вручную — скан не обязателен. При скане AP может на 5–10 с моргнуть.")
        OutlinedTextField(ui.ssid, { viewModel.setWifiSsid(it) }, label = { Text("Имя сети (SSID)") }, modifier = Modifier.fillMaxWidth())
        OutlinedTextField(
            ui.password,
            { viewModel.setWifiPassword(it) },
            label = { Text("Пароль") },
            visualTransformation = PasswordVisualTransformation(),
            modifier = Modifier.fillMaxWidth(),
        )
        PrimaryButton("Сканировать сети", onClick = { viewModel.scanWifi() }, enabled = !ui.scanning, modifier = Modifier.fillMaxWidth())
        ui.networks.forEach { net ->
            SecondaryButton(
                "${net.ssid}  ${net.rssi} dBm · ${if (net.secure) "защищена" else "открытая"}",
                onClick = { viewModel.setWifiSsid(net.ssid) },
                modifier = Modifier.fillMaxWidth(),
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            PrimaryButton(
                if (ui.saving) "Сохранение…" else "Сохранить и перезагрузить",
                onClick = { viewModel.saveWifi() },
                enabled = ui.ssid.isNotBlank() && !ui.saving,
                modifier = Modifier.weight(1f),
            )
            SecondaryButton("Забыть сеть", onClick = { viewModel.forgetWifi() }, modifier = Modifier.weight(1f))
        }
    }
}

@Composable
fun WearablesDialog(viewModel: TesterViewModel) {
    val scan by viewModel.wearableScan.collectAsStateWithLifecycle()
    val nearby by viewModel.wearableNearby.collectAsStateWithLifecycle()
    val status by viewModel.status.collectAsStateWithLifecycle()
    var showOthers by remember { mutableStateOf(false) }
    val likely = nearby.filter { it.likelyWellue || it.likelyBelt }
    val others = nearby.filter { !it.likelyWellue && !it.likelyBelt }
    val expandOthers = showOthers || likely.isEmpty()
    FullDialog(onDismiss = { viewModel.closeOverlay() }, pageKey = "wearables") {
        DialogHead("Кольцо и пояс", onClose = { viewModel.closeOverlay() })
        Muted("Скан телефона. Подключение — кнопками у строки.")
        PrimaryButton(
            text = if (scan.scanning) "Поиск ${scan.remainSec} с…" else "Искать снова",
            onClick = { viewModel.findWearables() },
            enabled = !scan.scanning,
            modifier = Modifier.fillMaxWidth(),
        )
        if (scan.hint.isNotBlank()) Muted(scan.hint)
        Muted("Кольцо: ${Formatters.wellueText(status.wellue).substringBefore('\n')}")
        Muted("Пояс: ${Formatters.coospoText(status.coospo).substringBefore('\n')}")
        if (likely.isNotEmpty()) {
            Muted("Похоже на кольцо или пояс")
            likely.forEach { WearableDeviceRow(it) { address, wellue -> viewModel.connectWearable(address, wellue) } }
        }
        if (others.isNotEmpty()) {
            if (likely.isNotEmpty()) {
                SecondaryButton(
                    if (expandOthers) "Скрыть остальные BLE" else "Остальные BLE (${others.size})",
                    onClick = { showOthers = !showOthers },
                    modifier = Modifier.fillMaxWidth(),
                )
            } else {
                Muted("Остальные BLE")
            }
            if (expandOthers) {
                others.forEach { WearableDeviceRow(it) { address, wellue -> viewModel.connectWearable(address, wellue) } }
            }
        }
        if (!scan.scanning && nearby.isEmpty()) {
            Muted("Пока ничего нет. Держите кольцо и пояс включёнными рядом с телефоном.")
        }
    }
}

@Composable
private fun WearableDeviceRow(
    device: NearbyBleDevice,
    onConnect: (String, Boolean) -> Unit,
) {
    OxyCard {
        Text("${device.name}  ${device.rssi} dBm", color = TextMain, fontSize = 14.sp)
        Muted(
            buildString {
                append(device.address)
                if (device.likelyWellue) append(" · похоже на кольцо")
                if (device.likelyBelt) append(" · похоже на пояс")
            },
        )
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            SecondaryButton("Как кольцо", onClick = { onConnect(device.address, true) })
            SecondaryButton("Как пояс", onClick = { onConnect(device.address, false) })
        }
    }
}

@Composable
fun HrvDialog(viewModel: TesterViewModel) {
    val ui by viewModel.hrv.collectAsStateWithLifecycle()
    val context = LocalContext.current
    FullDialog(onDismiss = { viewModel.closeOverlay() }) {
        DialogHead("Замер HRV", onClose = { viewModel.closeOverlay() })
        when (ui.step) {
            HrvStep.Setup -> {
                Muted("Длительность записи R-R с нагрудного ремня")
                Row {
                    listOf(30 to "30 сек", 120 to "2 мин", 180 to "3 мин", 300 to "5 мин").forEach { (sec, label) ->
                        Pill(label, selected = ui.durationSec == sec, onClick = { viewModel.setHrvDuration(sec) })
                    }
                }
                PrimaryButton("Старт", onClick = { viewModel.startHrvCountdown() }, modifier = Modifier.fillMaxWidth())
            }
            HrvStep.Countdown -> {
                Muted("Приготовьтесь…")
                Text("${ui.countdown}", fontSize = 64.sp, color = TextMain, fontWeight = FontWeight.Bold)
            }
            HrvStep.Recording -> {
                val dur = ui.durationSec * 1000L
                val left = (dur - ui.elapsedMs).coerceAtLeast(0)
                Text("Идёт замер… осталось ${Formatters.remainMmSs(left)}")
                LinearProgressIndicator(
                    progress = { if (dur > 0) (ui.elapsedMs / dur.toFloat()).coerceIn(0f, 1f) else 0f },
                    modifier = Modifier.fillMaxWidth(),
                )
                Muted("R-R: ${ui.rrCount}")
                SecondaryButton("Отмена", onClick = { viewModel.cancelHrv() }, modifier = Modifier.fillMaxWidth())
            }
            HrvStep.Result -> {
                val metrics = ui.metrics
                Muted("Время: ${ui.stamp} · ${Formatters.hrvDurationLabel(ui.durationSec)}")
                if (metrics != null) {
                    PoincareChart(metrics)
                    HrvMath.metricItems(metrics).chunked(3).forEach { row ->
                        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            row.forEach { (label, value) ->
                                OxyCard(Modifier.weight(1f)) {
                                    Muted(label)
                                    Text(value, color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.Bold)
                                }
                            }
                        }
                    }
                }
                Muted(ui.note)
                OutlinedTextField(ui.comment, { viewModel.setHrvComment(it) }, label = { Text("Комментарий") }, modifier = Modifier.fillMaxWidth())
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                    SecondaryButton(
                        "Сохранить PNG",
                        onClick = {
                            val m = ui.metrics ?: return@SecondaryButton
                            val bmp = Bitmap.createBitmap(840, 900, Bitmap.Config.ARGB_8888)
                            val c = AndroidCanvas(bmp)
                            c.drawColor(android.graphics.Color.parseColor("#1C1C1C"))
                            val p = Paint().apply {
                                color = android.graphics.Color.WHITE
                                textSize = 36f
                                isAntiAlias = true
                            }
                            c.drawText("Замер HRV", 32f, 56f, p)
                            p.textSize = 26f
                            p.color = android.graphics.Color.parseColor("#888888")
                            c.drawText(ui.stamp, 32f, 96f, p)
                            val plot = Bitmap.createBitmap(780, 520, Bitmap.Config.ARGB_8888)
                            // compact metrics dump
                            var y = 140f
                            p.color = android.graphics.Color.WHITE
                            p.textSize = 32f
                            HrvMath.metricItems(m).forEach { (k, v) ->
                                c.drawText("$k  $v", 32f, y, p)
                                y += 44f
                            }
                            p.textSize = 26f
                            p.color = android.graphics.Color.parseColor("#888888")
                            c.drawText(ui.note, 32f, y + 12f, p)
                            if (ui.comment.isNotBlank()) {
                                p.color = android.graphics.Color.WHITE
                                p.textSize = 30f
                                c.drawText(ui.comment, 32f, y + 60f, p)
                            }
                            plot.recycle()
                            SharePng.share(context, bmp, "hrv_${System.currentTimeMillis()}.png")
                        },
                        modifier = Modifier.weight(1f),
                    )
                    PrimaryButton("Новый замер", onClick = { viewModel.cancelHrv() }, modifier = Modifier.weight(1f))
                }
            }
            HrvStep.Error -> {
                Text(ui.error, color = ErrRed)
                PrimaryButton("Новый замер", onClick = { viewModel.cancelHrv() }, modifier = Modifier.fillMaxWidth())
            }
        }
    }
}

@Composable
fun CpDialog(viewModel: TesterViewModel) {
    val ui by viewModel.cp.collectAsStateWithLifecycle()
    val ch = ui.charts
    val origin = ch.startedAt
    FullDialog(onDismiss = { viewModel.closeOverlay() }) {
        DialogHead("Замер КП Бутейко", onClose = { viewModel.closeOverlay() })
        if (ui.ui == SessionUi.Recording || ui.ui == SessionUi.Afterglow || ui.ui == SessionUi.Done) {
            val sec = if (ui.ui == SessionUi.Recording) {
                ((System.currentTimeMillis() - (ch.startedAt ?: 0)) / 1000).toInt()
            } else {
                ch.durationSec
            }
            Text("КП $sec сек", fontSize = 32.sp, fontWeight = FontWeight.Bold, color = OkGreen)
        }
        Muted(
            when (ui.ui) {
                SessionUi.Ready -> "Нажмите СТАРТ. Графики — SpO₂ кольца и ЧСС ремня/кольца."
                SessionUi.Recording -> "Идёт замер"
                SessionUi.Afterglow -> "Восстановление 45 с"
                SessionUi.Done -> "Завершено"
                SessionUi.Idle -> ""
            },
        )
        ChartPanel("SpO₂", lastNowText(ch.spo2) { "%.0f %%".format(it) }) {
            TimeSeriesChart(
                listOf(ChartSeries(ch.spo2, OkGreen)),
                ch.windowSec,
                { "%.0f".format(it) },
                85f,
                100f,
                markers = finishMarker(ch.stopAt),
                timeOrigin = origin,
            )
        }
        ChartPanel("ЧСС", lastNowText(ch.hrCoospo.ifEmpty { ch.hrWellue }) { "%.0f bpm".format(it) }) {
            TimeSeriesChart(
                listOf(
                    ChartSeries(ch.hrCoospo, BeltRed),
                    ChartSeries(ch.hrWellue, Orange, dashed = true),
                ),
                ch.windowSec,
                { "%.0f".format(it) },
                40f,
                180f,
                markers = finishMarker(ch.stopAt),
                timeOrigin = origin,
            )
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            if (ui.ui == SessionUi.Ready) {
                PrimaryButton("СТАРТ", onClick = { viewModel.startCp() }, modifier = Modifier.weight(1f))
            }
            if (ui.ui == SessionUi.Recording) {
                PrimaryButton("Завершить", onClick = { viewModel.finishCp() }, modifier = Modifier.weight(1f))
            }
        }
    }
}

@Composable
fun PracticeDialog(viewModel: TesterViewModel) {
    val ui by viewModel.practice.collectAsStateWithLifecycle()
    val ch = ui.charts
    val origin = ch.startedAt
    FullDialog(onDismiss = { viewModel.closeOverlay() }) {
        DialogHead("Практика Бутейко на Тренажёре", onClose = { viewModel.closeOverlay() })
        if (ui.ui == SessionUi.Recording || ui.ui == SessionUi.Afterglow || ui.ui == SessionUi.Done) {
            val sec = if (ui.ui == SessionUi.Recording) {
                ((System.currentTimeMillis() - (ch.startedAt ?: 0)) / 1000).toInt()
            } else ch.durationSec
            Text(Formatters.practiceTimer(sec), fontSize = 40.sp, fontWeight = FontWeight.Bold, color = OkGreen)
            if (ui.ui == SessionUi.Afterglow) Muted("Ещё 60 с записи после завершения")
        } else {
            Muted("Нет контакта датчиков — графики появятся при подключении")
        }
        ChartPanel("Сатурация (кольцо)", lastNowText(ch.spo2) { "%.0f %%".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(ch.spo2, OkGreen)), ch.windowSec, { "%.0f".format(it) }, 85f, 100f, finishMarker(ch.stopAt), origin)
        }
        ChartPanel("HR", lastNowText(ch.hr) { "%.0f bpm".format(it) }) {
            TimeSeriesChart(
                listOf(ChartSeries(ch.hr, if (ui.useBelt) BeltRed else Orange, dashed = !ui.useBelt)),
                ch.windowSec,
                { "%.0f".format(it) },
                40f,
                180f,
                finishMarker(ch.stopAt),
                origin,
            )
        }
        ChartPanel("R-R", lastNowText(ch.rr) { "%.0f мс".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(ch.rr, BeltRed)), ch.windowSec, { "%.0f".format(it) }, 400f, 1200f, finishMarker(ch.stopAt), origin)
        }
        ChartPanel("Минутная вентиляция", lastNowText(ch.ve) { "%.1f л/мин".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(ch.ve, FlowViolet)), ch.windowSec, { "%.1f".format(it) }, 0f, 20f, finishMarker(ch.stopAt), origin)
        }
        ChartPanel("Поток", lastNowText(ch.slm) { "%.2f л/мин".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(ch.slm, FlowViolet, maxGapMs = 2500)), ch.windowSec, { "%.1f".format(it) }, -20f, 20f, finishMarker(ch.stopAt), origin)
        }
        ChartPanel("O₂ · рабочий", lastNowText(ch.o2) { "%.1f %%".format(it) }) {
            TimeSeriesChart(listOf(ChartSeries(ch.o2, OkGreen)), ch.windowSec, { "%.1f".format(it) }, 15f, 21f, finishMarker(ch.stopAt), origin)
        }
        ChartPanel("CO₂ · рабочий", lastNowText(ch.co2) { "%.2f %%".format(it) }) {
            val series = mutableListOf(ChartSeries(ch.co2, Co2Blue))
            if (ch.co2Raw.any { it.v != null }) series += ChartSeries(ch.co2Raw, Co2Raw, dashed = true)
            TimeSeriesChart(series, ch.windowSec, { "%.2f".format(it) }, 0f, 8f, finishMarker(ch.stopAt), origin)
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            if (ui.ui == SessionUi.Ready) PrimaryButton("СТАРТ", onClick = { viewModel.startPractice() }, modifier = Modifier.weight(1f))
            if (ui.ui == SessionUi.Recording) PrimaryButton("Завершить", onClick = { viewModel.finishPractice() }, modifier = Modifier.weight(1f))
            if (ui.ui == SessionUi.Done || ui.ui == SessionUi.Afterglow) {
                SecondaryButton("Повторение", onClick = { viewModel.repeatPractice() }, modifier = Modifier.weight(1f))
            }
        }
    }
}

@Composable
fun IgtDialog(viewModel: TesterViewModel) {
    val ui by viewModel.igt.collectAsStateWithLifecycle()
    val status by viewModel.status.collectAsStateWithLifecycle()
    val report by viewModel.igtMonitorReport.collectAsStateWithLifecycle()
    val igt = status.igt
    val proto = viewModel.selectedProtocol()
    val ch = ui.charts
    val page = when {
        ui.monitorOpen -> "monitor"
        ui.editorOpen -> "editor"
        ui.testOpen -> "test"
        else -> "main"
    }
    FullDialog(
        onDismiss = {
            when {
                ui.monitorOpen -> viewModel.closeIgtMonitorReport()
                ui.editorOpen -> viewModel.closeIgtEditor()
                ui.testOpen -> viewModel.closeIgtTest()
                else -> viewModel.closeOverlay()
            }
        },
        pageKey = page,
    ) {
        when {
            ui.monitorOpen -> IgtMonitorReportContent(report, onClose = { viewModel.closeIgtMonitorReport() })
            ui.editorOpen -> IgtEditorContent(viewModel, ui)
            ui.testOpen -> IgtTestContent(viewModel, ui, igt)
            else -> IgtMainContent(viewModel, ui, status, igt, proto, ch)
        }
    }
}

@Composable
private fun IgtMainContent(
    viewModel: TesterViewModel,
    ui: IgtUi,
    status: DeviceStatus,
    igt: IgtStatus,
    proto: IgtProtocol,
    ch: SessionCharts,
) {
    DialogHead(
            if (IgtLogic.isIdle(igt)) "ИГТ" else "ИГТ · ${igt.name.ifBlank { proto.name }}",
            onClose = { viewModel.closeOverlay() },
        )
        if (ui.step != IgtStep.Setup) {
            val badgeColor = when (igt.state) {
                "hypoxia" -> Orange
                "normoxia" -> NormoxiaGreen
                else -> TextMuted
            }
            Text(IgtLogic.phaseBadge(igt, ui.step), color = badgeColor, fontSize = 20.sp, fontWeight = FontWeight.Bold)
            Text(IgtLogic.countdownText(igt, ui.step), fontSize = 32.sp, fontWeight = FontWeight.Bold, color = TextMain)
            Muted(IgtLogic.statusLine(igt, status.sensors))
        }

        if (ui.step == IgtStep.Setup) {
            Muted("Протокол")
            ui.protocols.forEach { p ->
                Pill(
                    if (p.builtin) p.name else "${p.name} (свой)",
                    selected = p.id == ui.selectedId,
                    onClick = { viewModel.selectIgtProtocol(p.id) },
                )
            }
            Muted(IgtLogic.protocolMeta(proto))
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
                SecondaryButton("Создать / редактировать", onClick = { viewModel.openIgtEditor(false) }, modifier = Modifier.weight(1f))
                SecondaryButton("Тест-драйв", onClick = { viewModel.openIgtTest() }, modifier = Modifier.weight(1f))
            }
            IgtLogic.warnings(status.sensors).forEach { Muted("• $it") }
            val blocks = IgtLogic.blockers(status.sensors)
            if (blocks.isNotEmpty()) Text("Нельзя начать подготовку: ${blocks.joinToString(", ")}", color = ErrRed)
            if (ui.actionError.isNotBlank()) Text(ui.actionError, color = ErrRed)
        }

        if (ui.step == IgtStep.Prep || ui.step == IgtStep.Run || ui.step == IgtStep.Review || ui.step == IgtStep.Test) {
            if (ui.step == IgtStep.Review) {
                SecondaryButton(
                    if (ui.showFullSessionChart) "Окно 2 цикла" else "График всей сессии",
                    onClick = { viewModel.toggleIgtFullSessionChart() },
                    modifier = Modifier.fillMaxWidth(),
                )
            }
            IgtCharts(ch)
        }

        if (ui.step == IgtStep.Review) {
            Muted(IgtLogic.statusLine(igt, status.sensors))
            OutlinedTextField(ui.comment, { viewModel.setIgtComment(it) }, label = { Text("Комментарий") }, modifier = Modifier.fillMaxWidth())
        }

        if (ui.confirmFillTimeout) {
            OxyCard {
                Text(IgtLogic.fillTimeoutMessage(igt))
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    PrimaryButton("Продолжить", onClick = { viewModel.igtFillContinue() }, modifier = Modifier.weight(1f))
                    SecondaryButton("Остановить", onClick = { viewModel.igtFillAbort() }, modifier = Modifier.weight(1f))
                }
            }
        }

        if (ui.confirmAbort) {
            OxyCard {
                Text("Прервать практику? Клапаны и мотор выключатся, Servo1 уйдёт в нормоксию.")
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    PrimaryButton("Прервать", onClick = { viewModel.confirmIgtAbort() }, modifier = Modifier.weight(1f))
                    SecondaryButton("Отмена", onClick = { viewModel.cancelIgtAbort() }, modifier = Modifier.weight(1f))
                }
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
            if (ui.step == IgtStep.Setup || ui.step == IgtStep.Prep) {
                PrimaryButton(
                    "Начать практику",
                    onClick = { viewModel.igtStart() },
                    enabled = if (ui.step == IgtStep.Prep) igt.readyToStart else !IgtLogic.isBusy(igt),
                    modifier = Modifier.weight(1f),
                )
            }
            if (ui.step == IgtStep.Setup) {
                SecondaryButton(
                    "Подготовка",
                    onClick = { viewModel.igtPrepare() },
                    enabled = IgtLogic.blockers(status.sensors).isEmpty() && IgtLogic.isIdle(igt),
                    modifier = Modifier.weight(1f),
                )
            }
            if (IgtLogic.canSkipBufferPrep(igt)) {
                SecondaryButton("Пропустить переподготовку", onClick = { viewModel.igtSkipBuffer() })
            }
            if (ui.step == IgtStep.Prep || ui.step == IgtStep.Run || ui.step == IgtStep.Test) {
                SecondaryButton("Прервать", onClick = { viewModel.requestIgtAbort() })
            }
            if (ui.step == IgtStep.Review) {
                SecondaryButton("К протоколам", onClick = { viewModel.igtAck() }, modifier = Modifier.weight(1f))
            }
            SecondaryButton("Отчёт мониторинга", onClick = { viewModel.openIgtMonitorReport() })
        }
}

@Composable
private fun IgtEditorContent(viewModel: TesterViewModel, ui: IgtUi) {
    DialogHead("Протокол ИГТ", onClose = { viewModel.closeIgtEditor() })
    OutlinedTextField(ui.editName, { viewModel.setIgtEdit(name = it) }, label = { Text("Название") }, modifier = Modifier.fillMaxWidth())
    OutlinedTextField(ui.editO2, { viewModel.setIgtEdit(o2 = it) }, label = { Text("O₂ буфера гипоксии, %") }, modifier = Modifier.fillMaxWidth())
    OutlinedTextField(ui.editHyp, { viewModel.setIgtEdit(hyp = it) }, label = { Text("Гипоксия, мин") }, modifier = Modifier.fillMaxWidth())
    OutlinedTextField(ui.editNor, { viewModel.setIgtEdit(nor = it) }, label = { Text("Нормоксия, мин") }, modifier = Modifier.fillMaxWidth())
    OutlinedTextField(ui.editCycles, { viewModel.setIgtEdit(cycles = it) }, label = { Text("Циклы") }, modifier = Modifier.fillMaxWidth())
    if (ui.formError.isNotBlank()) Text(ui.formError, color = ErrRed)
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth()) {
        SecondaryButton("Новый", onClick = { viewModel.openIgtEditor(true) }, modifier = Modifier.weight(1f))
        PrimaryButton("Сохранить", onClick = { viewModel.saveIgtProtocol() }, modifier = Modifier.weight(1f))
        if (!ui.editorNew && ui.selectedId != IgtLogic.BUILTIN.id) {
            SecondaryButton("Удалить", onClick = { viewModel.deleteIgtProtocol() }, modifier = Modifier.weight(1f))
        }
    }
}

@Composable
private fun IgtTestContent(viewModel: TesterViewModel, ui: IgtUi, igt: IgtStatus) {
    DialogHead("Тест-драйв", onClose = { viewModel.closeIgtTest() })
    Muted("Отдельные шаги подготовки буфера. Доступно только когда ИГТ в состоянии idle.")
    val running = IgtLogic.isTest(igt)
    val busy = IgtLogic.isBusy(igt) && !running
    IgtLogic.TEST_ACTIONS.forEach { (action, label) ->
        SecondaryButton(label, onClick = { viewModel.igtTestStart(action) }, enabled = !running && !busy, modifier = Modifier.fillMaxWidth())
    }
    Muted(if (running) (IgtLogic.TEST_ACTIONS.find { it.first == igt.testAction }?.second ?: "Выполняется…") else if (busy) "ИГТ занят" else "Выберите действие")
    if (running) Text(IgtLogic.formatMmSs(igt.phaseRemainSec), fontSize = 28.sp, fontWeight = FontWeight.Bold)
    if (running) SecondaryButton("Остановить", onClick = { viewModel.igtTestStop() }, modifier = Modifier.fillMaxWidth())
    if (ui.actionError.isNotBlank()) Text(ui.actionError, color = ErrRed)
}

@Composable
private fun IgtCharts(ch: SessionCharts) {
    val chartOpts = igtChartOpts(ch)
    ChartPanel("O₂ · гипоксия", lastNowText(ch.o2) { "%.1f %%".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.o2, Orange)), ch.windowSec, { "%.1f".format(it) }, 9f, 21f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("Давление · гипоксия", lastNowText(ch.pressure) { "%.1f hPa".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.pressure, Orange)), ch.windowSec, { "%.1f".format(it) }, 950f, 1050f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("Минутная вентиляция", lastNowText(ch.ve) { "%.1f л/мин".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.ve, FlowViolet)), ch.windowSec, { "%.1f".format(it) }, 0f, 20f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("Поток", lastNowText(ch.slm) { "%.2f л/мин".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.slm, FlowViolet, maxGapMs = 2500)), ch.windowSec, { "%.1f".format(it) }, -20f, 20f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("O₂ · рабочий", lastNowText(ch.workO2) { "%.1f %%".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.workO2, OkGreen)), ch.windowSec, { "%.1f".format(it) }, 15f, 21f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("Давление · рабочий", lastNowText(ch.workPressure) { "%.1f hPa".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.workPressure, Orange)), ch.windowSec, { "%.1f".format(it) }, 950f, 1050f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("CO₂ · рабочий", lastNowText(ch.co2) { "%.2f %%".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.co2, Co2Blue)), ch.windowSec, { "%.2f".format(it) }, 0f, 8f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("Сатурация (кольцо)", lastNowText(ch.spo2) { "%.0f %%".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.spo2, OkGreen)), ch.windowSec, { "%.0f".format(it) }, 85f, 100f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("HR", lastNowText(ch.hr) { "%.0f bpm".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.hr, BeltRed)), ch.windowSec, { "%.0f".format(it) }, 40f, 180f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
    ChartPanel("R-R", lastNowText(ch.rr) { "%.0f мс".format(it) }) {
        TimeSeriesChart(listOf(ChartSeries(ch.rr, BeltRed)), ch.windowSec, { "%.0f".format(it) }, 400f, 1200f, chartOpts.markers, chartOpts.origin, chartOpts.scroll)
    }
}

private data class IgtChartOpts(
    val markers: List<com.oxypulse.esp32tester.ui.charts.ChartMarker>,
    val origin: Long?,
    val scroll: Boolean,
)

private fun igtChartOpts(ch: SessionCharts): IgtChartOpts = IgtChartOpts(
    markers = ch.phaseMarkers,
    origin = ch.chartOrigin,
    scroll = ch.scrollLive && !ch.fullSession,
)

@Composable
private fun ChartPanel(title: String, now: String, content: @Composable () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(4.dp), modifier = Modifier.fillMaxWidth().padding(vertical = 6.dp)) {
        Row(Modifier.fillMaxWidth()) {
            Text(title, modifier = Modifier.weight(1f))
            Text(now, color = TextMuted)
        }
        content()
    }
}

private fun finishMarker(stopAt: Long?) =
    if (stopAt != null) listOf(com.oxypulse.esp32tester.ui.charts.ChartMarker(stopAt, MarkerBlue)) else emptyList()
