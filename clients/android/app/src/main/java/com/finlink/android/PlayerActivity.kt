package com.finlink.android

import android.graphics.Bitmap
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
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
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
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
    private var connected by mutableStateOf(false)
    private var disconnectedReason by mutableStateOf<String?>(null)
    private var onScreenControlsEnabled by mutableStateOf(true)
    private var bilinearVideoFilter by mutableStateOf(false)

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
        // Keep the screen on for as long as the stream is being watched --
        // otherwise the system dims/locks mid-session same as it would
        // during any other idle screen. Tied to this window, so it's lifted
        // automatically once the Activity is no longer shown.
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        prefs = Prefs(this)
        keyCodeToBit = prefs.keyBindingsByKeyCode()
        onScreenControlsEnabled = prefs.onScreenControlsEnabled
        bilinearVideoFilter = prefs.bilinearVideoFilter

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
                        // Bilinear (Low) smooths the upscale from the GBA's
                        // native 240x160; None gives nearest-neighbor, the
                        // crisp/pixelated look -- the Settings toggle.
                        filterQuality = if (bilinearVideoFilter) FilterQuality.Low else FilterQuality.None,
                        modifier = Modifier.fillMaxSize()
                    )
                }

                // Only shown before the stream is actually up (connecting,
                // or a pre-connect failure) -- once streaming, an unexpected
                // drop shows the dialog below instead, not this.
                if (!connected) {
                    Text(
                        statusText,
                        color = Color.White,
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .padding(8.dp)
                            .background(Color(0x80000000))
                            .padding(horizontal = 6.dp, vertical = 2.dp)
                    )
                }

                // No manual disconnect button: the system back button already
                // finishes this Activity, which tears the session down via
                // onDestroy() -> disconnect().

                // Mobile-emulator-style overlay, matching the web client's
                // layout (GBAStreamClientPage.h): shoulder buttons flush in
                // the top corners, Select/Start centered at the top between
                // them, D-pad bottom-left, A/B diagonal cluster bottom-right
                // -- offset like the real GBA's button placement, not a
                // plain row.
                if (onScreenControlsEnabled) {
                    HoldButton(
                        "L", GbaStreamClient.KEY_L, shape = RoundedCornerShape(8.dp),
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .padding(top = 16.dp, start = 16.dp)
                            .size(width = 64.dp, height = 40.dp)
                    )
                    HoldButton(
                        "R", GbaStreamClient.KEY_R, shape = RoundedCornerShape(8.dp),
                        modifier = Modifier
                            .align(Alignment.TopEnd)
                            .padding(top = 16.dp, end = 16.dp)
                            .size(width = 64.dp, height = 40.dp)
                    )

                    Row(
                        modifier = Modifier.align(Alignment.TopCenter).padding(top = 16.dp),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        HoldButton(
                            "Select", GbaStreamClient.KEY_SELECT,
                            shape = RoundedCornerShape(50),
                            modifier = Modifier.size(width = 64.dp, height = 28.dp)
                        )
                        HoldButton(
                            "Start", GbaStreamClient.KEY_START,
                            shape = RoundedCornerShape(50),
                            modifier = Modifier.size(width = 64.dp, height = 28.dp)
                        )
                    }

                    DPad(modifier = Modifier.align(Alignment.BottomStart).padding(24.dp))
                    ActionButtons(modifier = Modifier.align(Alignment.BottomEnd).padding(24.dp))
                }
            }

            // onDisconnected() only ever fires for a drop the session didn't
            // ask for (handshake/connect failure, peer closed, protocol
            // error) -- a user-initiated exit goes through the system back
            // button -> finish() -> onDestroy() directly, never through
            // here. So every time this shows, it's genuinely unexpected.
            // Custom Dialog instead of AlertDialog: the default M3 AlertDialog
            // reserves generous fixed spacing between the message and the
            // button row that looked like dead space for a one-line reason.
            disconnectedReason?.let { reason ->
                Dialog(onDismissRequest = { finish() }) {
                    Surface(shape = RoundedCornerShape(16.dp), color = MaterialTheme.colorScheme.surface) {
                        Column(modifier = Modifier.padding(20.dp)) {
                            Text(stringResource(R.string.stream_lost_title), style = MaterialTheme.typography.titleMedium)
                            Spacer(Modifier.height(8.dp))
                            Text(reason, style = MaterialTheme.typography.bodyMedium)
                            Spacer(Modifier.height(12.dp))
                            TextButton(onClick = { finish() }, modifier = Modifier.align(Alignment.End)) {
                                Text(stringResource(R.string.ok))
                            }
                        }
                    }
                }
            }
        }
    }

    /** Cross-shaped D-pad: only the four edge-center cells of a 3x3 grid are
     * filled, which alone reads as a plus/cross, matching a real D-pad
     * instead of four buttons in a row. */
    @Composable
    private fun DPad(modifier: Modifier = Modifier) {
        val segment = 56.dp
        Box(modifier = modifier.size(segment * 3)) {
            HoldButton(
                "▲", GbaStreamClient.KEY_UP, shape = RoundedCornerShape(6.dp),
                modifier = Modifier.align(Alignment.TopCenter).size(segment)
            )
            HoldButton(
                "▼", GbaStreamClient.KEY_DOWN, shape = RoundedCornerShape(6.dp),
                modifier = Modifier.align(Alignment.BottomCenter).size(segment)
            )
            HoldButton(
                "◀", GbaStreamClient.KEY_LEFT, shape = RoundedCornerShape(6.dp),
                modifier = Modifier.align(Alignment.CenterStart).size(segment)
            )
            HoldButton(
                "▶", GbaStreamClient.KEY_RIGHT, shape = RoundedCornerShape(6.dp),
                modifier = Modifier.align(Alignment.CenterEnd).size(segment)
            )
        }
    }

    /** B bottom-left, A top-end within the same cluster -- the diagonal
     * offset the real GBA has between the two, not side-by-side. */
    @Composable
    private fun ActionButtons(modifier: Modifier = Modifier) {
        val size = 72.dp
        Box(modifier = modifier.size(width = size * 2, height = size * 1.6f)) {
            HoldButton(
                "B", GbaStreamClient.KEY_B, shape = CircleShape,
                modifier = Modifier.align(Alignment.BottomStart).size(size)
            )
            HoldButton(
                "A", GbaStreamClient.KEY_A, shape = CircleShape,
                modifier = Modifier.align(Alignment.TopEnd).size(size)
            )
        }
    }

    /** Plain Button/clickable() only fires on release-tap; a GBA button
     * needs a real press/release pair (held = keeps sending the bit), hence
     * detectTapGestures(onPress) + awaitRelease() instead. */
    @Composable
    private fun HoldButton(label: String, bit: Int, shape: Shape, modifier: Modifier = Modifier) {
        Box(
            modifier = modifier
                .background(MaterialTheme.colorScheme.secondaryContainer.copy(alpha = 0.85f), shape)
                .pointerInput(bit) {
                    detectTapGestures(onPress = {
                        touchMask = touchMask or bit
                        sendCombinedInput()
                        try {
                            awaitRelease()
                        } finally {
                            touchMask = touchMask and bit.inv()
                            sendCombinedInput()
                        }
                    })
                },
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
        connected = false
        disconnectedReason = null
        val c = GbaStreamClient(this)
        client = c
        statusText = getString(R.string.status_connecting)
        c.connect(host, port)
    }

    private fun disconnect() {
        client?.disconnect()
        client = null
        stopAudio()
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
        runOnUiThread { connected = true }
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
        // release the global JNI ref to this Activity, or both leak. A
        // user-initiated exit goes through the system back button instead,
        // straight to onDestroy(), never through here -- so this is always
        // an unexpected drop.
        runOnUiThread {
            val wasConnected = connected
            disconnect()
            connected = false
            if (wasConnected) {
                // Stream was up and dropped on its own -- the hidden status
                // text alone wouldn't be noticed mid-game, so surface it.
                disconnectedReason = reason
            } else {
                // Never got connected in the first place (e.g. handshake
                // failure) -- no stream was "lost", just show it inline.
                statusText = getString(R.string.status_error, reason)
            }
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
