package com.finlink.android

import android.app.Activity
import android.graphics.Bitmap
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Bundle
import android.view.MotionEvent
import android.widget.Button
import android.widget.EditText
import android.widget.ImageView
import android.widget.TextView
import java.nio.ByteBuffer

/**
 * Bare-bones demo activity: host:port entry, video as a plain ImageView,
 * PCM playback via AudioTrack, and hold-to-press buttons for the 10 GBA
 * buttons. No lobby/picker UI (see docs/protocol.md on why the lobby can't
 * offer a combined status anyway) -- point it directly at one player port.
 */
class MainActivity : Activity(), GbaStreamClient.Listener {

    private lateinit var hostPortInput: EditText
    private lateinit var connectButton: Button
    private lateinit var videoView: ImageView
    private lateinit var statusText: TextView

    private var client: GbaStreamClient? = null
    private var audioTrack: AudioTrack? = null
    private var keyMask = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        hostPortInput = findViewById(R.id.hostPortInput)
        connectButton = findViewById(R.id.connectButton)
        videoView = findViewById(R.id.videoView)
        statusText = findViewById(R.id.statusText)

        connectButton.setOnClickListener {
            if (client == null) connect() else disconnect()
        }

        bindButton(R.id.btnUp, GbaStreamClient.KEY_UP)
        bindButton(R.id.btnDown, GbaStreamClient.KEY_DOWN)
        bindButton(R.id.btnLeft, GbaStreamClient.KEY_LEFT)
        bindButton(R.id.btnRight, GbaStreamClient.KEY_RIGHT)
        bindButton(R.id.btnA, GbaStreamClient.KEY_A)
        bindButton(R.id.btnB, GbaStreamClient.KEY_B)
        bindButton(R.id.btnSelect, GbaStreamClient.KEY_SELECT)
        bindButton(R.id.btnStart, GbaStreamClient.KEY_START)
        bindButton(R.id.btnL, GbaStreamClient.KEY_L)
        bindButton(R.id.btnR, GbaStreamClient.KEY_R)
    }

    private fun bindButton(id: Int, bit: Int) {
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

    private fun connect() {
        val hostPort = hostPortInput.text.toString().trim()
        val colon = hostPort.lastIndexOf(':')
        val port = if (colon >= 0) hostPort.substring(colon + 1).toIntOrNull() else null
        if (colon < 0 || port == null) {
            statusText.text = getString(R.string.status_error, "erwartet host:port")
            return
        }
        val host = hostPort.substring(0, colon)

        keyMask = 0
        val c = GbaStreamClient(this)
        client = c
        statusText.text = getString(R.string.status_connecting)
        connectButton.text = getString(R.string.disconnect)
        c.connect(host, port)
    }

    private fun disconnect() {
        client?.disconnect()
        client = null
        stopAudio()
        connectButton.text = getString(R.string.connect)
        statusText.text = getString(R.string.status_disconnected)
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
            statusText.text = getString(R.string.status_error, reason)
            disconnect()
        }
    }

    override fun onDestroy() {
        client?.disconnect()
        client = null
        stopAudio()
        super.onDestroy()
    }
}
