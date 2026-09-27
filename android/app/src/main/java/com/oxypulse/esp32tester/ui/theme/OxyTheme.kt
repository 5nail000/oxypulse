package com.oxypulse.esp32tester.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

val Bg = Color(0xFF111111)
val CardBg = Color(0xFF1C1C1C)
val Line = Color(0xFF333333)
val TextMain = Color(0xFFEEEEEE)
val TextMuted = Color(0xFF888888)
val Accent = Color(0xFF2563EB)
val AccentDark = Color(0xFF1D4ED8)
val SecondaryBtn = Color(0xFF374151)
val OkGreen = Color(0xFF4ADE80)
val WarnYellow = Color(0xFFFBBF24)
val ErrRed = Color(0xFFF87171)
val Teal = Color(0xFF2DD4BF)
val Orange = Color(0xFFFB923C)
val BeltRed = Color(0xFFF87171)
val Co2Blue = Color(0xFF60A5FA)
val Co2Raw = Color(0xFF93C5FD)
val FlowViolet = Color(0xFFA78BFA)
val MarkerBlue = Color(0xFF38BDF8)
val NormoxiaGreen = Color(0xFF86EFAC)
val PlotBg = Color(0xFF101010)

private val Colors = darkColorScheme(
    primary = Accent,
    onPrimary = Color.White,
    secondary = SecondaryBtn,
    background = Bg,
    onBackground = TextMain,
    surface = CardBg,
    onSurface = TextMain,
    surfaceVariant = Color(0xFF222222),
    onSurfaceVariant = TextMuted,
    outline = Line,
    error = ErrRed,
    onError = Color.White,
)

@Composable
fun OxyTheme(content: @Composable () -> Unit) {
    MaterialTheme(colorScheme = Colors, content = content)
}
