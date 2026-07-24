#pragma once

#include <optional>
#include <string>
#include <vector>

// LAN discovery + lobby status polling, mirroring
// clients/switch/source/discovery.hpp. Both functions are blocking and
// meant to be called from a background thread.
namespace discovery {

// Host addresses on this console's local IPv4 /24 (see discovery.cpp for
// why /24 specifically -- the 3DS's soc service doesn't expose a subnet
// mask query the way Switch's nifm does).
std::vector<std::string> localSubnetHosts();

// This console's own IPv4 address as a dotted-quad string, or empty if
// gethostid() couldn't report one (no Wi-Fi connection, soc not
// initialized, ...). Meant as an on-screen diagnostic: if this comes back
// empty, 0.0.0.0 or a 127.x loopback address, no connect attempt to
// anything is going to work, which looks identical from the UI's side to
// "the streaming host has no free slot" -- this makes the difference
// visible.
std::string localIpString();

// GET / on port 6800 (the lobby port). true if something answered with
// HTTP 200.
bool probeLobby(const std::string &ip, int timeoutMs = 400);

// GET /status on host:port (one of the four player ports, 6801-6804).
// nullopt = unreachable; otherwise the "occupied" field from the JSON body.
std::optional<bool> fetchOccupied(const std::string &host, int port, int timeoutMs = 1500);

} // namespace discovery
