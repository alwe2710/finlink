package com.finlink.android

import android.app.Activity
import android.graphics.Bitmap
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.TextView
import java.net.HttpURLConnection
import java.net.URL
import java.nio.ByteBuffer
import org.json.JSONObject

/**
 * Bare-bones demo activity. Three states, one Activity (no navigation):
 *
 * 1. Lobby search: enter just the host (no port -- player ports are fixed
 *    by the protocol, see [GbaStreamClient.PLAYER_BASE_PORT]).
 * 2. Lobby picker: P1-P4, filled in from polling GET /status on all four
 *    player ports. Native equivalent of the web lobby's picker page
 *    (GBAStreamClientPage.h) -- see docs/protocol.md on why this has to
 *    poll each port individually rather than ask the lobby for a combined
 *    status.
 * 3. Connected: video as a plain ImageView, PCM playback via AudioTrack,
 *    hold-to-press buttons for the 10 GBA buttons.
 */
class MainActivity : Activity(), GbaStreamClient.Listener {

    private lateinit var lobbySearchRow: LinearLayout
    private lateinit var lobbyPickerRow: LinearLayout
    private lateinit var connectedRow: LinearLayout
    private lateinit var hostInput: EditText
    private lateinit var searchButton: Button
    private lateinit var playerButtons: List<Button>
    private lateinit var videoView: ImageView
    private lateinit var statusText: TextView

    private var client: GbaStreamClient? = null
    private var audioTrack: AudioTrack? = null
    private var keyMask = 0
    private var lastSearchedHost: String? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        lobbySearchRow = findViewById(R.id.lobbySearchRow)
        lobbyPickerRow = findViewById(R.id.lobbyPickerRow)
        connectedRow = findViewById(R.id.connectedRow)
        hostInput = findViewById(R.id.hostInput)
        searchButton = findViewById(R.id.searchButton)
        videoView = findViewById(R.id.videoView)
        statusText = findViewById(R.id.statusText)
        playerButtons = listOf(
            findViewById(R.id.btnP1), findViewById(R.id.btnP2),
            findViewById(R.id.btnP3), findViewById(R.id.btnP4)
        )

        searchButton.setOnClickListener { searchLobby() }
        playerButtons.forEachIndexed { index, button ->
            button.setOnClickListener {
                lastSearchedHost?.let { host -> connectTo(host, GbaStreamClient.PLAYER_BASE_PORT + index) }
            }
        }
        findViewById<Button>(R.id.disconnectButton).setOnClickListener { disconnect() }

        bindGbaButton(R.id.btnUp, GbaStreamClient.KEY_UP)
        bindGbaButton(R.id.btnDown, GbaStreamClient.KEY_DOWN)
        bindGbaButton(R.id.btnLeft, GbaStreamClient.KEY_LEFT)
        bindGbaButton(R.id.btnRight, GbaStreamClient.KEY_RIGHT)
        bindGbaButton(R.id.btnA, GbaStreamClient.KEY_A)
        bindGbaButton(R.id.btnB, GbaStreamClient.KEY_B)
        bindGbaButton(R.id.btnSelect, GbaStreamClient.KEY_SELECT)
        bindGbaButton(R.id.btnStart, GbaStreamClient.KEY_START)
        bindGbaButton(R.id.btnL, GbaStreamClient.KEY_L)
        bindGbaButton(R.id.btnR, GbaStreamClient.KEY_R)
    }

    private fun bindGbaButton(id: Int, bit: Int) {
        findViewById<Button>(id).setOnTouchListener { _, event ->
            when (event.action) {
                MotionEvent.ACTION_DOWN -> keyMask = keyMask or bit
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> keyMask = keyMask and bit.inv()
                else -> return@setOnTouchListener false
            }
            client?.sendInput(keyMask)
            true
        }
    }

    // --- Lobby search: plain java.net HTTP GET /status on each player port,
    // off the UI thread. This is ordinary status polling, not the GBA stream
    // itself, so it deliberately does NOT go through finlink_core/jni_bridge
    // -- no WebSocket involved here at all.

    private fun searchLobby() {
        val host = hostInput.text.toString().trim()
        if (host.isEmpty()) {
            statusText.text = getString(R.string.status_error, getString(R.string.lobby_host_required))
            return
        }

        searchButton.isEnabled = false
        lobbyPickerRow.visibility = View.GONE
        statusText.text = getString(R.string.lobby_searching)

        Thread {
            val occupied = (0 until GbaStreamClient.PLAYER_SLOT_COUNT).map { slot ->
                fetchOccupied(host, GbaStreamClient.PLAYER_BASE_PORT + slot)
            }
            runOnUiThread { applyLobbyResults(host, occupied) }
        }.start()
    }

    /** null = unreachable (port not configured as GBA (Client-Stream) at all, or unrelated error). */
    private fun fetchOccupied(host: String, port: Int): Boolean? {
        var connection: HttpURLConnection? = null
        return try {
            connection = (URL("http://$host:$port/status").openConnection() as HttpURLConnection).apply {
                connectTimeout = 1500
                readTimeout = 1500
                requestMethod = "GET"
            }
            val body = connection.inputStream.bufferedReader().use { it.readText() }
            JSONObject(body).optBoolean("occupied", false)
        } catch (e: Exception) {
            null
        } finally {
            connection?.disconnect()
        }
    }

    private fun applyLobbyResults(host: String, occupied: List<Boolean?>) {
        lastSearchedHost = host
        searchButton.isEnabled = true

        var anyFree = false
        playerButtons.forEachIndexed { index, button ->
            when (occupied[index]) {
                false -> {
                    button.isEnabled = true
                    button.alpha = 1.0f
                    anyFree = true
                }
                true -> {
                    button.isEnabled = false
                    button.alpha = 0.5f
                }
                null -> {
                    button.isEnabled = false
                    button.alpha = 0.3f
                }
            }
        }

        lobbyPickerRow.visibility = View.VISIBLE
        statusText.text = if (anyFree) getString(R.string.lobby_pick) else getString(R.string.lobby_none_free)
    }

    // --- Connection lifecycle ---

    private fun connectTo(host: String, port: Int) {
        keyMask = 0
        val c = GbaStreamClient(this)
        client = c

        lobbySearchRow.visibility = View.GONE
        lobbyPickerRow.visibility = View.GONE
        connectedRow.visibility = View.VISIBLE
        statusText.text = getString(R.string.status_connecting)

        c.connect(host, port)
    }

    private fun disconnect(resetStatusText: Boolean = true) {
        client?.disconnect()
        client = null
        stopAudio()

        connectedRow.visibility = View.GONE
        lobbySearchRow.visibility = View.VISIBLE
        // Deliberately not re-showing lobbyPickerRow: occupancy may have
        // changed since the last search, so require pressing "Suchen" again
        // rather than showing possibly-stale P1-P4 state.
        if (resetStatusText) {
            statusText.text = getString(R.string.status_disconnected)
        }
    }

    private fun stopAudio() {
        audioTrack?.stop()
        audioTrack?.release()
        audioTrack = null
    }

    // --- GbaStreamClient.Listener: called from the native session thread,
    // never the UI thread, so every callback must hop back via runOnUiThread
    // before touching a View. onAudioFrame is the one exception -- writing
    // to AudioTrack from a background thread is exactly what it's for.

    override fun onConnected() {
        runOnUiThread { statusText.text = getString(R.string.status_connected) }
    }

    override fun onVideoFrame(width: Int, height: Int, rgb565: ByteArray) {
        val bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.RGB_565)
        bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(rgb565))
        runOnUiThread { videoView.setImageBitmap(bitmap) }
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
            statusText.text = getString(R.string.status_error, reason)
        }
    }

    override fun onDestroy() {
        client?.disconnect()
        client = null
        stopAudio()
        super.onDestroy()
    }
}
