#pragma once

// Settings, mirroring clients/switch/source/prefs.hpp minus key bindings
// (see gba_buttons.hpp for why: the 3DS's physical buttons already match
// the GBA's layout, so there's nothing to remap). Persisted as a flat
// key=value text file on the SD card.
class Prefs {
  public:
    Prefs();

    void save();

    bool onScreenControlsEnabled = true;

    // true = bilinear filtering (smooth upscale), false = nearest-neighbor
    // (crisp/pixelated upscale, the default) -- same rationale as the
    // other clients' toggle.
    bool bilinearVideoFilter = false;

  private:
    void load();
};
