#pragma once

#include <array>
#include <cstdint>

#include <3ds.h>

extern "C" {
#include "finlink/protocol.h"
}

// One entry per physical GBA button, mirroring
// clients/switch/source/gba_buttons.hpp and
// clients/android/.../GbaButtons.kt. Unlike those two, the 3DS's physical
// button layout already matches the GBA's almost exactly (D-pad, A/B,
// L/R, Start/Select), so there's no per-button remapping UI here -- just
// this one fixed, obvious default mapping.
struct GbaButton {
    const char *label;
    uint16_t bit;
    u32 key;
};

inline constexpr std::array<GbaButton, 10> GBA_BUTTONS = { {
    { "Up", FINLINK_KEY_UP, KEY_DUP },
    { "Down", FINLINK_KEY_DOWN, KEY_DDOWN },
    { "Left", FINLINK_KEY_LEFT, KEY_DLEFT },
    { "Right", FINLINK_KEY_RIGHT, KEY_DRIGHT },
    { "Select", FINLINK_KEY_SELECT, KEY_SELECT },
    { "Start", FINLINK_KEY_START, KEY_START },
    { "L", FINLINK_KEY_L, KEY_L },
    { "R", FINLINK_KEY_R, KEY_R },
    { "B", FINLINK_KEY_B, KEY_B },
    { "A", FINLINK_KEY_A, KEY_A },
} };
