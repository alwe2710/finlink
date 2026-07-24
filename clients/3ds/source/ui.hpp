#pragma once

// Small hand-rolled widget helpers shared by every bottom-screen UI in
// main.cpp (Menu, Settings, the on-screen touch overlay while playing).
// There's no citro2d-equivalent of borealis/Compose to lean on here, so
// this is the same "raw rects + manual touch hit-testing" approach as
// clients/switch/source/video_view.cpp's on-screen controls, just in C
// instead of NanoVG/C++.

#include <citro2d.h>

namespace ui {

struct Rect {
    float x, y, w, h;

    bool contains(float px, float py) const {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

// One-frame-delayed touch state: `down` from this frame, `wasDown` from
// the previous one, so a "tap" (rising edge) and "hold" can both be told
// apart from the raw touch-screen position.
struct Touch {
    bool down = false;
    bool wasDown = false;
    float x = 0, y = 0;

    bool tappedIn(const Rect &r) const {
        return down && !wasDown && r.contains(x, y);
    }

    bool heldIn(const Rect &r) const {
        return down && r.contains(x, y);
    }
};

inline void drawRect(const Rect &r, u32 color) {
    C2D_DrawRectSolid(r.x, r.y, 0.5f, r.w, r.h, color);
}

inline void drawText(C2D_TextBuf buf, const char *str, float x, float y, float scale, u32 color) {
    C2D_Text text;
    C2D_TextParse(&text, buf, str);
    C2D_TextOptimize(&text);
    C2D_DrawText(&text, C2D_WithColor, x, y, 0.6f, scale, scale, color);
}

constexpr u32 kColorBg = C2D_Color32(24, 24, 28, 255);
constexpr u32 kColorButton = C2D_Color32(60, 90, 200, 255);
constexpr u32 kColorButtonDisabled = C2D_Color32(50, 50, 55, 255);
constexpr u32 kColorButtonHeld = C2D_Color32(90, 120, 230, 255);
constexpr u32 kColorText = C2D_Color32(240, 240, 240, 255);
constexpr u32 kColorTextDim = C2D_Color32(160, 160, 170, 255);
constexpr u32 kColorToggleOn = C2D_Color32(80, 190, 100, 255);
constexpr u32 kColorToggleOff = C2D_Color32(70, 70, 78, 255);

// Draws a button, returns whether it was tapped this frame. `enabled`
// only affects appearance/hit-testing, callers still own the action.
inline bool button(C2D_TextBuf textBuf, const Touch &touch, const Rect &r, const char *label, bool enabled = true) {
    bool held = enabled && touch.heldIn(r);
    drawRect(r, !enabled ? kColorButtonDisabled : (held ? kColorButtonHeld : kColorButton));
    drawText(textBuf, label, r.x + 8, r.y + r.h / 2 - 8, 0.5f, kColorText);
    return enabled && touch.tappedIn(r);
}

// Draws an on/off toggle pill, returns whether it was tapped this frame
// (callers flip their own bool on tap).
inline bool toggle(const Touch &touch, const Rect &r, bool on) {
    drawRect(r, on ? kColorToggleOn : kColorToggleOff);
    Rect knob { on ? r.x + r.w - r.h : r.x, r.y, r.h, r.h };
    drawRect(knob, kColorText);
    return touch.tappedIn(r);
}

} // namespace ui
