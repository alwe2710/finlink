package com.finlink.android

import android.os.Bundle
import android.view.KeyEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp

/**
 * On-screen-controls toggle + physical key binding per GBA button, both
 * persisted via [Prefs] and read back by PlayerActivity. Same
 * dispatchKeyEvent interception as before for capturing the next raw key
 * press while a binding is pending -- that's an Activity/Window-level
 * mechanism, unaffected by the UI underneath being Compose now.
 *
 * No fixed orientation (see AndroidManifest.xml): this is a form, so it
 * should follow however the device is actually held.
 */
@OptIn(ExperimentalMaterial3Api::class)
class SettingsActivity : ComponentActivity() {

    private lateinit var prefs: Prefs
    private var pendingBindTarget: GbaButton? = null
    private var onScreenControlsEnabled by mutableStateOf(true)
    private var bilinearVideoFilter by mutableStateOf(false)
    private val bindingTexts = mutableStateMapOf<GbaButton, String>()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        prefs = Prefs(this)
        onScreenControlsEnabled = prefs.onScreenControlsEnabled
        bilinearVideoFilter = prefs.bilinearVideoFilter
        GBA_BUTTONS.forEach { bindingTexts[it] = describeBinding(it) }

        setContent {
            FinlinkTheme {
                Surface(modifier = Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) {
                    Scaffold(
                        topBar = {
                            TopAppBar(
                                title = { Text(stringResource(R.string.settings)) },
                                navigationIcon = {
                                    TextButton(onClick = { finish() }) { Text(stringResource(R.string.back)) }
                                }
                            )
                        }
                    ) { innerPadding ->
                        Column(modifier = Modifier.padding(innerPadding).padding(16.dp).fillMaxSize()) {
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                    stringResource(R.string.settings_on_screen_controls),
                                    modifier = Modifier.weight(1f)
                                )
                                Switch(
                                    checked = onScreenControlsEnabled,
                                    onCheckedChange = {
                                        onScreenControlsEnabled = it
                                        prefs.onScreenControlsEnabled = it
                                    }
                                )
                            }

                            Spacer(Modifier.height(16.dp))
                            HorizontalDivider()
                            Spacer(Modifier.height(16.dp))

                            Text(stringResource(R.string.settings_display), style = MaterialTheme.typography.titleMedium)
                            Spacer(Modifier.height(8.dp))
                            Row(verticalAlignment = Alignment.CenterVertically) {
                                Text(
                                    stringResource(R.string.settings_bilinear_filter),
                                    modifier = Modifier.weight(1f)
                                )
                                Switch(
                                    checked = bilinearVideoFilter,
                                    onCheckedChange = {
                                        bilinearVideoFilter = it
                                        prefs.bilinearVideoFilter = it
                                    }
                                )
                            }

                            Spacer(Modifier.height(16.dp))
                            HorizontalDivider()
                            Spacer(Modifier.height(16.dp))

                            Text(stringResource(R.string.settings_key_bindings), style = MaterialTheme.typography.titleMedium)
                            Spacer(Modifier.height(8.dp))

                            LazyColumn(modifier = Modifier.weight(1f)) {
                                items(GBA_BUTTONS) { button ->
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        modifier = Modifier.fillMaxWidth().padding(vertical = 8.dp)
                                    ) {
                                        Text(button.label, modifier = Modifier.weight(1f))
                                        Text(
                                            bindingTexts[button] ?: "",
                                            modifier = Modifier.padding(end = 8.dp),
                                            color = MaterialTheme.colorScheme.onSurfaceVariant
                                        )
                                        TextButton(onClick = {
                                            pendingBindTarget = button
                                            bindingTexts[button] = getString(R.string.settings_press_key)
                                        }) {
                                            Text(stringResource(R.string.settings_bind))
                                        }
                                        TextButton(onClick = {
                                            prefs.clearKeyBinding(button)
                                            bindingTexts[button] = describeBinding(button)
                                        }) {
                                            Text(stringResource(R.string.settings_clear))
                                        }
                                    }
                                    HorizontalDivider()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    private fun describeBinding(button: GbaButton): String {
        val code = prefs.getKeyBinding(button) ?: return getString(R.string.settings_unbound)
        return KeyEvent.keyCodeToString(code).removePrefix("KEYCODE_")
    }

    /** Intercepts the next key press while a binding is pending, regardless
     * of which composable has focus. */
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val target = pendingBindTarget
        if (target != null && event.action == KeyEvent.ACTION_DOWN) {
            prefs.setKeyBinding(target, event.keyCode)
            pendingBindTarget = null
            bindingTexts[target] = describeBinding(target)
            return true
        }
        return super.dispatchKeyEvent(event)
    }
}
