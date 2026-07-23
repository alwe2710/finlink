package com.finlink.android

import android.content.Context
import android.content.SharedPreferences

/**
 * Settings, all set from SettingsActivity and read by PlayerActivity: an
 * optional physical-key (keyboard/game controller) binding per GBA button,
 * whether the on-screen touch overlay is shown, and whether upscaled video
 * uses bilinear or nearest-neighbor filtering.
 */
class Prefs(context: Context) {

    private val prefs: SharedPreferences =
        context.getSharedPreferences("finlink_settings", Context.MODE_PRIVATE)

    fun getKeyBinding(button: GbaButton): Int? {
        val value = prefs.getInt(prefKeyFor(button), NO_KEYCODE)
        return if (value == NO_KEYCODE) null else value
    }

    fun setKeyBinding(button: GbaButton, androidKeyCode: Int) {
        prefs.edit().putInt(prefKeyFor(button), androidKeyCode).apply()
    }

    fun clearKeyBinding(button: GbaButton) {
        prefs.edit().remove(prefKeyFor(button)).apply()
    }

    /** androidKeyCode -> GBA button bit, only for buttons that have a binding set. */
    fun keyBindingsByKeyCode(): Map<Int, Int> =
        GBA_BUTTONS.mapNotNull { button -> getKeyBinding(button)?.let { it to button.bit } }.toMap()

    var onScreenControlsEnabled: Boolean
        get() = prefs.getBoolean(PREF_ON_SCREEN_CONTROLS, true)
        set(value) = prefs.edit().putBoolean(PREF_ON_SCREEN_CONTROLS, value).apply()

    /** true = bilinear filtering (smooth upscale), false = nearest-neighbor
     * filtering (crisp/pixelated upscale, the default). The GBA's native
     * 240x160 framebuffer is upscaled a lot to fill the screen, so this is
     * the usual "smooth vs. pixelated" toggle emulators offer for that. */
    var bilinearVideoFilter: Boolean
        get() = prefs.getBoolean(PREF_BILINEAR_VIDEO_FILTER, false)
        set(value) = prefs.edit().putBoolean(PREF_BILINEAR_VIDEO_FILTER, value).apply()

    private fun prefKeyFor(button: GbaButton) = "keybind_${button.prefKey}"

    companion object {
        private const val PREF_ON_SCREEN_CONTROLS = "on_screen_controls"
        private const val PREF_BILINEAR_VIDEO_FILTER = "bilinear_video_filter"
        private const val NO_KEYCODE = -1
    }
}
