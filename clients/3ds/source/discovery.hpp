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

// GET / on port 6800 (the lobby port). true if something answered with
// HTTP 200.
bool probeLobby(const std::string &ip, int timeoutMs = 400);

// GET /status on host:port (one of the four player ports, 6801-6804).
// nullopt = unreachable; otherwise the "occupied" field from the JSON body.
std::optional<bool> fetchOccupied(const std::string &host, int port, int timeoutMs = 1500);

} // namespace discovery
