package com.finlink.android

import android.graphics.Bitmap
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import java.nio.ByteBuffer

/**
 * The actual stream view: connects to the host:port passed in via Intent
 * extras, shows video full-screen (ContentScale.Fit -- scales up, never
 * stretched, exactly like the old XML's fitCenter), plays audio, and
 * accepts input from both the on-screen button row (if enabled in Settings)
 * and any physical key/controller bindings set there. Owns the one
 * GbaStreamClient instance for its lifetime; MenuActivity and
 * SettingsActivity never touch it.
 *
 * Stays landscape-locked (AndroidManifest.xml) unlike Menu/Settings: the GBA
 * stream itself is a fixed wide aspect ratio.
 */
class PlayerActivity : ComponentActivity(), GbaStreamClient.Listener {

    private lateinit var prefs: Prefs

    private var client: GbaStreamClient? = null
    private var audioTrack: AudioTrack? = null

    private var videoBitmap by mutableStateOf<Bitmap?>(null)
    private var statusText by mutableStateOf("")
    private var onScreenControlsEnabled by mutableStateOf(true)

    // Touch and physical-key input are tracked separately and OR'd together
    // when sent, so releasing one source doesn't clobber bits the other
    // source is still holding -- mirrors how the original web client merges
    // keyboard/touch/gamepad input (see docs/protocol.md's source notes).
    private var touchMask = 0
    private var physicalMask = 0
    private var keyCodeToBit: Map<Int, Int> = emptyMap()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableImmersiveMode()

        prefs = Prefs(this)
        keyCodeToBit = prefs.keyBindingsByKeyCode()
        onScreenControlsEnabled = prefs.onScreenControlsEnabled

        setContent {
            FinlinkTheme {
                PlayerScreen()
            }
        }

        val host = intent.getStringExtra(EXTRA_HOST)
        val port = intent.getIntExtra(EXTRA_PORT, -1)
        if (host.isNullOrEmpty() || port <= 0) {
            statusText = getString(R.string.status_error, "kein Host übergeben")
            return
        }
        connectTo(host, port)
    }

    @Composable
    private fun PlayerScreen() {
        Surface(modifier = Modifier.fillMaxSize(), color = Color.Black) {
            Box(modifier = Modifier.fillMaxSize()) {
                videoBitmap?.let { bitmap ->
                    Image(
                        bitmap = bitmap.asImageBitmap(),
                        contentDescription = null,
                        contentScale = ContentScale.Fit,
                        modifier = Modifier.fillMaxSize()
                    )
                }

                Text(
                    statusText,
                    color = Color.White,
                    modifier = Modifier
                        .align(Alignment.TopStart)
                        .padding(8.dp)
                        .background(Color(0x80000000))
                        .padding(horizontal = 6.dp, vertical = 2.dp)
                )

                TextButton(
                    onClick = { finish() },
                    modifier = Modifier.align(Alignment.TopEnd).padding(8.dp)
                ) {
                    Text(stringResource(R.string.disconnect))
                }

                if (onScreenControlsEnabled) {
                    Row(
                        modifier = Modifier
                            .align(Alignment.BottomCenter)
                            .fillMaxWidth()
                    ) {
                        GBA_BUTTONS.forEach { button ->
                            HoldButton(
                                label = button.label,
                                modifier = Modifier.weight(1f),
                                onPressChange = { pressed ->
                                    touchMask = if (pressed) touchMask or button.bit else touchMask and button.bit.inv()
                                    sendCombinedInput()
                                }
                            )
                        }
                    }
                }
            }
        }
    }

    /** Plain Button/clickable() only fires on release-tap; a GBA button
     * needs a real press/release pair (held = keeps sending the bit), hence
     * detectTapGestures(onPress) + awaitRelease() instead. */
    @Composable
    private fun HoldButton(label: String, onPressChange: (Boolean) -> Unit, modifier: Modifier = Modifier) {
        Box(
            modifier = modifier
                .padding(2.dp)
                .background(
                    MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.85f),
                    RoundedCornerShape(8.dp)
                )
                .pointerInput(Unit) {
                    detectTapGestures(onPress = {
                        onPressChange(true)
                        try {
                            awaitRelease()
                        } finally {
                            onPressChange(false)
                        }
                    })
                }
                .padding(vertical = 14.dp),
            contentAlignment = Alignment.Center
        ) {
            Text(label, color = MaterialTheme.colorScheme.onSecondaryContainer)
        }
    }

    @Suppress("DEPRECATION")
    private fun enableImmersiveMode() {
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                or View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                or View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                or View.SYSTEM_UI_FLAG_FULLSCREEN
                or View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
            )
    }

    @Suppress("DEPRECATION")
    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) enableImmersiveMode()
    }

    // Physical keyboard/controller input, per the bindings set in
    // SettingsActivity. Unbound keys fall through to the system default
    // (e.g. volume/back keys keep working).

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        val bit = keyCodeToBit[keyCode] ?: return super.onKeyDown(keyCode, event)
        physicalMask = physicalMask or bit
        sendCombinedInput()
        return true
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        val bit = keyCodeToBit[keyCode] ?: return super.onKeyUp(keyCode, event)
        physicalMask = physicalMask and bit.inv()
        sendCombinedInput()
        return true
    }

    private fun sendCombinedInput() {
        client?.sendInput(touchMask or physicalMask)
    }

    private fun connectTo(host: String, port: Int) {
        touchMask = 0
        physicalMask = 0
        val c = GbaStreamClient(this)
        client = c
        statusText = getString(R.string.status_connecting)
        c.connect(host, port)
    }

    private fun disconnect(resetStatusText: Boolean = true) {
        client?.disconnect()
        client = null
        stopAudio()
        if (resetStatusText) {
            statusText = getString(R.string.status_disconnected)
        }
    }

    private fun stopAudio() {
        audioTrack?.stop()
        audioTrack?.release()
        audioTrack = null
    }

    // --- GbaStreamClient.Listener: called from the native session thread,
    // never the UI thread, so every callback must hop back via runOnUiThread
    // before touching Compose state. onAudioFrame is the one exception --
    // writing to AudioTrack from a background thread is exactly what it's for.

    override fun onConnected() {
        runOnUiThread { statusText = getString(R.string.status_connected) }
    }

    override fun onVideoFrame(width: Int, height: Int, rgb565: ByteArray) {
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565)
        bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(rgb565))
        runOnUiThread { videoBitmap = bitmap }
    }

    override fun onAudioFrame(sampleRate: Int, channels: Int, pcm: ShortArray) {
        var track = audioTrack
        if (track == null || track.sampleRate != sampleRate) {
            track?.stop()
            track?.release()
            val channelConfig =
                if (channels >= 2) AudioFormat.CHANNEL_OUT_STEREO else AudioFormat.CHANNEL_OUT_MONO
            val minBufSize =
                AudioTrack.getMinBufferSize(sampleRate, channelConfig, AudioFormat.ENCODING_PCM_16BIT)
            track = AudioTrack(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
                AudioFormat.Builder()
                    .setSampleRate(sampleRate)
                    .setChannelMask(channelConfig)
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .build(),
                maxOf(minBufSize, 4096) * 2,
                AudioTrack.MODE_STREAM,
                AudioManager.AUDIO_SESSION_ID_GENERATE
            )
            track.play()
            audioTrack = track
        }
        track.write(pcm, 0, pcm.size)
    }

    override fun onDisconnected(reason: String) {
        // The native session thread calls this right before it exits on its
        // own (connect/handshake failure, peer closed, protocol error) --
        // must still route through disconnect() to join the thread and
        // release the global JNI ref to this Activity, or both leak.
        runOnUiThread {
            disconnect(resetStatusText = false)
            statusText = getString(R.string.status_error, reason)
        }
    }

    override fun onDestroy() {
        disconnect()
        super.onDestroy()
    }

    companion object {
        const val EXTRA_HOST = "host"
        const val EXTRA_PORT = "port"
    }
}
