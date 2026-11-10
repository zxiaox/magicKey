#include "../src/warcraft_mana.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

struct TestDirectory {
    wchar_t path[MAX_PATH]{};
    TestDirectory() {
        wchar_t root[MAX_PATH]{};
        DWORD length = GetTempPathW(MAX_PATH, root);
        require(length && length < MAX_PATH, "Could not locate the test temp directory");
        require(GetTempFileNameW(root, L"MKT", 0, path), "Could not reserve test directory");
        require(DeleteFileW(path) && CreateDirectoryW(path, nullptr), "Could not create test directory");
    }
    ~TestDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
}

int main() {
    try {
        using namespace magickey;
        TestDirectory temp;
        std::filesystem::path folder(temp.path);
        auto gameDll = folder / L"Game.dll";
        require(!warcraftVersion(gameDll), "A missing DLL should not report a version");
        wchar_t exe[MAX_PATH]{};
        DWORD exeLength = GetModuleFileNameW(nullptr, exe, MAX_PATH);
        require(exeLength && exeLength < MAX_PATH, "Could not locate the version fixture");
        require(CopyFileW(exe, gameDll.c_str(), TRUE), "Could not copy the version fixture");
        const auto version = warcraftVersion(gameDll);
        require(version == WarcraftVersion{1, 27, 0, 52240} && supportsWarcraftMana(*version), "1.27a not recognized");
        require(!supportsWarcraftMana({1, 27, 0, 5224}), "Unknown build should not be accepted");
        require(!supportsWarcraftMana({1, 20, 4, 6074}), "Unsupported patch should not be accepted");

        auto profile = folder / L"诸葛亮.ini";
        std::ofstream(profile, std::ios::binary) << "[CUSTOMBARS]\r\nISENABLED = no\r\n"
                                 "[MANABAR]\r\nISENABLED = no\r\nISHEROONLY = Off\r\nHEIGHT = .004\r\n"
                                 "[OTHER]\r\nSETTING = keep\r\n";
        require(wfeHeroManaEnabled(profile) == false, "Disabled profile should be recognized");
        require(saveWfeHeroMana(profile, true), "Could not enable mana bars");
        require(wfeHeroManaEnabled(profile) == true, "Expected hero-only mana bars to be enabled");
        require(saveWfeHeroMana(profile, false), "Could not disable mana bars");
        require(wfeHeroManaEnabled(profile) == false, "Expected mana bars to be disabled");
        wchar_t value[16]{};
        GetPrivateProfileStringW(L"OTHER", L"SETTING", L"", value, 16, profile.c_str());
        require(std::wstring(value) == L"keep", "An unrelated profile setting changed");
        const auto wfe = folder / L"WFE 工具";
        const auto profiles = wfe / L"Profiles";
        std::filesystem::create_directories(profiles);
        const auto wfeApp = wfe / L"WFEApp.exe";
        std::ofstream{wfeApp};
        const auto settings = wfe / L"WFESettings.ini";
        const auto selectedProfile = profiles / L"WFEConfig.ini";
        std::ofstream(settings) << "[SELECTED]\nFILE=WFEConfig.ini\n";
        require(activeWfeProfile(wfeApp) == selectedProfile, "WFE's selected profile was not found");
        require(saveWfeHeroMana(selectedProfile, true), "Missing WFE profile should be created");
        require(wfeHeroManaEnabled(selectedProfile) == true, "New WFE profile should enable hero mana");
        require(saveWfeHeroMana(selectedProfile, false), "Could not disable the WFE profile");
        const auto gameConfig = folder / L"WFEConfig.ini";
        require(saveWfeHeroMana(gameConfig, false, selectedProfile), "Could not copy the profile into the game folder");
        require(wfeHeroManaEnabled(gameConfig) == false, "Game-folder copy has the wrong settings");
        std::ofstream(gameConfig, std::ios::app) << "[OTHER]\nSETTING=keep\n";
        require(saveWfeHeroMana(gameConfig, true, selectedProfile), "Could not update the existing game-folder copy");
        require(wfeHeroManaEnabled(gameConfig) == true, "Game-folder copy should enable hero mana");
        GetPrivateProfileStringW(L"OTHER", L"SETTING", L"", value, 16, gameConfig.c_str());
        require(std::wstring(value) == L"keep", "Copying over the game config lost an existing setting");
        std::ofstream(settings) << "[SELECTED]\nFILE=..\\outside.ini\n";
        require(!activeWfeProfile(wfeApp), "A profile outside WFE's Profiles directory should be rejected");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
