package com.oxypulse.esp32tester.ui.charts

import android.graphics.Paint
import android.graphics.Typeface
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipRect
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.unit.dp
import com.oxypulse.esp32tester.domain.ChartPoint
import com.oxypulse.esp32tester.domain.HrvMetrics
import com.oxypulse.esp32tester.ui.theme.Line
import com.oxypulse.esp32tester.ui.theme.PlotBg
import com.oxypulse.esp32tester.ui.theme.Teal
import kotlin.math.max
import kotlin.math.min

data class ChartSeries(
    val points: List<ChartPoint>,
    val color: Color,
    val dashed: Boolean = false,
    val maxGapMs: Long = 2500L,
)

data class ChartMarker(
    val t: Long,
    val color: Color,
    val dashed: Boolean = false,
)

@Composable
fun TimeSeriesChart(
    series: List<ChartSeries>,
    windowSec: Int,
    yFormat: (Float) -> String = { "%.1f".format(it) },
    fallbackMin: Float = 0f,
    fallbackMax: Float = 1f,
    markers: List<ChartMarker> = emptyList(),
    timeOrigin: Long? = null,
    scrollFromOrigin: Boolean = false,
    now: Long = System.currentTimeMillis(),
    modifier: Modifier = Modifier
        .fillMaxWidth()
        .height(148.dp),
) {
    Canvas(modifier = modifier) {
        val padL = 42.dp.toPx()
        val padR = 8.dp.toPx()
        val padT = 8.dp.toPx()
        val padB = 22.dp.toPx()
        val plotW = (size.width - padL - padR).coerceAtLeast(1f)
        val plotH = (size.height - padT - padB).coerceAtLeast(1f)
        val x0 = padL
        val y0 = padT

        drawRect(PlotBg, Offset(x0, y0), Size(plotW, plotH))

        val vals = series.flatMap { it.points.mapNotNull { p -> p.v } }
        var yMin = fallbackMin
        var yMax = fallbackMax
        if (vals.isNotEmpty()) {
            yMin = vals.minOrNull() ?: fallbackMin
            yMax = vals.maxOrNull() ?: fallbackMax
            if (yMin == yMax) {
                val span = max(1f, (fallbackMax - fallbackMin) * 0.08f)
                yMin -= span
                yMax += span
            }
            val pad = (yMax - yMin) * 0.12f
            yMin -= pad
            yMax += pad
        }
        val ySpan = (yMax - yMin).let { if (it == 0f) 1f else it }
        val winMs = windowSec * 1000L
        val start = when {
            timeOrigin != null && scrollFromOrigin && now - timeOrigin > winMs -> now - winMs
            timeOrigin != null -> timeOrigin
            else -> now - winMs
        }
        val spanMs = max(1L, if (timeOrigin != null && scrollFromOrigin) winMs else now - start)

        fun xOf(t: Long) = x0 + ((t - start).toFloat() / spanMs) * plotW
        fun yOf(v: Float) = y0 + plotH - ((v - yMin) / ySpan) * plotH

        val gridPaint = android.graphics.Paint().apply {
            color = android.graphics.Color.parseColor("#777777")
            textSize = 10.dp.toPx()
            isAntiAlias = true
        }
        val native = drawContext.canvas.nativeCanvas
        for (i in 0..4) {
            val y = y0 + plotH * i / 4f
            drawLine(Color.White.copy(alpha = 0.06f), Offset(x0, y), Offset(x0 + plotW, y), 1f)
            val v = yMax - ySpan * i / 4f
            native.drawText(yFormat(v), 4f, y + 4f, gridPaint)
        }
        drawRect(Line, Offset(x0, y0), Size(plotW, plotH), style = Stroke(1f))

        for (m in markers) {
            val x = xOf(m.t)
            if (x < x0 || x > x0 + plotW) continue
            drawLine(
                m.color,
                Offset(x, y0),
                Offset(x, y0 + plotH),
                strokeWidth = 1.5f,
                pathEffect = if (m.dashed) PathEffect.dashPathEffect(floatArrayOf(8f, 6f)) else null,
            )
        }

        for (s in series) {
            val pts = s.points
            if (pts.isEmpty()) continue
            val path = Path()
            var started = false
            var lastT: Long? = null
            for (p in pts) {
                val v = p.v
                if (v == null || !v.isFinite()) {
                    started = false
                    lastT = p.t
                    continue
                }
                if (lastT != null && p.t - lastT > s.maxGapMs) started = false
                val x = xOf(p.t)
                val y = yOf(v)
                if (!started) {
                    path.moveTo(x, y)
                    started = true
                } else {
                    path.lineTo(x, y)
                }
                lastT = p.t
            }
            drawPath(
                path,
                s.color,
                style = Stroke(
                    width = 1.8f,
                    pathEffect = if (s.dashed) PathEffect.dashPathEffect(floatArrayOf(10f, 8f)) else null,
                ),
            )
        }
    }
}

@Composable
fun PoincareChart(
    metrics: HrvMetrics,
    modifier: Modifier = Modifier
        .fillMaxWidth()
        .height(280.dp),
) {
    Canvas(modifier = modifier) {
        if (!metrics.ok) return@Canvas
        val padT = 18.dp.toPx()
        val padR = 16.dp.toPx()
        val padB = 28.dp.toPx()
        val padL = 42.dp.toPx()
        val innerW = max(1f, size.width - padL - padR)
        val innerH = max(1f, size.height - padT - padB)
        val side = min(innerW, innerH)
        val x0 = padL + (innerW - side) / 2f
        val y0 = padT + (innerH - side) / 2f
        val mean = metrics.meanRr
        var lo = mean * 0.85f
        var hi = mean * 1.15f
        val span0 = max(metrics.sd2 * 2.4f, 80f)
        lo = min(lo, mean - span0)
        hi = max(hi, mean + span0)
        for (p in metrics.pairs) {
            lo = min(lo, min(p.x, p.y))
            hi = max(hi, max(p.x, p.y))
        }
        var range = max(40f, hi - lo)
        lo -= range * 0.04f
        hi += range * 0.04f
        range = hi - lo
        fun toX(v: Float) = x0 + ((v - lo) / range) * side
        fun toY(v: Float) = y0 + side - ((v - lo) / range) * side
        val pxPerMs = side / range

        drawRect(PlotBg, Offset(x0, y0), Size(side, side))
        val labelPaint = Paint().apply {
            color = android.graphics.Color.parseColor("#777777")
            textSize = 10.dp.toPx()
            isAntiAlias = true
            typeface = Typeface.SANS_SERIF
        }
        val native = drawContext.canvas.nativeCanvas
        for (i in 0..4) {
            val v = lo + range * i / 4f
            val x = toX(v)
            val y = toY(v)
            drawLine(Color.White.copy(alpha = 0.06f), Offset(x0, y), Offset(x0 + side, y), 1f)
            drawLine(Color.White.copy(alpha = 0.06f), Offset(x, y0), Offset(x, y0 + side), 1f)
            native.drawText(v.toInt().toString(), x0 - 6f, y, labelPaint.apply { textAlign = Paint.Align.RIGHT })
            native.drawText(v.toInt().toString(), x, y0 + side + 14f, labelPaint.apply { textAlign = Paint.Align.CENTER })
        }
        drawRect(Line, Offset(x0, y0), Size(side, side), style = Stroke(1f))
        drawLine(Color(0xFF94A3B8).copy(alpha = 0.45f), Offset(toX(lo), toY(lo)), Offset(toX(hi), toY(hi)), 1f)

        val cx = toX(mean)
        val cy = toY(mean)
        clipRect(x0, y0, x0 + side, y0 + side) {
            val rx = metrics.sd2 * pxPerMs
            val ry = metrics.sd1 * pxPerMs
            native.save()
            native.translate(cx, cy)
            native.rotate(-45f)
            native.drawOval(
                -rx,
                -ry,
                rx,
                ry,
                Paint().apply {
                    color = android.graphics.Color.parseColor("#B394A3B8")
                    style = Paint.Style.STROKE
                    strokeWidth = 1.2f
                    isAntiAlias = true
                },
            )
            native.restore()
            val s2 = (metrics.sd2 * pxPerMs) / 1.4142135f
            val s1 = (metrics.sd1 * pxPerMs) / 1.4142135f
            drawLine(Color(0xFF94A3B8).copy(alpha = 0.7f), Offset(cx - s2, cy + s2), Offset(cx + s2, cy - s2), 1.2f)
            drawLine(Color(0xFF94A3B8).copy(alpha = 0.7f), Offset(cx - s1, cy - s1), Offset(cx + s1, cy + s1), 1.2f)
            for (p in metrics.pairs) {
                drawCircle(Teal, 2.2.dp.toPx(), Offset(toX(p.x), toY(p.y)))
            }
        }
    }
}

fun lastNowText(points: List<ChartPoint>, format: (Float) -> String): String {
    val vals = points.mapNotNull { it.v }
    if (vals.isEmpty()) return "—"
    val last = vals.last()
    val minV = vals.minOrNull() ?: last
    val maxV = vals.maxOrNull() ?: last
    return if (minV == maxV) format(last) else "${format(last)}  ·  ${format(minV)}–${format(maxV)}"
}
