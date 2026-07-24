#include "video_view.hpp"

#include <algorithm>

extern "C" {
#include "finlink/protocol.h"
}

VideoView::VideoView() {
    this->setWidthPercentage(100);
    this->setHeightPercentage(100);
}

VideoView::~VideoView() {
    // Image deletion needs a live NVGcontext, which we don't keep a
    // reference to outside of draw() -- leaking the GL texture here is
    // fine, the process/console reclaims it on exit, and the player is
    // only ever created/destroyed a handful of times per session.
}

void VideoView::setFrame(uint32_t width, uint32_t height, const std::vector<uint8_t> &rgb565) {
    std::vector<uint8_t> rgba(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; i++) {
        uint16_t px = static_cast<uint16_t>(rgb565[i * 2] | (rgb565[i * 2 + 1] << 8));
        uint8_t r5 = (px >> 11) & 0x1F;
        uint8_t g6 = (px >> 5) & 0x3F;
        uint8_t b5 = px & 0x1F;
        rgba[i * 4 + 0] = static_cast<uint8_t>((r5 * 255 + 15) / 31);
        rgba[i * 4 + 1] = static_cast<uint8_t>((g6 * 255 + 31) / 63);
        rgba[i * 4 + 2] = static_cast<uint8_t>((b5 * 255 + 15) / 31);
        rgba[i * 4 + 3] = 255;
    }

    std::lock_guard<std::mutex> lock(frameMutex);
    pendingWidth = width;
    pendingHeight = height;
    pendingRgba = std::move(rgba);
    frameDirty = true;
}

void VideoView::setBilinearFilter(bool bilinear) {
    wantBilinear = bilinear;
    std::lock_guard<std::mutex> lock(frameMutex);
    frameDirty = frameDirty || (imageId >= 0 && imageBilinear != bilinear);
}

void VideoView::setOnScreenControlsEnabled(bool enabled) {
    onScreenControlsEnabled = enabled;
    if (!enabled) {
        touchMask = 0;
    }
}

std::vector<VideoView::TouchButton> VideoView::layoutButtons(float width, float height) const {
    std::vector<TouchButton> buttons;
    const float pad = 24;

    buttons.push_back({ "L", FINLINK_KEY_L, pad, pad, 90, 50, false });
    buttons.push_back({ "R", FINLINK_KEY_R, width - pad - 90, pad, 90, 50, false });

    const float selStartW = 90, selStartH = 36, gap = 12;
    float centerX = width / 2;
    buttons.push_back({ "Select", FINLINK_KEY_SELECT, centerX - selStartW - gap / 2, pad, selStartW, selStartH,
                         false });
    buttons.push_back({ "Start", FINLINK_KEY_START, centerX + gap / 2, pad, selStartW, selStartH, false });

    const float segment = 64;
    float dpadX = pad, dpadY = height - pad - segment * 3;
    buttons.push_back({ "^", FINLINK_KEY_UP, dpadX + segment, dpadY, segment, segment, false });
    buttons.push_back({ "v", FINLINK_KEY_DOWN, dpadX + segment, dpadY + segment * 2, segment, segment, false });
    buttons.push_back({ "<", FINLINK_KEY_LEFT, dpadX, dpadY + segment, segment, segment, false });
    buttons.push_back({ ">", FINLINK_KEY_RIGHT, dpadX + segment * 2, dpadY + segment, segment, segment, false });

    const float actionSize = 84;
    float clusterX = width - pad - actionSize * 2;
    float clusterY = height - pad - actionSize * 1.6f;
    buttons.push_back({ "B", FINLINK_KEY_B, clusterX, clusterY + actionSize * 0.6f, actionSize, actionSize, true });
    buttons.push_back({ "A", FINLINK_KEY_A, clusterX + actionSize, clusterY, actionSize, actionSize, true });

    return buttons;
}

void VideoView::draw(NVGcontext *vg, float x, float y, float width, float height, brls::Style style,
                      brls::FrameContext *ctx) {
    (void)style;
    (void)ctx;

    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, height);
    nvgFillColor(vg, nvgRGB(0, 0, 0));
    nvgFill(vg);

    {
        std::lock_guard<std::mutex> lock(frameMutex);
        if (frameDirty && !pendingRgba.empty()) {
            int flags = wantBilinear ? 0 : NVG_IMAGE_NEAREST;
            if (imageId < 0 || imageWidth != static_cast<int>(pendingWidth) ||
                imageHeight != static_cast<int>(pendingHeight) || imageBilinear != wantBilinear) {
                if (imageId >= 0) {
                    nvgDeleteImage(vg, imageId);
                }
                imageId = nvgCreateImageRGBA(vg, static_cast<int>(pendingWidth), static_cast<int>(pendingHeight),
                                              flags, pendingRgba.data());
                imageWidth = static_cast<int>(pendingWidth);
                imageHeight = static_cast<int>(pendingHeight);
                imageBilinear = wantBilinear;
            } else {
                nvgUpdateImage(vg, imageId, pendingRgba.data());
            }
            frameDirty = false;
        }
    }

    if (imageId >= 0 && imageWidth > 0 && imageHeight > 0) {
        float scale = std::min(width / imageWidth, height / imageHeight);
        float dw = imageWidth * scale, dh = imageHeight * scale;
        float dx = x + (width - dw) / 2, dy = y + (height - dh) / 2;

        NVGpaint paint = nvgImagePattern(vg, dx, dy, dw, dh, 0, imageId, 1.0f);
        nvgBeginPath(vg);
        nvgRect(vg, dx, dy, dw, dh);
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    if (!onScreenControlsEnabled) {
        touchMask = 0;
        return;
    }

    auto buttons = layoutButtons(width, height);

    std::vector<brls::RawTouchState> touches;
    brls::Application::getPlatform()->getInputManager()->updateTouchStates(&touches);

    uint16_t newMask = 0;
    for (const auto &button : buttons) {
        float bx = x + button.x, by = y + button.y;
        bool held = false;
        for (const auto &touch : touches) {
            if (!touch.pressed) {
                continue;
            }
            if (touch.position.x >= bx && touch.position.x <= bx + button.w && touch.position.y >= by &&
                touch.position.y <= by + button.h) {
                held = true;
                break;
            }
        }
        if (held) {
            newMask |= button.bit;
        }

        nvgBeginPath(vg);
        if (button.round) {
            nvgCircle(vg, bx + button.w / 2, by + button.h / 2, button.w / 2);
        } else {
            nvgRoundedRect(vg, bx, by, button.w, button.h, 8);
        }
        nvgFillColor(vg, held ? nvgRGBA(255, 255, 255, 130) : nvgRGBA(255, 255, 255, 70));
        nvgFill(vg);

        nvgFontFaceId(vg, brls::Application::getDefaultFont());
        nvgFontSize(vg, 18);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 220));
        nvgText(vg, bx + button.w / 2, by + button.h / 2, button.label, nullptr);
    }

    touchMask = newMask;
}
