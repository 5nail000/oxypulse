package com.oxypulse.esp32tester.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.key
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.oxypulse.esp32tester.domain.LinkState
import com.oxypulse.esp32tester.ui.theme.Accent
import com.oxypulse.esp32tester.ui.theme.CardBg
import com.oxypulse.esp32tester.ui.theme.ErrRed
import com.oxypulse.esp32tester.ui.theme.Line
import com.oxypulse.esp32tester.ui.theme.OkGreen
import com.oxypulse.esp32tester.ui.theme.SecondaryBtn
import com.oxypulse.esp32tester.ui.theme.TextMain
import com.oxypulse.esp32tester.ui.theme.TextMuted
import com.oxypulse.esp32tester.ui.theme.WarnYellow

@Composable
fun OxyCard(modifier: Modifier = Modifier, content: @Composable ColumnScope.() -> Unit) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .border(1.dp, Line, RoundedCornerShape(8.dp))
            .background(CardBg)
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(10.dp),
        content = content,
    )
}

@Composable
fun SectionTitle(text: String) {
    Text(text, color = TextMain, fontSize = 18.sp, fontWeight = FontWeight.SemiBold)
}

@Composable
fun Muted(text: String, modifier: Modifier = Modifier) {
    Text(text, color = TextMuted, fontSize = 13.sp, modifier = modifier)
}

@Composable
fun PrimaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    Button(
        onClick = onClick,
        enabled = enabled,
        modifier = modifier,
        colors = ButtonDefaults.buttonColors(containerColor = Accent, disabledContainerColor = Accent.copy(alpha = 0.35f)),
    ) { Text(text) }
}

@Composable
fun SecondaryButton(
    text: String,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
) {
    Button(
        onClick = onClick,
        enabled = enabled,
        modifier = modifier,
        colors = ButtonDefaults.buttonColors(
            containerColor = SecondaryBtn,
            disabledContainerColor = SecondaryBtn.copy(alpha = 0.35f),
        ),
    ) { Text(text) }
}

@Composable
fun Pill(
    text: String,
    selected: Boolean,
    onClick: () -> Unit,
    modifier: Modifier = Modifier,
) {
    TextButton(
        onClick = onClick,
        modifier = modifier,
        colors = ButtonDefaults.textButtonColors(
            contentColor = if (selected) TextMain else TextMuted,
        ),
    ) {
        Box(
            modifier = Modifier
                .clip(RoundedCornerShape(16.dp))
                .background(if (selected) Accent else Color(0xFF222222))
                .border(1.dp, if (selected) Accent else Line, RoundedCornerShape(16.dp))
                .padding(horizontal = 12.dp, vertical = 6.dp),
        ) {
            Text(text, fontSize = 13.sp, color = if (selected) Color.White else TextMuted)
        }
    }
}

@Composable
fun SensorTile(
    title: String,
    value: String,
    ok: Boolean,
    trendLabel: String? = "Тренд",
    trendOn: Boolean = false,
    onTrend: (() -> Unit)? = null,
    extra: @Composable ColumnScope.() -> Unit = {},
) {
    Column(
        modifier = Modifier
            .fillMaxWidth()
            .clip(RoundedCornerShape(8.dp))
            .border(1.dp, if (ok) Line else ErrRed.copy(alpha = 0.7f), RoundedCornerShape(8.dp))
            .background(Color(0xFF181818))
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
            Text(title, color = TextMuted, fontSize = 13.sp, modifier = Modifier.weight(1f))
            if (onTrend != null && trendLabel != null) {
                TextButton(onClick = onTrend) {
                    Text(
                        trendLabel,
                        color = if (trendOn) OkGreen else TextMuted,
                        fontSize = 12.sp,
                    )
                }
            }
        }
        Text(value, color = if (ok) TextMain else ErrRed, fontSize = 14.sp)
        extra()
    }
}

@Composable
fun LinkBar(state: LinkState, text: String, meta: String) {
    val color = when (state) {
        LinkState.Live -> OkGreen
        LinkState.Stale -> WarnYellow
        LinkState.Offline -> ErrRed
        LinkState.Idle -> TextMuted
    }
    val bg = when (state) {
        LinkState.Live -> OkGreen.copy(alpha = 0.08f)
        LinkState.Stale -> WarnYellow.copy(alpha = 0.10f)
        LinkState.Offline -> ErrRed.copy(alpha = 0.12f)
        LinkState.Idle -> CardBg
    }
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .background(bg)
            .padding(horizontal = 16.dp, vertical = 10.dp),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Box(
            modifier = Modifier
                .size(8.dp)
                .clip(CircleShape)
                .background(color),
        )
        Text(text, color = color, fontWeight = FontWeight.SemiBold, fontSize = 14.sp)
        Text(meta, color = color.copy(alpha = 0.9f), fontSize = 12.sp, modifier = Modifier.weight(1f))
    }
}

@Composable
fun FullDialog(
    onDismiss: () -> Unit,
    pageKey: Any = Unit,
    content: @Composable ColumnScope.() -> Unit,
) {
    BackHandler(onBack = onDismiss)
    key(pageKey) {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .background(Color(0xFF111111))
                .padding(16.dp)
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(12.dp),
            content = content,
        )
    }
}

@Composable
fun DialogHead(title: String, onClose: () -> Unit, trailing: @Composable RowScope.() -> Unit = {}) {
    Row(verticalAlignment = Alignment.CenterVertically, modifier = Modifier.fillMaxWidth()) {
        Text(title, color = TextMain, fontSize = 20.sp, fontWeight = FontWeight.SemiBold, modifier = Modifier.weight(1f))
        trailing()
        SecondaryButton("Закрыть", onClose)
    }
}
