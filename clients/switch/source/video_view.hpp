#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include <borealis.hpp>

// Fullscreen GBA video + on-screen touch overlay, all in one view: renders
// the latest decoded frame (scaled to fit, aspect-preserved, like the
// Android client's ContentScale.Fit) via a NanoVG image pattern, and draws
// + hit-tests the touch button overlay directly against raw touch state --
// see clients/android/.../PlayerActivity.kt's HoldButton composables for
// the layout this mirrors (L/R corners, Select/Start top-center, D-pad
// bottom-left, A/B diagonal bottom-right).
class VideoView : public brls::View {
  public:
    VideoView();
    ~VideoView() override;

    void draw(NVGcontext *vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext *ctx) override;

    // Called from the session's background thread; only stores the frame,
    // the actual GL/NanoVG upload happens on the render thread inside draw().
    void setFrame(uint32_t width, uint32_t height, const std::vector<uint8_t> &rgb565);

    void setBilinearFilter(bool bilinear);
    void setOnScreenControlsEnabled(bool enabled);

    // Bitmask of currently-held on-screen touch buttons, read once per
    // frame by PlayerActivity and OR'd with the physical-input mask.
    uint16_t getTouchMask() const {
        return touchMask;
    }

  private:
    std::mutex frameMutex;
    uint32_t pendingWidth = 0, pendingHeight = 0;
    std::vector<uint8_t> pendingRgba;
    bool frameDirty = false;

    int imageId = -1;
    int imageWidth = 0, imageHeight = 0;
    bool imageBilinear = false;
    bool wantBilinear = false;

    bool onScreenControlsEnabled = true;
    uint16_t touchMask = 0;

    struct TouchButton {
        const char *label;
        uint16_t bit;
        float x, y, w, h;
        bool round;
    };
    std::vector<TouchButton> layoutButtons(float width, float height) const;
};
