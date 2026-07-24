#pragma once

#include <map>
#include <string>

// Settings, read/written from SettingsActivity and read by PlayerActivity:
// on-screen-touch-overlay toggle, video filter (bilinear vs nearest), and a
// per-GBA-button physical controller binding. Persisted as a flat
// key=value text file on the SD card -- there's no SharedPreferences
// equivalent in libnx, and the handful of keys here don't warrant pulling
// in a real config/JSON library. Mirrors clients/android/.../Prefs.kt.
class Prefs {
  public:
    Prefs();

    void save();

    bool onScreenControlsEnabled = true;

    // true = bilinear filtering (smooth upscale), false = nearest-neighbor
    // (crisp/pixelated upscale, the default) -- same rationale as the
    // Android client's toggle: the GBA's native 240x160 framebuffer is
    // upscaled a lot to fill the screen.
    bool bilinearVideoFilter = false;

    // No override stored yet -- caller should fall back to
    // GbaButton::defaultController.
    static constexpr int kNoOverride = -2;
    // User explicitly cleared the binding -- no input at all for that GBA
    // button, matching the Android client's "unbound" semantics once a
    // binding has been touched in Settings.
    static constexpr int kExplicitlyUnbound = -1;

    // Returns the bound brls::ControllerButton for prefKey, or one of the
    // two sentinels above.
    int keyBinding(const std::string &prefKey) const;
    void setKeyBinding(const std::string &prefKey, int controllerButton);
    void clearKeyBinding(const std::string &prefKey);
    bool hasExplicitBinding(const std::string &prefKey) const;

  private:
    std::map<std::string, std::string> values;

    void load();
    std::string path() const;
};
