#include "menu_activity.hpp"

#include <array>
#include <optional>
#include <thread>

#include "discovery.hpp"
#include "player_activity.hpp"
#include "settings_activity.hpp"

namespace {

constexpr int kLobbyPort = 6800;
constexpr int kPlayerBasePort = 6801;
constexpr int kPlayerSlotCount = 4;

void addDivider(brls::Box *parent) {
    auto *rect = new brls::Rectangle(nvgRGBA(255, 255, 255, 40));
    rect->setWidthPercentage(100);
    rect->setHeight(1);
    rect->setMarginTop(12);
    rect->setMarginBottom(12);
    parent->addView(rect);
}

} // namespace

brls::View *MenuActivity::createContentView() {
    auto *column = new brls::Box();
    column->setAxis(brls::Axis::COLUMN);
    column->setAlignItems(brls::AlignItems::STRETCH);
    column->setPadding(24, 32, 24, 32);

    hostInput = new brls::InputCell();
    hostInput->init(
        "Host", "", [](std::string) {}, "z.B. 192.168.1.5", "IP-Adresse des Streaming-Hosts");
    column->addView(hostInput);

    auto *connectCell = new brls::DetailCell();
    connectCell->setText("Verbinden");
    connectCell->registerClickAction([this](brls::View *) {
        std::string host = hostInput->getValue();
        if (host.empty()) {
            statusLabel->setText("Bitte zuerst einen Host eingeben.");
            return true;
        }
        runSearch(host);
        return true;
    });
    column->addView(connectCell);

    slotRow = new brls::Box();
    slotRow->setAxis(brls::Axis::ROW);
    slotRow->setMarginTop(8);
    column->addView(slotRow);

    statusLabel = new brls::Label();
    statusLabel->setText("Nicht verbunden.");
    statusLabel->setMarginTop(8);
    column->addView(statusLabel);

    addDivider(column);

    auto *discoverCell = new brls::DetailCell();
    discoverCell->setText("Netzwerk durchsuchen");
    discoverCell->registerClickAction([this](brls::View *) {
        startDiscovery();
        return true;
    });
    column->addView(discoverCell);

    discoveryStatusLabel = new brls::Label();
    discoveryStatusLabel->setText("");
    discoveryStatusLabel->setMarginTop(4);
    column->addView(discoveryStatusLabel);

    discoveredList = new brls::Box();
    discoveredList->setAxis(brls::Axis::COLUMN);
    discoveredList->setMarginTop(4);
    column->addView(discoveredList);

    addDivider(column);

    auto *settingsCell = new brls::DetailCell();
    settingsCell->setText("Einstellungen");
    settingsCell->registerClickAction([](brls::View *) {
        brls::Application::pushActivity(new SettingsActivity());
        return true;
    });
    column->addView(settingsCell);

    auto *scroll = new brls::ScrollingFrame();
    scroll->setGrow(1.0f);
    scroll->setContentView(column);

    auto *frame = new brls::AppletFrame(scroll);
    frame->setTitle("finlink");
    return frame;
}

void MenuActivity::runSearch(const std::string &host) {
    if (searching) {
        return;
    }
    searching = true;
    slotRow->clearViews();
    statusLabel->setText("Suche...");

    std::thread([this, host]() {
        std::array<std::optional<bool>, kPlayerSlotCount> occupied;
        for (int slot = 0; slot < kPlayerSlotCount; slot++) {
            occupied[slot] = discovery::fetchOccupied(host, kPlayerBasePort + slot);
        }

        brls::sync([this, host, occupied]() {
            lastSearchedHost = host;
            searching = false;

            bool anyFree = false;
            for (int slot = 0; slot < kPlayerSlotCount; slot++) {
                auto *button = new brls::Button();
                button->setText("P" + std::to_string(slot + 1));
                button->setGrow(1.0f);
                button->setMarginLeft(slot == 0 ? 0 : 8);

                if (!occupied[slot].has_value()) {
                    button->setState(brls::ButtonState::DISABLED);
                } else if (*occupied[slot]) {
                    button->setState(brls::ButtonState::DISABLED);
                } else {
                    anyFree = true;
                    int port = kPlayerBasePort + slot;
                    button->registerClickAction([this, host, port](brls::View *) {
                        launchPlayer(host, port);
                        return true;
                    });
                }
                slotRow->addView(button);
            }

            statusLabel->setText(anyFree ? "Freien Slot wählen." : "Kein freier Slot auf diesem Host.");
        });
    }).detach();
}

void MenuActivity::startDiscovery() {
    if (discovering) {
        return;
    }
    discovering = true;
    discoveredList->clearViews();
    discoveryStatusLabel->setText("Suche läuft...");

    std::thread([this]() {
        auto hosts = discovery::localSubnetHosts();
        if (hosts.empty()) {
            brls::sync([this]() {
                discovering = false;
                discoveryStatusLabel->setText("Kein lokales Netzwerk gefunden.");
            });
            return;
        }

        for (const auto &ip : hosts) {
            if (discovery::probeLobby(ip)) {
                brls::sync([this, ip]() {
                    auto *cell = new brls::DetailCell();
                    cell->setText(ip);
                    cell->registerClickAction([this, ip](brls::View *) {
                        hostInput->setValue(ip);
                        runSearch(ip);
                        return true;
                    });
                    discoveredList->addView(cell);
                });
            }
        }

        brls::sync([this]() {
            discovering = false;
            discoveryStatusLabel->setText(discoveredList->getChildren().empty() ? "Nichts gefunden."
                                                                                  : "Suche abgeschlossen.");
        });
    }).detach();
}

void MenuActivity::launchPlayer(const std::string &host, int port) {
    brls::Application::pushActivity(new PlayerActivity(host, port));
}
