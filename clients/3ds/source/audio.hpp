#pragma once

#include <cstdint>
#include <vector>

#include <3ds.h>

// GBA audio playback via NDSP, on a single fixed channel. Unlike the
// Switch client (audout is fixed at 48kHz/stereo, needing a resampler),
// NDSP takes an arbitrary sample rate per ndspChnSetRate(), so the
// server's audio is just remixed to stereo and played as-is.
class AudioPlayer {
  public:
    AudioPlayer();
    ~AudioPlayer();

    // Called from the session's background thread.
    void play(uint32_t sampleRate, uint8_t channels, std::vector<int16_t> pcm);

  private:
    static constexpr int kChannel = 0;

    // NDSP wavebufs must point at linear (DSP-visible) memory, not regular
    // heap allocations -- each pending buffer owns its ndspWaveBuf plus the
    // linearAlloc'd sample data, kept alive until NDSP marks it done.
    struct PendingBuffer {
        ndspWaveBuf wavebuf {};
        int16_t *linearData = nullptr;
    };
    std::vector<PendingBuffer *> pending;

    bool open = false;
    uint32_t currentRate = 0;

    void reclaim();
};
