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

    if (!brls::Application::init()) {
        brls::Logger::error("Unable to init Borealis application");
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

    socketExit();
    return EXIT_SUCCESS;
}
