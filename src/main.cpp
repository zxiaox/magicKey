#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <iterator>
#include <mutex>
#include "core.hpp"
#include "key_output.hpp"
#include "localization.hpp"
#include "key_capture.hpp"
#include "updater.hpp"
#include "theme_ui.hpp"

using namespace mywarkey;
constexpr UINT PauseMessage = WM_APP + 1, InputError = WM_APP + 2, CapturedKey = WM_APP + 3, UpdateFinished = WM_APP + 4;
enum ControlId { ProfileBox = 101, NewProfile, RenameProfile, DeleteProfile, ProfileName,
    MappingList, SourceBox, TargetBox, AddMapping, RemoveMapping, BlockWindows, Toggle, LanguageBox, ScanCodes, DisableTarget, CheckUpdates, ThemeBox, SectionProfile, SectionMappings };
constexpr wchar_t ClassName[] = L"mywarkey.Portable.Window";
HWND window, profileBox, nameEdit, mappingList, sourceBox, targetBox, blockBox, toggleButton, statusLabel, languageBox, scanBox;
std::vector<std::pair<HWND, std::wstring>> localizedControls;
HFONT font;
magickey::ThemeUI themeUI;
HWND themeBox;
HIMAGELIST rowImages;
HANDLE hookThread, hookReady;
DWORD hookThreadId;
bool hookInstalled = false;
Settings settings;
std::filesystem::path settingsPath;
std::filesystem::path themePath;
bool enabled = false;
bool inputFailed = false;
std::mutex engineMutex;
Remapper engine;
KeyCapture capture;
unsigned captureGeneration = 0;
int sourceChoice = VK_NUMPAD7, targetChoice = 'Q';
Profile liveProfile;
bool liveEnabled = false;
bool liveScanCodes = true;
magickey::updates::Client updateClient;
bool downloadingUpdate = false, closeAfterUpdate = false;

const wchar_t* tr(const wchar_t* text) { return translate(text, settings.language); }

std::wstring wide(const std::string& text) {
    if (text.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0);
    if (!n) throw std::runtime_error("Profile name is not valid UTF-8.");
    std::wstring out(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), out.data(), n);
    return out;
}
std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), out.data(), n, nullptr, nullptr);
    return out;
}
void error(const std::wstring& message) { MessageBoxW(window, tr(message.c_str()), L"MagicKey", MB_OK | MB_ICONWARNING); }

struct Key { int vk; std::wstring label; };
std::vector<Key> keys;
void initKeys() {
    for (int c = 'A'; c <= 'Z'; ++c) keys.push_back({c, std::wstring(1, wchar_t(c))});
    for (int c = '0'; c <= '9'; ++c) keys.push_back({c, std::wstring(1, wchar_t(c))});
    for (int i = 0; i < 10; ++i) keys.push_back({VK_NUMPAD0 + i, L"Numpad " + std::to_wstring(i)});
    for (int i = 0; i < 24; ++i) keys.push_back({VK_F1 + i, L"F" + std::to_wstring(i + 1)});
    const Key extra[] = {
        {VK_SPACE,L"Space"},{VK_RETURN,L"Enter"},{VK_TAB,L"Tab"},{VK_ESCAPE,L"Escape"},
        {VK_BACK,L"Backspace"},{VK_CAPITAL,L"Caps Lock"},{VK_LSHIFT,L"Left Shift"},
        {VK_RSHIFT,L"Right Shift"},{VK_LCONTROL,L"Left Ctrl"},{VK_RCONTROL,L"Right Ctrl"},
        {VK_LMENU,L"Left Alt"},{VK_RMENU,L"Right Alt"},{VK_LWIN,L"Left Windows"},
        {VK_RWIN,L"Right Windows"},{VK_APPS,L"Menu"},{VK_LEFT,L"Left arrow"},
        {VK_RIGHT,L"Right arrow"},{VK_UP,L"Up arrow"},{VK_DOWN,L"Down arrow"},
        {VK_HOME,L"Home"},{VK_END,L"End"},{VK_PRIOR,L"Page Up"},{VK_NEXT,L"Page Down"},
        {VK_INSERT,L"Insert"},{VK_DELETE,L"Delete key"},{VK_SNAPSHOT,L"Print Screen"},
        {VK_SCROLL,L"Scroll Lock"},{VK_NUMLOCK,L"Num Lock"},
        {VK_ADD,L"Numpad +"},{VK_SUBTRACT,L"Numpad -"},{VK_MULTIPLY,L"Numpad *"},
        {VK_DIVIDE,L"Numpad /"},{VK_DECIMAL,L"Numpad decimal"},
        {VK_OEM_1,L"; / :"},{VK_OEM_PLUS,L"= / +"},{VK_OEM_COMMA,L", / <"},
        {VK_OEM_MINUS,L"- / _"},{VK_OEM_PERIOD,L". / >"},{VK_OEM_2,L"/ / ?"},
        {VK_OEM_3,L"` / ~"},{VK_OEM_4,L"[ / {"},{VK_OEM_5,L"\\ / |"},
        {VK_OEM_6,L"] / }"},{VK_OEM_7,L"' / \""},{VK_OEM_102,L"ISO extra key"}};
    keys.insert(keys.end(), std::begin(extra), std::end(extra));
}
std::wstring keyName(int vk) {
    if (!vk) return tr(L"Disabled");
    if (vk >= '0' && vk <= '9')
        return std::wstring(tr(L"Top row")) + L" " + wchar_t(vk);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
        return std::wstring(tr(L"Numpad")) + L" " + std::to_wstring(vk - VK_NUMPAD0);
    for (const auto& k : keys) if (k.vk == vk) return tr(k.label.c_str());
    return std::wstring(tr(L"Key")) + L" " + std::to_wstring(vk);
}
bool extended(int vk) {
    switch (vk) {
        case VK_RCONTROL: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_APPS:
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR:
        case VK_NEXT: case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN:
        case VK_DIVIDE: case VK_NUMLOCK: case VK_SNAPSHOT: return true;
        default: return false;
    }
}
void sendKey(int vk, bool down) {
    // Pair a key-up with exactly the scan code/layout/mode used by its key-down.
    // This also handles layout changes while a remapped key is held.
    static std::array<KeyOutput, 256> heldOutput{};
    static std::array<bool, 256> outputHeld{};
    KeyOutput output;
    if (outputHeld[vk]) output = heldOutput[vk];
    else {
        DWORD targetThread = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
        HKL layout = GetKeyboardLayout(targetThread);
        unsigned scan = MapVirtualKeyExW(vk, MAPVK_VK_TO_VSC_EX, layout);
        output = keyOutput(vk, true, scan, (scan & 0xFF00) == 0xE000 || extended(vk), liveScanCodes);
    }
    if (down) { heldOutput[vk] = output; outputHeld[vk] = true; }
    else { output.flags |= KEYEVENTF_KEYUP; outputHeld[vk] = false; }
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = output.vk;
    input.ki.wScan = output.scan;
    input.ki.dwFlags = output.flags;
    if (SendInput(1, &input, sizeof(input)) != 1 && !inputFailed) {
        inputFailed = true;
        PostMessageW(window, InputError, 0, 0);
    }
}
void syncEngine() {
    std::lock_guard<std::mutex> lock(engineMutex);
    engine.release(sendKey);
    liveProfile = settings.profiles[settings.selected];
    liveEnabled = enabled;
    liveScanCodes = settings.scanCodes;
}

int sourceKey(const KBDLLHOOKSTRUCT& k) {
    return physicalKey(int(k.vkCode), k.scanCode, (k.flags & LLKHF_EXTENDED) != 0);
}
LRESULT CALLBACK keyboardProc(int code, WPARAM message, LPARAM value) {
    if (code != HC_ACTION) return CallNextHookEx(nullptr, code, message, value);
    const auto& key = *reinterpret_cast<KBDLLHOOKSTRUCT*>(value);
    // Never remap injected events: A -> Q, Q -> A must not recurse.
    if (key.flags & LLKHF_INJECTED) return CallNextHookEx(nullptr, code, message, value);
    static std::array<bool, 256> physical{};
    static bool emergencyHeld = false;
    bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    int source = sourceKey(key);
    if (source < 1 || source > 254) return CallNextHookEx(nullptr, code, message, value);
    bool wasDown = physical[source];
    physical[source] = down;
    if (source == VK_F12) {
        if (down && (physical[VK_LCONTROL] || physical[VK_RCONTROL]) &&
            (physical[VK_LMENU] || physical[VK_RMENU])) {
            emergencyHeld = true;
            {
                std::lock_guard<std::mutex> lock(engineMutex);
                liveEnabled = false;
                capture.cancel();
                ++captureGeneration;
                engine.release(sendKey);
            }
            PostMessageW(window, PauseMessage, 0, 0);
            return 1;
        }
        if (emergencyHeld) {
            if (!down) {
                emergencyHeld = false;
                std::lock_guard<std::mutex> lock(engineMutex);
                capture.event(source, false, wasDown);
            }
            return 1;
        }
    }
    bool swallowed;
    {
        std::lock_guard<std::mutex> lock(engineMutex);
        if (GetForegroundWindow() != window) capture.cancel();
        auto result = capture.event(source, down, wasDown);
        if (result.slot != CaptureSlot::None)
            PostMessageW(window, CapturedKey, WPARAM(captureGeneration),
                MAKELPARAM(result.key, int(result.slot)));
        if (result.swallow) return 1;
        // Keep the profile editor usable even when a profile remaps typing keys.
        swallowed = engine.event(source, down, liveProfile,
            liveEnabled && GetForegroundWindow() != window, sendKey);
    }
    return swallowed ? 1 : CallNextHookEx(nullptr, code, message, value);
}
DWORD WINAPI keyboardThread(void*) {
    MSG message;
    PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE); // Create the message queue.
    HHOOK hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboardProc, GetModuleHandleW(nullptr), 0);
    hookInstalled = hook != nullptr;
    SetEvent(hookReady);
    if (!hook) return 1;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    UnhookWindowsHookEx(hook);
    return 0;
}

bool commit(Settings candidate) {
    try {
        auto temporary = settingsPath;
        temporary += L".tmp";
        writeTemporary(temporary, candidate);
        if (!MoveFileExW(temporary.c_str(), settingsPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not replace the profile file. Check that the folder is writable.");
        settings = std::move(candidate);
        syncEngine();
        return true;
    } catch (const std::exception& e) { error(wide(e.what())); return false; }
}
HWND control(const wchar_t* type, const wchar_t* label, DWORD style, int id, int x, int y, int w, int h) {
    if (std::wstring_view(type) == WC_COMBOBOXW) style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    HWND child = CreateWindowExW(0, type, tr(label), WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, window, reinterpret_cast<HMENU>(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(child, WM_SETFONT, WPARAM(std::wstring_view(type) == L"STATIC" ? themeUI.small() : font), TRUE);
    if (*label && (std::wstring_view(type) == L"STATIC" || std::wstring_view(type) == L"BUTTON"))
        localizedControls.emplace_back(child, label);
    return child;
}
void refreshThemes() {
    SendMessageW(themeBox, CB_RESETCONTENT, 0, 0);
    for (const auto& theme : magickey::Themes)
        SendMessageW(themeBox, CB_ADDSTRING, 0, LPARAM(tr(theme.name)));
    SendMessageW(themeBox, CB_SETCURSEL, int(themeUI.theme()), 0);
}
void changeTheme(int choice) {
    if (!magickey::validTheme(choice)) return;
    try {
        auto temporary = themePath; temporary += L".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        magickey::writeTheme(output, magickey::ThemeId(choice));
        output.close();
        if (!output || !MoveFileExW(temporary.c_str(), themePath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Theme save failed");
        themeUI.apply(window, mappingList, magickey::ThemeId(choice));
    } catch (...) { error(L"Could not save the theme. Check that the app folder is writable."); }
    refreshThemes();
}
void updateCaptureButtons() {
    CaptureSlot slot;
    { std::lock_guard<std::mutex> lock(engineMutex); slot = capture.activeSlot(); }
    auto source = slot == CaptureSlot::Source ? std::wstring(tr(L"Press a key...")) : keyName(sourceChoice);
    auto target = slot == CaptureSlot::Target ? std::wstring(tr(L"Press a key...")) : keyName(targetChoice);
    SetWindowTextW(sourceBox, source.c_str());
    SetWindowTextW(targetBox, target.c_str());
}
void cancelCapture() {
    { std::lock_guard<std::mutex> lock(engineMutex); capture.cancel(); ++captureGeneration; }
    updateCaptureButtons();
}
void updateUpdateButton() {
    HWND button = GetDlgItem(window, CheckUpdates);
    const wchar_t* text = closeAfterUpdate ? L"Cancelling update..." : updateClient.busy() ?
        (downloadingUpdate ? L"Downloading update..." : L"Checking for updates...") : L"Check for updates";
    SetWindowTextW(button, tr(text));
    EnableWindow(button, !updateClient.busy());
}
void beginUpdateCheck() {
    if (updateClient.busy()) return;
    downloadingUpdate = false;
    try { updateClient.check(window, UpdateFinished); }
    catch (...) { error(magickey::updates::failureMessage(magickey::updates::Failure::Network)); }
    updateUpdateButton();
}
void refresh();
void finishUpdate() {
    using namespace magickey::updates;
    auto result = updateClient.take();
    updateUpdateButton();
    if (closeAfterUpdate) {
        discardStaging(result.staging);
        DestroyWindow(window);
        return;
    }
    switch (result.outcome) {
        case Outcome::Current:
            MessageBoxW(window, tr(L"You are using the latest version of MagicKey."), L"MagicKey", MB_OK | MB_ICONINFORMATION);
            break;
        case Outcome::Available: {
            auto prompt = std::wstring(tr(L"A new version is available: ")) + wide(result.release.tag) +
                tr(L"\n\nDownload and install it now? MagicKey will restart after downloading. Your profiles and language settings will be kept.");
            if (MessageBoxW(window, prompt.c_str(), tr(L"MagicKey update"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
                downloadingUpdate = true;
                try { updateClient.download(window, UpdateFinished, std::move(result.release)); }
                catch (...) { error(failureMessage(Failure::Network)); }
                updateUpdateButton();
            }
            break;
        }
        case Outcome::Downloaded:
            cancelCapture(); enabled = false; syncEngine();
            try { launchInstaller(result, settings.language); DestroyWindow(window); }
            catch (...) { discardStaging(result.staging); refresh(); error(failureMessage(Failure::Install)); }
            break;
        case Outcome::Failed: error(failureMessage(result.failure)); break;
        case Outcome::Cancelled: break;
    }
}
void refresh() {
    for (const auto& entry : localizedControls) SetWindowTextW(entry.first, tr(entry.second.c_str()));
    SendMessageW(languageBox, CB_SETCURSEL, settings.language, 0);
    SendMessageW(scanBox, BM_SETCHECK, settings.scanCodes ? BST_CHECKED : BST_UNCHECKED, 0);
    refreshThemes();
    updateCaptureButtons();
    LVCOLUMNW column{}; column.mask = LVCF_TEXT;
    column.pszText = const_cast<wchar_t*>(tr(L"When you press"));
    ListView_SetColumn(mappingList, 0, &column);
    column.pszText = const_cast<wchar_t*>(tr(L"Send instead"));
    ListView_SetColumn(mappingList, 1, &column);
    SendMessageW(profileBox, CB_RESETCONTENT, 0, 0);
    for (const auto& p : settings.profiles) {
        auto label = wide(p.name);
        SendMessageW(profileBox, CB_ADDSTRING, 0, LPARAM(label.c_str()));
    }
    SendMessageW(profileBox, CB_SETCURSEL, settings.selected, 0);
    const auto& p = settings.profiles[settings.selected];
    SetWindowTextW(nameEdit, wide(p.name).c_str());
    SendMessageW(blockBox, BM_SETCHECK, p.blockWindows ? BST_CHECKED : BST_UNCHECKED, 0);
    ListView_DeleteAllItems(mappingList);
    for (auto [from, to] : p.mappings) {
        auto source = keyName(from), target = keyName(to);
        LVITEMW item{};
        item.mask = LVIF_TEXT | LVIF_PARAM;
        item.iItem = ListView_GetItemCount(mappingList);
        item.pszText = source.data(); item.lParam = from;
        int row = ListView_InsertItem(mappingList, &item);
        ListView_SetItemText(mappingList, row, 1, target.data());
    }
    SetWindowTextW(toggleButton, tr(enabled ? L"Pause remapping" : L"Enable remapping"));
    SetWindowTextW(statusLabel, tr(enabled ? L"Remapping active" : L"Remapping paused"));
    EnableWindow(GetDlgItem(window, DeleteProfile), settings.profiles.size() > 1);
    InvalidateRect(window, nullptr, FALSE);
    updateUpdateButton();
}
std::string enteredName() {
    wchar_t buffer[81]{};
    GetWindowTextW(nameEdit, buffer, 81);
    std::wstring name(buffer);
    auto first = name.find_first_not_of(L" \t\r\n");
    if (first == std::wstring::npos) return {};
    name = name.substr(first, name.find_last_not_of(L" \t\r\n") - first + 1);
    for (wchar_t c : name) if (c < 32) return {};
    return utf8(name);
}
void makeInterface() {
    control(L"STATIC", L"Language", 0, 0, 592, 13, 130, 20);
    languageBox = control(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, LanguageBox, 592, 38, 130, 220);
    for (auto name : {L"English", L"简体中文", L"日本語", L"한국어"})
        SendMessageW(languageBox, CB_ADDSTRING, 0, LPARAM(name));
    control(L"STATIC", L"Theme", 0, 0, 738, 13, 138, 20);
    themeBox = control(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_TABSTOP, ThemeBox, 738, 38, 138, 220);
    control(L"STATIC", L"PROFILE", 0, SectionProfile, 44, 138, 206, 20);
    profileBox = control(WC_COMBOBOXW, L"", CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP, ProfileBox, 44, 166, 206, 300);
    control(L"STATIC", L"Profile name", 0, 0, 44, 211, 206, 20);
    nameEdit = control(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP, ProfileName, 44, 236, 206, 32);
    SendMessageW(nameEdit, EM_SETLIMITTEXT, 80, 0);
    control(L"BUTTON", L"New", WS_TABSTOP, NewProfile, 44, 280, 98, 32);
    control(L"BUTTON", L"Rename", WS_TABSTOP, RenameProfile, 152, 280, 98, 32);
    control(L"BUTTON", L"Delete", WS_TABSTOP, DeleteProfile, 44, 322, 206, 32);
    control(L"STATIC", L"New profiles include:\nQ  \x2192  Numpad 7\nW  \x2192  Numpad 8\nR  \x2192  Numpad 5", 0, 0, 44, 393, 206, 86);
    control(L"STATIC", L"KEY MAPPINGS", 0, SectionMappings, 310, 138, 300, 20);
    control(L"BUTTON", L"Remove", WS_TABSTOP, RemoveMapping, 766, 133, 88, 30);
    mappingList = control(WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | WS_TABSTOP,
        MappingList, 310, 176, 544, 176);
    ListView_SetExtendedListViewStyle(mappingList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    rowImages = ImageList_Create(1, 36, ILC_COLOR32, 1, 0);
    ListView_SetImageList(mappingList, rowImages, LVSIL_SMALL);
    LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<wchar_t*>(L"When you press"); column.cx = 252;
    ListView_InsertColumn(mappingList, 0, &column);
    column.pszText = const_cast<wchar_t*>(L"Send instead"); column.cx = 292;
    ListView_InsertColumn(mappingList, 1, &column);
    control(L"STATIC", L"From key", 0, 0, 310, 365, 190, 20);
    control(L"STATIC", L"To key", 0, 0, 522, 365, 190, 20);
    sourceBox = control(L"BUTTON", L"", WS_TABSTOP, SourceBox, 310, 390, 190, 40);
    targetBox = control(L"BUTTON", L"", WS_TABSTOP, TargetBox, 522, 390, 190, 40);
    control(L"BUTTON", L"Disabled", WS_TABSTOP, DisableTarget, 730, 390, 124, 40);
    control(L"BUTTON", L"Add / update", WS_TABSTOP, AddMapping, 730, 441, 124, 30);
    control(L"STATIC", L"Click a key button, then press and release a key. Click again to cancel.", 0, 0, 310, 441, 398, 34);
    blockBox = control(L"BUTTON", L"Disable both Windows keys in this profile", BS_AUTOCHECKBOX | WS_TABSTOP,
        BlockWindows, 310, 508, 544, 30);
    scanBox = control(L"BUTTON", L"Game-compatible input (scan codes)", BS_AUTOCHECKBOX | WS_TABSTOP,
        ScanCodes, 310, 546, 544, 30);
    statusLabel = control(L"STATIC", L"", 0, 0, 58, 626, 394, 22);
    SendMessageW(statusLabel, WM_SETFONT, WPARAM(font), TRUE);
    control(L"BUTTON", L"Check for updates", WS_TABSTOP, CheckUpdates, 474, 620, 184, 32);
    toggleButton = control(L"BUTTON", L"", BS_DEFPUSHBUTTON | WS_TABSTOP, Toggle, 676, 620, 178, 32);
    for (HWND box : {profileBox, languageBox, themeBox}) SendMessageW(box, CB_SETITEMHEIGHT, WPARAM(-1), 28);
    themeUI.apply(window, mappingList, themeUI.theme());
    refresh();
}
void command(int id, int notification) {
    if (id == SourceBox || id == TargetBox) {
        if (notification != BN_CLICKED) return;
        auto slot = id == SourceBox ? CaptureSlot::Source : CaptureSlot::Target;
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            ++captureGeneration;
            if (capture.activeSlot() == slot) capture.cancel();
            else { engine.release(sendKey); capture.arm(slot); }
        }
        updateCaptureButtons();
        return;
    }
    if (id == ProfileName || (notification != BN_CLICKED && notification != CBN_SELCHANGE)) return;
    cancelCapture();
    if (id == ThemeBox && notification == CBN_SELCHANGE) { changeTheme(int(SendMessageW(themeBox, CB_GETCURSEL, 0, 0))); return; }
    if (id == CheckUpdates) { beginUpdateCheck(); return; }
    if (id == DisableTarget) { targetChoice = 0; updateCaptureButtons(); return; }
    auto candidate = settings;
    auto& p = candidate.profiles[candidate.selected];
    if (id == Toggle) {
        enabled = !enabled;
        { std::lock_guard<std::mutex> lock(engineMutex); inputFailed = false; }
        syncEngine(); refresh(); return;
    }
    if (id == ProfileBox && notification == CBN_SELCHANGE) {
        auto i = SendMessageW(profileBox, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR) return;
        candidate.selected = std::size_t(i);
    } else if (id == LanguageBox && notification == CBN_SELCHANGE) {
        auto i = SendMessageW(languageBox, CB_GETCURSEL, 0, 0);
        if (i == CB_ERR) return;
        candidate.language = int(i);
    } else if (id == ScanCodes) {
        candidate.scanCodes = SendMessageW(scanBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    } else if (id == NewProfile || id == RenameProfile) {
        auto name = enteredName();
        if (name.empty()) { error(L"Enter a profile name (up to 80 characters)."); return; }
        if (candidate.profiles.size() >= 100 && id == NewProfile) { error(L"You can save up to 100 profiles."); return; }
        for (std::size_t i = 0; i < candidate.profiles.size(); ++i)
            if (candidate.profiles[i].name == name && (id == NewProfile || i != candidate.selected)) {
                error(L"That profile name already exists. Enter a different name first."); return;
            }
        if (id == RenameProfile) p.name = name;
        else { candidate.profiles.push_back(newProfile(name)); candidate.selected = candidate.profiles.size() - 1; }
    } else if (id == DeleteProfile) {
        if (candidate.profiles.size() == 1) return;
        if (MessageBoxW(window, tr(L"Delete this profile and its mappings?"), tr(L"Delete profile"), MB_YESNO | MB_ICONQUESTION) != IDYES) return;
        candidate.profiles.erase(candidate.profiles.begin() + candidate.selected);
        candidate.selected = std::min(candidate.selected, candidate.profiles.size() - 1);
    } else if (id == BlockWindows) {
        p.blockWindows = SendMessageW(blockBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
    } else if (id == AddMapping) {
        int source = sourceChoice, target = targetChoice;
        if (source < 0 || target < 0) { error(L"Select a source key and a destination."); return; }
        if (source == target) { error(L"Choose a different destination, or remove the mapping to restore the key."); return; }
        p.mappings[source] = target;
    } else if (id == RemoveMapping) {
        int row = ListView_GetNextItem(mappingList, -1, LVNI_SELECTED);
        if (row == -1) { error(L"Select a mapping in the list to remove it."); return; }
        LVITEMW item{}; item.mask = LVIF_PARAM; item.iItem = row;
        ListView_GetItem(mappingList, &item);
        p.mappings.erase(int(item.lParam));
    } else return;
    commit(std::move(candidate));
    refresh();
}
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
        case WM_CREATE: window = hwnd; makeInterface(); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            RECT bounds; GetClientRect(hwnd, &bounds);
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, bounds.right, bounds.bottom);
            auto old = SelectObject(memory, bitmap);
            themeUI.paint(memory, bounds, enabled, settings.language);
            BitBlt(dc, 0, 0, bounds.right, bounds.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory);
            EndPaint(hwnd, &paint); return 0;
        }
        case WM_PRINTCLIENT: {
            RECT bounds; GetClientRect(hwnd, &bounds);
            themeUI.paint(reinterpret_cast<HDC>(w), bounds, enabled, settings.language);
            return 0;
        }
        case WM_MEASUREITEM: {
            auto item = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (item->CtlType == ODT_COMBOBOX) { item->itemHeight = 28; return TRUE; }
            break;
        }
        case WM_DRAWITEM: {
            auto item = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (item->CtlType == ODT_COMBOBOX) { themeUI.combo(item); return TRUE; }
            break;
        }
        case WM_COMMAND: command(LOWORD(w), HIWORD(w)); return 0;
        case UpdateFinished: finishUpdate(); return 0;
        case WM_NOTIFY: {
            auto header = reinterpret_cast<NMHDR*>(l);
            if (header->code == NM_CUSTOMDRAW) {
                if (header->hwndFrom == mappingList) return themeUI.list(reinterpret_cast<NMLVCUSTOMDRAW*>(l));
                if (header->hwndFrom == ListView_GetHeader(mappingList)) return themeUI.header(reinterpret_cast<NMCUSTOMDRAW*>(l));
                auto role = magickey::ButtonRole::Secondary;
                if (header->idFrom == Toggle || header->idFrom == AddMapping) role = magickey::ButtonRole::Primary;
                if (header->idFrom == DeleteProfile) role = magickey::ButtonRole::Danger;
                if (header->idFrom == SourceBox || header->idFrom == TargetBox) role = magickey::ButtonRole::Key;
                if (header->idFrom == BlockWindows || header->idFrom == ScanCodes) role = magickey::ButtonRole::Checkbox;
                wchar_t kind[32]{}; GetClassNameW(header->hwndFrom, kind, 32);
                if (_wcsicmp(kind, L"Button") != 0) return CDRF_DODEFAULT;
                CaptureSlot slot;
                { std::lock_guard<std::mutex> lock(engineMutex); slot = capture.activeSlot(); }
                const bool listening = (header->idFrom == SourceBox && slot == CaptureSlot::Source) ||
                    (header->idFrom == TargetBox && slot == CaptureSlot::Target);
                return themeUI.button(reinterpret_cast<NMCUSTOMDRAW*>(l), role, listening);
            }
            if (header->idFrom == MappingList && header->code == LVN_GETEMPTYMARKUP) {
                auto empty = reinterpret_cast<NMLVEMPTYMARKUP*>(l);
                empty->dwFlags = EMF_CENTERED;
                wcsncpy_s(empty->szMarkup, std::size(empty->szMarkup), tr(L"No mappings yet. Capture two keys to add one."), _TRUNCATE);
                return TRUE;
            }
            if (header->idFrom == MappingList && header->code == LVN_ITEMCHANGED) {
                auto change = reinterpret_cast<NMLISTVIEW*>(l);
                if (change->uNewState & LVIS_SELECTED) {
                    cancelCapture();
                    int key = int(change->lParam);
                    sourceChoice = key;
                    const auto& mappings = settings.profiles[settings.selected].mappings;
                    auto found = mappings.find(key);
                    if (found != mappings.end()) targetChoice = found->second;
                    updateCaptureButtons();
                }
            }
            return 0;
        }
        case CapturedKey: {
            bool current;
            { std::lock_guard<std::mutex> lock(engineMutex); current = unsigned(w) == captureGeneration; }
            if (current && GetForegroundWindow() == window) {
                if (CaptureSlot(HIWORD(l)) == CaptureSlot::Source) sourceChoice = LOWORD(l);
                else targetChoice = LOWORD(l);
                updateCaptureButtons();
            }
            return 0;
        }
        case WM_ACTIVATE:
            if (LOWORD(w) == WA_INACTIVE) cancelCapture();
            if (LOWORD(w) != WA_INACTIVE) { std::lock_guard<std::mutex> lock(engineMutex); engine.release(sendKey); }
            return 0;
        case PauseMessage: cancelCapture(); enabled = false; syncEngine(); refresh(); return 0;
        case InputError:
            enabled = false; syncEngine(); refresh();
            error(L"Windows could not send a remapped key. Remapping is paused. If the target app runs as administrator, run MagicKey at the same privilege level.");
            return 0;
        case WM_CTLCOLORSTATIC: {
            const auto& p = themeUI.colors();
            HWND child = reinterpret_cast<HWND>(l);
            auto ink = child == statusLabel ? (enabled ? p.success : p.text) : p.muted;
            int id = GetDlgCtrlID(child);
            if (id == SectionProfile || id == SectionMappings) ink = p.accent;
            SetTextColor(reinterpret_cast<HDC>(w), magickey::color(ink));
            SetBkMode(reinterpret_cast<HDC>(w), TRANSPARENT);
            return LRESULT(themeUI.staticBackground(child, hwnd));
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(w), magickey::color(themeUI.colors().text));
            SetBkColor(reinterpret_cast<HDC>(w), magickey::color(themeUI.colors().control));
            return LRESULT(themeUI.input());
        case WM_CLOSE:
            if (updateClient.busy()) {
                cancelCapture(); enabled = false; syncEngine();
                closeAfterUpdate = true; updateClient.cancel(); updateUpdateButton();
            } else DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            cancelCapture();
            enabled = false; syncEngine();
            if (hookThreadId) PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
            PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    if (auto helperResult = magickey::updates::handleUpdaterArguments()) return *helperResult;
    HANDLE single = CreateMutexW(nullptr, FALSE, L"Local\\mywarkey.Portable.SingleInstance");
    if (!single) return 1;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND existing = FindWindowW(ClassName, nullptr);
        if (existing) { ShowWindow(existing, SW_RESTORE); SetForegroundWindow(existing); }
        CloseHandle(single); return 0;
    }
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES};
    InitCommonControlsEx(&controls);
    initKeys();
    wchar_t executable[32768]{};
    DWORD length = GetModuleFileNameW(nullptr, executable, DWORD(std::size(executable)));
    if (!length || length >= std::size(executable)) { CloseHandle(single); return 1; }
    settingsPath = std::filesystem::path(executable).parent_path() / L"mywarkey.profiles";
    if (std::filesystem::exists(settingsPath)) {
        try {
            std::ifstream in(settingsPath, std::ios::binary);
            auto loaded = readSettings(in);
            for (const auto& p : loaded.profiles) (void)wide(p.name);
            settings = std::move(loaded);
        } catch (const std::exception& e) {
            // Preserve the user's original; never silently overwrite damaged profiles.
            MessageBoxW(nullptr, (std::wstring(tr(L"Cannot load mywarkey.profiles: ")) + tr(wide(e.what()).c_str()) +
                tr(L"\n\nThe file has not been changed. Restore it from a backup or rename it to start fresh.")).c_str(),
                L"MagicKey", MB_OK | MB_ICONERROR);
            CloseHandle(single); return 1;
        }
    }
    liveProfile = settings.profiles[settings.selected];
    liveScanCodes = settings.scanCodes;
    bool invalidTheme = false;
    themePath = std::filesystem::path(executable).parent_path() / L"MagicKey.theme";
    auto theme = magickey::ThemeId::Ivory;
    if (std::filesystem::exists(themePath)) {
        try { std::ifstream input(themePath, std::ios::binary); theme = magickey::readTheme(input); }
        catch (...) { invalidTheme = true; }
    }
    themeUI.initialize(theme);
    font = themeUI.body();
    WNDCLASSW wc{};
    wc.lpfnWndProc = windowProc; wc.hInstance = instance; wc.lpszClassName = ClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hbrBackground = nullptr;
    RegisterClassW(&wc);
    RECT bounds{0, 0, 900, 680};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    AdjustWindowRect(&bounds, style, FALSE);
    window = CreateWindowExW(WS_EX_CONTROLPARENT, ClassName, L"MagicKey", style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    if (!window) { CloseHandle(single); return 1; }
    hookReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (hookReady) hookThread = CreateThread(nullptr, 0, keyboardThread, nullptr, 0, &hookThreadId);
    if (!hookReady || !hookThread || WaitForSingleObject(hookReady, 5000) != WAIT_OBJECT_0 || !hookInstalled) {
        error(L"The keyboard hook could not start. Close MagicKey and try again.");
        DestroyWindow(window);
    } else {
        ShowWindow(window, show); UpdateWindow(window);
        if (invalidTheme) error(L"The saved theme could not be loaded. Ivory is being used. Your profiles are unchanged.");
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (hookThread) { WaitForSingleObject(hookThread, 5000); CloseHandle(hookThread); }
    if (hookReady) CloseHandle(hookReady);
    if (rowImages) ImageList_Destroy(rowImages);
    CloseHandle(single);
    return 0;
}
