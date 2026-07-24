#pragma once

#include <borealis.hpp>

// Landing screen (Menu/Settings/Player, same three-screen structure as
// clients/android/.../MenuActivity.kt): manual host entry + P1-P4 picker
// (poll GET /status on all four player ports), or LAN discovery (sweep the
// local subnet for a host answering on the lobby port). Picking a free P
// slot pushes PlayerActivity; the settings row pushes SettingsActivity.
// Neither owns any GbaSession -- that's entirely PlayerActivity's job.
class MenuActivity : public brls::Activity {
  public:
    brls::View *createContentView() override;

  private:
    brls::InputCell *hostInput = nullptr;
    brls::Box *slotRow = nullptr;
    brls::Label *statusLabel = nullptr;
    brls::Label *discoveryStatusLabel = nullptr;
    brls::Box *discoveredList = nullptr;

    std::string lastSearchedHost;
    bool searching = false;
    bool discovering = false;

    void runSearch(const std::string &host);
    void startDiscovery();
    void launchPlayer(const std::string &host, int port);
};
