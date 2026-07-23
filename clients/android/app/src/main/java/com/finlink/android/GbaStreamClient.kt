package com.finlink.android

/**
 * Thin Kotlin shell around the native session: connects, decodes, and sends
 * input entirely in C (jni_bridge.c + finlink_core). This class only owns
 * the JNI handle and forwards calls; there's no protocol/transport logic on
 * the JVM side at all.
 */
class GbaStreamClient(private val listener: Listener) {

    interface Listener {
        fun onConnected()
        fun onVideoFrame(width: Int, height: Int, rgb565: ByteArray)
        fun onAudioFrame(sampleRate: Int, channels: Int, pcm: ShortArray)
        fun onDisconnected(reason: String)
    }

    @Volatile
    private var nativeHandle: Long = 0

    /** Spawns a background native thread; connect result arrives via onConnected/onDisconnected. */
    fun connect(host: String, port: Int) {
        nativeHandle = nativeConnect(host, port, listener)
    }

    /** Cheap: just records the latest key state, the native session loop sends it. */
    fun sendInput(keyMask: Int) {
        val handle = nativeHandle
        if (handle != 0L) nativeSendInput(handle, keyMask)
    }

    fun disconnect() {
        val handle = nativeHandle
        if (handle != 0L) {
            nativeHandle = 0
            nativeDisconnect(handle)
        }
    }

    private external fun nativeConnect(host: String, port: Int, listener: Listener): Long
    private external fun nativeSendInput(handle: Long, keyMask: Int)
    private external fun nativeDisconnect(handle: Long)

    companion object {
        // Player ports 6801-6804 (docs/protocol.md); the lobby itself (6800)
        // only ever serves the HTML page on any path, so a native picker has
        // to poll each player port's /status individually -- there's no
        // combined status endpoint to ask instead.
        const val PLAYER_BASE_PORT = 6801
        const val PLAYER_SLOT_COUNT = 4

        // Mirrors finlink_key in core/include/finlink/protocol.h.
        const val KEY_A = 1 shl 0
        const val KEY_B = 1 shl 1
        const val KEY_SELECT = 1 shl 2
        const val KEY_START = 1 shl 3
        const val KEY_RIGHT = 1 shl 4
        const val KEY_LEFT = 1 shl 5
        const val KEY_UP = 1 shl 6
        const val KEY_DOWN = 1 shl 7
        const val KEY_R = 1 shl 8
        const val KEY_L = 1 shl 9

        init {
            System.loadLibrary("finlink_android")
        }
    }
}
