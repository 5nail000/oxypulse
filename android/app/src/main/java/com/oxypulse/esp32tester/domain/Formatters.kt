package com.oxypulse.esp32tester.domain

object Formatters {
    fun pinLabel(pin: Int): String = when (pin) {
        25 -> "GPIO 25 · V1"
        26 -> "GPIO 26 · V2"
        27 -> "GPIO 27 · V3"
        13 -> "GPIO 13 · V4"
        14 -> "GPIO 14 · V5"
        21 -> "GPIO 21 · V6"
        33 -> "GPIO 33 · O₂ мотор"
        4 -> "GPIO 4 · Servo1"
        else -> "GPIO $pin"
    }

    fun flowPhase(phase: String): String = when (phase) {
        "inhale" -> "Вдох"
        "exhale" -> "Выдох"
        else -> "Покой"
    }

    fun o2Text(s: O2Status): String =
        if (s.ok) "O₂: ${"%.1f".format(s.percent)} % (${"%.2f".format(s.mv)} mV)" else "нет данных"

    fun ocsText(s: O2Status): String {
        val hasFields = s.fieldA != 0f || s.fieldB != 0f || s.fieldC != 0f || s.percent != 0f
        if (!s.ok && !hasFields) return "нет данных"
        val o2Raw = if (s.o2Field == 7 || s.percent != 0f) s.percent else s.fieldA
        val t = if (s.tempC != 0f) s.tempC else s.fieldC
        val pKpa = when {
            s.pressureHpa > 0f -> s.pressureHpa / 10f
            s.statusRaw in 300..15000 -> s.statusRaw / 10f
            else -> Float.NaN
        }
        return buildString {
            append("O₂ (OCS): ${"%.1f".format(o2Raw)} %")
            if (!s.percentCo2Comp.isNaN()) {
                append("\nO₂ (CO₂): ${"%.1f".format(s.percentCo2Comp)} %")
            }
            append("\nRH (OCS): ${"%.1f".format(s.rh)} %")
            append("\nT (OCS): ${"%.1f".format(t)} °C")
            if (!pKpa.isNaN()) {
                append("\nP: ${"%.1f".format(pKpa)} кПа")
            }
            if (s.frame.isNotBlank() && s.frame != "live") {
                append("\nкадр: ${s.frame}")
            }
        }
    }

    fun isOcsWorkSensor(s: O2Status): Boolean =
        s.sensor.contains("OCS", ignoreCase = true) ||
            s.sensor.contains("3RL", ignoreCase = true)

    fun pressureText(s: PressureStatus): String {
        if (s.sensor == "OCS-3RL") {
            val raw = if (s.hpa > 0f) s.hpa else (s.pInKpa * 256 + s.pOutKpa).toFloat()
            if (raw <= 0f) return "нет данных"
            return "${"%.1f".format(raw / 10f)} кПа"
        }
        if (!s.ok) return "нет данных"
        val name = s.sensor.ifBlank { "P" }
        return "$name: ${"%.1f".format(s.hpa)} hPa, ${"%.1f".format(s.tempC)} °C"
    }
    fun flowText(s: FlowStatus): String =
        if (s.ok) {
            "Поток: ${"%.2f".format(s.slm)} л/мин\n" +
                "Вдох (>0): ${"%.3f".format(s.inhaleL)} л\n" +
                "Выдох (<0): ${"%.3f".format(s.exhaleL)} л\n" +
                "Минутная вентиляция (30 с): ${"%.1f".format(s.veLpm)} л/мин"
        } else {
            "нет данных"
        }

    fun co2Text(s: Co2Status): String {
        val hasStream = s.ticks >= 0 || !s.tempC.isNaN() || !s.rh.isNaN() || !s.shtTempC.isNaN()
        if (!s.ok && !hasStream) return "нет данных"
        val warmup = if (s.warmingUp) " (прогрев)" else ""
        val pct = co2Percent(s)
        val est = if (s.dynamicComp) ", оценка" else ""
        val rawLine = if (s.dynamicComp) {
            val rawPct = s.ppmRaw / 10000f
            "\nДатчик: ${"%.2f".format(rawPct)} % (${s.ppmRaw} ppm)"
        } else {
            ""
        }
        val temp = s.tempCEst ?: s.tempC
        val rh = s.rhEst ?: s.rh
        val extra = buildString {
            if (s.ticks >= 0) append("\nticks: ${s.ticks}")
            if (!temp.isNaN()) append("\nT(STC31): ${"%.2f".format(temp)} °C")
            if (!rh.isNaN()) append("\nRH(SHT): ${"%.1f".format(rh)} %")
            if (!s.shtTempC.isNaN()) append("\nT(SHT): ${"%.1f".format(s.shtTempC)} °C")
        }
        return "CO₂: ${"%.3f".format(pct)} % (${s.ppm} ppm$est)$warmup$rawLine$extra"
    }

    fun co2Percent(s: Co2Status): Float {
        // percent — источник истины (STC31 vol%); ppm обрезается на uint16 при >6.55%.
        if (s.percent != 0f) return s.percent
        if (s.ppm != 0) return s.ppm / 10000f
        return s.percent
    }

    fun wellueText(s: WellueStatus): String {
        val error = s.error.takeIf { it.isNotBlank() }
        if (!s.connected) return error ?: "поиск…"
        if (!s.ok) return if (error != null) "подключено — нет данных\n$error" else "подключено — нет данных"
        val name = s.name.ifBlank { "Wellue" }
        return "$name\nSpO₂: ${s.spo2} %\nЧСС: ${s.hr} bpm\nБатарея: ${s.battery} %\nКонтакт: ${if (s.contact) "да" else "нет"}\nMotion: ${s.motion}"
    }

    fun coospoText(s: CoospoStatus): String {
        val error = s.error.takeIf { it.isNotBlank() }
        if (!s.connected) return error ?: "поиск…"
        if (!s.ok) return if (error != null) "подключено — нет данных\n$error" else "подключено — нет данных"
        val name = s.name.ifBlank { "Нагрудный HR" }
        val rr = if (s.rrMs > 0) "\nR-R: ${s.rrMs} мс" else ""
        return "$name\nЧСС: ${s.bpm} bpm$rr\nКонтакт: ${if (s.contact) "да" else "нет"}"
    }

    fun bleOk(s: WellueStatus) = s.connected && s.ok
    fun bleOk(s: CoospoStatus) = s.connected && s.ok

    fun wifiModalStatus(wifi: WifiStatus): String {
        val parts = mutableListOf("AP ${wifi.apSsid} → ${wifi.apIp}")
        val sta = wifi.sta
        if (sta.configured) {
            if (sta.connected) {
                parts += "STA «${sta.ssid}» → ${sta.ip} (${sta.rssi} dBm)"
            } else {
                parts += "STA «${sta.ssid}»: ${sta.error.ifBlank { "не подключена" }}"
            }
        } else {
            parts += "STA не настроена"
        }
        return parts.joinToString(" · ")
    }

    fun wifiLanHint(wifi: WifiStatus, host: String): String? {
        val sta = wifi.sta
        val onAp = host.isBlank() || host == wifi.apIp || host.startsWith("192.168.4.")
        if (sta.connected && sta.ip.isNotBlank()) {
            return if (onAp) {
                "В LAN: ${sta.ip} (${sta.ssid.ifBlank { "WiFi" }})"
            } else {
                "STA: ${sta.ssid.ifBlank { "—" }} · ${sta.ip} · RSSI ${sta.rssi} dBm"
            }
        }
        if (sta.configured && sta.error.isNotBlank()) {
            return "STA «${sta.ssid.ifBlank { "—" }}»: ${sta.error}"
        }
        return null
    }

    fun formatAge(ms: Long): String {
        if (ms < 1000) return "только что"
        val sec = ms / 1000
        if (sec < 60) return "$sec с назад"
        return "${sec / 60} мин назад"
    }

    fun hrvDurationLabel(sec: Int): String = when {
        sec < 60 -> "$sec сек"
        sec % 60 == 0 -> "${sec / 60} мин"
        else -> "${sec / 60}:${"%02d".format(sec % 60)}"
    }

    fun remainMmSs(ms: Long): String {
        val total = ((ms + 999) / 1000).coerceAtLeast(0)
        return "${total / 60}:${"%02d".format(total % 60)}"
    }

    fun practiceTimer(sec: Int): String =
        if (sec < 60) sec.toString() else "%d:%02d".format(sec / 60, sec % 60)
}
