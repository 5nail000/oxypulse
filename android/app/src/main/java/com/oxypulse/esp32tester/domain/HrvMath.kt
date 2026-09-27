package com.oxypulse.esp32tester.domain

import kotlin.math.sqrt

object HrvMath {
    const val MIN_RR = 15

    fun compute(rrRaw: List<Int>): HrvMetrics {
        val raw = rrRaw.filter { it > 0 }
        val filtered = mutableListOf<Int>()
        for (v in raw) {
            if (v < 250 || v > 2000) continue
            if (filtered.isNotEmpty()) {
                val prev = filtered.last()
                if (kotlin.math.abs(v - prev) > prev * 0.25f) continue
            }
            filtered += v
        }
        val n = filtered.size
        if (n < MIN_RR) {
            return HrvMetrics(ok = false, rawCount = raw.size, filteredCount = n)
        }
        val mean = filtered.average().toFloat()
        var varSum = 0.0
        for (v in filtered) {
            val d = v - mean
            varSum += d * d
        }
        val sdnn = sqrt(varSum / (n - 1)).toFloat()
        var diffSq = 0.0
        var nn50 = 0
        val diffs = n - 1
        for (i in 1 until n) {
            val d = (filtered[i] - filtered[i - 1]).toDouble()
            diffSq += d * d
            if (kotlin.math.abs(d) > 50) nn50 += 1
        }
        val rmssd = if (diffs > 0) sqrt(diffSq / diffs).toFloat() else 0f
        val pnn50 = if (diffs > 0) nn50.toFloat() / diffs else 0f
        val sd1 = rmssd / sqrt(2.0).toFloat()
        val sd2 = sqrt((2.0 * sdnn * sdnn - sd1 * sd1).coerceAtLeast(0.0)).toFloat()
        val pairs = (0 until n - 1).map {
            PoincarePair(filtered[it].toFloat(), filtered[it + 1].toFloat())
        }
        return HrvMetrics(
            ok = true,
            rawCount = raw.size,
            filteredCount = n,
            meanRr = mean,
            hr = 60000f / mean,
            sdnn = sdnn,
            rmssd = rmssd,
            pnn50 = pnn50,
            sd1 = sd1,
            sd2 = sd2,
            pairs = pairs,
        )
    }

    fun metricItems(metrics: HrvMetrics): List<Pair<String, String>> = listOf(
        "R-R (ms)" to metrics.meanRr.toInt().toString(),
        "HR (bpm)" to metrics.hr.toInt().toString(),
        "SDNN" to metrics.sdnn.toInt().toString(),
        "rMSSD" to metrics.rmssd.toInt().toString(),
        "pNN50" to "%.2f".format(metrics.pnn50),
        "SD1 / SD2" to "${metrics.sd1.toInt()} / ${metrics.sd2.toInt()}",
    )

    fun mapApiError(code: String): String = when (code) {
        "belt_not_connected" -> "Ремень не подключён"
        "already_running", "busy" -> "Замер уже идёт, подождите секунду"
        "invalid_duration" -> "Неверная длительность"
        "empty_body", "bad_json" -> "Некорректный запрос"
        else -> code.ifBlank { "ошибка HRV" }
    }
}
