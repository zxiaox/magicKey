#include "updater.hpp"
#include "localization.hpp"
#include <winhttp.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <algorithm>
#include <chrono>
#include <cwctype>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>

namespace magickey::updates {
namespace {
struct Error : std::runtime_error {
    Failure failure;
    explicit Error(Failure reason) : std::runtime_error("MagicKey update failed"), failure(reason) {}
};
struct Internet {
    HINTERNET value = nullptr;
    explicit Internet(HINTERNET h) : value(h) { if (!h) throw Error(Failure::Network); }
    ~Internet() { if (value) WinHttpCloseHandle(value); }
    Internet(const Internet&) = delete;
};
struct Handle {
    HANDLE value = nullptr;
    explicit Handle(HANDLE h) : value(h) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
};
std::wstring toWide(const std::string& text) {
    int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!size) throw Error(Failure::InvalidRelease);
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), result.data(), size);
    return result;
}
void checkCancelled(const std::atomic<bool>& cancelled) {
    if (cancelled.load()) throw Error(Failure::Cancelled);
}
struct Address { std::wstring host, path; };
Address address(const std::wstring& url) {
    URL_COMPONENTS parts{}; parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
    parts.dwUserNameLength = parts.dwPasswordLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), DWORD(url.size()), 0, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != INTERNET_DEFAULT_HTTPS_PORT ||
        parts.dwUserNameLength || parts.dwPasswordLength)
        throw Error(Failure::InvalidRelease);
    Address result{std::wstring(parts.lpszHostName, parts.dwHostNameLength),
        std::wstring(parts.lpszUrlPath, parts.dwUrlPathLength)};
    std::transform(result.host.begin(), result.host.end(), result.host.begin(),
        [](wchar_t c) { return wchar_t(std::towlower(c)); });
    if (result.host != L"api.github.com" && result.host != L"github.com" &&
        result.host != L"release-assets.githubusercontent.com" && result.host != L"objects.githubusercontent.com")
        throw Error(Failure::InvalidRelease);
    if (parts.dwExtraInfoLength) result.path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    return result;
}
void request(std::wstring url, bool metadata, std::uint64_t maximum,
    const std::atomic<bool>& cancelled, const std::function<void(const char*, std::size_t)>& sink) {
    Internet session(WinHttpOpen(L"MagicKey/" MAGICKEY_VERSION_W, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!WinHttpSetTimeouts(session.value, 10000, 10000, 15000, 15000)) throw Error(Failure::Network);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::minutes(3);
    for (int redirect = 0; redirect <= 8; ++redirect) {
        checkCancelled(cancelled);
        auto target = address(url);
        Internet connection(WinHttpConnect(session.value, target.host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0));
        Internet query(WinHttpOpenRequest(connection.value, L"GET", target.path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
        DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
        if (!WinHttpSetOption(query.value, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy)))
            throw Error(Failure::Network);
        const wchar_t* headers = metadata ? L"Accept: application/vnd.github+json\r\nCache-Control: no-cache\r\n" :
            L"Accept: application/octet-stream\r\n";
        if (!WinHttpSendRequest(query.value, headers, DWORD(-1), WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
            !WinHttpReceiveResponse(query.value, nullptr)) throw Error(Failure::Network);
        DWORD status = 0, length = sizeof(status);
        if (!WinHttpQueryHeaders(query.value, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX)) throw Error(Failure::Network);
        if (status == 301 || status == 302 || status == 303 || status == 307 || status == 308) {
            DWORD bytes = 0;
            WinHttpQueryHeaders(query.value, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                nullptr, &bytes, WINHTTP_NO_HEADER_INDEX);
            if (!bytes || bytes > 32768) throw Error(Failure::InvalidRelease);
            std::vector<wchar_t> location(bytes / sizeof(wchar_t) + 1, 0);
            if (!WinHttpQueryHeaders(query.value, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX,
                location.data(), &bytes, WINHTTP_NO_HEADER_INDEX)) throw Error(Failure::Network);
            url = location.data();
            (void)address(url); // Reject non-HTTPS and unexpected redirect destinations.
            continue;
        }
        if (status == 404) throw Error(Failure::NoRelease);
        if (status == 403 || status == 429) throw Error(Failure::RateLimit);
        if (status != 200) throw Error(Failure::Network);
        std::uint64_t total = 0;
        char buffer[65536];
        while (true) {
            checkCancelled(cancelled);
            if (std::chrono::steady_clock::now() > deadline) throw Error(Failure::Network);
            DWORD received = 0;
            if (!WinHttpReadData(query.value, buffer, sizeof(buffer), &received)) throw Error(Failure::Network);
            if (!received) return;
            total += received;
            if (total > maximum) throw Error(Failure::InvalidDownload);
            sink(buffer, received);
        }
    }
    throw Error(Failure::Network);
}
std::string getText(const std::string& url, bool metadata, std::size_t limit, const std::atomic<bool>& cancelled) {
    std::string result;
    request(toWide(url), metadata, limit, cancelled, [&](const char* data, std::size_t count) { result.append(data, count); });
    return result;
}
std::string verifyExecutable(const std::filesystem::path& path, const std::string& expected) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw Error(Failure::InvalidDownload);
    std::vector<unsigned char> header(65536);
    input.read(reinterpret_cast<char*>(header.data()), std::streamsize(header.size()));
    header.resize(std::size_t(input.gcount()));
    if (!isX64Executable(header)) throw Error(Failure::InvalidDownload);
    input.clear(); input.seekg(0);
    std::vector<unsigned char> object;
    struct Cng {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        BCRYPT_HASH_HANDLE hash = nullptr;
        ~Cng() { if (hash) BCryptDestroyHash(hash); if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0); }
    } cng;
    if (BCryptOpenAlgorithmProvider(&cng.algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)) throw Error(Failure::Checksum);
    DWORD objectSize = 0, returned = 0;
    if (BCryptGetProperty(cng.algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
        sizeof(objectSize), &returned, 0)) throw Error(Failure::Checksum);
    object.resize(objectSize);
    if (BCryptCreateHash(cng.algorithm, &cng.hash, object.data(), objectSize, nullptr, 0, 0)) throw Error(Failure::Checksum);
    char bytes[65536];
    while (input.read(bytes, sizeof(bytes)) || input.gcount()) {
        if (BCryptHashData(cng.hash, reinterpret_cast<PUCHAR>(bytes), ULONG(input.gcount()), 0)) throw Error(Failure::Checksum);
    }
    if (!input.eof()) throw Error(Failure::InvalidDownload);
    unsigned char digest[32];
    if (BCryptFinishHash(cng.hash, digest, sizeof(digest), 0)) throw Error(Failure::Checksum);
    // Release before its backing object buffer is destroyed.
    BCryptDestroyHash(cng.hash); cng.hash = nullptr;
    std::ostringstream hex;
    for (unsigned char byte : digest) hex << std::hex << std::setfill('0') << std::setw(2) << unsigned(byte);
    if (hex.str() != normalizeHash(expected)) throw Error(Failure::Checksum);
    return hex.str();
}
std::filesystem::path makeStaging() {
    const auto folder = executablePath().parent_path();
    for (int attempt = 0; attempt < 5; ++attempt) {
        unsigned char random[12];
        if (BCryptGenRandom(nullptr, random, sizeof(random), BCRYPT_USE_SYSTEM_PREFERRED_RNG)) throw Error(Failure::Install);
        std::wostringstream name;
        name << L".MagicKey-update-";
        for (unsigned char byte : random) name << std::hex << std::setfill(L'0') << std::setw(2) << unsigned(byte);
        auto path = folder / name.str();
        if (CreateDirectoryW(path.c_str(), nullptr)) return path;
        if (GetLastError() != ERROR_ALREADY_EXISTS) throw Error(Failure::NotWritable);
    }
    throw Error(Failure::NotWritable);
}
void startProcess(const std::filesystem::path& executable, const std::wstring& arguments = L"") {
    auto command = quoteArgument(executable.native()) + (arguments.empty() ? L"" : L" " + arguments);
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
        executable.parent_path().c_str(), &startup, &process)) throw Error(Failure::Install);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
}
bool validStaging(const std::filesystem::path& staging, const std::filesystem::path& folder) {
    std::error_code ec;
    const auto name = staging.filename().wstring();
    return name.rfind(L".MagicKey-update-", 0) == 0 && name.size() == 41 &&
        std::filesystem::equivalent(staging.parent_path(), folder, ec) && !ec &&
        !(GetFileAttributesW(staging.c_str()) & FILE_ATTRIBUTE_REPARSE_POINT);
}
int applyUpdate(int argc, wchar_t** argv) {
    int language = 0;
    std::filesystem::path target;
    bool originalExited = false;
    try {
        if (argc != 6) throw Error(Failure::Install);
        wchar_t* end = nullptr;
        auto inherited = std::wcstoull(argv[2], &end, 10);
        if (!inherited || *end || inherited > std::numeric_limits<std::uintptr_t>::max()) throw Error(Failure::Install);
        Handle parent(reinterpret_cast<HANDLE>(std::uintptr_t(inherited)));
        if (!GetProcessId(parent.value) || GetProcessId(parent.value) == GetCurrentProcessId()) throw Error(Failure::Install);
        language = int(std::wcstol(argv[5], &end, 10));
        if (*end || language < 0 || language > 3) throw Error(Failure::Install);
        target = std::filesystem::path(argv[3]);
        auto own = executablePath();
        auto staging = own.parent_path();
        if (!target.is_absolute() || target.extension() != L".exe" || own.filename() != L"MagicKey.updater.exe" ||
            !validStaging(staging, target.parent_path())) throw Error(Failure::Install);
        std::wstring hashWide(argv[4]);
        auto hash = normalizeHash(std::string(hashWide.begin(), hashWide.end()));
        auto staged = staging / L"MagicKey.new.exe";
        if (WaitForSingleObject(parent.value, 30000) != WAIT_OBJECT_0) throw Error(Failure::Install);
        originalExited = true;
        verifyExecutable(staged, hash);
        auto backup = target; backup += L".backup";
        if (!CopyFileW(target.c_str(), backup.c_str(), FALSE)) throw Error(Failure::NotWritable);
        if (!MoveFileExW(staged.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw Error(Failure::NotWritable);
        try {
            startProcess(target, L"--magickey-cleanup-update " + quoteArgument(staging.native()));
        } catch (...) {
            if (!MoveFileExW(backup.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                throw Error(Failure::Install);
            throw;
        }
        return 0;
    } catch (...) {
        MessageBoxW(nullptr, mywarkey::translate(L"The update could not be installed. Your profiles are unchanged. The previous executable or its .backup file can be used to recover.", language),
            L"MagicKey", MB_OK | MB_ICONWARNING);
        if (originalExited && !target.empty()) { try { startProcess(target); } catch (...) {} }
        return 1;
    }
}
} // namespace

const wchar_t* failureMessage(Failure failure) {
    switch (failure) {
        case Failure::NoRelease: return L"No public release is available yet. Publish a stable GitHub release with MagicKey.exe and its checksum.";
        case Failure::RateLimit: return L"GitHub is limiting update requests. Try again later.";
        case Failure::InvalidRelease: return L"The release information is invalid or missing the Windows executable or checksum.";
        case Failure::InvalidDownload: return L"The downloaded file is incomplete or is not a Windows x64 executable. Nothing was installed.";
        case Failure::Checksum: return L"The update checksum could not be verified. Nothing was installed.";
        case Failure::NotWritable: return L"The app folder is not writable. Move MagicKey to a writable folder and try again.";
        case Failure::Install: return L"The updater could not start. The current app and your profiles have not been replaced.";
        case Failure::Cancelled: return L"Update cancelled.";
        default: return L"Could not connect to GitHub. Check your internet connection and try again.";
    }
}
std::filesystem::path executablePath() {
    std::wstring path(32768, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    if (!length || length >= path.size()) throw Error(Failure::Install);
    path.resize(length);
    return path;
}
void discardStaging(const std::filesystem::path& staging) {
    if (staging.empty() || !validStaging(staging, executablePath().parent_path())) return;
    std::error_code ec;
    std::filesystem::remove_all(staging, ec);
}
Client::~Client() { cancel(); if (worker.joinable()) worker.join(); }
Result Client::take() {
    if (worker.joinable()) worker.join();
    running = false;
    std::lock_guard<std::mutex> lock(resultMutex);
    return std::move(result);
}
void Client::start(HWND window, UINT message, Release release, bool downloading) {
    if (running.load()) return;
    if (worker.joinable()) worker.join();
    cancelled = false; running = true;
    try {
        worker = std::thread([this, window, message, release = std::move(release), downloading]() {
            Result finished;
            try {
                if (!downloading) {
                    auto text = getText(LatestApi, true, 2 * 1024 * 1024, cancelled);
                    try { finished.release = parseRelease(text); }
                    catch (const std::exception&) { throw Error(Failure::InvalidRelease); }
                    finished.outcome = parseVersion(finished.release.tag) > parseVersion(MAGICKEY_VERSION) ?
                        Outcome::Available : Outcome::Current;
                } else {
                    finished.release = release;
                    finished.hash = release.digest;
                    if (!release.checksumUrl.empty()) {
                        std::string checksum;
                        try { checksum = parseChecksum(getText(release.checksumUrl, false, 4096, cancelled)); }
                        catch (const Error&) { throw; }
                        catch (const std::exception&) { throw Error(Failure::Checksum); }
                        if (!finished.hash.empty() && finished.hash != checksum) throw Error(Failure::Checksum);
                        finished.hash = checksum;
                    }
                    finished.staging = makeStaging();
                    auto file = finished.staging / L"MagicKey.new.exe";
                    std::ofstream out(file, std::ios::binary | std::ios::trunc);
                    if (!out) throw Error(Failure::NotWritable);
                    request(toWide(release.url), false, release.size, cancelled, [&](const char* data, std::size_t count) {
                        out.write(data, std::streamsize(count));
                        if (!out) throw Error(Failure::NotWritable);
                    });
                    out.close();
                    if (!out || std::filesystem::file_size(file) != release.size) throw Error(Failure::InvalidDownload);
                    verifyExecutable(file, finished.hash);
                    checkCancelled(cancelled);
                    if (!CopyFileW(executablePath().c_str(), (finished.staging / L"MagicKey.updater.exe").c_str(), TRUE))
                        throw Error(Failure::NotWritable);
                    finished.outcome = Outcome::Downloaded;
                }
                checkCancelled(cancelled);
            } catch (const Error& e) {
                finished.failure = e.failure;
                finished.outcome = e.failure == Failure::Cancelled ? Outcome::Cancelled : Outcome::Failed;
            } catch (...) {
                finished.failure = downloading ? Failure::InvalidDownload : Failure::InvalidRelease;
                finished.outcome = Outcome::Failed;
            }
            if (finished.outcome == Outcome::Failed || finished.outcome == Outcome::Cancelled) {
                try { discardStaging(finished.staging); } catch (...) {}
                finished.staging.clear();
            }
            { std::lock_guard<std::mutex> lock(resultMutex); result = std::move(finished); }
            PostMessageW(window, message, 0, 0);
        });
    } catch (...) { running = false; throw; }
}
void launchInstaller(const Result& result, int language) {
    if (result.outcome != Outcome::Downloaded || !validStaging(result.staging, executablePath().parent_path()))
        throw Error(Failure::Install);
    Handle parent(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, TRUE, GetCurrentProcessId()));
    if (!parent.value) throw Error(Failure::Install);
    SIZE_T bytes = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
    std::vector<unsigned char> storage(bytes);
    auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if (!InitializeProcThreadAttributeList(attributes, 1, 0, &bytes)) throw Error(Failure::Install);
    struct Attributes { LPPROC_THREAD_ATTRIBUTE_LIST value; ~Attributes() { DeleteProcThreadAttributeList(value); } } guard{attributes};
    if (!UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, &parent.value, sizeof(parent.value), nullptr, nullptr))
        throw Error(Failure::Install);
    const auto helper = result.staging / L"MagicKey.updater.exe";
    auto arguments = quoteArgument(helper.native()) + L" --magickey-apply-update " +
        std::to_wstring(reinterpret_cast<std::uintptr_t>(parent.value)) + L" " + quoteArgument(executablePath().native()) +
        L" " + quoteArgument(toWide(result.hash)) + L" " + std::to_wstring(language);
    STARTUPINFOEXW startup{}; startup.StartupInfo.cb = sizeof(startup); startup.lpAttributeList = attributes;
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(helper.c_str(), arguments.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT,
        nullptr, result.staging.c_str(), &startup.StartupInfo, &process)) throw Error(Failure::Install);
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
}
std::optional<int> handleUpdaterArguments() {
    int count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return 1;
    struct Args { wchar_t** value; ~Args() { LocalFree(value); } } guard{arguments};
    if (count >= 2 && std::wstring_view(arguments[1]) == L"--magickey-apply-update") return applyUpdate(count, arguments);
    if (count == 3 && std::wstring_view(arguments[1]) == L"--magickey-cleanup-update") {
        try {
            const std::filesystem::path staging(arguments[2]);
            for (int attempt = 0; attempt < 10; ++attempt) {
                discardStaging(staging);
                if (!std::filesystem::exists(staging)) break;
                Sleep(50);
            }
        } catch (...) {} // A leftover helper is harmless; settings are never removed.
    }
    return std::nullopt;
}
} // namespace magickey::updates
