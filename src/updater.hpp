#pragma once
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <thread>
#include "update_core.hpp"

namespace magickey::updates {
enum class Failure { Network, NoRelease, RateLimit, InvalidRelease, InvalidDownload, Checksum,
    NotWritable, Install, Cancelled };
const wchar_t* failureMessage(Failure failure);
enum class Outcome { Current, Available, Downloaded, Failed, Cancelled };
struct Result {
    Outcome outcome = Outcome::Failed;
    Failure failure = Failure::Network;
    Release release;
    std::filesystem::path staging;
    std::string hash;
};
class Client {
    std::thread worker;
    std::atomic<bool> cancelled{false}, running{false};
    std::mutex resultMutex;
    Result result;
    void start(HWND window, UINT message, Release release, bool downloading);
public:
    ~Client();
    bool busy() const { return running.load(); }
    void cancel() { cancelled = true; }
    void check(HWND window, UINT message) { start(window, message, {}, false); }
    void download(HWND window, UINT message, Release release) { start(window, message, std::move(release), true); }
    Result take();
};
std::filesystem::path executablePath();
void discardStaging(const std::filesystem::path& staging);
void launchInstaller(const Result& result, int language);
// Run before the single-instance mutex; the temporary helper must coexist with its parent.
std::optional<int> handleUpdaterArguments();
}
