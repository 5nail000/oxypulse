package com.oxypulse.esp32tester.domain

import kotlin.math.max

data class IgtPhaseRecord(
    val key: String,
    val label: String,
    val cycle: Int,
    val startedAt: Long,
    var endedAt: Long? = null,
    var o2Start: Float? = null,
    var o2Min: Float? = null,
    var o2Max: Float? = null,
    var o2End: Float? = null,
    var inhaleL: Float? = null,
    var exhaleL: Float? = null,
    var endReason: String? = null,
    var plannedSec: Int? = null,
)

data class IgtCycleRow(
    val cycle: Int,
    val hypPlannedSec: Int,
    val norPlannedSec: Int,
    var hypActualSec: Int? = null,
    var norActualSec: Int? = null,
    var sensorWarn: Boolean = false,
)

data class IgtLinkDrop(
    val at: Long,
    val recoveredAt: Long? = null,
    val phase: String = "",
)

data class IgtMonitorReport(
    val sessionStartedAt: Long? = null,
    val sessionEndedAt: Long? = null,
    val protocolName: String = "",
    val finishReason: String = "",
    val phases: List<IgtPhaseRecord> = emptyList(),
    val cycles: List<IgtCycleRow> = emptyList(),
    val linkDrops: List<IgtLinkDrop> = emptyList(),
) {
    fun isRecent(now: Long = System.currentTimeMillis(), ttlMs: Long = 86_400_000L): Boolean {
        val end = sessionEndedAt ?: sessionStartedAt ?: return false
        return now - end <= ttlMs
    }

    companion object {
        fun empty() = IgtMonitorReport()
    }
}

class IgtMonitor {
    private var active = false
    private var protocolName = ""
    private var hypSec = 0
    private var norSec = 0
    private var totalCycles = 0
    private var sessionStartedAt: Long? = null
    private var sessionEndedAt: Long? = null
    private var finishReason = ""
    private val phases = mutableListOf<IgtPhaseRecord>()
    private val cycleRows = mutableMapOf<Int, IgtCycleRow>()
    private val linkDrops = mutableListOf<IgtLinkDrop>()
    private var currentPracticeKey: String? = null
    private var currentPrepKey: String? = null
    private var lastVolumeKey: String? = null

    fun beginSession(name: String, hypSec: Int, norSec: Int, cycles: Int, now: Long) {
        reset()
        active = true
        protocolName = name
        this.hypSec = hypSec
        this.norSec = norSec
        totalCycles = cycles
        sessionStartedAt = now
        for (c in 1..cycles) {
            cycleRows[c] = IgtCycleRow(c, hypSec, norSec)
        }
    }

    fun endSession(reason: String, now: Long) {
        closeByKey(currentPracticeKey, now, reason)
        closeByKey(currentPrepKey, now, reason)
        currentPracticeKey = null
        currentPrepKey = null
        finishReason = reason
        sessionEndedAt = now
        active = false
    }

    fun reset() {
        active = false
        protocolName = ""
        hypSec = 0
        norSec = 0
        totalCycles = 0
        sessionStartedAt = null
        sessionEndedAt = null
        finishReason = ""
        phases.clear()
        cycleRows.clear()
        linkDrops.clear()
        currentPracticeKey = null
        currentPrepKey = null
        lastVolumeKey = null
    }

    fun onStatus(igt: IgtStatus, sensors: Sensors, now: Long) {
        if (!active && !IgtLogic.isLive(igt) && !IgtLogic.isReview(igt)) return
        if (!active && IgtLogic.isLive(igt)) {
            beginSession(
                igt.name.ifBlank { protocolName },
                igt.hypoxiaSec.takeIf { it > 0 } ?: hypSec,
                igt.normoxiaSec.takeIf { it > 0 } ?: norSec,
                igt.cycles.takeIf { it > 0 } ?: totalCycles,
                now,
            )
        }

        if (IgtLogic.isReview(igt)) {
            if (sessionEndedAt == null && (active || sessionStartedAt != null)) {
                if (igt.lastPhaseVolumeOk && igt.lastPhaseKind.isNotBlank()) {
                    attachFirmwareVolume(igt, now)
                }
                endSession(finishReasonOf(igt), now)
            }
            return
        }
        if (!active) return

        val practiceKey = phaseKey(igt)
        if (practiceKey != currentPracticeKey) {
            closeByKey(currentPracticeKey, now, "смена фазы")
            openPhase(practiceKey, phaseLabel(igt), igt, sensors, now, plannedSec(igt), forceHypo = false)
            currentPracticeKey = practiceKey
        }
        updatePhase(
            phaseByKey(currentPracticeKey),
            igt,
            sensors,
            trackVolume = isPracticeVolume(practiceKey),
            forceHypo = false,
        )

        val prepKey = backgroundPrepKey(igt)
        if (prepKey != currentPrepKey) {
            closeByKey(currentPrepKey, now, "смена фазы")
            currentPrepKey = prepKey
            if (prepKey != null) {
                openPhase(prepKey, backgroundPrepLabel(igt), igt, sensors, now, planned = null, forceHypo = true)
            }
        }
        if (currentPrepKey != null) {
            updatePhase(phaseByKey(currentPrepKey), igt, sensors, trackVolume = false, forceHypo = true)
        }

        if (igt.lastPhaseVolumeOk && igt.lastPhaseKind.isNotBlank()) {
            attachFirmwareVolume(igt, now)
        }
    }

    fun snapshot(): IgtMonitorReport {
        val reason = when {
            finishReason.isNotBlank() -> finishReason
            active -> "идёт · ${phaseByKey(currentPracticeKey)?.label ?: "—"}"
            else -> ""
        }
        return IgtMonitorReport(
            sessionStartedAt = sessionStartedAt,
            sessionEndedAt = sessionEndedAt,
            protocolName = protocolName,
            finishReason = reason,
            phases = phases.toList(),
            cycles = cycleRows.values.sortedBy { it.cycle },
            linkDrops = linkDrops.toList(),
        )
    }

    fun noteLinkDrop(now: Long, phase: String) {
        if (!active && sessionStartedAt == null) return
        val last = linkDrops.lastOrNull()
        if (last != null && last.recoveredAt == null) return
        linkDrops += IgtLinkDrop(at = now, phase = phase)
    }

    fun noteLinkRecovered(now: Long) {
        val idx = linkDrops.indexOfLast { it.recoveredAt == null }
        if (idx < 0) return
        linkDrops[idx] = linkDrops[idx].copy(recoveredAt = now)
    }

    private fun finishReasonOf(igt: IgtStatus): String {
        val phase = phaseByKey(currentPracticeKey)?.label ?: phaseLabel(igt)
        return when (igt.state) {
            "done" -> igt.error.ifBlank { "протокол завершён" }
            "aborted" -> igt.error.ifBlank { "прервано · $phase" }
            "error" -> igt.error.ifBlank { "ошибка · $phase" }
            else -> igt.error.ifBlank { phase }
        }
    }

    private fun phaseByKey(key: String?): IgtPhaseRecord? {
        if (key == null) return null
        return phases.lastOrNull { it.key == key && it.endedAt == null }
    }

    private fun openPhase(
        key: String,
        label: String,
        igt: IgtStatus,
        sensors: Sensors,
        now: Long,
        planned: Int?,
        forceHypo: Boolean,
    ) {
        val o2 = o2Sample(igt, sensors, forceHypo)
        phases += IgtPhaseRecord(
            key = key,
            label = label,
            cycle = igt.cycle,
            startedAt = now,
            o2Start = o2,
            o2Min = o2,
            o2Max = o2,
            plannedSec = planned,
        )
    }

    private fun closeByKey(key: String?, now: Long, reason: String) {
        val phase = phaseByKey(key) ?: return
        phase.endedAt = now
        phase.endReason = reason
        val actualSec = max(0, ((now - phase.startedAt) / 1000L).toInt())
        when {
            phase.key.startsWith("hyp:") -> cycleRows[phase.cycle]?.hypActualSec = actualSec
            phase.key.startsWith("nor:") -> cycleRows[phase.cycle]?.norActualSec = actualSec
        }
    }

    private fun updatePhase(
        phase: IgtPhaseRecord?,
        igt: IgtStatus,
        sensors: Sensors,
        trackVolume: Boolean,
        forceHypo: Boolean,
    ) {
        if (phase == null) return
        val o2 = o2Sample(igt, sensors, forceHypo)
        if (o2 != null) {
            phase.o2Min = minOf(phase.o2Min ?: o2, o2)
            phase.o2Max = maxOf(phase.o2Max ?: o2, o2)
            phase.o2End = o2
        }
        if (igt.sensorWarn) {
            cycleRows[igt.cycle]?.sensorWarn = true
        }
        if (trackVolume && sensors.flow.ok) {
            phase.inhaleL = sensors.flow.inhaleL.coerceAtLeast(0f)
            phase.exhaleL = sensors.flow.exhaleL.coerceAtLeast(0f)
        }
    }

    private fun attachFirmwareVolume(igt: IgtStatus, now: Long) {
        val kind = igt.lastPhaseKind
        val cycle = if (igt.lastPhaseCycle > 0) igt.lastPhaseCycle else igt.cycle
        val volumeKey = "$kind:$cycle"
        if (volumeKey == lastVolumeKey) return
        lastVolumeKey = volumeKey
        val targetKey = if (kind == "hypoxia") "hyp:$cycle" else "nor:$cycle"
        val phase = phases.lastOrNull { it.key == targetKey && it.endedAt != null }
            ?: phases.lastOrNull { it.key == targetKey }
        if (phase == null) return
        phase.inhaleL = igt.lastPhaseInhaleL.coerceAtLeast(0f)
        phase.exhaleL = igt.lastPhaseExhaleL.coerceAtLeast(0f)
        phase.endedAt = phase.endedAt ?: now
    }

    private fun plannedSec(igt: IgtStatus): Int? = when (igt.state) {
        "hypoxia" -> igt.hypoxiaSec
        "normoxia" -> if (igt.waitingBuffer) null else igt.normoxiaSec
        else -> null
    }

    companion object {
        private val BG_PREP = setOf("prime", "primary_fill", "recirculate", "ready")

        fun phaseKey(igt: IgtStatus): String = when {
            igt.state == "test" -> "test:${igt.testAction.ifBlank { "?" }}"
            igt.state == "hypoxia" -> "hyp:${igt.cycle}"
            igt.state == "normoxia" -> "nor:${igt.cycle}"
            igt.bufferPrep != "idle" -> "prep:${igt.bufferPrep}:${igt.cycle}"
            else -> "st:${igt.state}:${igt.cycle}"
        }

        fun backgroundPrepKey(igt: IgtStatus): String? {
            if (!IgtLogic.isRun(igt)) return null
            if (igt.bufferPrep !in BG_PREP) return null
            return "bgprep:${igt.bufferPrep}:${igt.cycle}"
        }

        fun phaseLabel(igt: IgtStatus): String = when (igt.state) {
            "check_sensors" -> "Проверка датчиков"
            "prime" -> "Выкачка буфера"
            "primary_fill" -> "Наполнение буфера"
            "recirculate" -> "Рециркуляция"
            "ready" -> "Буфер готов"
            "hypoxia" -> "Гипоксия ${igt.cycle}/${igt.cycles}"
            "normoxia" -> "Нормоксия ${igt.cycle}/${igt.cycles}"
            "test" -> IgtLogic.TEST_ACTIONS.find { it.first == igt.testAction }?.second ?: "Тест-драйв"
            "done" -> "Завершено"
            "aborted" -> "Прервано"
            "error" -> "Ошибка: ${igt.error.ifBlank { "—" }}"
            else -> igt.state.ifBlank { "—" }
        }

        fun backgroundPrepLabel(igt: IgtStatus): String {
            val step = when (igt.bufferPrep) {
                "prime" -> "выкачка буфера"
                "primary_fill" -> "наполнение буфера"
                "recirculate" -> "рециркуляция"
                "ready" -> "буфер готов"
                else -> igt.bufferPrep
            }
            return "Фон: $step"
        }

        fun o2Sample(igt: IgtStatus, sensors: Sensors, forceHypo: Boolean = false): Float? {
            val hypo = sensors.hypoxiaO2.percent.takeIf { sensors.hypoxiaO2.ok }
            val work = sensors.workO2.percent.takeIf { sensors.workO2.ok }
            return if (forceHypo || usesHypoO2(igt)) hypo else work
        }

        private fun isPracticeVolume(key: String): Boolean =
            key.startsWith("hyp:") || key.startsWith("nor:")

        private fun usesHypoO2(igt: IgtStatus): Boolean = when (igt.state) {
            "hypoxia", "prime", "primary_fill", "recirculate", "ready", "check_sensors" -> true
            "normoxia" -> igt.bufferPrep == "prime" ||
                igt.bufferPrep == "primary_fill" ||
                igt.bufferPrep == "recirculate"
            "test" -> igt.testAction.startsWith("hypo")
            else -> false
        }

        fun formatReport(r: IgtMonitorReport): String {
            if (r.sessionStartedAt == null) return "Нет данных мониторинга."
            val sb = StringBuilder()
            sb.append("Протокол: ${r.protocolName.ifBlank { "—" }}\n")
            sb.append("Старт: ${fmtTime(r.sessionStartedAt)}\n")
            if (r.sessionEndedAt != null) {
                sb.append("Конец: ${fmtTime(r.sessionEndedAt)}\n")
            }
            sb.append("Причина: ${r.finishReason.ifBlank { "не указана" }}\n")
            if (r.linkDrops.isNotEmpty()) {
                sb.append("\n— Обрывы BLE-GATT (протокол не прерывался) —\n")
                r.linkDrops.forEach { d ->
                    val rec = d.recoveredAt
                    val dur = rec?.let { max(0, ((it - d.at) / 1000L).toInt()) }
                    sb.append("  ${fmtTime(d.at)}")
                    if (rec != null) {
                        sb.append("–${fmtTime(rec)}")
                        if (dur != null) sb.append(" (${dur} с)")
                    } else {
                        sb.append("–…")
                    }
                    if (d.phase.isNotBlank()) sb.append(" · ${d.phase}")
                    sb.append('\n')
                }
            }
            if (r.cycles.isNotEmpty()) {
                sb.append("\n— Циклы (план / факт, сек) —\n")
                r.cycles.forEach { c ->
                    sb.append(
                        "#${c.cycle}: гип ${c.hypPlannedSec}/${c.hypActualSec ?: "—"}" +
                            " · нор ${c.norPlannedSec}/${c.norActualSec ?: "—"}" +
                            if (c.sensorWarn) " · ⚠ датчики" else "",
                    )
                    sb.append('\n')
                }
            }
            sb.append("\n— Фазы —\n")
            r.phases.forEach { p ->
                val dur = p.endedAt?.let { ((it - p.startedAt) / 1000).toInt() } ?: "…"
                sb.append("${p.label} · ${dur} с")
                p.plannedSec?.let { sb.append(" (план $it с)") }
                sb.append('\n')
                if (p.o2Start != null) {
                    sb.append(
                        "  O₂: ${fmt1(p.o2Start)} → min ${fmt1(p.o2Min)} / max ${fmt1(p.o2Max)}" +
                            " → ${fmt1(p.o2End)} %\n",
                    )
                }
                if (isPracticeVolume(p.key)) {
                    sb.append(
                        "  Вент.: вдох ${fmt3(p.inhaleL ?: 0f)} л · выдох ${fmt3(p.exhaleL ?: 0f)} л\n",
                    )
                }
                p.endReason?.takeIf { it.isNotBlank() && it != "смена фазы" }?.let {
                    sb.append("  ($it)\n")
                }
            }
            return sb.toString().trim()
        }

        private fun fmtTime(ms: Long): String =
            java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.getDefault()).format(java.util.Date(ms))

        private fun fmt1(v: Float?) = v?.let { "%.1f".format(it) } ?: "—"
        private fun fmt3(v: Float?) = v?.let { "%.3f".format(it) } ?: "—"
    }
}
