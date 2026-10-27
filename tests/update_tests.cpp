#include "../src/update_core.hpp"
#include <iostream>
#include <functional>

using namespace magickey::updates;
void require(bool value) { if (!value) throw std::runtime_error("Update test failed"); }
void rejects(const std::function<void()>& call) {
    bool failed = false;
    try { call(); } catch (const std::exception&) { failed = true; }
    require(failed);
}
nlohmann::json fixture() {
    const std::string prefix = ReleasesUrl + "/download/v1.3.0/";
    return {{"tag_name", "v1.3.0"}, {"draft", false}, {"prerelease", false},
        {"body", "Notes mention a fake tag_name: v999.0.0"},
        {"assets", nlohmann::json::array({
            {{"name", "MagicKey.exe"}, {"browser_download_url", prefix + "MagicKey.exe"},
                {"size", 1024}, {"digest", "sha256:" + std::string(64, 'a')}},
            {{"name", "MagicKey.exe.sha256"}, {"browser_download_url", prefix + "MagicKey.exe.sha256"}, {"size", 79}}
        })}};
}
int main() {
    require(parseVersion("v1.10.0") > parseVersion("1.9.0"));
    require(parseVersion("1.2.0") == parseVersion("v1.2.0"));
    require(parseVersion("0.0.0") < parseVersion(MAGICKEY_VERSION));
    for (auto bad : {"", "v", "1", "1.2", "1.2.3.4", "-1.2.3", "1.2.x", "1.2.3-beta",
        "1.2.3+metadata", "01.2.3", "1.2.3 ", "9999999.2.3", "v1.2.3/evil"})
        rejects([&] { (void)parseVersion(bad); });
    require(parseChecksum(std::string(64, 'A') + "  MagicKey.exe\r\n") == std::string(64, 'a'));
    require(parseChecksum(std::string(64, 'a') + " *MagicKey.exe\n") == std::string(64, 'a'));
    for (const std::string& bad : std::vector<std::string>{"", "bad MagicKey.exe", std::string(64, 'g') + " MagicKey.exe"})
        rejects([&] { (void)parseChecksum(bad); });
    rejects([] { parseChecksum(std::string(64, 'a') + " other.exe"); });
    rejects([] { parseChecksum(std::string(64, 'a') + " MagicKey.exe extra"); });
    auto json = fixture();
    auto release = parseRelease(json.dump());
    require(release.tag == "v1.3.0" && release.size == 1024 && release.digest == std::string(64, 'a'));
    require(release.url == ReleasesUrl + "/download/v1.3.0/MagicKey.exe");
    auto bad = json; bad["draft"] = true; rejects([&] { parseRelease(bad.dump()); });
    bad = json; bad["prerelease"] = true; rejects([&] { parseRelease(bad.dump()); });
    bad = json; bad["tag_name"] = "v1.3.0/../../evil"; rejects([&] { parseRelease(bad.dump()); });
    for (const auto& url : {"http://github.com/zxiaox/magicKey/releases/download/v1.3.0/MagicKey.exe",
        "https://github.com.evil.test/zxiaox/magicKey/releases/download/v1.3.0/MagicKey.exe",
        "https://github.com/other/repo/releases/download/v1.3.0/MagicKey.exe",
        "https://github.com/zxiaox/magicKey/releases/download/v1.2.0/MagicKey.exe"}) {
        bad = json; bad["assets"][0]["browser_download_url"] = url;
        rejects([&] { parseRelease(bad.dump()); });
    }
    for (const auto& size : {nlohmann::json(-1), nlohmann::json(0), nlohmann::json(1.5),
        nlohmann::json(MaxExecutableSize + 1), nlohmann::json("1024")}) {
        bad = json; bad["assets"][0]["size"] = size;
        rejects([&] { parseRelease(bad.dump()); });
    }
    bad = json; bad["assets"].push_back(bad["assets"][0]); rejects([&] { parseRelease(bad.dump()); });
    bad = json; bad["assets"] = nlohmann::json::array(); rejects([&] { parseRelease(bad.dump()); });
    bad = json; bad["assets"] = nlohmann::json::object(); rejects([&] { parseRelease(bad.dump()); });
    bad = json; bad["assets"][0]["digest"] = nullptr;
    require(parseRelease(bad.dump()).digest.empty()); // Checksum asset is a valid fallback.
    bad["assets"].erase(1); rejects([&] { parseRelease(bad.dump()); });
    bad = json; bad["assets"].erase(1);
    require(!parseRelease(bad.dump()).digest.empty()); // API digest also works without checksum asset.
    rejects([] { parseRelease("not JSON"); });
    std::vector<unsigned char> pe(256, 0);
    pe[0] = 'M'; pe[1] = 'Z'; pe[60] = 128;
    pe[128] = 'P'; pe[129] = 'E'; pe[132] = 0x64; pe[133] = 0x86;
    pe[152] = 0x0B; pe[153] = 0x02;
    require(isX64Executable(pe));
    pe[132] = 0x4C; require(!isX64Executable(pe)); // x86 is not an x64 update.
    pe[132] = 0x64; pe[60] = pe[61] = pe[62] = pe[63] = 0xFF;
    require(!isX64Executable(pe));
    require(!isX64Executable({}));
    require(quoteArgument(L"") == L"\"\"");
    require(quoteArgument(L"C:\\Magic Key\\") == L"\"C:\\Magic Key\\\\\"");
    require(quoteArgument(L"a\"b") == L"\"a\\\"b\"");
    require(quoteArgument(L"C:\\测试 & app\\MagicKey.exe") == L"\"C:\\测试 & app\\MagicKey.exe\"");
    std::cout << "Update versions, release metadata, URLs, checksums, PE headers and argument quoting passed.\n";
}
