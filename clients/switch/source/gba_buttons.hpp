#pragma once

#include <array>
#include <cstdint>

#include <borealis/core/input.hpp>

extern "C" {
#include "finlink/protocol.h"
}

// One entry per physical GBA button, shared between PlayerActivity (touch
// overlay + physical input polling) and SettingsActivity (key binding
// list), mirroring clients/android/.../GbaButtons.kt so both clients stay
// in sync with a single source of truth for labels/bits/pref keys.
struct GbaButton {
    const char *label;
    uint16_t bit;
    const char *prefKey;
    // Sensible out-of-the-box mapping to a physical Switch button, applied
    // whenever no user override is stored in Prefs. Unlike the Android
    // client (which ships with everything unbound, since touch is always
    // available there), the Switch's touchscreen only exists in handheld
    // mode -- docked play needs a working default from first launch.
    brls::ControllerButton defaultController;
};

inline constexpr std::array<GbaButton, 10> GBA_BUTTONS = { {
    { "Up", FINLINK_KEY_UP, "UP", brls::BUTTON_UP },
    { "Down", FINLINK_KEY_DOWN, "DOWN", brls::BUTTON_DOWN },
    { "Left", FINLINK_KEY_LEFT, "LEFT", brls::BUTTON_LEFT },
    { "Right", FINLINK_KEY_RIGHT, "RIGHT", brls::BUTTON_RIGHT },
    { "Select", FINLINK_KEY_SELECT, "SELECT", brls::BUTTON_BACK },
    { "Start", FINLINK_KEY_START, "START", brls::BUTTON_START },
    { "L", FINLINK_KEY_L, "L", brls::BUTTON_LB },
    { "R", FINLINK_KEY_R, "R", brls::BUTTON_RB },
    { "B", FINLINK_KEY_B, "B", brls::BUTTON_B },
    { "A", FINLINK_KEY_A, "A", brls::BUTTON_A },
} };
