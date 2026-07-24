#pragma once

#include <functional>

#include <borealis/core/box.hpp>

// Invisible zero-size view whose only job is to run a callback once per
// frame from draw(), which borealis calls unconditionally for every
// VISIBLE view in the tree regardless of what currently has input focus.
// Used for continuous physical-controller polling (PlayerActivity's game
// input, SettingsActivity's "press a button to bind" capture) instead of
// hooking into the focus/click-action dispatch system, which is exactly
// the kind of double-dispatch the Android client's D-pad key-binding bug
// came from (a key press being consumed by both the binding capture and
// Compose's own focus navigation at once).
class FramePoller : public brls::Box {
  public:
    explicit FramePoller(std::function<void()> onFrame) : onFrame(std::move(onFrame)) {
        this->setWidth(0);
        this->setHeight(0);
    }

    void draw(NVGcontext *vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext *ctx) override {
        (void)vg;
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)style;
        (void)ctx;
        if (onFrame) {
            onFrame();
        }
    }

  private:
    std::function<void()> onFrame;
};
