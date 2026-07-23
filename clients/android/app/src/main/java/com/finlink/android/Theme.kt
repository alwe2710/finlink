package com.finlink.android

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext

// Matches assets/logo/finlink-logo.png: dark navy background, cyan glow.
private val FinlinkCyan = Color(0xFF4DD8E8)
private val FinlinkCyanMuted = Color(0xFF1E8FA6)
private val FinlinkNavy = Color(0xFF0A1128)
private val FinlinkNavyLight = Color(0xFF16213E)

private val FinlinkDarkScheme = darkColorScheme(
    primary = FinlinkCyan,
    onPrimary = Color.Black,
    secondary = FinlinkCyanMuted,
    background = FinlinkNavy,
    surface = FinlinkNavyLight,
)

private val FinlinkLightScheme = lightColorScheme(
    primary = FinlinkCyanMuted,
    secondary = FinlinkCyan,
)

/**
 * Material 3 theme, with dynamic color (Android 12+) preferred when
 * available and a fixed cyan-on-navy scheme derived from the app logo as
 * the fallback everywhere else.
 */
@Composable
fun FinlinkTheme(
    darkTheme: Boolean = isSystemInDarkTheme(),
    dynamicColor: Boolean = true,
    content: @Composable () -> Unit
) {
    val context = LocalContext.current
    val colorScheme = when {
        dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ->
            if (darkTheme) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        darkTheme -> FinlinkDarkScheme
        else -> FinlinkLightScheme
    }

    MaterialTheme(
        colorScheme = colorScheme,
        content = content
    )
}
