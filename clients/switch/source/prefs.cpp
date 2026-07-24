#include "prefs.hpp"

#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace {
constexpr const char *kDir = "sdmc:/switch/finlink";
constexpr const char *kFile = "sdmc:/switch/finlink/settings.cfg";
} // namespace

std::string Prefs::path() const {
    return kFile;
}

Prefs::Prefs() {
    load();
}

void Prefs::load() {
    std::ifstream in(path());
    if (!in.is_open()) {
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        values[line.substr(0, eq)] = line.substr(eq + 1);
    }

    auto it = values.find("on_screen_controls");
    if (it != values.end()) {
        onScreenControlsEnabled = it->second == "1";
    }
    it = values.find("bilinear_video_filter");
    if (it != values.end()) {
        bilinearVideoFilter = it->second == "1";
    }
}

void Prefs::save() {
    mkdir(kDir, 0777);

    values["on_screen_controls"] = onScreenControlsEnabled ? "1" : "0";
    values["bilinear_video_filter"] = bilinearVideoFilter ? "1" : "0";

    std::ofstream out(path(), std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    for (const auto &[key, value] : values) {
        out << key << "=" << value << "\n";
    }
}

int Prefs::keyBinding(const std::string &prefKey) const {
    auto cleared = values.find("keybind_" + prefKey + "_cleared");
    if (cleared != values.end() && cleared->second == "1") {
        return kExplicitlyUnbound;
    }
    auto it = values.find("keybind_" + prefKey);
    if (it == values.end()) {
        return kNoOverride;
    }
    return std::stoi(it->second);
}

void Prefs::setKeyBinding(const std::string &prefKey, int controllerButton) {
    values["keybind_" + prefKey] = std::to_string(controllerButton);
    values.erase("keybind_" + prefKey + "_cleared");
    save();
}

void Prefs::clearKeyBinding(const std::string &prefKey) {
    values.erase("keybind_" + prefKey);
    values["keybind_" + prefKey + "_cleared"] = "1";
    save();
}

bool Prefs::hasExplicitBinding(const std::string &prefKey) const {
    return values.count("keybind_" + prefKey) > 0;
}
