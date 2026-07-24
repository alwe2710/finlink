#pragma once

#include <map>
#include <string>

#include <borealis.hpp>

#include "prefs.hpp"

// On-screen-touch-overlay toggle, video filter toggle, and physical
// controller binding per GBA button -- same structure as
// clients/android/.../SettingsActivity.kt. Binding capture is done via a
// FramePoller (see frame_poller.hpp) rather than hooking into borealis's
// own focus/action dispatch, specifically to avoid the kind of
// double-dispatch bug the Android client hit doing it the other way.
class SettingsActivity : public brls::Activity {
  public:
    brls::View *createContentView() override;

  private:
    Prefs prefs;
    brls::Box *bindingsList = nullptr;
    std::string pendingBindPrefKey;
    bool captureWaitingForRelease = false;
    std::map<std::string, brls::Label *> bindingLabels;

    void refreshBindingRow(const std::string &prefKey);
    void onControllerButtonPressed(brls::ControllerButton button);
    std::string describeBinding(const std::string &prefKey, int defaultController) const;
};
