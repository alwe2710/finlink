#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <borealis.hpp>
#include <switch.h>

#include "prefs.hpp"
#include "session.hpp"
#include "video_view.hpp"

// The actual stream view: connects to host:port (passed in via the
// constructor, set by MenuActivity), shows video full-screen, plays
// audio, and accepts input from both the on-screen touch overlay (if
// enabled in Settings) and whatever physical controller bindings are set
// there. Owns the one GbaSession instance for its lifetime -- Menu and
// Settings never touch it. Mirrors
// clients/android/.../PlayerActivity.kt.
//
// Not wrapped in an AppletFrame: B is a GBA input here (mapped to the GBA
// B button by default), not "go back" like every other screen -- unlike
// Android, where the system back button/gesture is a channel entirely
// separate from any of the app's own key handling, every one of the
// Switch's face/shoulder/dpad buttons is claimed by the GBA button
// mapping. Exiting instead requires holding ZL+ZR (BUTTON_LT+BUTTON_RT,
// the triggers -- distinct from the L/R bumpers used for GBA L/R), shown
// as an on-screen hint.
class PlayerActivity : public brls::Activity {
  public:
    PlayerActivity(std::string host, int port);
    ~PlayerActivity() override;

    brls::View *createContentView() override;

  private:
    std::string host;
    int port;

    Prefs prefs;
    GbaSession session;
    VideoView *videoView = nullptr;
    brls::Label *statusLabel = nullptr;

    bool connected = false;
    uint16_t physicalMask = 0;
    float exitHoldSeconds = 0.0f;

    bool audioOpen = false;

    void onFrameTick();
    void sendCombinedInput();
    void playAudio(uint32_t sampleRate, uint8_t channels, std::vector<int16_t> pcm);
    void reclaimAudioBuffers();
    void closeAudio();
    void showDisconnectDialog(const std::string &reason);
};
