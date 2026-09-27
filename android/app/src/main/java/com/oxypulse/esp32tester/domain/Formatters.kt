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

    fun pressureText(s: PressureStatus): String =
        if (s.ok) "${"%.1f".format(s.hpa)} hPa, ${"%.1f".format(s.tempC)} °C" else "нет данных"

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
        if (!s.ok) return "нет данных"
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
        val tempRaw = if (s.dynamicComp) " (датчик ${"%.1f".format(s.tempC)})" else ""
        val rhRaw = if (s.dynamicComp) " (датчик ${"%.1f".format(s.rh)})" else ""
        return "CO₂: ${"%.2f".format(pct)} % (${s.ppm} ppm$est)$warmup$rawLine\n" +
            "T: ${"%.1f".format(temp)} °C$tempRaw\nRH: ${"%.1f".format(rh)} %$rhRaw"
    }

    fun co2Percent(s: Co2Status): Float {
        if (s.ppm != 0) return (s.ppm / 10000f)
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
