#include "../src/core.hpp"
#include "../src/key_output.hpp"
#include "../src/localization.hpp"
#include "../src/key_capture.hpp"
#include "../src/toggle_hotkey.hpp"
#include "../src/profile_hotkeys.hpp"
#include <iostream>
#include <sstream>
#include <utility>

void require(bool value) { if (!value) throw std::runtime_error("Test failed"); }
int main() {
    using namespace mywarkey;
    // The reserved physical chord toggles once per press, even if modifiers
    // are released before F12. Plain/repeated F12 must not start the chord.
    ToggleHotkey shortcut;
    require(!shortcut.event(0x7B, true, false, true, false).swallow);
    require(!shortcut.event(0x7B, true, true, true, true).toggle);
    require(!shortcut.event(0x7B, false, true, true, true).swallow);
    bool active = true;
    auto press = shortcut.event(0x7B, true, false, true, true);
    require(press.swallow && press.toggle); active = !active; require(!active);
    for (int i = 0; i < 20; ++i) {
        auto repeat = shortcut.event(0x7B, true, true, true, true);
        require(repeat.swallow && !repeat.toggle);
    }
    require(!shortcut.event('A', true, false, true, true).swallow);
    auto release = shortcut.event(0x7B, false, true, false, false);
    require(release.swallow && !release.toggle);
    press = shortcut.event(0x7B, true, false, true, true);
    require(press.toggle); active = !active; require(active);
    require(shortcut.event(0x7B, false, true, true, true).swallow);
    require(!shortcut.event(0x7B, true, false, false, false).swallow);
    ProfileHotkeys profileKeys;
    for (int i = 0; i < 10; ++i) {
        int digit = i == 9 ? '0' : '1' + i;
        require(profileDigit(digit) == i);
        auto selected = profileKeys.event(digit, true, false, true, false, false, 10);
        require(selected.swallow && selected.index == i);
        require(profileKeys.event(digit, true, true, true, false, false, 10).index == -1);
        // Consume key-up even after modifiers are released or profiles deleted.
        require(profileKeys.event(digit, false, true, false, false, false, 0).swallow);
        require(!profileKeys.event(digit, true, false, true, true, false, 10).swallow); // AltGr.
        require(!profileKeys.event(digit, true, false, true, false, true, 10).swallow);
        require(!profileKeys.event(digit, true, false, false, false, false, 10).swallow);
    }
    require(!profileKeys.event('2', true, false, true, false, false, 1).swallow);
    require(!profileKeys.event('1', true, true, true, false, false, 10).swallow);
    require(!profileKeys.event(0x61, true, false, true, false, false, 10).swallow); // Numpad 1.
    require(profileShortcut(0) == L"Alt + 1" && profileShortcut(9) == L"Alt + 0" && profileShortcut(10).empty());
    require(warcraftExecutable(L"War3.exe") && warcraftExecutable(L"Warcraft III.exe"));
    require(warcraftExecutable(L"WARCRAFT III REFORGED.EXE"));
    require(!warcraftExecutable(L"notepad.exe") && !warcraftExecutable(L"War3.exe.bak"));
    require(warcraftWindowClass(L"Warcraft III") && warcraftWindowClass(L"WarcraftIII"));
    require(warcraftWindowClass(L"WARCRAFT III"));
    require(!warcraftWindowClass(L"Notepad") && !warcraftWindowClass(L"OsWindow"));
    require(!warcraftWindowClass(L"Qt5QWindowIcon") && !warcraftWindowClass(L"Warcraft III Launcher"));
    require(configurationMessage(L"测试 日本語 한국어") == L"Configuration 测试 日本語 한국어 enabled.");
    require(configurationMessage(L"a\n|b") == L"Configuration a  b enabled.");
    require(toggleMessage(true) == L"magicKey enabled." && toggleMessage(false) == L"magicKey disabled.");
    Settings settings;
    require(settings.profiles[0].mappings == std::map<int, int>{{0x67, 'Q'}, {0x68, 'W'}, {0x65, 'R'}});
    require(settings.scanCodes && settings.language == 0);
    settings.language = 3;
    settings.scanCodes = false;
    settings.profiles.push_back({"Game \"two\" \\ backup", true, {{0x5B, 0}, {'X', 'Q'}}});
    settings.selected = 1;
    std::stringstream file;
    writeSettings(file, settings);
    auto loaded = readSettings(file);
    require(loaded.selected == 1 && loaded.profiles.size() == 2);
    require(loaded.profiles[1].name == settings.profiles[1].name);
    require(loaded.profiles[1].blockWindows && loaded.profiles[0].mappings.at(0x68) == 'W');
    require(loaded.language == 3 && !loaded.scanCodes);
    // New profiles get independent forward mappings, without changing the
    // existing Default profile or copying edits from the selected profile.
    const std::map<int, int> newDefaults{{'Q', 0x67}, {'W', 0x68}, {'R', 0x65}};
    Settings created;
    const auto existingMappings = created.profiles[0].mappings;
    created.profiles.push_back(newProfile("First game"));
    require(created.profiles.back().mappings == newDefaults && !created.profiles.back().blockWindows);
    created.profiles.back().mappings['Q'] = 'T';
    created.profiles.push_back(newProfile("Second game"));
    require(created.profiles.back().name == "Second game" && created.profiles.back().mappings == newDefaults);
    require(created.profiles[0].mappings == existingMappings && created.profiles[1].mappings.at('Q') == 'T');
    created.profiles.push_back({"Existing empty profile", false, {}});
    std::stringstream createdFile;
    writeSettings(createdFile, created);
    const auto restored = readSettings(createdFile);
    require(restored.profiles[2].mappings == newDefaults && restored.profiles[3].mappings.empty());
    std::istringstream legacy("MYWARKEY 1 0 2 \"Default\" 0 2 65 81 87 103 \"Custom\" 1 1 65 90");
    auto upgraded = readSettings(legacy);
    require(upgraded.profiles[0].mappings == Settings{}.profiles[0].mappings);
    require(upgraded.profiles[1].mappings.at('A') == 'Z' && upgraded.profiles[1].blockWindows);
    require(upgraded.language == 0 && upgraded.scanCodes);
    std::istringstream oldTopRow("MYWARKEY 2 0 2 2 0 \"Default\" 0 2 55 81 56 87 \"Custom\" 0 2 55 81 56 87");
    auto upgradedTopRow = readSettings(oldTopRow);
    require(upgradedTopRow.profiles[0].mappings == defaultMappings());
    require(upgradedTopRow.profiles[1].mappings == std::map<int, int>{{'7', 'Q'}, {'8', 'W'}});
    require(upgradedTopRow.language == 2 && !upgradedTopRow.scanCodes);
    std::istringstream modifiedDefault("MYWARKEY 2 0 1 0 1 \"Default\" 0 3 55 81 56 87 65 90");
    require(readSettings(modifiedDefault).profiles[0].mappings.at('A') == 'Z');
    std::istringstream customized("MYWARKEY 1 0 1 \"Default\" 0 1 65 90");
    require(readSettings(customized).profiles[0].mappings.at('A') == 'Z');
    for (const auto* text : {"", "MYWARKEY 3 0 1", "MYWARKEY 1 5 1 \"a\" 0 0",
             "MYWARKEY 2 0 1 4 1 \"a\" 0 0", "MYWARKEY 2 0 1 0 2 \"a\" 0 0",
             "MYWARKEY 2 0 1", "MYWARKEY 2 0 1 -1 0 \"a\" 0 0",
             "MYWARKEY 1 0 1 \"a\" 0 1 65 999", "MYWARKEY 1 0 1 \"a\" 0 2 65 66 65 67",
             "MYWARKEY 1 0 1 \"a\" 0 0 garbage", "MYWARKEY 1 0 2 \"a\" 0 0 \"a\" 0 0"}) {
        bool rejected = false;
        try { std::istringstream bad(text); (void)readSettings(bad); }
        catch (const std::runtime_error&) { rejected = true; }
        require(rejected);
    }
    Remapper engine;
    Profile p{"Test", true, {{'A', 'Q'}, {'W', 0x67}, {'B', 'Q'}, {'Z', 0}}};
    std::vector<std::pair<int, bool>> events;
    auto send = [&](int key, bool down) { events.emplace_back(key, down); };
    Remapper newProfileEngine;
    const auto fresh = newProfile("Game");
    for (auto [source, target] : newDefaults) {
        require(newProfileEngine.event(source, true, fresh, true, send));
        require(events.back() == std::make_pair(target, true));
        require(newProfileEngine.event(source, false, fresh, true, send));
        require(events.back() == std::make_pair(target, false));
    }
    for (int unchanged : std::array<int, 6>{'7', '8', '5', 0x67, 0x68, 0x65})
        require(!newProfileEngine.event(unchanged, true, fresh, true, send));
    events.clear();
    Remapper defaultsEngine;
    const auto& defaults = settings.profiles[0];
    for (int digit : {'7', '8', '5'})
        require(!defaultsEngine.event(digit, true, defaults, true, send));
    require(events.empty());
    for (auto [source, target] : defaultMappings()) {
        require(defaultsEngine.event(source, true, defaults, true, send));
        require(events.back() == std::make_pair(target, true));
        require(defaultsEngine.event(source, false, defaults, true, send));
        require(events.back() == std::make_pair(target, false));
    }
    events.clear();
    require(engine.event('A', true, p, true, send));
    require(engine.event('A', true, p, true, send)); // repeat
    require(engine.event('B', true, p, true, send)); // same destination
    require(engine.event('A', false, p, true, send));
    require(events.size() == 2);
    engine.event('B', false, p, true, send);
    require(events.back() == std::make_pair(int('Q'), false));
    engine.event('W', true, p, true, send);
    require(events.back() == std::make_pair(0x67, true));
    engine.release(send);
    require(events.back() == std::make_pair(0x67, false));
    auto size = events.size();
    require(engine.event('W', true, p, false, send));
    require(engine.event('W', false, p, false, send));
    require(events.size() == size);
    require(!engine.event('A', true, p, false, send));
    require(!engine.event('C', true, p, true, send));
    require(engine.event(0x5B, true, p, true, send));
    require(engine.event(0x5B, false, p, true, send));
    require(engine.event('Z', true, p, true, send));
    engine.event('Z', false, p, true, send);
    require(events.size() == size);
    // Destination remains paired even if the caller changes the profile before key-up.
    engine.event('A', true, p, true, send);
    p.mappings['A'] = 'R';
    engine.event('A', false, p, true, send);
    require(events.back() == std::make_pair(int('Q'), false));
    // Game mode sends a physical scan code and clears wVk. Both halves use scan mode.
    auto qDown = keyOutput('Q', true, 0x10, false, true);
    auto qUp = keyOutput('Q', false, 0x10, false, true);
    require(qDown.vk == 0 && qDown.scan == 0x10 && qDown.flags == 0x8);
    require(qUp.vk == 0 && qUp.scan == 0x10 && qUp.flags == 0xA);
    auto rightCtrl = keyOutput(0xA3, false, 0xE01D, true, true);
    require(rightCtrl.scan == 0x1D && rightCtrl.flags == 0xB);
    auto numpad = keyOutput(0x67, true, 0x47, false, true);
    require(numpad.scan == 0x47 && numpad.flags == 0x8);
    auto vkMode = keyOutput('Q', true, 0x10, false, false);
    require(vkMode.vk == 'Q' && vkMode.flags == 0);
    auto fallback = keyOutput('Q', true, 0, false, true);
    require(fallback.vk == 'Q' && fallback.flags == 0);
    // Every top-row digit remains distinct from the corresponding physical numpad
    // key, including the navigation VKs Windows uses with Num Lock off.
    const unsigned padScans[] = {0x52, 0x4F, 0x50, 0x51, 0x4B, 0x4C, 0x4D, 0x47, 0x48, 0x49};
    const int navigation[] = {0x2D, 0x23, 0x28, 0x22, 0x25, 0x0C, 0x27, 0x24, 0x26, 0x21};
    for (int digit = 0; digit < 10; ++digit) {
        require(physicalKey('0' + digit, digit ? digit + 1 : 0x0B, false) == '0' + digit);
        require(physicalKey(0x60 + digit, padScans[digit], false) == 0x60 + digit);
        require(physicalKey(navigation[digit], padScans[digit], false) == 0x60 + digit);
        require(physicalKey(navigation[digit], padScans[digit], true) == navigation[digit]);
    }
    require(physicalKey(0x2E, 0x53, false) == 0x6E); // Numpad decimal, Num Lock off
    require(physicalKey(0x2E, 0x53, true) == 0x2E); // Dedicated Delete
    require(physicalKey(0x6F, 0x35, true) == 0x6F); // Numpad divide
    KeyCapture capture;
    require(!capture.event('7', true, false).swallow); // Idle: no interception.
    capture.arm(CaptureSlot::Source);
    require(capture.event('7', true, false).swallow);
    require(capture.event('7', true, true).slot == CaptureSlot::None); // Repeat cannot finish capture.
    auto captured = capture.event('7', false, true);
    require(captured.swallow && captured.key == '7' && captured.slot == CaptureSlot::Source);
    require(capture.activeSlot() == CaptureSlot::None);
    capture.arm(CaptureSlot::Target);
    capture.event(physicalKey(0x24, 0x47, false), true, false);
    captured = capture.event(physicalKey(0x24, 0x47, false), false, true);
    require(captured.swallow && captured.key == 0x67 && captured.slot == CaptureSlot::Target);
    // Cancel while a key is held: eat repeats and key-up, but never update a choice.
    capture.arm(CaptureSlot::Source);
    capture.event(0x0D, true, false);
    capture.cancel();
    require(capture.event(0x0D, true, true).swallow);
    captured = capture.event(0x0D, false, true);
    require(captured.swallow && captured.slot == CaptureSlot::None);
    require(!capture.event(0x0D, true, false).swallow);
    // Keys already down at arming time aren't candidates; their release must pass.
    capture.arm(CaptureSlot::Source);
    require(capture.event(0xA0, true, true).swallow);
    require(!capture.event(0xA0, false, true).swallow);
    require(capture.activeSlot() == CaptureSlot::Source);
    // Extra keys during capture cannot navigate the UI or leak orphaned releases.
    capture.event('Q', true, false);
    capture.event(0x09, true, false);
    require(capture.event('Q', false, true).key == 'Q');
    captured = capture.event(0x09, false, true);
    require(captured.swallow && captured.slot == CaptureSlot::None);
    for (int special : {0x1B, 0x0D, 0x09, 0x5B, 0xA3}) {
        capture.arm(CaptureSlot::Target);
        require(capture.event(special, true, false).swallow);
        captured = capture.event(special, false, true);
        require(captured.swallow && captured.key == special && captured.slot == CaptureSlot::Target);
    }
    // Both numeric source identities survive saving and can produce different outputs.
    Settings numeric;
    numeric.profiles[0].mappings = {{'7', 'Q'}, {0x67, 'W'}};
    std::stringstream numericFile;
    writeSettings(numericFile, numeric);
    auto numericProfile = readSettings(numericFile).profiles[0];
    require(numericProfile.mappings.at('7') == 'Q' && numericProfile.mappings.at(0x67) == 'W');
    // Every catalog entry is complete and resolves for all four languages.
    std::map<std::wstring_view, bool> unique;
    for (const auto& entry : translations) {
        require(unique.emplace(entry.en, true).second);
        require(*entry.en && *entry.zh && *entry.ja && *entry.ko);
        require(std::wstring_view(translate(entry.en, 0)) == entry.en);
        require(std::wstring_view(translate(entry.en, 1)) == entry.zh);
        require(std::wstring_view(translate(entry.en, 2)) == entry.ja);
        require(std::wstring_view(translate(entry.en, 3)) == entry.ko);
    }
    require(std::wstring_view(translate(L"User profile", 2)) == L"User profile");
    std::cout << "Profile migration, localization, key capture, numeric keypad, output and remapping tests passed.\n";
}
