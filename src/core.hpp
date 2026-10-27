#pragma once
#include <array>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mywarkey {
struct Profile {
    std::string name;
    bool blockWindows = false;
    std::map<int, int> mappings; // 0 means disable the source key.
};
inline std::map<int, int> defaultMappings() {
    return {{0x67, 'Q'}, {0x68, 'W'}, {0x65, 'R'}}; // Numpad 7, 8, 5.
}
inline Profile newProfile(std::string name) {
    return {std::move(name), false, {{'Q', 0x67}, {'W', 0x68}, {'R', 0x65}}};
}
struct Settings {
    std::size_t selected = 0;
    int language = 0; // English, Simplified Chinese, Japanese, Korean.
    bool scanCodes = true;
    std::vector<Profile> profiles{{"Default", false, defaultMappings()}};
};

inline Settings readSettings(std::istream& in) {
    Settings result;
    std::string magic;
    int version;
    std::size_t count;
    if (!(in >> magic >> version >> result.selected >> count) || magic != "MYWARKEY" ||
        (version != 1 && version != 2) || count == 0 || count > 100)
        throw std::runtime_error("Invalid profile file header.");
    if (version == 2) {
        int scanCodes;
        if (!(in >> result.language >> scanCodes) || result.language < 0 || result.language > 3 ||
            (scanCodes != 0 && scanCodes != 1))
            throw std::runtime_error("Invalid application preferences.");
        result.scanCodes = scanCodes != 0;
    }
    result.profiles.clear();
    for (std::size_t i = 0; i < count; ++i) {
        Profile p;
        int blocked;
        std::size_t mappingCount;
        if (!(in >> std::quoted(p.name) >> blocked >> mappingCount) ||
            p.name.empty() || p.name.size() > 256 || (blocked != 0 && blocked != 1) || mappingCount > 254)
            throw std::runtime_error("Invalid profile.");
        for (unsigned char c : p.name)
            if (c < 32) throw std::runtime_error("Invalid profile name.");
        for (const auto& existing : result.profiles)
            if (existing.name == p.name) throw std::runtime_error("Duplicate profile name.");
        p.blockWindows = blocked != 0;
        for (std::size_t j = 0; j < mappingCount; ++j) {
            int from, to;
            if (!(in >> from >> to) || from < 1 || from > 254 || to < 0 || to > 254 ||
                !p.mappings.emplace(from, to).second)
                throw std::runtime_error("Invalid key mapping.");
        }
        result.profiles.push_back(std::move(p));
    }
    in >> std::ws;
    if (!in.eof() || result.selected >= result.profiles.size())
        throw std::runtime_error("Invalid profile file contents.");
    for (auto& p : result.profiles)
        if (p.name == "Default" && !p.blockWindows &&
            ((version == 1 && p.mappings == std::map<int, int>{{'A', 'Q'}, {'W', 0x67}}) ||
             p.mappings == std::map<int, int>{{'7', 'Q'}, {'8', 'W'}}))
            p.mappings = defaultMappings();
    return result;
}

inline void writeSettings(std::ostream& out, const Settings& settings) {
    out << "MYWARKEY 2 " << settings.selected << ' ' << settings.profiles.size() << ' '
        << settings.language << ' ' << settings.scanCodes << '\n';
    for (const auto& p : settings.profiles) {
        out << std::quoted(p.name) << ' ' << p.blockWindows << ' ' << p.mappings.size() << '\n';
        for (auto [from, to] : p.mappings) out << from << ' ' << to << '\n';
    }
    if (!out) throw std::runtime_error("Could not write profiles.");
}

// File replacement is platform-specific; the UI replaces the temporary file atomically.
inline void writeTemporary(const std::filesystem::path& path, const Settings& settings) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot save profiles beside the executable. Move MagicKey to a writable folder.");
    writeSettings(out, settings);
    out.close();
    if (!out) throw std::runtime_error("Could not finish saving profiles.");
}

// The sink receives target virtual-key codes and true for key-down, false for key-up.
// Remember each held source's original destination; repeated downs don't add references.
class Remapper {
    std::map<int, int> held;
    std::array<unsigned, 256> references{};
public:
    template<class Sink>
    bool event(int source, bool down, const Profile& profile, bool enabled, Sink&& send) {
        auto active = held.find(source);
        if (!down) {
            if (active == held.end()) return false;
            const int target = active->second;
            held.erase(active);
            if (target && --references[target] == 0) send(target, false);
            return true;
        }
        if (active != held.end()) {
            if (active->second) send(active->second, true);
            return true;
        }
        if (!enabled) return false;
        auto mapping = profile.mappings.find(source);
        int target;
        if (profile.blockWindows && (source == 0x5B || source == 0x5C)) target = 0;
        else if (mapping != profile.mappings.end()) target = mapping->second;
        else return false;
        held[source] = target;
        if (target && references[target]++ == 0) send(target, true);
        return true;
    }
    template<class Sink>
    void release(Sink&& send) {
        for (int k = 1; k < 256; ++k)
            if (references[k]) send(k, false);
        references.fill(0);
        // Continue swallowing held source keys until their physical release, including repeats.
        for (auto& entry : held) entry.second = 0;
    }
};
} // namespace mywarkey
