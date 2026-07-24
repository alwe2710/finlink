#pragma once

#include <optional>
#include <string>
#include <vector>

// LAN discovery + lobby status polling, mirroring
// clients/android/.../MenuActivity.kt's two ways of finding a host. Both
// functions are blocking and meant to be called from a background thread,
// same as the Android client calls them off the UI thread.
namespace discovery {

// Host addresses (excluding network/broadcast) on this console's local
// IPv4 subnet, or empty if the subnet is implausibly large (a misdetected
// huge mask would otherwise mean scanning millions of hosts) or nifm
// couldn't report one.
std::vector<std::string> localSubnetHosts();

// GET / on port 6800 (the lobby port -- Dolphin doesn't advertise itself
// over mDNS/UPnP/SSDP, so this is a plain sweep, not a real discovery
// protocol). true if something answered with HTTP 200.
bool probeLobby(const std::string &ip, int timeoutMs = 400);

// GET /status on host:port (one of the four player ports, 6801-6804).
// nullopt = unreachable; otherwise the "occupied" field from the JSON body.
std::optional<bool> fetchOccupied(const std::string &host, int port, int timeoutMs = 1500);

} // namespace discovery
