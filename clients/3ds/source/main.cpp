// finlink for Nintendo 3DS: top screen shows the GBA stream, bottom
// screen shows Menu/Settings before connecting and the on-screen touch
// controls (+ a "Trennen" button) while playing -- the 3DS's dual screens
// map onto Menu/Settings/Player far more naturally than the
// single-screen-at-a-time approach the other clients use. See
// clients/3ds/README.md.
//
// No borealis/Compose-equivalent UI framework here: widgets are hand-rolled
// citro2d rects/text with manual touch hit-testing (ui.hpp), same spirit as
// clients/switch/source/video_view.cpp's on-screen controls.

#include <array>
#include <atomic>
#include <cstdlib>
#include <malloc.h>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <3ds.h>
#include <citro2d.h>

#include "audio.hpp"
#include "discovery.hpp"
#include "gba_buttons.hpp"
#include "prefs.hpp"
#include "session.hpp"
#include "ui.hpp"
#include "video_tex.hpp"

namespace {

constexpr int kLobbyPort = 6800;
constexpr int kPlayerBasePort = 6801;
constexpr int kPlayerSlotCount = 4;
constexpr u32 kSocBufferSize = 0x100000;

enum class BottomScreenState { MENU, SETTINGS };

// Everything the background search/discovery threads write, read by the
// main loop each frame. One coarse mutex: updates are infrequent (once
// per search/probe), so there's no reason to split it up further.
struct MenuState {
    std::mutex mutex;

    std::string hostText;
    bool searching = false;
    std::string statusText = "Nicht verbunden.";
    std::array<std::optional<bool>, kPlayerSlotCount> slotOccupied {};
    bool pickerVisible = false;
    std::string lastSearchedHost;

    bool discovering = false;
    std::string discoveryStatusText;
    std::vector<std::string> discoveredHosts;
};

void runSearch(MenuState *menu, std::string host) {
    {
        std::lock_guard<std::mutex> lock(menu->mutex);
        if (menu->searching) {
            return;
        }
        menu->searching = true;
        menu->pickerVisible = false;
        menu->statusText = "Suche...";
    }

    std::thread([menu, host]() {
        std::array<std::optional<bool>, kPlayerSlotCount> occupied;
        for (int slot = 0; slot < kPlayerSlotCount; slot++) {
            occupied[slot] = discovery::fetchOccupied(host, kPlayerBasePort + slot);
        }

        std::lock_guard<std::mutex> lock(menu->mutex);
        menu->lastSearchedHost = host;
        menu->searching = false;
        menu->slotOccupied = occupied;
        menu->pickerVisible = true;

        bool anyFree = false;
        for (const auto &o : occupied) {
            if (o.has_value() && !*o) {
                anyFree = true;
            }
        }
        menu->statusText = anyFree ? "Freien Slot waehlen." : "Kein freier Slot auf diesem Host.";
    }).detach();
}

void startDiscovery(MenuState *menu) {
    {
        std::lock_guard<std::mutex> lock(menu->mutex);
        if (menu->discovering) {
            return;
        }
        menu->discovering = true;
        menu->discoveredHosts.clear();
        menu->discoveryStatusText = "Suche laeuft...";
    }

    std::thread([menu]() {
        auto hosts = discovery::localSubnetHosts();
        if (hosts.empty()) {
            std::lock_guard<std::mutex> lock(menu->mutex);
            menu->discovering = false;
            menu->discoveryStatusText = "Kein lokales Netzwerk gefunden.";
            return;
        }

        for (const auto &ip : hosts) {
            if (discovery::probeLobby(ip)) {
                std::lock_guard<std::mutex> lock(menu->mutex);
                menu->discoveredHosts.push_back(ip);
            }
        }

        std::lock_guard<std::mutex> lock(menu->mutex);
        menu->discovering = false;
        menu->discoveryStatusText =
            menu->discoveredHosts.empty() ? "Nichts gefunden." : "Suche abgeschlossen.";
    }).detach();
}

// Blocking software-keyboard prompt for the host IP -- must be called
// outside C3D_FrameBegin/End, the applet draws its own frames while up.
std::string promptForHost(const std::string &initial) {
    SwkbdState swkbd;
    char buf[64];
    swkbdInit(&swkbd, SWKBD_TYPE_NORMAL, 2, sizeof(buf) - 1);
    swkbdSetHintText(&swkbd, "IP-Adresse, z.B. 192.168.1.5");
    if (!initial.empty()) {
        swkbdSetInitialText(&swkbd, initial.c_str());
    }
    SwkbdButton button = swkbdInputText(&swkbd, buf, sizeof(buf));
    if (button == SWKBD_BUTTON_CONFIRM) {
        return std::string(buf);
    }
    return initial;
}

void drawMenuScreen(C2D_TextBuf textBuf, const ui::Touch &touch, MenuState *menu, BottomScreenState *screenState,
                     GbaSession *session, VideoTex *videoTex, AudioPlayer *audio, std::atomic<bool> *connected,
                     std::string *connectedHost) {
    // Snapshot under a short lock, then draw/hit-test from local copies --
    // promptForHost() below blocks for as long as the user is typing, and
    // runSearch()/startDiscovery() spawn threads that take menu->mutex
    // themselves, so the lock can't be held across any of those calls.
    std::string hostText, statusText, discoveryStatusText, lastSearchedHost;
    bool searching, pickerVisible, discovering;
    std::array<std::optional<bool>, kPlayerSlotCount> slotOccupied;
    std::vector<std::string> discoveredHosts;
    {
        std::lock_guard<std::mutex> lock(menu->mutex);
        hostText = menu->hostText;
        searching = menu->searching;
        statusText = menu->statusText;
        slotOccupied = menu->slotOccupied;
        pickerVisible = menu->pickerVisible;
        lastSearchedHost = menu->lastSearchedHost;
        discovering = menu->discovering;
        discoveryStatusText = menu->discoveryStatusText;
        discoveredHosts = menu->discoveredHosts;
    }

    ui::Rect hostRect { 8, 8, 240, 28 };
    ui::drawRect(hostRect, ui::kColorButtonDisabled);
    ui::drawText(textBuf, hostText.empty() ? "Host eingeben..." : hostText.c_str(), hostRect.x + 8, hostRect.y + 6,
                 0.5f, hostText.empty() ? ui::kColorTextDim : ui::kColorText);
    if (touch.tappedIn(hostRect)) {
        std::string newHost = promptForHost(hostText);
        std::lock_guard<std::mutex> lock(menu->mutex);
        menu->hostText = newHost;
    }

    ui::Rect connectRect { 252, 8, 60, 28 };
    if (ui::button(textBuf, touch, connectRect, "Los", !searching && !hostText.empty())) {
        runSearch(menu, hostText);
    }

    if (pickerVisible) {
        for (int slot = 0; slot < kPlayerSlotCount; slot++) {
            ui::Rect r { 8.0f + slot * 76.0f, 44, 70, 26 };
            bool free = slotOccupied[slot].has_value() && !*slotOccupied[slot];
            char label[4];
            snprintf(label, sizeof(label), "P%d", slot + 1);
            if (ui::button(textBuf, touch, r, label, free)) {
                int port = kPlayerBasePort + slot;
                *connectedHost = lastSearchedHost;
                session->connect(lastSearchedHost, port,
                    GbaSession::Listener {
                        .onConnected = [connected]() { *connected = true; },
                        .onVideoFrame =
                            [videoTex](uint32_t w, uint32_t h, std::vector<uint8_t> rgb) {
                                videoTex->setFrame(w, h, rgb);
                            },
                        .onAudioFrame =
                            [audio](uint32_t rate, uint8_t ch, std::vector<int16_t> pcm) {
                                audio->play(rate, ch, std::move(pcm));
                            },
                        .onDisconnected =
                            [connected, menu](std::string reason) {
                                *connected = false;
                                std::lock_guard<std::mutex> l(menu->mutex);
                                menu->statusText = "Fehler: " + reason;
                            },
                    });
            }
        }
    }

    ui::drawText(textBuf, statusText.c_str(), 8, 74, 0.42f, ui::kColorTextDim);

    ui::Rect discRect { 8, 96, 304, 24 };
    if (ui::button(textBuf, touch, discRect, "Netzwerk durchsuchen", !discovering)) {
        startDiscovery(menu);
    }
    ui::drawText(textBuf, discoveryStatusText.c_str(), 8, 124, 0.42f, ui::kColorTextDim);

    int shown = 0;
    for (const auto &ip : discoveredHosts) {
        if (shown >= 3) {
            break;
        }
        ui::Rect r { 8, 140.0f + shown * 20.0f, 304, 18 };
        if (ui::button(textBuf, touch, r, ip.c_str())) {
            {
                std::lock_guard<std::mutex> lock(menu->mutex);
                menu->hostText = ip;
            }
            runSearch(menu, ip);
        }
        shown++;
    }
    if (static_cast<int>(discoveredHosts.size()) > shown) {
        char more[32];
        snprintf(more, sizeof(more), "+%d weitere", static_cast<int>(discoveredHosts.size()) - shown);
        ui::drawText(textBuf, more, 8, 200, 0.4f, ui::kColorTextDim);
    }

    ui::Rect settingsRect { 8, 210, 304, 24 };
    if (ui::button(textBuf, touch, settingsRect, "Einstellungen")) {
        *screenState = BottomScreenState::SETTINGS;
    }
}

void drawSettingsScreen(C2D_TextBuf textBuf, const ui::Touch &touch, Prefs *prefs, VideoTex *videoTex,
                         BottomScreenState *screenState) {
    ui::drawText(textBuf, "Einstellungen", 8, 8, 0.55f, ui::kColorText);

    ui::drawText(textBuf, "Bildschirmsteuerung", 8, 44, 0.45f, ui::kColorText);
    ui::Rect t1 { 250, 40, 60, 28 };
    if (ui::toggle(touch, t1, prefs->onScreenControlsEnabled)) {
        prefs->onScreenControlsEnabled = !prefs->onScreenControlsEnabled;
        prefs->save();
    }

    ui::drawText(textBuf, "Bilineare Filterung", 8, 84, 0.45f, ui::kColorText);
    ui::Rect t2 { 250, 80, 60, 28 };
    if (ui::toggle(touch, t2, prefs->bilinearVideoFilter)) {
        prefs->bilinearVideoFilter = !prefs->bilinearVideoFilter;
        prefs->save();
        videoTex->setBilinearFilter(prefs->bilinearVideoFilter);
    }

    ui::Rect backRect { 8, 210, 304, 24 };
    if (ui::button(textBuf, touch, backRect, "Zurueck")) {
        *screenState = BottomScreenState::MENU;
    }
}

// Cross-shaped D-pad + diagonal A/B cluster + L/R/Select/Start, matching
// the other clients' on-screen layout (see
// clients/switch/source/video_view.cpp's layoutButtons()), adapted to the
// bottom screen's 320x240.
uint16_t drawPlayerControls(C2D_TextBuf textBuf, const ui::Touch &touch) {
    uint16_t mask = 0;

    ui::Rect l { 8, 8, 60, 32 };
    ui::Rect r { 252, 8, 60, 32 };
    ui::drawRect(l, touch.heldIn(l) ? ui::kColorButtonHeld : ui::kColorButton);
    ui::drawText(textBuf, "L", l.x + 24, l.y + 8, 0.5f, ui::kColorText);
    if (touch.heldIn(l)) mask |= FINLINK_KEY_L;
    ui::drawRect(r, touch.heldIn(r) ? ui::kColorButtonHeld : ui::kColorButton);
    ui::drawText(textBuf, "R", r.x + 24, r.y + 8, 0.5f, ui::kColorText);
    if (touch.heldIn(r)) mask |= FINLINK_KEY_R;

    ui::Rect sel { 100, 8, 56, 26 };
    ui::Rect start { 164, 8, 56, 26 };
    ui::drawRect(sel, touch.heldIn(sel) ? ui::kColorButtonHeld : ui::kColorButton);
    ui::drawText(textBuf, "Select", sel.x + 6, sel.y + 6, 0.4f, ui::kColorText);
    if (touch.heldIn(sel)) mask |= FINLINK_KEY_SELECT;
    ui::drawRect(start, touch.heldIn(start) ? ui::kColorButtonHeld : ui::kColorButton);
    ui::drawText(textBuf, "Start", start.x + 10, start.y + 6, 0.4f, ui::kColorText);
    if (touch.heldIn(start)) mask |= FINLINK_KEY_START;

    const float seg = 46;
    float dpadX = 16, dpadY = 96;
    ui::Rect up { dpadX + seg, dpadY, seg, seg };
    ui::Rect down { dpadX + seg, dpadY + seg * 2, seg, seg };
    ui::Rect left { dpadX, dpadY + seg, seg, seg };
    ui::Rect right { dpadX + seg * 2, dpadY + seg, seg, seg };
    for (auto &&[rect, bit, label] : { std::tuple { up, FINLINK_KEY_UP, "^" }, std::tuple { down, FINLINK_KEY_DOWN, "v" },
                                        std::tuple { left, FINLINK_KEY_LEFT, "<" }, std::tuple { right, FINLINK_KEY_RIGHT, ">" } }) {
        ui::drawRect(rect, touch.heldIn(rect) ? ui::kColorButtonHeld : ui::kColorButton);
        ui::drawText(textBuf, label, rect.x + seg / 2 - 4, rect.y + seg / 2 - 8, 0.5f, ui::kColorText);
        if (touch.heldIn(rect)) mask |= bit;
    }

    float clusterX = 210, clusterY = 90;
    const float actionSize = 52;
    ui::Rect bBtn { clusterX, clusterY + actionSize * 0.6f, actionSize, actionSize };
    ui::Rect aBtn { clusterX + actionSize, clusterY, actionSize, actionSize };
    ui::drawRect(bBtn, touch.heldIn(bBtn) ? ui::kColorButtonHeld : ui::kColorButton);
    ui::drawText(textBuf, "B", bBtn.x + actionSize / 2 - 4, bBtn.y + actionSize / 2 - 8, 0.5f, ui::kColorText);
    if (touch.heldIn(bBtn)) mask |= FINLINK_KEY_B;
    ui::drawRect(aBtn, touch.heldIn(aBtn) ? ui::kColorButtonHeld : ui::kColorButton);
    ui::drawText(textBuf, "A", aBtn.x + actionSize / 2 - 4, aBtn.y + actionSize / 2 - 8, 0.5f, ui::kColorText);
    if (touch.heldIn(aBtn)) mask |= FINLINK_KEY_A;

    return mask;
}

} // namespace

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    srand(static_cast<unsigned>(svcGetSystemTick()));

    gfxInitDefault();
    gfxSet3D(false);

    u32 *socBuf = static_cast<u32 *>(memalign(0x1000, kSocBufferSize));
    if (socBuf) {
        socInit(socBuf, kSocBufferSize);
    }

    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();

    C3D_RenderTarget *topTarget = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottomTarget = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    C2D_TextBuf textBuf = C2D_TextBufNew(4096);

    Prefs prefs;
    MenuState menu;
    GbaSession session;
    VideoTex videoTex;
    AudioPlayer audio;
    videoTex.setBilinearFilter(prefs.bilinearVideoFilter);

    BottomScreenState screenState = BottomScreenState::MENU;
    // Written from the session's background thread (onConnected/
    // onDisconnected callbacks) and read every frame on the main thread.
    std::atomic<bool> connected { false };
    std::string connectedHost;
    uint16_t physicalMask = 0;
    ui::Touch touch;

    while (aptMainLoop()) {
        hidScanInput();
        touchPosition rawTouch;
        hidTouchRead(&rawTouch);
        touch.wasDown = touch.down;
        touch.down = (hidKeysHeld() & KEY_TOUCH) != 0;
        touch.x = rawTouch.px;
        touch.y = rawTouch.py;

        if (connected) {
            u32 held = hidKeysHeld();
            uint16_t newMask = 0;
            for (const auto &b : GBA_BUTTONS) {
                if (held & b.key) {
                    newMask |= b.bit;
                }
            }
            physicalMask = newMask;
        }

        videoTex.upload();
        C2D_TextBufClear(textBuf);

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);

        C2D_TargetClear(topTarget, ui::kColorBg);
        C2D_SceneBegin(topTarget);
        if (connected && videoTex.hasFrame()) {
            videoTex.drawFitted(0, 0, 400, 240);
        } else {
            ui::drawText(textBuf, "finlink", 150, 100, 0.9f, ui::kColorText);
            if (connected) {
                ui::drawText(textBuf, "Warte auf Bild...", 130, 140, 0.5f, ui::kColorTextDim);
            }
        }

        C2D_TargetClear(bottomTarget, ui::kColorBg);
        C2D_SceneBegin(bottomTarget);

        uint16_t touchMask = 0;
        if (connected) {
            if (prefs.onScreenControlsEnabled) {
                touchMask = drawPlayerControls(textBuf, touch);
            } else {
                ui::drawText(textBuf, "Verbunden - physische Tasten aktiv", 8, 100, 0.45f, ui::kColorTextDim);
            }
            ui::Rect disconnectRect { 8, 206, 304, 26 };
            if (ui::button(textBuf, touch, disconnectRect, "Trennen")) {
                session.disconnect();
                connected = false;
            }
            session.sendInput(touchMask | physicalMask);
        } else if (screenState == BottomScreenState::MENU) {
            drawMenuScreen(textBuf, touch, &menu, &screenState, &session, &videoTex, &audio, &connected,
                           &connectedHost);
        } else {
            drawSettingsScreen(textBuf, touch, &prefs, &videoTex, &screenState);
        }

        C3D_FrameEnd(0);
    }

    session.disconnect();
    C2D_TextBufDelete(textBuf);
    C2D_Fini();
    C3D_Fini();
    if (socBuf) {
        socExit();
        free(socBuf);
    }
    gfxExit();
    return 0;
}
