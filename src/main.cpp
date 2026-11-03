#include <windows.h>
#include <commctrl.h>
#include <algorithm>
#include <iterator>
#include <mutex>
#include <atomic>
#include "core.hpp"
#include "key_output.hpp"
#include "localization.hpp"
#include "key_capture.hpp"
#include "toggle_hotkey.hpp"
#include "profile_hotkeys.hpp"
#include "game_notifications.hpp"
#include "updater.hpp"
#include "theme_ui.hpp"
#include "app_identity.hpp"

using namespace mywarkey;
constexpr UINT ToggleMessage = WM_APP + 1, InputError = WM_APP + 2, CapturedKey = WM_APP + 3, UpdateFinished = WM_APP + 4, ProfileSwitch = WM_APP + 5, QuickChatSend = WM_APP + 6;
enum ControlId { ProfileBox = 101, NewProfile, RenameProfile, DeleteProfile, ProfileName,
    MappingList, SourceBox, TargetBox, AddMapping, RemoveMapping, BlockWindows, Toggle, LanguageBox, ScanCodes, DisableTarget, CheckUpdates, ThemeBox, SectionProfile, SectionMappings, NewMapping, QuickChatOpen };
constexpr int NumpadBase = 300;
constexpr int ComboList = 410, ComboFirst = 411, ComboSecond = 412, ComboThird = 413, ComboTarget = 414, ComboSave = 415, ComboNew = 416, ComboRemove = 417, ComboMode = 418;
CombinationMode comboMode = CombinationMode::Together;
HWND comboModeBox, comboHint;
HWND comboList;
std::array<HWND,4> comboButtons{};
std::array<int,3> comboSources{'A','B',0};
int comboDestination = 'D', comboSelection = -1, comboCapture = -1;
int quickEditingSlot = 0;
bool quickLoading = false, quickDirty = false;
HWND inputForeground = nullptr;
constexpr wchar_t ClassName[] = L"mywarkey.Portable.Window";
HWND window, profileBox, nameEdit, mappingList, sourceBox, targetBox, toggleButton, statusLabel, scanBox;
std::vector<std::pair<HWND, std::wstring>> localizedControls;
HFONT font;
magickey::ThemeUI themeUI;
constexpr int LanguageMenu = 2000, ThemeMenu = 2010, NotificationDiagnostics = 2020;
HIMAGELIST rowImages;
std::array<HWND, 6> numpadButtons{};
int numpadEditing = -1, editingSource = -1;
bool refreshingMappings = false;
std::string editorProfile;
std::array<bool, 10> liveQuickAssigned{};
HWND quickChatWindow = nullptr, quickSlotBox = nullptr, quickTextEdit = nullptr;
void openQuickChat();
bool editorActive() {
    HWND foreground = GetForegroundWindow();
    return foreground == window || (foreground && GetAncestor(foreground, GA_ROOTOWNER) == window);
}
HANDLE hookThread, hookReady;
DWORD hookThreadId;
bool hookInstalled = false;
Settings settings;
std::filesystem::path settingsPath;
std::filesystem::path themePath;
bool enabled = true;
bool inputFailed = false;
std::mutex engineMutex;
Remapper engine;
KeyCapture capture;
unsigned captureGeneration = 0;
int sourceChoice = VK_NUMPAD7, targetChoice = 'Q';
Profile liveProfile;
bool liveEnabled = false;
bool liveScanCodes = true;
std::size_t liveProfileCount = 1;
GameNotifications gameNotifications;
std::atomic_bool notificationTyping{false}, cancelNotification{false};
constexpr UINT_PTR NotificationTimer = 77;
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
    liveProfile.blockWindows = settings.blockWindows;
    for (int i = 0; i < 10; ++i) liveQuickAssigned[i] = !settings.quickChat[i].empty();
    liveEnabled = enabled;
    liveScanCodes = settings.scanCodes;
    liveProfileCount = settings.profiles.size();
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
    static ToggleHotkey toggleHotkey;
    static ProfileHotkeys profileHotkeys;
    static ProfileHotkeys quickChatHotkeys(false);
    bool down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
    int source = sourceKey(key);
    if (source < 1 || source > 254) return CallNextHookEx(nullptr, code, message, value);
    if (down && notificationTyping.load()) cancelNotification = true;
    bool wasDown = physical[source];
    physical[source] = down;
    auto shortcut = toggleHotkey.event(source, down, wasDown,
        physical[VK_LCONTROL] || physical[VK_RCONTROL], physical[VK_LMENU] || physical[VK_RMENU]);
    if (shortcut.swallow) {
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            if (shortcut.toggle) {
                inputFailed = false;
                capture.cancel();
                ++captureGeneration;
                engine.release(sendKey);
            }
            if (!down) capture.event(source, false, wasDown);
        }
        if (shortcut.toggle) PostMessageW(window, ToggleMessage, 0, LPARAM(GetForegroundWindow()));
        return 1;
    }
    std::size_t profileCount;
    { std::lock_guard<std::mutex> lock(engineMutex); profileCount = liveProfileCount; }
    auto profileKey = profileHotkeys.event(source, down, wasDown,
        physical[VK_LMENU], physical[VK_LCONTROL] || physical[VK_RCONTROL],
        physical[VK_LSHIFT] || physical[VK_RSHIFT], profileCount);
    if (profileKey.swallow) {
        {
            std::lock_guard<std::mutex> lock(engineMutex);
            if (profileKey.index >= 0) { capture.cancel(); ++captureGeneration; engine.release(sendKey); }
            if (!down) capture.event(source, false, wasDown);
        }
        if (profileKey.index >= 0) PostMessageW(window, ProfileSwitch, profileKey.index, LPARAM(GetForegroundWindow()));
        return 1;
    }
    int digit = profileDigit(source);
    bool assigned = false;
    if (digit >= 0) { std::lock_guard<std::mutex> lock(engineMutex); assigned = liveQuickAssigned[digit]; }
    bool eligible = assigned && physical[VK_LMENU] && !physical[VK_LCONTROL] && !physical[VK_RCONTROL] &&
        !physical[VK_LSHIFT] && !physical[VK_RSHIFT];
    if (eligible && down && !wasDown) eligible = isWarcraftForeground(GetForegroundWindow());
    auto chatKey = quickChatHotkeys.event(source, down, wasDown, physical[VK_LMENU] || physical[VK_RMENU],
        physical[VK_LCONTROL] || physical[VK_RCONTROL], physical[VK_LSHIFT] || physical[VK_RSHIFT], eligible ? 10 : 0);
    if (chatKey.swallow) {
        { std::lock_guard<std::mutex> lock(engineMutex); if (chatKey.index >= 0) { engine.release(sendKey); capture.cancel(); ++captureGeneration; } else if (!down) capture.event(source, false, wasDown); }
        if (chatKey.index >= 0) PostMessageW(window, QuickChatSend, chatKey.index, LPARAM(GetForegroundWindow()));
        return 1;
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
        HWND focused = GetForegroundWindow();
        if (focused != inputForeground) { engine.release(sendKey); inputForeground = focused; }
        // Keep the profile editor usable even when a profile remaps typing keys.
        swallowed = engine.event(source, down, liveProfile,
            liveEnabled && !notificationTyping.load() && !editorActive(), sendKey, GetTickCount64());
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
    if (std::wstring_view(type) == L"BUTTON" && (style & BS_TYPEMASK) != BS_AUTOCHECKBOX) style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
    if (std::wstring_view(type) == WC_COMBOBOXW) style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
    HWND child = CreateWindowExW(0, type, tr(label), WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, window, reinterpret_cast<HMENU>(INT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
    SendMessageW(child, WM_SETFONT, WPARAM(std::wstring_view(type) == L"STATIC" ? themeUI.smallTextFont() : font), TRUE);
    if (std::wstring_view(type) == WC_COMBOBOXW) themeUI.prepareCombo(child);
    if (std::wstring_view(type) == L"BUTTON") themeUI.prepareButton(child);
    if (*label && (std::wstring_view(type) == L"STATIC" || std::wstring_view(type) == L"BUTTON"))
        localizedControls.emplace_back(child, label);
    return child;
}
void refreshMenus() {
    HMENU bar = CreateMenu(), options = CreatePopupMenu(), languages = CreatePopupMenu(), themes = CreatePopupMenu();
    int index = 0;
    for (auto name : {L"English", L"简体中文", L"日本語", L"한국어"}) {
        AppendMenuW(languages, MF_STRING | (index == settings.language ? MF_CHECKED : 0), LanguageMenu + index, name);
        ++index;
    }
    for (int i = 0; i < int(std::size(magickey::Themes)); ++i)
        AppendMenuW(themes, MF_STRING | (i == int(themeUI.theme()) ? MF_CHECKED : 0), ThemeMenu + i, tr(magickey::Themes[i].name));
    AppendMenuW(options, MF_POPUP, UINT_PTR(languages), tr(L"Language"));
    AppendMenuW(options, MF_POPUP, UINT_PTR(themes), tr(L"Theme"));
    AppendMenuW(options, MF_STRING | (settings.scanCodes ? MF_CHECKED : 0), ScanCodes, tr(L"Game-compatible input (scan codes)"));
    AppendMenuW(options, MF_STRING | (settings.blockWindows ? MF_CHECKED : 0), BlockWindows, tr(L"Disable Windows keys"));
    AppendMenuW(options, MF_STRING, QuickChatOpen, tr(L"Quick Chat"));
    AppendMenuW(options, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(options, MF_STRING, CheckUpdates, tr(L"Check for updates"));
    AppendMenuW(options, MF_STRING, NotificationDiagnostics, tr(L"Notification diagnostics"));
    AppendMenuW(bar, MF_POPUP, UINT_PTR(options), tr(L"Settings"));
    HMENU previous = GetMenu(window);
    SetMenu(window, bar); DrawMenuBar(window);
    if (previous) DestroyMenu(previous);
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
    refreshMenus();
}
void updateCaptureButtons() {
    CaptureSlot slot;
    { std::lock_guard<std::mutex> lock(engineMutex); slot = capture.activeSlot(); }
    auto source = slot == CaptureSlot::Source ? std::wstring(tr(L"Press a key...")) : keyName(sourceChoice);
    auto target = slot == CaptureSlot::Target && numpadEditing < 0 && comboCapture < 0 ? std::wstring(tr(L"Press a key...")) : keyName(targetChoice);
    SetWindowTextW(sourceBox, source.c_str());
    SetWindowTextW(targetBox, target.c_str());
    const auto& mappings = settings.profiles[settings.selected].mappings;
    SetWindowTextW(GetDlgItem(window, AddMapping), tr(editingSource >= 0 || mappings.count(sourceChoice) ? L"Update" : L"Add"));
    for (int i = 0; i < 4; ++i) {
        int value = i == 3 ? comboDestination : comboSources[i];
        auto label = comboCapture == i && slot != CaptureSlot::None ? std::wstring(tr(L"Press a key...")) : value ? keyName(value) : std::wstring(L"-");
        SetWindowTextW(comboButtons[i],label.c_str());
    }
    SendMessageW(comboModeBox,CB_RESETCONTENT,0,0);
    for (auto name : {L"Input chord",L"Together",L"Sequence"}) SendMessageW(comboModeBox,CB_ADDSTRING,0,LPARAM(tr(name)));
    SendMessageW(comboModeBox,CB_SETCURSEL,int(comboMode),0);
    SetWindowTextW(comboHint,tr(comboMode == CombinationMode::InputChord ? L"Press 2–3 keys within 150 ms. Right-click the third key to clear it." : L"Right key triggers the left keys. Right-click the third key to clear it."));
    InvalidateRect(window,nullptr,FALSE);
    SetWindowTextW(GetDlgItem(window,ComboSave),tr(comboSelection >= 0 ? L"Update" : L"Add"));
    for (int i = 0; i < 6; ++i) {
        auto found = std::find_if(mappings.begin(),mappings.end(),[i](const auto& entry) { return entry.second == CustomNumpadKeys[i]; });
        auto text = numpadEditing == i && slot == CaptureSlot::Target ? std::wstring(tr(L"Press a key...")) :
            found == mappings.end() ? std::wstring{} : keyName(found->first);
        SetWindowTextW(numpadButtons[i], text.c_str());
    }
}
void cancelCapture() {
    numpadEditing = -1; comboCapture = -1;
    { std::lock_guard<std::mutex> lock(engineMutex); capture.cancel(); ++captureGeneration; }
    updateCaptureButtons();
}
void updateUpdateButton() {
    const wchar_t* text = closeAfterUpdate ? L"Cancelling update..." : updateClient.busy() ?
        (downloadingUpdate ? L"Downloading update..." : L"Checking for updates...") : L"Check for updates";
    HMENU menu = GetSubMenu(GetMenu(window), 0);
    ModifyMenuW(menu, CheckUpdates, MF_BYCOMMAND | MF_STRING | (updateClient.busy() ? MF_GRAYED : MF_ENABLED), CheckUpdates, tr(text));
}
void beginUpdateCheck() {
    if (updateClient.busy()) return;
    downloadingUpdate = false;
    try { updateClient.check(window, UpdateFinished); }
    catch (...) { error(magickey::updates::failureMessage(magickey::updates::Failure::Network)); }
    updateUpdateButton();
}
void refresh();
bool saveQuickChat(bool clear = false, int nextSlot = -1);
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
            if (quickChatWindow && quickDirty && !saveQuickChat()) { discardStaging(result.staging); return; }
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
    refreshMenus();
    updateCaptureButtons();
    LVCOLUMNW column{}; column.mask = LVCF_TEXT;
    column.pszText = const_cast<wchar_t*>(tr(L"When you press"));
    ListView_SetColumn(mappingList, 0, &column);
    column.pszText = const_cast<wchar_t*>(tr(L"Send instead"));
    ListView_SetColumn(mappingList, 1, &column);
    SendMessageW(profileBox, CB_RESETCONTENT, 0, 0);
    for (std::size_t i = 0; i < settings.profiles.size(); ++i) {
        auto label = wide(settings.profiles[i].name);
        if (i < 10) label += L"  [" + profileShortcut(i) + L"]";
        SendMessageW(profileBox, CB_ADDSTRING, 0, LPARAM(label.c_str()));
    }
    SendMessageW(profileBox, CB_SETCURSEL, settings.selected, 0);
    const auto& p = settings.profiles[settings.selected];
    SetWindowTextW(nameEdit, wide(p.name).c_str());
    if (editorProfile != p.name) {
        comboSelection = -1; comboMode = CombinationMode::Together; comboSources = {'A','B',0}; comboDestination = 'D';
        if (!p.combinations.empty()) { comboSelection = 0; const auto& c = p.combinations[0]; comboMode = c.mode; comboSources = {c.keys[0],c.keys[1],c.keys.size() == 3 ? c.keys[2] : 0}; comboDestination = c.target; }
        editorProfile = p.name; editingSource = -1; sourceChoice = 'A'; targetChoice = 'Q';
        for (auto [from, to] : p.mappings) if (!customNumpadKey(to)) { editingSource = sourceChoice = from; targetChoice = to; break; }
    }
    if (editingSource >= 0 && !p.mappings.count(editingSource)) editingSource = -1;
    refreshingMappings = true;
    ListView_DeleteAllItems(mappingList);
    for (auto [from, to] : p.mappings) {
        if (customNumpadKey(to)) continue;
        auto source = keyName(from), target = keyName(to);
        LVITEMW item{};
        item.mask = LVIF_PARAM;
        item.iItem = ListView_GetItemCount(mappingList);
        item.lParam = from;
        int row = ListView_InsertItem(mappingList, &item);
        
        if (from == editingSource) ListView_SetItemState(mappingList, row, LVIS_SELECTED, LVIS_SELECTED);
    }
    SendMessageW(comboList,LB_RESETCONTENT,0,0);
    for (const auto& c : p.combinations) {
        std::wstring label;
        for (int key : c.keys) { if (!label.empty()) label += L" + "; label += keyName(key); }
        if (c.mode == CombinationMode::InputChord) label += L"  \x2192  " + keyName(c.target);
        else {
            label = keyName(c.target) + L"  \x2192  ";
            for (int key : c.keys) { if (label.back() != L' ') label += c.mode == CombinationMode::Together ? L" + " : L" \x2192 "; label += keyName(key); }
            label += c.mode == CombinationMode::Together ? L"  [" + std::wstring(tr(L"Together")) + L"]" : L"  [" + std::wstring(tr(L"Sequence")) + L"]";
        }
        SendMessageW(comboList,LB_ADDSTRING,0,LPARAM(label.c_str()));
    }
    SendMessageW(comboList,LB_SETCURSEL,comboSelection,0);
    refreshingMappings = false; updateCaptureButtons();
    RECT listBounds; GetClientRect(mappingList, &listBounds);
    ListView_SetColumnWidth(mappingList, 1, listBounds.right - ListView_GetColumnWidth(mappingList, 0));
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
    control(L"STATIC",L"Configuration Schemes",0,SectionProfile,44,130,230,20);
    profileBox = control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST | WS_VSCROLL | WS_TABSTOP,ProfileBox,44,156,230,300);
    control(L"STATIC",L"Profile name",0,0,286,130,240,20);
    nameEdit = control(L"EDIT",L"",WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,ProfileName,286,156,240,32);
    SendMessageW(nameEdit,EM_SETLIMITTEXT,80,0);
    control(L"BUTTON",L"New",WS_TABSTOP,NewProfile,540,156,86,32);
    control(L"BUTTON",L"Rename",WS_TABSTOP,RenameProfile,636,156,104,32);
    control(L"BUTTON",L"Delete",WS_TABSTOP,DeleteProfile,750,156,104,32);
    control(L"STATIC",L"Custom Numeric Keypad",0,0,44,230,256,20);
    for (int i = 0; i < 6; ++i) {
        int x = 44 + (i % 2)*132, y = 264 + (i/2)*56;
        auto digit = std::to_wstring(CustomNumpadKeys[i]-VK_NUMPAD0) + L" \x2192";
        control(L"STATIC",digit.c_str(),0,0,x,y+9,28,24); localizedControls.pop_back();
        numpadButtons[i] = control(L"BUTTON",L"",WS_TABSTOP,NumpadBase+i,x+32,y,92,40);
    }
    control(L"BUTTON",L"Quick Chat",WS_TABSTOP,QuickChatOpen,44,452,256,34);
    control(L"STATIC",L"Alt + 1–0: Quick Chat",0,0,44,494,256,20);
    control(L"STATIC",L"Key Mapping",0,SectionMappings,360,222,250,20);
    control(L"BUTTON",L"New",WS_TABSTOP,NewMapping,666,216,88,30);
    control(L"BUTTON",L"Remove",WS_TABSTOP,RemoveMapping,766,216,88,30);
    control(L"STATIC",L"When you press",0,0,372,250,190,20);
    control(L"STATIC",L"Send instead",0,0,584,250,250,20);
    mappingList = control(WC_LISTVIEWW,L"",LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_NOLABELWRAP | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_SHAREIMAGELISTS | WS_TABSTOP,MappingList,360,274,494,124);
    ListView_SetExtendedListViewStyle(mappingList,LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
    HWND tip = ListView_SetToolTips(mappingList,nullptr);
    if (tip) DestroyWindow(tip);
    ShowWindow(ListView_GetHeader(mappingList),SW_HIDE);
    rowImages = ImageList_Create(1,30,ILC_COLOR32,1,0);
    ListView_SetImageList(mappingList,rowImages,LVSIL_SMALL);
    LVCOLUMNW column{}; column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<wchar_t*>(L"When you press"); column.cx = 212; ListView_InsertColumn(mappingList,0,&column);
    column.pszText = const_cast<wchar_t*>(L"Send instead"); column.cx = 282; ListView_InsertColumn(mappingList,1,&column);
    sourceBox = control(L"BUTTON",L"",WS_TABSTOP,SourceBox,360,408,132,30);
    targetBox = control(L"BUTTON",L"",WS_TABSTOP,TargetBox,506,408,132,30);
    control(L"BUTTON",L"Disabled",WS_TABSTOP,DisableTarget,650,408,90,30);
    control(L"BUTTON",L"Add",WS_TABSTOP,AddMapping,750,408,104,30);
    control(L"STATIC",L"Click a key, then press its replacement.",0,0,360,442,494,16);
    control(L"STATIC",L"Combinations",0,0,360,484,106,20);
    comboModeBox = control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST | WS_TABSTOP,ComboMode,470,478,184,200);
    for (auto mode : {L"Input chord",L"Together",L"Sequence"}) SendMessageW(comboModeBox,CB_ADDSTRING,0,LPARAM(tr(mode)));
    SendMessageW(comboModeBox,CB_SETITEMHEIGHT,WPARAM(-1),28);
    control(L"BUTTON",L"New",WS_TABSTOP,ComboNew,666,478,88,28);
    control(L"BUTTON",L"Remove",WS_TABSTOP,ComboRemove,766,478,88,28);
    comboList = control(L"LISTBOX",L"",LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | WS_VSCROLL | WS_TABSTOP,ComboList,360,510,494,48);
    comboButtons[0] = control(L"BUTTON",L"",WS_TABSTOP,ComboFirst,360,570,82,30);
    comboButtons[1] = control(L"BUTTON",L"",WS_TABSTOP,ComboSecond,462,570,82,30);
    comboButtons[2] = control(L"BUTTON",L"",WS_TABSTOP,ComboThird,564,570,82,30);
    comboButtons[3] = control(L"BUTTON",L"",WS_TABSTOP,ComboTarget,672,570,74,30);
    control(L"BUTTON",L"Add",WS_TABSTOP,ComboSave,758,570,96,30);
    comboHint = control(L"STATIC",L"Press 2–3 keys within 150 ms. Right-click the third key to clear it.",0,0,360,610,494,30);
    statusLabel = control(L"STATIC",L"",0,0,58,692,590,22);
    SendMessageW(statusLabel,WM_SETFONT,WPARAM(font),TRUE);
    toggleButton = control(L"BUTTON",L"",WS_TABSTOP,Toggle,676,686,178,32);
    SendMessageW(profileBox,CB_SETITEMHEIGHT,WPARAM(-1),28);
    themeUI.apply(window,mappingList,themeUI.theme()); refresh();
}

void loadQuickChatSlot(bool loadText = true) {
    quickLoading = true;
    SendMessageW(quickSlotBox,CB_RESETCONTENT,0,0);
    for (int i = 0; i < 10; ++i) {
        auto label = std::wstring(L"Alt + ") + (i == 9 ? L'0' : wchar_t(L'1'+i)) + L" — ";
        auto text = wide(settings.quickChat[i]);
        label += text.empty() ? tr(L"Not assigned") : text.substr(0,28);
        SendMessageW(quickSlotBox,CB_ADDSTRING,0,LPARAM(label.c_str()));
    }
    SendMessageW(quickSlotBox,CB_SETCURSEL,quickEditingSlot,0);
    if (loadText) SetWindowTextW(quickTextEdit,wide(settings.quickChat[quickEditingSlot]).c_str());
    quickLoading = false;
}
bool saveQuickChat(bool clear, int nextSlot) {
    KillTimer(quickChatWindow,88);
    wchar_t value[121]{}; GetWindowTextW(quickTextEdit,value,121);
    auto candidate = settings;
    auto text = clear ? std::string{} : utf8(value);
    if (text.size() > 512) { SetWindowTextW(GetDlgItem(quickChatWindow,405),tr(L"Quick Chat message is too long.")); return false; }
    candidate.quickChat[quickEditingSlot] = text;
    candidate.quickChatSlot = nextSlot >= 0 ? nextSlot : quickEditingSlot;
    if (!commit(std::move(candidate))) { SetWindowTextW(GetDlgItem(quickChatWindow,405),tr(L"Not saved")); return false; }
    bool changed = nextSlot >= 0 && nextSlot != quickEditingSlot;
    quickEditingSlot = settings.quickChatSlot; quickDirty = false;
    loadQuickChatSlot(clear || changed);
    SetWindowTextW(GetDlgItem(quickChatWindow,405),tr(L"Saved"));
    refresh(); return true;
}
LRESULT CALLBACK quickChatProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    switch (message) {
        case WM_CREATE: {
            auto child = [hwnd](const wchar_t* type, const wchar_t* label, DWORD style, int id, int x, int y, int width, int height) {
                if (std::wstring_view(type) == L"BUTTON") style = (style & ~BS_TYPEMASK) | BS_OWNERDRAW;
                if (std::wstring_view(type) == WC_COMBOBOXW) style |= CBS_OWNERDRAWFIXED | CBS_HASSTRINGS;
                HWND item = CreateWindowExW(0, type, tr(label), WS_CHILD | WS_VISIBLE | style, x,y,width,height,hwnd,HMENU(INT_PTR(id)),GetModuleHandleW(nullptr),nullptr);
                SendMessageW(item, WM_SETFONT, WPARAM(font), TRUE);
                if (std::wstring_view(type) == L"BUTTON") themeUI.prepareButton(item);
                if (std::wstring_view(type) == WC_COMBOBOXW) themeUI.prepareCombo(item);
                return item;
            };
            child(L"STATIC", L"Quick Chat shortcut", 0, 0, 20,18,560,24);
            quickSlotBox = child(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST | WS_TABSTOP,401,20,48,560,300);
            quickEditingSlot = settings.quickChatSlot; quickDirty = false;
            child(L"STATIC", L"Message", 0, 0, 20,96,560,24);
            quickTextEdit = child(L"EDIT",L"",WS_BORDER | ES_AUTOHSCROLL | WS_TABSTOP,402,20,126,560,34);
            SendMessageW(quickTextEdit, EM_SETLIMITTEXT, 120, 0);
            child(L"STATIC", L"Saved messages send only while Warcraft is foreground. Use with chat closed.",0,0,20,177,560,48);
            child(L"STATIC",L"Saved",0,405,20,244,280,24);
            child(L"BUTTON", L"Save", WS_TABSTOP,403,320,240,120,34);
            child(L"BUTTON", L"Clear", WS_TABSTOP,404,460,240,120,34);
            SendMessageW(quickSlotBox, CB_SETITEMHEIGHT, WPARAM(-1), 28);
            loadQuickChatSlot(); themeUI.apply(hwnd, nullptr, themeUI.theme()); magickey::setWindowIdentity(hwnd); return 0;
        }
        case WM_COMMAND: {
            int id = LOWORD(w);
            if (id == 402 && HIWORD(w) == EN_CHANGE && !quickLoading) {
                quickDirty = true; SetWindowTextW(GetDlgItem(hwnd,405),tr(L"Saving..."));
                SetTimer(hwnd,88,350,nullptr); return 0;
            }
            if (id == 401 && HIWORD(w) == CBN_SELCHANGE && !quickLoading) {
                int next = int(SendMessageW(quickSlotBox,CB_GETCURSEL,0,0));
                if (next >= 0 && next < 10 && !saveQuickChat(false,next)) SendMessageW(quickSlotBox,CB_SETCURSEL,quickEditingSlot,0);
                return 0;
            }
            if (id == 403 || id == 404) { saveQuickChat(id == 404); return 0; }
            break;
        }
        case WM_TIMER:
            if (w == 88) { KillTimer(hwnd,88); if (quickDirty) saveQuickChat(); }
            return 0;
        case WM_MEASUREITEM: {
            auto item = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (item->CtlType == ODT_COMBOBOX) { item->itemHeight = 28; return TRUE; }
            break;
        }
        case WM_DRAWITEM: {
            auto item = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (item->CtlType == ODT_COMBOBOX) { themeUI.combo(item); return TRUE; }
            if (item->CtlType == ODT_BUTTON) { themeUI.ownerButton(item, item->CtlID == 403 ? magickey::ButtonRole::Primary : magickey::ButtonRole::Secondary, false); return TRUE; }
            break;
        }
        case WM_ERASEBKGND: { RECT bounds; GetClientRect(hwnd,&bounds); FillRect(reinterpret_cast<HDC>(w),&bounds,themeUI.panel()); return 1; }
        case WM_CTLCOLORSTATIC: SetTextColor(reinterpret_cast<HDC>(w),magickey::color(themeUI.colors().text)); SetBkMode(reinterpret_cast<HDC>(w),TRANSPARENT); return LRESULT(themeUI.panel());
        case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: SetTextColor(reinterpret_cast<HDC>(w),magickey::color(themeUI.colors().text)); SetBkColor(reinterpret_cast<HDC>(w),magickey::color(themeUI.colors().control)); return LRESULT(themeUI.input());
        case WM_CLOSE: if (!quickDirty || saveQuickChat()) DestroyWindow(hwnd); return 0;
        case WM_DESTROY: KillTimer(hwnd,88); magickey::clearWindowIdentity(hwnd); quickChatWindow = nullptr; quickSlotBox = quickTextEdit = nullptr; return 0;
    }
    return DefWindowProcW(hwnd,message,w,l);
}
void openQuickChat() {
    if (quickChatWindow) { ShowWindow(quickChatWindow,SW_RESTORE); SetForegroundWindow(quickChatWindow); return; }
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc); wc.lpfnWndProc = quickChatProc; wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"MagicKey.QuickChat"; wc.hCursor = LoadCursorW(nullptr,IDC_ARROW);
    wc.hIcon = magickey::appIcon(false); wc.hIconSm = magickey::appIcon(true); RegisterClassExW(&wc);
    RECT bounds{0,0,600,294}; DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN;
    AdjustWindowRect(&bounds,style,FALSE);
    quickChatWindow = CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,tr(L"Quick Chat"),style,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,window,nullptr,wc.hInstance,nullptr);
    ShowWindow(quickChatWindow,SW_SHOW); SetForegroundWindow(quickChatWindow);
}
void command(int id, int notification) {
    if (id == ComboMode && notification == CBN_SELCHANGE) {
        int selected = int(SendMessageW(comboModeBox,CB_GETCURSEL,0,0));
        if (selected < 0 || selected > 2) return;
        cancelCapture(); comboMode = CombinationMode(selected); updateCaptureButtons(); return;
    }
    if (id >= ComboFirst && id <= ComboTarget && notification == BN_CLICKED) {
        int field = id-ComboFirst; bool again = comboCapture == field;
        cancelCapture();
        if (!again) { comboCapture = field; std::lock_guard<std::mutex> lock(engineMutex); engine.release(sendKey); capture.arm(CaptureSlot::Target); ++captureGeneration; }
        updateCaptureButtons(); return;
    }
    if (id >= NumpadBase && id < NumpadBase + 6 && notification == BN_CLICKED) {
        int index = id - NumpadBase;
        bool again = numpadEditing == index;
        cancelCapture();
        if (!again) {
            numpadEditing = index;
            std::lock_guard<std::mutex> lock(engineMutex);
            engine.release(sendKey); capture.arm(CaptureSlot::Target); ++captureGeneration;
        }
        updateCaptureButtons(); return;
    }
    if (id == SourceBox || id == TargetBox) {
        numpadEditing = -1;
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
    if (id == ProfileName || (notification != BN_CLICKED && notification != CBN_SELCHANGE && notification != LBN_SELCHANGE)) return;
    cancelCapture();
    if (id >= ThemeMenu && id < ThemeMenu + 4) { changeTheme(id - ThemeMenu); return; }
    if (id == ComboList && notification == LBN_SELCHANGE) {
        int index = int(SendMessageW(comboList,LB_GETCURSEL,0,0));
        const auto& list = settings.profiles[settings.selected].combinations;
        if (index >= 0 && index < int(list.size())) { comboSelection = index; const auto& c = list[index]; comboMode = c.mode; comboSources = {c.keys[0],c.keys[1],c.keys.size()==3 ? c.keys[2] : 0}; comboDestination = c.target; updateCaptureButtons(); }
        return;
    }
    if (id == ComboNew) { comboSelection = -1; comboMode = CombinationMode::Together; comboSources = {'A','B',0}; comboDestination = 'D'; SendMessageW(comboList,LB_SETCURSEL,WPARAM(-1),0); updateCaptureButtons(); return; }
    if (id == ComboSave || id == ComboRemove) {
        auto candidate = settings; auto& list = candidate.profiles[candidate.selected].combinations;
        if (id == ComboRemove) {
            if (comboSelection < 0 || comboSelection >= int(list.size())) return;
            list.erase(list.begin()+comboSelection); comboSelection = -1;
        } else {
            try {
                std::vector<int> keys{comboSources[0],comboSources[1]}; if (comboSources[2]) keys.push_back(comboSources[2]);
                if (comboMode != CombinationMode::InputChord && comboDestination == 0) throw std::runtime_error("Choose a trigger key.");
                keys = combinationKeys(std::move(keys),comboMode);
                for (int i = 0; i < int(list.size()); ++i) if (i != comboSelection && (comboMode == CombinationMode::InputChord ? list[i].mode == comboMode && list[i].keys == keys : list[i].mode != CombinationMode::InputChord && list[i].target == comboDestination)) throw std::runtime_error("That combination already exists.");
                if (comboSelection < 0 && list.size() >= 100) throw std::runtime_error("Too many combinations.");
                if (comboSelection >= 0 && comboSelection < int(list.size())) list[comboSelection] = {keys,comboDestination,comboMode};
                else { list.push_back({keys,comboDestination,comboMode}); comboSelection = int(list.size())-1; }
            } catch (const std::exception& e) { error(wide(e.what())); return; }
        }
        commit(std::move(candidate)); refresh(); return;
    }
    if (id == QuickChatOpen) { openQuickChat(); return; }
    if (id == NewMapping) { editingSource = -1; sourceChoice = 'A'; targetChoice = 'Q';
        while (sourceChoice < 'Z' && settings.profiles[settings.selected].mappings.count(sourceChoice)) ++sourceChoice;
        refreshingMappings = true; ListView_SetItemState(mappingList, -1, 0, LVIS_SELECTED); refreshingMappings = false; updateCaptureButtons(); return; }
    if (id == NotificationDiagnostics) { MessageBoxW(window, gameNotifications.diagnostics().c_str(), tr(L"Notification diagnostics"), MB_OK | MB_ICONINFORMATION); return; }
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
    } else if (id >= LanguageMenu && id < LanguageMenu + 4) {
        candidate.language = id - LanguageMenu;
    } else if (id == ScanCodes) {
        candidate.scanCodes = !settings.scanCodes;
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
        candidate.blockWindows = !settings.blockWindows;
    } else if (id == AddMapping) {
        int source = sourceChoice, target = targetChoice;
        if (source < 0 || target < 0) { error(L"Select a source key and a destination."); return; }
        if (source == target) { error(L"Choose a different destination, or remove the mapping to restore the key."); return; }
        if (editingSource >= 0 && editingSource != source) p.mappings.erase(editingSource);
        p.mappings[source] = target;
    } else if (id == RemoveMapping) {
        int row = ListView_GetNextItem(mappingList, -1, LVNI_SELECTED);
        if (row == -1) { error(L"Select a mapping in the list to remove it."); return; }
        LVITEMW item{}; item.mask = LVIF_PARAM; item.iItem = row;
        ListView_GetItem(mappingList, &item);
        p.mappings.erase(int(item.lParam));
    } else return;
    if (commit(std::move(candidate)) && id == AddMapping) editingSource = sourceChoice;
    refresh();
}
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (message == taskbarCreated) { magickey::setWindowIdentity(hwnd); return 0; }
    switch (message) {
        case WM_APP+20: case WM_APP+21: {
            int key = int(w);
            if (message == WM_APP+21) { auto& mappings = settings.profiles[settings.selected].mappings; auto found = mappings.find(key); if (found != mappings.end()) key = found->second; }
            auto text = keyName(key); wcsncpy_s(reinterpret_cast<wchar_t*>(l),512,text.c_str(),_TRUNCATE); return 0;
        }
        case WM_CREATE: window = hwnd; makeInterface(); SetTimer(hwnd, NotificationTimer, 25, nullptr); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            RECT bounds; GetClientRect(hwnd, &bounds);
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, bounds.right, bounds.bottom);
            auto old = SelectObject(memory, bitmap);
            themeUI.paint(memory, bounds, enabled, settings.language, comboMode != CombinationMode::InputChord);
            BitBlt(dc, 0, 0, bounds.right, bounds.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory);
            EndPaint(hwnd, &paint); return 0;
        }
        case WM_PRINTCLIENT: {
            RECT bounds; GetClientRect(hwnd, &bounds);
            themeUI.paint(reinterpret_cast<HDC>(w), bounds, enabled, settings.language, comboMode != CombinationMode::InputChord);
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
            if (item->CtlType == ODT_BUTTON) {
                auto role = magickey::ButtonRole::Secondary;
                if (item->CtlID == Toggle || item->CtlID == AddMapping || item->CtlID == ComboSave) role = magickey::ButtonRole::Primary;
                if (item->CtlID == ScanCodes) role = magickey::ButtonRole::Checkbox;
                if (item->CtlID == SourceBox || item->CtlID == TargetBox || (item->CtlID >= NumpadBase && item->CtlID < NumpadBase + 6) || (item->CtlID >= ComboFirst && item->CtlID <= ComboTarget)) role = magickey::ButtonRole::Key;
                CaptureSlot slot;
                { std::lock_guard<std::mutex> lock(engineMutex); slot = capture.activeSlot(); }
                bool listening = (slot == CaptureSlot::Source && item->CtlID == SourceBox) ||
                    (slot == CaptureSlot::Target && ((numpadEditing < 0 && comboCapture < 0 && item->CtlID == TargetBox) || item->CtlID == NumpadBase + numpadEditing || (comboCapture >= 0 && item->CtlID == ComboFirst+comboCapture)));
                themeUI.ownerButton(item, role, listening, settings.scanCodes); return TRUE;
            }
            break;
        }
        case WM_CONTEXTMENU: {
            int id = GetDlgCtrlID(reinterpret_cast<HWND>(w));
            if (id == ComboThird) { cancelCapture(); comboSources[2] = 0; updateCaptureButtons(); return 0; }
            if (id >= NumpadBase && id < NumpadBase + 6) {
                cancelCapture();
                HMENU menu = CreatePopupMenu();
                
                AppendMenuW(menu, MF_STRING, 2, tr(L"Restore original key"));
                POINT at; GetCursorPos(&at);
                int choice = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, at.x, at.y, 0, hwnd, nullptr);
                DestroyMenu(menu);
                if (choice) {
                    auto candidate = settings; int key = CustomNumpadKeys[id - NumpadBase];
                    auto& mappings = candidate.profiles[candidate.selected].mappings;
                    for (auto i = mappings.begin(); i != mappings.end();) { if (i->second == key) i = mappings.erase(i); else ++i; }
                    commit(std::move(candidate)); refresh();
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND: command(LOWORD(w), HIWORD(w)); return 0;
        case UpdateFinished: finishUpdate(); return 0;
        case WM_NOTIFY: {
            auto header = reinterpret_cast<NMHDR*>(l);
            if (header->code == NM_CUSTOMDRAW) {
                if (header->hwndFrom == mappingList) return themeUI.list(reinterpret_cast<NMLVCUSTOMDRAW*>(l));
                if (header->hwndFrom == ListView_GetHeader(mappingList)) return themeUI.header(reinterpret_cast<NMCUSTOMDRAW*>(l));
                if (header->idFrom == ScanCodes) return themeUI.button(reinterpret_cast<NMCUSTOMDRAW*>(l), magickey::ButtonRole::Checkbox, false, settings.scanCodes);
                return CDRF_DODEFAULT;
            }
            if (header->idFrom == MappingList && header->code == LVN_GETEMPTYMARKUP) {
                auto empty = reinterpret_cast<NMLVEMPTYMARKUP*>(l);
                empty->dwFlags = EMF_CENTERED;
                wcsncpy_s(empty->szMarkup, std::size(empty->szMarkup), tr(L"No mappings yet. Capture two keys to add one."), _TRUNCATE);
                return TRUE;
            }
            if (header->idFrom == MappingList && header->code == LVN_ITEMCHANGED) {
                auto change = reinterpret_cast<NMLISTVIEW*>(l);
                if (!refreshingMappings && (change->uNewState & LVIS_SELECTED)) {
                    cancelCapture();
                    int key = int(change->lParam);
                    editingSource = sourceChoice = key;
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
            { std::lock_guard<std::mutex> lock(engineMutex); current = unsigned(w) == captureGeneration; if (current) capture.cancel(); }
            if (current && GetForegroundWindow() == window) {
                if (comboCapture >= 0) {
                    if (comboCapture == 3) comboDestination = LOWORD(l); else comboSources[comboCapture] = LOWORD(l);
                    comboCapture = -1; updateCaptureButtons(); return 0;
                }
                if (numpadEditing >= 0) {
                    int to = CustomNumpadKeys[numpadEditing], from = LOWORD(l);
                    numpadEditing = -1;
                    auto candidate = settings;
                    auto& mappings = candidate.profiles[candidate.selected].mappings;
                    for (auto i = mappings.begin(); i != mappings.end();) { if (i->second == to) i = mappings.erase(i); else ++i; }
                    if (from == to) mappings.erase(from);
                    else candidate.profiles[candidate.selected].mappings[from] = to;
                    commit(std::move(candidate)); refresh(); return 0;
                }
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
        case ToggleMessage:
            // Serialize toggles and profile changes on the UI thread so rapid
            // alternating shortcuts preserve event order and final state.
            enabled = !enabled;
            { std::lock_guard<std::mutex> lock(engineMutex); inputFailed = false; }
            syncEngine();
            gameNotifications.queue(reinterpret_cast<HWND>(l), toggleMessage(enabled, settings.language));
            cancelNotification = false; notificationTyping = gameNotifications.typing();
            refresh(); return 0;
        case QuickChatSend:
            if (w < 10 && !settings.quickChat[w].empty()) {
                gameNotifications.queue(reinterpret_cast<HWND>(l), wide(settings.quickChat[w]));
                cancelNotification = false; notificationTyping = gameNotifications.typing();
            }
            return 0;
        case ProfileSwitch: {
            if (w >= settings.profiles.size() || w >= 10) return 0;
            auto candidate = settings; candidate.selected = std::size_t(w);
            if (commit(std::move(candidate))) {
                enabled = true; syncEngine(); refresh();
                gameNotifications.queue(reinterpret_cast<HWND>(l), configurationMessage(wide(settings.profiles[settings.selected].name), settings.language));
                cancelNotification = false; notificationTyping = gameNotifications.typing();
            }
            return 0;
        }
        case WM_TIMER:
            if (w == NotificationTimer) {
                if (cancelNotification.exchange(false)) gameNotifications.cancel();
                if (gameNotifications.readyToStart()) {
                    notificationTyping = true;
                    std::lock_guard<std::mutex> lock(engineMutex);
                    engine.release(sendKey);
                }
                { std::lock_guard<std::mutex> lock(engineMutex); HWND focused = GetForegroundWindow(); if (focused != inputForeground) { engine.release(sendKey); inputForeground = focused; } engine.tick(GetTickCount64(),liveProfile,liveEnabled && !notificationTyping.load() && !editorActive(),sendKey); }
                gameNotifications.tick();
                notificationTyping = gameNotifications.typing();
            }
            return 0;
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
        case WM_CTLCOLORBTN:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX:
            SetTextColor(reinterpret_cast<HDC>(w), magickey::color(themeUI.colors().text));
            SetBkColor(reinterpret_cast<HDC>(w), magickey::color(themeUI.colors().control));
            return LRESULT(themeUI.input());
        case WM_CLOSE:
            if (quickChatWindow && quickDirty && !saveQuickChat()) return 0;
            if (updateClient.busy()) {
                cancelCapture(); enabled = false; syncEngine();
                closeAfterUpdate = true; updateClient.cancel(); updateUpdateButton();
            } else DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            magickey::clearWindowIdentity(hwnd);
            KillTimer(hwnd, NotificationTimer); gameNotifications.cancel(); notificationTyping = false;
            cancelCapture();
            enabled = false; syncEngine();
            if (hookThreadId) PostThreadMessageW(hookThreadId, WM_QUIT, 0, 0);
            PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, message, w, l);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    if (auto helperResult = magickey::updates::handleUpdaterArguments()) return *helperResult;
    magickey::initializeAppIdentity();
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
            for (const auto& text : loaded.quickChat) (void)wide(text);
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
    liveProfile.blockWindows = settings.blockWindows;
    for (int i = 0; i < 10; ++i) liveQuickAssigned[i] = !settings.quickChat[i].empty();
    liveScanCodes = settings.scanCodes;
    liveProfileCount = settings.profiles.size();
    bool invalidTheme = false;
    themePath = std::filesystem::path(executable).parent_path() / L"MagicKey.theme";
    auto theme = magickey::ThemeId::Ivory;
    if (std::filesystem::exists(themePath)) {
        try { std::ifstream input(themePath, std::ios::binary); theme = magickey::readTheme(input); }
        catch (...) { invalidTheme = true; }
    }
    themeUI.initialize(theme);
    font = themeUI.body();
    WNDCLASSEXW wc{}; wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = windowProc; wc.hInstance = instance; wc.lpszClassName = ClassName;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW); wc.hIcon = magickey::appIcon(false); wc.hIconSm = magickey::appIcon(true);
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);
    RECT bounds{0, 0, 900, 734};
    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    AdjustWindowRect(&bounds, style, TRUE);
    window = CreateWindowExW(WS_EX_CONTROLPARENT, ClassName, L"MagicKey", style, CW_USEDEFAULT, CW_USEDEFAULT,
        bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr, instance, nullptr);
    if (!window) { CloseHandle(single); return 1; }
    magickey::setWindowIdentity(window);
    hookReady = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (hookReady) hookThread = CreateThread(nullptr, 0, keyboardThread, nullptr, 0, &hookThreadId);
    if (!hookReady || !hookThread || WaitForSingleObject(hookReady, 5000) != WAIT_OBJECT_0 || !hookInstalled) {
        error(L"The keyboard hook could not start. Close MagicKey and try again.");
        DestroyWindow(window);
    } else {
        syncEngine(); refresh();
        ShowWindow(window, show); UpdateWindow(window);
        if (invalidTheme) error(L"The saved theme could not be loaded. Ivory is being used. Your profiles are unchanged.");
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!(quickChatWindow && IsDialogMessageW(quickChatWindow, &msg)) && !IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (hookThread) { WaitForSingleObject(hookThread, 5000); CloseHandle(hookThread); }
    if (hookReady) CloseHandle(hookReady);
    if (rowImages) ImageList_Destroy(rowImages);
    CloseHandle(single);
    return 0;
}
