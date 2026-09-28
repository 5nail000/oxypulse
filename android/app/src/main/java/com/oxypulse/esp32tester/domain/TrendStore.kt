package com.oxypulse.esp32tester.domain

class TrendStore {
    val t = mutableListOf<Long>()
    val espMs = mutableListOf<Long?>()
    val spo2 = mutableListOf<Float?>()
    val hrWellue = mutableListOf<Float?>()
    val hrCoospo = mutableListOf<Float?>()
    val co2 = mutableListOf<Float?>()
    val co2Raw = mutableListOf<Float?>()
    val o2 = mutableListOf<Float?>()
    val flow = mutableListOf<Float?>()
    val pressure = mutableListOf<Float?>()
    val workO2 = mutableListOf<Float?>()
    val workPressure = mutableListOf<Float?>()
    val rr = mutableListOf<Float?>()
    val slm = mutableListOf<Float?>()

    fun clear() {
        listOf(
            t, espMs, spo2, hrWellue, hrCoospo, co2, co2Raw, o2, flow,
            pressure, workO2, workPressure, rr, slm,
        ).forEach { it.clear() }
    }

    fun lastEspMs(): Long? = espMs.asReversed().firstOrNull { it != null }

    fun size(): Int = t.size

    fun window(values: List<Float?>, windowSec: Int, now: Long = System.currentTimeMillis()): List<ChartPoint> {
        val start = now - windowSec * 1000L
        val out = ArrayList<ChartPoint>()
        for (i in t.indices) {
            if (t[i] >= start) out += ChartPoint(t[i], values.getOrNull(i))
        }
        return out
    }

    fun windowScroll(
        values: List<Float?>,
        origin: Long,
        windowSec: Int,
        now: Long = System.currentTimeMillis(),
        fullSession: Boolean = false,
    ): List<ChartPoint> {
        if (fullSession || t.isEmpty()) {
            return (0 until t.size).map { ChartPoint(t[it], values.getOrNull(it)) }
        }
        val winMs = windowSec * 1000L
        val start = if (now - origin <= winMs) origin else now - winMs
        val out = ArrayList<ChartPoint>()
        for (i in t.indices) {
            if (t[i] >= start) out += ChartPoint(t[i], values.getOrNull(i))
        }
        return out
    }

    fun lastFinite(values: List<Float?>): Float? {
        for (i in values.indices.reversed()) {
            val v = values[i]
            if (v != null && v.isFinite()) return v
        }
        return null
    }

    fun pushLive(status: DeviceStatus, sources: TrendSources) {
        val s = status.sensors
        val wellueLive = sources.wellue && s.wellue.ok && s.wellue.contact
        val coospoLive = sources.hr && s.coospo.ok && s.coospo.contact
        val rrLive = sources.rr && s.coospo.ok && s.coospo.contact
        val sampleMs = status.uptimeMs
        val last = lastEspMs()
        if (last != null && sampleMs - last < 500) return
        t += System.currentTimeMillis()
        espMs += sampleMs
        spo2 += if (wellueLive) saneSpo2(s.wellue.spo2) else null
        hrWellue += if (wellueLive) saneHr(s.wellue.hr) else null
        hrCoospo += if (coospoLive) saneHr(s.coospo.bpm) else null
        co2 += if (sources.co2) finiteOrNull(s.co2.ok, Formatters.co2Percent(s.co2)) else null
        co2Raw += if (sources.co2 && s.co2.dynamicComp) {
            finiteOrNull(s.co2.ok, s.co2.ppmRaw / 10000f)
        } else {
            null
        }
        o2 += if (sources.o2) finiteOrNull(s.hypoxiaO2.ok, s.hypoxiaO2.percent) else null
        flow += if (sources.flow) finiteOrNull(s.flow.ok, s.flow.veLpm) else null
        pressure += if (sources.pressure) finiteOrNull(s.hypoxiaPressure.ok, s.hypoxiaPressure.hpa) else null
        workO2 += when {
            !sources.workO2 -> null
            // не рисуем TEMP(~25) пока O₂ не привязан к полю
            s.workO2.o2Field < 0 -> null
            else -> finiteOrNull(s.workO2.ok, s.workO2.percent)
        }
        workPressure += if (sources.workPressure) finiteOrNull(s.workPressure.ok, s.workPressure.hpa) else null
        rr += if (rrLive) saneRr(s.coospo.rrMs) else null
        slm += finiteOrNull(s.flow.ok, s.flow.slm)
        trim()
    }

    fun applyPage(page: TrendPage, replace: Boolean) {
        if (replace) clear()
        val lastEsp = lastEspMs()
        val wallNow = System.currentTimeMillis()
        for (i in 0 until page.count) {
            val sampleMs = page.t0Ms + (page.offset + i) * page.intervalMs
            if (lastEsp != null && sampleMs <= lastEsp) continue
            t += wallNow - (page.nowMs - sampleMs).coerceAtLeast(0)
            espMs += sampleMs
            spo2 += saneSpo2(page.spo2.getOrNull(i))
            hrWellue += saneHr(page.hrWellue.getOrNull(i))
            hrCoospo += saneHr(page.hrCoospo.getOrNull(i))
            co2 += ppmToPercent(page.co2.getOrNull(i))
            co2Raw += if (page.co2Raw.isNotEmpty()) ppmToPercent(page.co2Raw.getOrNull(i)) else null
            o2 += fromScaled10(page.o2.getOrNull(i))
            flow += fromScaled10(page.flow.getOrNull(i))
            pressure += fromScaled10(page.pressure.getOrNull(i))
            workO2 += fromScaled10(page.workO2.getOrNull(i))
            workPressure += fromScaled10(page.workPressure.getOrNull(i))
            rr += saneRr(page.rr.getOrNull(i))
            slm += null
        }
        trim()
    }

    private fun trim() {
        while (t.size > CHART_MAX) {
            t.removeAt(0)
            espMs.removeAt(0)
            spo2.removeAt(0)
            hrWellue.removeAt(0)
            hrCoospo.removeAt(0)
            co2.removeAt(0)
            co2Raw.removeAt(0)
            o2.removeAt(0)
            flow.removeAt(0)
            pressure.removeAt(0)
            workO2.removeAt(0)
            workPressure.removeAt(0)
            rr.removeAt(0)
            if (slm.isNotEmpty()) slm.removeAt(0)
        }
    }

    companion object {
        const val CHART_MAX = 4500
        const val MISSING = -30000

        fun saneSpo2(value: Int?): Float? {
            val n = value ?: return null
            if (n <= MISSING || n < 50 || n > 100) return null
            return n.toFloat()
        }

        fun saneHr(value: Int?): Float? {
            val n = value ?: return null
            if (n <= MISSING || n < 25 || n > 240) return null
            return n.toFloat()
        }

        fun saneRr(value: Int?): Float? {
            val n = value ?: return null
            if (n <= MISSING || n < 250 || n > 2000) return null
            return n.toFloat()
        }

        fun fromScaled10(value: Int?): Float? {
            val n = value ?: return null
            if (n <= MISSING) return null
            return n / 10f
        }

        fun ppmToPercent(value: Int?): Float? {
            val n = value ?: return null
            if (n <= MISSING) return null
            return n / 10000f
        }

        fun finiteOrNull(ok: Boolean, value: Float?): Float? {
            if (!ok) return null
            val n = value ?: return null
            return if (n.isFinite()) n else null
        }
    }
}
