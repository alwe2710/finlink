// finlink for Nintendo Switch: Menu -> Settings / Player, same structure
// as clients/android/. See clients/switch/README.md for the toolchain and
// architecture notes.

#include <borealis.hpp>
#include <switch.h>

#include "menu_activity.hpp"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    // Needed before any BSD socket use (GbaSession, discovery::probeLobby
    // etc.) -- libnx doesn't bring sockets up on its own.
    socketInitializeDefault();

    // Mounts the RomFS embedded into this .nro by elf2nro --romfsdir=
    // (see CMakeLists.txt) at "romfs:/" -- borealis's Switch font loader
    // reads resources/material/MaterialIcons-Regular.ttf from there during
    // Application::init(). Without this call romfs:/ paths just fail to
    // open; was missing from the initial version of this file.
    romfsInit();

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
        romfsExit();
        socketExit();
        return EXIT_FAILURE;
    }

    brls::Application::createWindow("finlink");

    // Start (Plus) is a GBA button (see PlayerActivity), not a "quit app"
    // shortcut here -- the Android client has no such gesture either, and
    // Application::pushActivity() would otherwise bind Start to
    // Application::quit() on every single activity it pushes, including
    // the player.
    brls::Application::setGlobalQuit(false);

    brls::Application::pushActivity(new MenuActivity());

    while (brls::Application::mainLoop())
        ;

    romfsExit();
    socketExit();
    return EXIT_SUCCESS;
}
