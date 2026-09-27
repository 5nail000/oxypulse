package com.oxypulse.esp32tester.domain

object IgtLogic {
    val BUILTIN = IgtProtocol(
        id = "daily",
        name = "Гипоксия/Нормоксия на каждый день",
        o2Target = 15f,
        hypoxiaMin = 6,
        normoxiaMin = 5,
        cycles = 6,
        builtin = true,
    )

    val TEST_ACTIONS = listOf(
        "hypo_evac" to "Опустошение буфера гипоксии (V1)",
        "hyper_evac" to "Опустошение буфера гипероксии (V2)",
        "work_flush" to "Продувка рабочего контура (V3)",
        "hypo_fill" to "Первичное заполнение буфера гипоксии (V4)",
        "hypo_recirculate" to "Рециркуляция гипоксии до 15% O₂ (V1+V4)",
    )

    fun isIdle(igt: IgtStatus?) = igt == null || igt.state.isBlank() || igt.state == "idle"
    fun isBusy(igt: IgtStatus?) = !isIdle(igt)
    fun isTest(igt: IgtStatus?) = igt?.state == "test"
    fun isPrep(igt: IgtStatus?) = igt?.state in PREP_STATES
    fun isRun(igt: IgtStatus?) = igt?.state == "hypoxia" || igt?.state == "normoxia"
    fun isReview(igt: IgtStatus?) = igt?.state in REVIEW_STATES
    fun isLive(igt: IgtStatus?) = isPrep(igt) || isRun(igt) || isTest(igt)

    fun canSkipBufferPrep(igt: IgtStatus?): Boolean {
        if (igt == null || igt.state != "normoxia") return false
        if (igt.cycle >= igt.cycles) return false
        return igt.bufferPrep == "prime" || igt.bufferPrep == "primary_fill"
    }

    fun blockers(sensors: Sensors): List<String> {
        val errs = mutableListOf<String>()
        if (!sensors.hypoxiaO2.ok) errs += "нет O₂ буфера гипоксии"
        if (!sensors.hypoxiaPressure.ok) errs += "нет давления буфера гипоксии"
        return errs
    }

    fun warnings(sensors: Sensors): List<String> {
        val items = mutableListOf<String>()
        if (!sensors.bridge.ok) items += "UART-мост BLE не на связи"
        if (!sensors.wellue.ok || !sensors.wellue.contact) items += "кольцо Wellue не подключено"
        if (!sensors.coospo.ok || !sensors.coospo.contact) items += "нагрудный ремень не подключён"
        if (!sensors.workO2.ok) items += "нет O₂ рабочего контура"
        if (!sensors.workPressure.ok) items += "нет давления рабочего контура"
        if (!sensors.co2.ok) items += "нет CO₂ рабочего контура"
        return items
    }

    fun protocolMeta(p: IgtProtocol): String {
        val total = (p.hypoxiaMin + p.normoxiaMin) * 60 * p.cycles
        return "${fmt1(p.o2Target)}% O₂ · ${p.hypoxiaMin} мин гипоксия / ${p.normoxiaMin} мин нормоксия × ${p.cycles} = ${formatMmSs(total)}"
    }

    fun liveChartWindowSec(p: IgtProtocol, igt: IgtStatus?): Int {
        val hyp = igt?.hypoxiaSec?.takeIf { it > 0 } ?: p.hypoxiaMin * 60
        val nor = igt?.normoxiaSec?.takeIf { it > 0 } ?: p.normoxiaMin * 60
        return maxOf(120, 2 * (hyp + nor))
    }

    fun protocolWindowSec(p: IgtProtocol, igt: IgtStatus?): Int {
        val fromIgt = if (igt != null && igt.hypoxiaSec > 0 && igt.normoxiaSec > 0 && igt.cycles > 0) {
            (igt.hypoxiaSec + igt.normoxiaSec) * igt.cycles
        } else {
            0
        }
        val fromP = (p.hypoxiaMin + p.normoxiaMin) * 60 * p.cycles
        val total = maxOf(fromIgt, fromP, 300)
        return minOf(4500, maxOf(300, total + 60))
    }

    fun fillTimeoutMessage(igt: IgtStatus): String {
        val next = when {
            igt.state == "test" && igt.testAction == "hypo_fill" -> "завершить тест наполнения"
            igt.bufferPrep == "primary_fill" || igt.state == "primary_fill" ->
                "перейти к рециркуляции"
            else -> "продолжить"
        }
        return "Наполнение не завершилось за отведённое время (${igt.error.ifBlank { "таймаут" }}). $next?"
    }

    fun validateCustom(name: String, o2: Float, hyp: Int, nor: Int, cycles: Int): String? {
        if (name.isBlank()) return "Укажите название"
        if (o2 < 9f || o2 > 19f) return "O₂ 9–19%"
        if (hyp !in 1..15) return "Гипоксия 1–15 мин"
        if (nor !in 1..15) return "Нормоксия 1–15 мин"
        if (cycles !in 1..12) return "Циклы 1–12"
        return null
    }

    fun statusLine(igt: IgtStatus, sensors: Sensors): String {
        val o2 = if (sensors.hypoxiaO2.ok) "${fmt1(sensors.hypoxiaO2.percent)}%" else "—"
        return when (igt.state) {
            "check_sensors" -> "Проверка датчиков…"
            "prime" -> primeText(sensors)
            "primary_fill" -> "Первичное наполнение буфера · осталось ${formatMmSs(igt.phaseRemainSec)}"
            "recirculate" -> "Рециркуляция $o2 → ${fmt1(igt.o2Target)}%"
            "ready" -> "Буфер готов ($o2). Можно начинать практику."
            "hypoxia" -> if (igt.sensorWarn) {
                "Гипоксия · нет части датчиков, фазы идут по таймеру"
            } else {
                "Фаза гипоксии"
            }
            "normoxia" -> normoxiaLine(igt, sensors, o2)
            "done" -> igt.error.ifBlank { "Протокол завершён" }
            "aborted" -> igt.error.ifBlank { "Практика прервана" }
            "error" -> igt.error.ifBlank { "Ошибка подготовки" }
            "test" -> TEST_ACTIONS.find { it.first == igt.testAction }?.second ?: "Тест-драйв"
            else -> ""
        }
    }

    fun phaseBadge(igt: IgtStatus, step: IgtStep): String = when {
        step == IgtStep.Prep -> "Подготовка"
        igt.state == "hypoxia" -> "Гипоксия ${igt.cycle}/${igt.cycles}"
        igt.state == "normoxia" -> "Нормоксия ${igt.cycle}/${igt.cycles}"
        igt.state == "done" -> "Завершено"
        igt.state == "aborted" -> "Прервано"
        igt.state == "error" -> "Ошибка"
        igt.state == "test" -> "Тест-драйв"
        else -> "—"
    }

    fun countdownText(igt: IgtStatus, step: IgtStep): String {
        if (step != IgtStep.Prep && step != IgtStep.Run && step != IgtStep.Test) return "—"
        if (igt.readyToStart) return "готово"
        if (igt.waitingBuffer) return "буфер"
        return formatMmSs(igt.phaseRemainSec)
    }

    fun formatMmSs(sec: Int): String {
        val s = sec.coerceAtLeast(0)
        return "%02d:%02d".format(s / 60, s % 60)
    }

    fun stepOf(igt: IgtStatus?): IgtStep = when {
        isPrep(igt) -> IgtStep.Prep
        isRun(igt) -> IgtStep.Run
        isTest(igt) -> IgtStep.Test
        isReview(igt) -> IgtStep.Review
        else -> IgtStep.Setup
    }

    private fun primeText(s: Sensors): String {
        val p = if (s.hypoxiaPressure.ok) "${fmt1(s.hypoxiaPressure.hpa)} hPa" else "—"
        val wp = if (s.workPressure.ok) "${fmt1(s.workPressure.hpa)} hPa" else "—"
        val fromBase = if (s.hypoxiaPressure.ok && s.hypoxiaPressure.baselineOk) {
            fmt1(s.hypoxiaPressure.baselineHpa - s.hypoxiaPressure.hpa)
        } else {
            "—"
        }
        val delta = if (s.hypoxiaPressure.ok && s.workPressure.ok) {
            fmt1(s.workPressure.hpa - s.hypoxiaPressure.hpa)
        } else {
            "—"
        }
        return "Выкачка буфера · гипо $p (от базы −$fromBase), раб. $wp, Δ $delta"
    }

    private fun normoxiaLine(igt: IgtStatus, sensors: Sensors, o2: String): String {
        if (igt.waitingBuffer) return "Нормоксия завершена · ждём готовности буфера гипоксии"
        return when (igt.bufferPrep) {
            "prime" -> "Нормоксия · ${primeText(sensors)}"
            "primary_fill" -> "Нормоксия · наполнение буфера · ${formatMmSs(igt.bufferPrepRemainSec)}"
            "recirculate" -> "Нормоксия · рециркуляция $o2 → ${fmt1(igt.o2Target)}%"
            "ready" -> if (igt.cycle < igt.cycles) {
                "Нормоксия · буфер гипоксии готов к следующему циклу"
            } else if (igt.sensorWarn) {
                "Нормоксия · нет части датчиков, фазы идут по таймеру"
            } else {
                "Фаза нормоксии"
            }
            else -> if (igt.sensorWarn) {
                "Нормоксия · нет части датчиков, фазы идут по таймеру"
            } else {
                "Фаза нормоксии"
            }
        }
    }

    private fun fmt1(v: Float) = "%.1f".format(v)

    private val PREP_STATES = setOf("check_sensors", "prime", "primary_fill", "recirculate", "ready")
    private val REVIEW_STATES = setOf("done", "aborted", "error")
}
