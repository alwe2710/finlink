#include "discovery.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <switch.h>

namespace {

// Minimal blocking-with-timeout HTTP/1.0 GET. Returns the response body
// (headers stripped) on a 200, empty optional otherwise -- good enough for
// the two tiny JSON/empty responses this protocol's lobby endpoints return
// (docs/protocol.md), not a general-purpose HTTP client.
std::optional<std::string> httpGet(const std::string &host, int port, const std::string &path, int timeoutMs) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return std::nullopt;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        close(fd);
        return std::nullopt;
    }

    int rc = ::connect(fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        close(fd);
        return std::nullopt;
    }
    if (rc < 0) {
        struct pollfd pfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
        if (poll(&pfd, 1, timeoutMs) <= 0 || !(pfd.revents & POLLOUT)) {
            close(fd);
            return std::nullopt;
        }
        int err = 0;
        socklen_t errlen = sizeof(err);
        getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &errlen);
        if (err != 0) {
            close(fd);
            return std::nullopt;
        }
    }

    std::string request = "GET " + path + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: close\r\n\r\n";
    if (send(fd, request.data(), request.size(), 0) < 0) {
        close(fd);
        return std::nullopt;
    }

    std::string response;
    char chunk[1024];
    for (;;) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN, .revents = 0 };
        int pr = poll(&pfd, 1, timeoutMs);
        if (pr <= 0) {
            break;
        }
        ssize_t n = recv(fd, chunk, sizeof(chunk), 0);
        if (n <= 0) {
            break;
        }
        response.append(chunk, static_cast<size_t>(n));
        if (response.size() > 65536) { // guard against a runaway response
            break;
        }
    }
    close(fd);

    if (response.compare(0, 5, "HTTP/") != 0) {
        return std::nullopt;
    }
    size_t space = response.find(' ');
    if (space == std::string::npos || response.compare(space + 1, 3, "200") != 0) {
        return std::nullopt;
    }
    size_t headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        return response.substr(space); // status line only, no body -- fine for probeLobby
    }
    return response.substr(headerEnd + 4);
}

} // namespace

namespace discovery {

std::vector<std::string> localSubnetHosts() {
    std::vector<std::string> hosts;

    if (R_FAILED(nifmInitialize(NifmServiceType_User))) {
        return hosts;
    }

    u32 addr = 0, mask = 0, gateway = 0, dns1 = 0, dns2 = 0;
    Result rc = nifmGetCurrentIpConfigInfo(&addr, &mask, &gateway, &dns1, &dns2);
    nifmExit();
    if (R_FAILED(rc) || addr == 0 || mask == 0) {
        return hosts;
    }

    uint32_t ip = ntohl(addr);
    uint32_t netmask = ntohl(mask);
    uint32_t hostBits = 32 - __builtin_popcount(netmask);
    if (hostBits < 2 || hostBits > 10) { // exclude /31,/32 (no usable hosts) and anything bigger than a /22
        return hosts;
    }

    uint32_t network = ip & netmask;
    uint32_t maxHosts = (1u << hostBits) - 2; // exclude network + broadcast address
    hosts.reserve(maxHosts);
    for (uint32_t offset = 1; offset <= maxHosts; offset++) {
        uint32_t hostIp = htonl(network + offset);
        char buf[INET_ADDRSTRLEN];
        struct in_addr in;
        in.s_addr = hostIp;
        inet_ntop(AF_INET, &in, buf, sizeof(buf));
        hosts.emplace_back(buf);
    }
    return hosts;
}

bool probeLobby(const std::string &ip, int timeoutMs) {
    return httpGet(ip, 6800, "/", timeoutMs).has_value();
}

std::optional<bool> fetchOccupied(const std::string &host, int port, int timeoutMs) {
    auto body = httpGet(host, port, "/status", timeoutMs);
    if (!body) {
        return std::nullopt;
    }
    // Small enough response (docs/protocol.md: {"occupied": true|false}) that
    // a substring search is simpler and more robust here than pulling in a
    // JSON parser for one boolean field.
    if (body->find("\"occupied\":true") != std::string::npos ||
        body->find("\"occupied\": true") != std::string::npos) {
        return true;
    }
    return false;
}

} // namespace discovery
