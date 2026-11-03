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
#include <cstdint>
#include "combination.hpp"

namespace mywarkey {
struct Profile {
    std::string name;
    bool blockWindows = false;
    std::map<int, int> mappings; // 0 means disable the source key.
    std::vector<Combination> combinations;
};
inline std::map<int, int> defaultMappings() {
    return {{'Q',0x67}, {'W',0x68}, {'R',0x64}, {'T',0x65}};
}
inline Profile newProfile(std::string name) {
    return {std::move(name), false, defaultMappings()};
}
struct Settings {
    std::size_t selected = 0;
    int quickChatSlot = 0;
    int language = 0; // English, Simplified Chinese, Japanese, Korean.
    bool scanCodes = true;
    bool blockWindows = false;
    std::array<std::string, 10> quickChat{};
    std::vector<Profile> profiles{{"Default", false, defaultMappings()}};
};

inline Settings readSettings(std::istream& in) {
    Settings result;
    std::string magic;
    int version;
    std::size_t count;
    if (!(in >> magic >> version >> result.selected >> count) || magic != "MYWARKEY" ||
        (version < 1 || version > 5) || count == 0 || count > 100)
        throw std::runtime_error("Invalid profile file header.");
    if (version >= 2) {
        int scanCodes;
        if (!(in >> result.language >> scanCodes) || result.language < 0 || result.language > 3 ||
            (scanCodes != 0 && scanCodes != 1))
            throw std::runtime_error("Invalid application preferences.");
        result.scanCodes = scanCodes != 0;
    }
    if (version >= 3) {
        int blocked;
        if (!(in >> blocked) || (blocked != 0 && blocked != 1)) throw std::runtime_error("Invalid global Windows-key setting.");
        result.blockWindows = blocked != 0;
    }
    if (version >= 4 && (!(in >> result.quickChatSlot) || result.quickChatSlot < 0 || result.quickChatSlot > 9))
        throw std::runtime_error("Invalid Quick Chat slot.");
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
        if (version >= 4) {
            int count;
            if (!(in >> count) || count < 0 || count > 100) throw std::runtime_error("Invalid combination count.");
            for (int n = 0; n < count; ++n) {
                int length; Combination c;
                if (version >= 5) { int mode; if (!(in >> mode) || mode < 0 || mode > 2) throw std::runtime_error("Invalid combination mode."); c.mode = CombinationMode(mode); }
                if (!(in >> length) || length < 2 || length > 3) throw std::runtime_error("Invalid combination size.");
                for (int k = 0, key; k < length; ++k) { if (!(in >> key)) throw std::runtime_error("Invalid combination key."); c.keys.push_back(key); }
                c.keys = combinationKeys(std::move(c.keys),c.mode);
                if (!(in >> c.target) || c.target < 0 || c.target > 254 || (c.mode != CombinationMode::InputChord && c.target == 0)) throw std::runtime_error("Invalid combination target.");
                for (const auto& old : p.combinations) if ((c.mode == CombinationMode::InputChord && old.mode == c.mode && old.keys == c.keys) || (c.mode != CombinationMode::InputChord && old.mode != CombinationMode::InputChord && old.target == c.target)) throw std::runtime_error("Duplicate combination.");
                p.combinations.push_back(std::move(c));
            }
        }
        result.profiles.push_back(std::move(p));
    }
    if (version >= 3) {
        std::string marker; int slots;
        if (!(in >> marker >> slots) || marker != "QUICKCHAT" || slots != 10) throw std::runtime_error("Invalid quick chat settings.");
        for (auto& message : result.quickChat) {
            if (!(in >> std::quoted(message)) || message.size() > 512) throw std::runtime_error("Invalid quick chat message.");
            for (unsigned char c : message) if (c < 32 || c == 127) throw std::runtime_error("Quick chat must be a single line.");
        }
    }
    in >> std::ws;
    if (!in.eof() || result.selected >= result.profiles.size())
        throw std::runtime_error("Invalid profile file contents.");
    if (version < 3) result.blockWindows = result.profiles[result.selected].blockWindows;
    for (auto& p : result.profiles)
        if (p.name == "Default" && !p.blockWindows &&
            ((version == 1 && p.mappings == std::map<int, int>{{'A', 'Q'}, {'W', 0x67}}) ||
             p.mappings == std::map<int, int>{{'7', 'Q'}, {'8', 'W'}}))
            p.mappings = defaultMappings();
    return result;
}

inline void writeSettings(std::ostream& out, const Settings& settings) {
    out << "MYWARKEY 5 " << settings.selected << ' ' << settings.profiles.size() << ' '
        << settings.language << ' ' << settings.scanCodes << ' ' << settings.blockWindows << ' ' << settings.quickChatSlot << '\n';
    for (const auto& p : settings.profiles) {
        out << std::quoted(p.name) << ' ' << p.blockWindows << ' ' << p.mappings.size() << '\n';
        for (auto [from, to] : p.mappings) out << from << ' ' << to << '\n';
        out << p.combinations.size() << '\n';
        for (const auto& c : p.combinations) {
            out << int(c.mode) << ' ' << c.keys.size(); for (int key : c.keys) out << ' ' << key;
            out << ' ' << c.target << '\n';
        }
    }
    out << "QUICKCHAT 10\n";
    for (const auto& message : settings.quickChat) out << std::quoted(message) << '\n';
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
    std::vector<int> pending;
    std::vector<Combination> active;
    std::uint64_t deadline = 0, clockNow = 0;
    std::array<std::uint64_t,256> minimumRelease{};
    std::array<bool,256> delayedRelease{};
    bool contains(const std::vector<int>& keys, int key) const { return std::find(keys.begin(),keys.end(),key) != keys.end(); }
    struct MacroRun { std::vector<int> keys; CombinationMode mode; std::size_t index = 0; bool down = true; std::uint64_t next = 0; };
    std::vector<MacroRun> macros;
    bool allowed(const Combination& c, const Profile& p) const { return c.mode == CombinationMode::InputChord && (!p.blockWindows || (!contains(c.keys,0x5B) && !contains(c.keys,0x5C))); }
    template<class Sink> void start(int target, Sink& send, bool buffered = false) {
        if (target && references[target]++ == 0) {
            if (!delayedRelease[target]) send(target,true);
            delayedRelease[target] = false;
            minimumRelease[target] = buffered ? clockNow + 50 : 0;
        }
    }
    template<class Sink> void stop(int target, Sink& send) {
        if (target && references[target] && --references[target] == 0) {
            if (clockNow < minimumRelease[target]) delayedRelease[target] = true;
            else send(target,false);
        }
    }
    template<class Sink> bool normal(int source, bool down, const Profile& p, bool enabled, Sink& send, bool force = false) {
        auto found = held.find(source);
        if (!down) {
            if (found == held.end()) return false;
            stop(found->second,send); held.erase(found); return true;
        }
        if (found != held.end()) { if (found->second) send(found->second,true); return true; }
        if (!enabled) return false;
        auto mapping = p.mappings.find(source);
        int target;
        if (p.blockWindows && (source == 0x5B || source == 0x5C)) target = 0;
        else if (mapping != p.mappings.end()) target = mapping->second;
        else if (force) target = source;
        else return false;
        held[source] = target; start(target,send,force); return true;
    }
    const Combination* complete(const Profile& p) const {
        const Combination* best = nullptr;
        for (const auto& c : p.combinations) if (allowed(c,p)) {
            bool full = true;
            for (int key : c.keys) if (!contains(pending,key)) full = false;
            if (full && (!best || c.keys.size() > best->keys.size())) best = &c;
        }
        return best;
    }
    bool extendable(const Profile& p, const std::vector<int>& keys) const {
        for (const auto& c : p.combinations) if (allowed(c,p)) {
            bool all = true; for (int key : keys) if (!contains(c.keys,key)) all = false;
            if (all) return true;
        }
        return false;
    }
    template<class Sink> void resolve(const Profile& p, Sink& send) {
        auto chosen = complete(p);
        if (chosen) {
            active.push_back(*chosen); start(chosen->target,send,true);
            for (int key : chosen->keys) held[key] = 0;
            for (int key : pending) if (!contains(chosen->keys,key)) normal(key,true,p,true,send,true);
        } else for (int key : pending) normal(key,true,p,true,send,true);
        pending.clear();
    }
public:
    static constexpr std::uint64_t CombinationWindowMs = 150;
    template<class Sink> void tick(std::uint64_t now, const Profile& p, bool enabled, Sink&& send) {
        clockNow = now;
        if (!enabled && !macros.empty()) release(send);
        if (!macros.empty() && now >= macros[0].next) {
            auto& run = macros[0];
            if (run.mode == CombinationMode::Together) {
                for (auto key = run.keys.rbegin(); key != run.keys.rend(); ++key) stop(*key,send);
                macros.clear();
            } else if (run.down) {
                stop(run.keys[run.index],send); run.down = false; run.next = now + 20;
                if (++run.index == run.keys.size()) macros.clear();
            } else { start(run.keys[run.index],send,true); run.down = true; run.next = now + 60; }
        }
        for (int key = 1; key < 256; ++key) if (delayedRelease[key] && !references[key] && now >= minimumRelease[key]) {
            delayedRelease[key] = false; send(key,false);
        }
        if (enabled && !pending.empty() && now >= deadline) resolve(p,send);
    }
    template<class Sink> bool event(int source, bool down, const Profile& p, bool enabled, Sink&& send, std::uint64_t now = 0) {
        if (!enabled && !pending.empty()) release(send);
        tick(now,p,enabled,send);
        if (!down) {
            bool owned = contains(pending,source);
            if (owned) resolve(p,send);
            for (auto i = active.begin(); i != active.end();) {
                if (contains(i->keys,source)) { stop(i->target,send); i = active.erase(i); owned = true; }
                else ++i;
            }
            return normal(source,false,p,enabled,send) || owned;
        }
        if (held.count(source)) return normal(source,true,p,enabled,send);
        if (contains(pending,source)) return true;
        if (!enabled) return false;
        if (!(p.blockWindows && (source == 0x5B || source == 0x5C))) {
            for (const auto& c : p.combinations) if (c.mode != CombinationMode::InputChord && c.target == source) {
                if (!pending.empty()) resolve(p,send);
                held[source] = 0;
                if (macros.empty()) {
                    auto outputs = c.keys;
                    if (c.mode == CombinationMode::Together) std::stable_partition(outputs.begin(),outputs.end(),[](int key) {
                        return key == 0x10 || key == 0x11 || key == 0x12 || (key >= 0xA0 && key <= 0xA5) || key == 0x5B || key == 0x5C;
                    });
                    macros.push_back({outputs,c.mode,0,true,now+60});
                    if (c.mode == CombinationMode::Together) for (int key : outputs) start(key,send,true);
                    else start(c.keys[0],send,true);
                }
                return true;
            }
        }
        std::vector<int> candidate = pending; candidate.push_back(source);
        if (!pending.empty() && !extendable(p,candidate)) { resolve(p,send); candidate = {source}; }
        if (extendable(p,candidate)) {
            if (pending.empty()) deadline = now + CombinationWindowMs;
            pending.push_back(source);
            if (auto chosen = complete(p)) {
                bool longer = false;
                for (const auto& c : p.combinations) if (allowed(c,p) && c.keys.size() > chosen->keys.size()) {
                    bool superset = true; for (int key : pending) if (!contains(c.keys,key)) superset = false;
                    if (superset) longer = true;
                }
                if (!longer) resolve(p,send);
            }
            return true;
        }
        return normal(source,true,p,true,send);
    }
    template<class Sink> void release(Sink&& send) {
        for (int key = 1; key < 256; ++key) if (references[key] || delayedRelease[key]) send(key,false);
        references.fill(0); delayedRelease.fill(false); minimumRelease.fill(0);
        for (auto& entry : held) entry.second = 0;
        for (int key : pending) held[key] = 0;
        pending.clear(); active.clear(); macros.clear();
    }
};
} // namespace mywarkey
