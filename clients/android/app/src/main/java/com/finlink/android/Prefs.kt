package com.finlink.android

import android.content.Context
import android.content.SharedPreferences

/**
 * The two settings this app has: an optional physical-key (keyboard/game
 * controller) binding per GBA button, and whether the on-screen touch
 * overlay is shown in the player. Both are set from SettingsActivity and
 * read by PlayerActivity.
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

    private fun prefKeyFor(button: GbaButton) = "keybind_${button.prefKey}"

    companion object {
        private const val PREF_ON_SCREEN_CONTROLS = "on_screen_controls"
        private const val NO_KEYCODE = -1
    }
}
