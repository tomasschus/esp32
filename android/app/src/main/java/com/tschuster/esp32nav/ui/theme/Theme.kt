package com.tschuster.esp32nav.ui.theme

import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color

private val DarkColors = darkColorScheme(
    primary        = Color(0xFFE94560),
    onPrimary      = Color(0xFFFFFFFF),
    secondary      = Color(0xFF4DA6FF),
    onSecondary    = Color(0xFFFFFFFF),
    background     = Color(0xFF1A1A2E),
    onBackground   = Color(0xFFEEEEEE),
    surface        = Color(0xFF16213E),
    onSurface      = Color(0xFFEEEEEE),
    surfaceVariant = Color(0xFF0F2040),
    outline        = Color(0xFF1A3A6A),
    error          = Color(0xFFFF6B6B),
)

@Composable
fun ESP32NavTheme(content: @Composable () -> Unit) {
    MaterialTheme(
        colorScheme = DarkColors,
        content     = content,
    )
}
