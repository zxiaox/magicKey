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
constexpr int MapRowBase = 10000, ComboRowBase = 20000, MapScroll = 500, ComboScroll = 501;
struct MappingRow { int sent = -1, pressed = 0, original = 0; };
struct ComboRow { std::array<int,3> keys{}; int trigger = -1, original = -1; CombinationMode mode = CombinationMode::Together; };
std::vector<MappingRow> mappingRows;
std::vector<ComboRow> combinationRows;
std::vector<HWND> rowControls;
std::string rowsProfile;
int mapOffset = 0, comboOffset = 0, rowCapture = -1;
bool buildingRows = false;
void renderRows();
bool saveRow(int id);
int quickEditingSlot = 0;
bool quickLoading = false, quickDirty = false;
HWND inputForeground = nullptr;
constexpr wchar_t ClassName[] = L"mywarkey.Portable.Window";
HWND window, profileBox, nameEdit, toggleButton, statusLabel;
std::vector<std::pair<HWND, std::wstring>> localizedControls;
HFONT font;
magickey::ThemeUI themeUI;
constexpr int LanguageMenu = 2000, ThemeMenu = 2010, NotificationDiagnostics = 2020;
std::array<HWND, 6> numpadButtons{};
int numpadEditing = -1;
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
Profile liveProfile;
bool liveEnabled = false;
bool liveScanCodes = true;
std::size_t liveProfileCount = 1;
GameNotifications gameNotifications;
std::atomic_bool notificationTyping{false}, cancelNotification{false};
constexpr UINT_PTR NotificationTimer = 77;
magickey::updates::Client updateClient;
bool downloadingUpdate = false, closeAfterUpdate = false, silentUpdateCheck = false;

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
        themeUI.apply(window, nullptr, magickey::ThemeId(choice));
    } catch (...) { error(L"Could not save the theme. Check that the app folder is writable."); }
    refreshMenus();
}
void updateCaptureButtons() {
    CaptureSlot slot;
    { std::lock_guard<std::mutex> lock(engineMutex); slot = capture.activeSlot(); }
    const auto& mappings = settings.profiles[settings.selected].mappings;
    if (rowCapture >= 0) SetWindowTextW(GetDlgItem(window,rowCapture),tr(L"Press a key..."));
    for (int i = 0; i < 6; ++i) {
        auto found = std::find_if(mappings.begin(),mappings.end(),[i](const auto& entry) { return entry.second == CustomNumpadKeys[i]; });
        auto text = numpadEditing == i && slot == CaptureSlot::Target ? std::wstring(tr(L"Press a key...")) :
            found == mappings.end() ? std::wstring{} : keyName(found->first);
        SetWindowTextW(numpadButtons[i], text.c_str());
    }
}
void cancelCapture() {
    bool wasRow = rowCapture >= 0;
    numpadEditing = -1; rowCapture = -1;
    { std::lock_guard<std::mutex> lock(engineMutex); capture.cancel(); ++captureGeneration; }
    if (wasRow) renderRows();
    updateCaptureButtons();
}
void updateUpdateButton() {
    const wchar_t* text = closeAfterUpdate ? L"Cancelling update..." : updateClient.busy() ?
        (downloadingUpdate ? L"Downloading update..." : L"Checking for updates...") : L"Check for updates";
    HMENU menu = GetSubMenu(GetMenu(window), 0);
    ModifyMenuW(menu, CheckUpdates, MF_BYCOMMAND | MF_STRING | (updateClient.busy() ? MF_GRAYED : MF_ENABLED), CheckUpdates, tr(text));
}
void beginUpdateCheck(bool silent = false) {
    if (updateClient.busy()) return;
    downloadingUpdate = false; silentUpdateCheck = silent;
    try { updateClient.check(window, UpdateFinished); }
    catch (...) { if (!silent) error(magickey::updates::failureMessage(magickey::updates::Failure::Network)); }
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
            if (!silentUpdateCheck) MessageBoxW(window, tr(L"You are using the latest version of MagicKey."), L"MagicKey", MB_OK | MB_ICONINFORMATION);
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
        case Outcome::Failed: if (!silentUpdateCheck || downloadingUpdate) error(failureMessage(result.failure)); break;
        case Outcome::Cancelled: break;
    }
}
void refresh() {
    for (const auto& entry : localizedControls) SetWindowTextW(entry.first, tr(entry.second.c_str()));
    refreshMenus();
    updateCaptureButtons();
    SendMessageW(profileBox, CB_RESETCONTENT, 0, 0);
    for (std::size_t i = 0; i < settings.profiles.size(); ++i) {
        auto label = wide(settings.profiles[i].name);
        if (i < 10) label += L"  [" + profileShortcut(i) + L"]";
        SendMessageW(profileBox, CB_ADDSTRING, 0, LPARAM(label.c_str()));
    }
    SendMessageW(profileBox, CB_SETCURSEL, settings.selected, 0);
    const auto& p = settings.profiles[settings.selected];
    SetWindowTextW(nameEdit, wide(p.name).c_str());
    if (rowsProfile != p.name) {
        rowsProfile = p.name; mappingRows.clear(); combinationRows.clear(); mapOffset = comboOffset = 0;
        for (auto [from,to] : p.mappings) if (!customNumpadKey(to)) mappingRows.push_back({to,from,from});
        for (int i = 0; i < int(p.combinations.size()); ++i) {
            const auto& c = p.combinations[i];
            combinationRows.push_back({{c.keys[0],c.keys[1],c.keys.size()==3 ? c.keys[2] : 0},c.target,i,c.mode});
        }
        if (mappingRows.empty()) mappingRows.push_back({});
        if (combinationRows.empty()) combinationRows.push_back({});
    }
    renderRows(); updateCaptureButtons();
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
void renderRows() {
    int focusedId = GetDlgCtrlID(GetFocus());
    buildingRows = true;
    for (HWND item : rowControls) DestroyWindow(item);
    rowControls.clear();
    auto add = [](const wchar_t* type, const std::wstring& text, DWORD style, int id, int x, int y, int width, int height) {
        HWND item = control(type,L"",style,id,x,y,width,height);
        SetWindowTextW(item,text.c_str()); rowControls.push_back(item); return item;
    };
    auto scroll = [](int id, int& offset, int count, int page) {
        offset = std::clamp(offset,0,std::max(0,count-page));
        SCROLLINFO info{sizeof(info),SIF_RANGE|SIF_PAGE|SIF_POS,0,std::max(0,count-1),UINT(page),offset};
        SetScrollInfo(GetDlgItem(window,id),SB_CTL,&info,TRUE);
        EnableWindow(GetDlgItem(window,id),count > page);
    };
    scroll(MapScroll,mapOffset,int(mappingRows.size()),4);
    scroll(ComboScroll,comboOffset,int(combinationRows.size()),3);
    for (int i = mapOffset; i < std::min(int(mappingRows.size()),mapOffset+4); ++i) {
        const auto& r = mappingRows[i]; int y=292+(i-mapOffset)*44, id=MapRowBase+i*3;
        add(L"BUTTON",r.sent < 0 ? L"" : keyName(r.sent),WS_TABSTOP,id,360,y,180,36);
        add(L"STATIC",L"\x2190",0,0,542,y+6,24,26);
        add(L"BUTTON",r.pressed ? keyName(r.pressed) : L"",WS_TABSTOP,id+1,568,y,180,36);
        add(L"BUTTON",L"\x00d7",WS_TABSTOP,id+2,770,y,64,36);
    }
    for (int i = comboOffset; i < std::min(int(combinationRows.size()),comboOffset+3); ++i) {
        const auto& r = combinationRows[i]; int y=560+(i-comboOffset)*44, id=ComboRowBase+i*6;
        for (int f=0; f<3; ++f) {
            add(L"BUTTON",r.keys[f] ? keyName(r.keys[f]) : L"",WS_TABSTOP,id+f,44+f*116,y,96,36);
            if (f<2) add(L"STATIC",r.mode == CombinationMode::Sequence ? L"\x2192" : L"+",0,0,140+f*116,y+6,20,26);
        }
        add(L"STATIC",r.mode == CombinationMode::InputChord ? L"\x2192" : L"\x2190",0,0,380,y+6,28,26);
        add(L"BUTTON",r.trigger >= 0 ? keyName(r.trigger) : L"",WS_TABSTOP,id+3,410,y,134,36);
        HWND mode = add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST | WS_TABSTOP,id+4,570,y,184,200);
        for (auto name : {L"Input chord",L"Together",L"Sequence"}) SendMessageW(mode,CB_ADDSTRING,0,LPARAM(tr(name)));
        SendMessageW(mode,CB_SETITEMHEIGHT,WPARAM(-1),28); SendMessageW(mode,CB_SETCURSEL,int(r.mode),0);
        add(L"BUTTON",L"\x00d7",WS_TABSTOP,id+5,770,y,64,36);
    }
    buildingRows = false;
    if (focusedId >= MapRowBase) { HWND next=GetDlgItem(window,focusedId); if (next) SetFocus(next); }
    InvalidateRect(window,nullptr,FALSE);
}
bool saveRow(int id) {
    auto candidate = settings;
    auto& profile = candidate.profiles[candidate.selected];
    if (id < ComboRowBase) {
        auto& row = mappingRows[(id-MapRowBase)/3];
        if (row.sent < 0 || !row.pressed) {
            if (row.original) { profile.mappings.erase(row.original); if (!commit(std::move(candidate))) return false; row.original=0; }
            return true;
        }
        if (row.sent == row.pressed) { error(L"Choose a different destination, or remove the mapping to restore the key."); return false; }
        if (row.pressed != row.original && profile.mappings.count(row.pressed)) { error(L"That key already has a mapping."); return false; }
        profile.mappings.erase(row.original); profile.mappings[row.pressed] = row.sent;
        if (!commit(std::move(candidate))) return false; row.original = row.pressed;
    } else {
        auto& row = combinationRows[(id-ComboRowBase)/6];
        std::vector<int> keys; for (int key : row.keys) if (key) keys.push_back(key);
        if (keys.size() < 2 || row.trigger < 0 || (row.mode != CombinationMode::InputChord && !row.trigger)) {
            if (row.original >= 0) {
                int old = row.original;
                profile.combinations.erase(profile.combinations.begin()+old);
                if (!commit(std::move(candidate))) return false;
                row.original=-1; for (auto& r : combinationRows) if (r.original > old) --r.original;
            }
            return true;
        }
        try {
            keys = combinationKeys(std::move(keys),row.mode);
            const auto& list = profile.combinations;
            for (int i=0; i<int(list.size()); ++i) if (i != row.original &&
                (row.mode == CombinationMode::InputChord ? list[i].mode == row.mode && list[i].keys == keys : list[i].mode != CombinationMode::InputChord && list[i].target == row.trigger))
                throw std::runtime_error("That combination already exists.");
            Combination entry{keys,row.trigger,row.mode};
            int index = row.original < 0 ? int(list.size()) : row.original;
            if (row.original < 0) profile.combinations.push_back(entry); else profile.combinations[index]=entry;
            if (!commit(std::move(candidate))) return false; row.original=index;
        } catch (const std::exception& e) { error(wide(e.what())); return false; }
    }
    return true;
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
    control(L"STATIC",L"Key Mapping",0,SectionMappings,360,228,270,22);
    control(L"BUTTON",L"Add row",WS_TABSTOP,NewMapping,740,222,114,30);
    control(L"STATIC",L"Key to Send",0,0,360,264,190,22);
    control(L"STATIC",L"Key to Press",0,0,568,264,190,22);
    control(L"SCROLLBAR",L"",SBS_VERT,MapScroll,854,292,16,176);
    control(L"STATIC",L"Multi-Key Combinations",0,0,44,498,390,22);
    control(L"BUTTON",L"Add row",WS_TABSTOP,416,740,492,114,30);
    control(L"STATIC",L"Keys to Send",0,0,44,532,330,20);
    control(L"STATIC",L"Key to Press",0,0,410,532,130,20);
    control(L"STATIC",L"Mode",0,0,570,532,200,20);
    control(L"SCROLLBAR",L"",SBS_VERT,ComboScroll,854,560,16,132);
    control(L"BUTTON",L"Quick Chat",WS_TABSTOP,QuickChatOpen,44,438,124,30);
    control(L"STATIC",L"Alt + 1–0",0,0,180,444,124,20);
    control(L"STATIC",L"Click to capture. Right-click to clear. Changes save automatically.",0,0,44,706,810,20);
    statusLabel = control(L"STATIC",L"",0,0,58,752,590,22);
    SendMessageW(statusLabel,WM_SETFONT,WPARAM(font),TRUE);
    toggleButton = control(L"BUTTON",L"",WS_TABSTOP,Toggle,676,746,178,32);
    SendMessageW(profileBox,CB_SETITEMHEIGHT,WPARAM(-1),28);
    themeUI.apply(window,nullptr,themeUI.theme()); refresh();
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
    if (buildingRows) return;
    if (id >= MapRowBase && id < ComboRowBase + 600) {
        bool combo = id >= ComboRowBase; int field = (id-(combo ? ComboRowBase : MapRowBase)) % (combo ? 6 : 3);
        int index = (id-(combo ? ComboRowBase : MapRowBase)) / (combo ? 6 : 3);
        if (combo ? index >= int(combinationRows.size()) : index >= int(mappingRows.size())) return;
        if (combo && field == 4 && notification == CBN_SELCHANGE) {
            int value = int(SendMessageW(GetDlgItem(window,id),CB_GETCURSEL,0,0));
            cancelCapture(); auto old = combinationRows[index]; combinationRows[index].mode = CombinationMode(value); if (!saveRow(id)) combinationRows[index]=old; renderRows(); return;
        }
        if (notification != BN_CLICKED) return;
        if (field == (combo ? 5 : 2)) {
            cancelCapture(); auto candidate = settings;
            if (combo) {
                int original = combinationRows[index].original;
                if (original >= 0) candidate.profiles[candidate.selected].combinations.erase(candidate.profiles[candidate.selected].combinations.begin()+original);
                if (!commit(std::move(candidate))) return;
                combinationRows.erase(combinationRows.begin()+index);
                for (auto& row : combinationRows) if (row.original > original && original >= 0) --row.original;
            } else {
                candidate.profiles[candidate.selected].mappings.erase(mappingRows[index].original);
                if (!commit(std::move(candidate))) return;
                mappingRows.erase(mappingRows.begin()+index);
            }
            renderRows(); return;
        }
        bool again = rowCapture == id; cancelCapture(); renderRows();
        if (!again) { rowCapture = id; std::lock_guard<std::mutex> lock(engineMutex); engine.release(sendKey); capture.arm(CaptureSlot::Target); ++captureGeneration; }
        updateCaptureButtons(); return;
    }
    if (id >= NumpadBase && id < NumpadBase + 6 && notification == BN_CLICKED) {
        bool again = numpadEditing == id-NumpadBase; cancelCapture(); renderRows();
        if (!again) { numpadEditing = id-NumpadBase; std::lock_guard<std::mutex> lock(engineMutex); engine.release(sendKey); capture.arm(CaptureSlot::Target); ++captureGeneration; }
        updateCaptureButtons(); return;
    }
    if (id == ProfileName || notification == EN_CHANGE) return;
    cancelCapture();
    if (id >= ThemeMenu && id < ThemeMenu + 4) { changeTheme(id-ThemeMenu); renderRows(); return; }
    if (id == NewMapping || id == 416) {
        if (id == NewMapping) { if (mappingRows.size() >= 254) return; mappingRows.push_back({}); mapOffset = std::max(0,int(mappingRows.size())-4); }
        else { if (combinationRows.size() >= 100) return; combinationRows.push_back({}); comboOffset = std::max(0,int(combinationRows.size())-3); }
        renderRows(); return;
    }
    if (id == QuickChatOpen) { openQuickChat(); return; }
    if (id == NotificationDiagnostics) { MessageBoxW(window, gameNotifications.diagnostics().c_str(), tr(L"Notification diagnostics"), MB_OK | MB_ICONINFORMATION); return; }
    if (id == CheckUpdates) { beginUpdateCheck(); return; }
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
    } else return;
    commit(std::move(candidate));
    refresh();
}
LRESULT CALLBACK windowProc(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    static const UINT taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
    if (message == taskbarCreated) { magickey::setWindowIdentity(hwnd); return 0; }
    switch (message) {
        case WM_CREATE: window = hwnd; makeInterface(); SetTimer(hwnd, NotificationTimer, 25, nullptr); return 0;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(hwnd, &paint);
            RECT bounds; GetClientRect(hwnd, &bounds);
            HDC memory = CreateCompatibleDC(dc);
            HBITMAP bitmap = CreateCompatibleBitmap(dc, bounds.right, bounds.bottom);
            auto old = SelectObject(memory, bitmap);
            themeUI.paint(memory, bounds, enabled, settings.language, false);
            BitBlt(dc, 0, 0, bounds.right, bounds.bottom, memory, 0, 0, SRCCOPY);
            SelectObject(memory, old); DeleteObject(bitmap); DeleteDC(memory);
            EndPaint(hwnd, &paint); return 0;
        }
        case WM_PRINTCLIENT: {
            RECT bounds; GetClientRect(hwnd, &bounds);
            themeUI.paint(reinterpret_cast<HDC>(w), bounds, enabled, settings.language, false);
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
                if (item->CtlID == Toggle || item->CtlID == AddMapping) role = magickey::ButtonRole::Primary;
                if (item->CtlID == ScanCodes) role = magickey::ButtonRole::Checkbox;
                bool dynamicKey = (item->CtlID >= MapRowBase && item->CtlID < ComboRowBase && (item->CtlID-MapRowBase)%3 < 2) ||
                    (item->CtlID >= ComboRowBase && (item->CtlID-ComboRowBase)%6 < 4);
                if (dynamicKey || (item->CtlID >= NumpadBase && item->CtlID < NumpadBase+6)) role = magickey::ButtonRole::Key;
                bool listening = rowCapture == int(item->CtlID) || numpadEditing >= 0 && item->CtlID == NumpadBase+numpadEditing;
                themeUI.ownerButton(item, role, listening, settings.scanCodes); return TRUE;
            }
            break;
        }
        case WM_CONTEXTMENU: {
            int id = GetDlgCtrlID(reinterpret_cast<HWND>(w));
            if (id >= MapRowBase && id < ComboRowBase+600) {
                cancelCapture(); auto oldMaps=mappingRows; auto oldCombos=combinationRows;
                bool changed=false;
                if (id >= ComboRowBase) {
                    int i=(id-ComboRowBase)/6, field=(id-ComboRowBase)%6;
                    if (i < int(combinationRows.size()) && field < 4) { if (field<3) combinationRows[i].keys[field]=0; else combinationRows[i].trigger=-1; changed=true; }
                } else {
                    int i=(id-MapRowBase)/3, field=(id-MapRowBase)%3;
                    if (i < int(mappingRows.size()) && field < 2) {
                        if (field == 0) {
                            HMENU menu=CreatePopupMenu(); AppendMenuW(menu,MF_STRING,1,tr(L"Clear")); AppendMenuW(menu,MF_STRING,2,tr(L"Disabled"));
                            POINT at; GetCursorPos(&at); int choice=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,at.x,at.y,0,hwnd,nullptr); DestroyMenu(menu);
                            if (choice) { mappingRows[i].sent=choice==2 ? 0 : -1; changed=true; }
                        } else { mappingRows[i].pressed=0; changed=true; }
                    }
                }
                if (changed && !saveRow(id)) { mappingRows=std::move(oldMaps); combinationRows=std::move(oldCombos); }
                renderRows(); updateCaptureButtons(); return 0;
            }
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
                    commit(std::move(candidate)); rowsProfile.clear(); refresh();
                }
                return 0;
            }
            break;
        }
        case WM_COMMAND: command(LOWORD(w), HIWORD(w)); return 0;
        case UpdateFinished: finishUpdate(); return 0;
        case WM_VSCROLL: {
            int id = GetDlgCtrlID(reinterpret_cast<HWND>(l));
            if (id != MapScroll && id != ComboScroll) break;
            cancelCapture(); int& offset = id == MapScroll ? mapOffset : comboOffset;
            int page = id == MapScroll ? 4 : 3;
            switch (LOWORD(w)) {
                case SB_LINEUP: --offset; break; case SB_LINEDOWN: ++offset; break;
                case SB_PAGEUP: offset -= page; break; case SB_PAGEDOWN: offset += page; break;
                case SB_THUMBTRACK: case SB_THUMBPOSITION: offset = HIWORD(w); break;
            }
            renderRows(); return 0;
        }
        case CapturedKey: {
            bool current;
            { std::lock_guard<std::mutex> lock(engineMutex); current = unsigned(w) == captureGeneration; if (current) capture.cancel(); }
            if (current && GetForegroundWindow() == window) {
                if (rowCapture >= 0) {
                    int id = rowCapture; rowCapture = -1; auto oldMaps=mappingRows; auto oldCombos=combinationRows;
                    if (id >= ComboRowBase) { int i=(id-ComboRowBase)/6, f=(id-ComboRowBase)%6; if (f == 3) combinationRows[i].trigger=LOWORD(l); else combinationRows[i].keys[f]=LOWORD(l); }
                    else { int i=(id-MapRowBase)/3, f=(id-MapRowBase)%3; if (f == 0) mappingRows[i].sent=LOWORD(l); else mappingRows[i].pressed=LOWORD(l); }
                    if (!saveRow(id)) { mappingRows=std::move(oldMaps); combinationRows=std::move(oldCombos); } renderRows(); updateCaptureButtons(); return 0;
                }
                if (numpadEditing >= 0) {
                    int to = CustomNumpadKeys[numpadEditing], from = LOWORD(l);
                    numpadEditing = -1;
                    auto candidate = settings;
                    auto& mappings = candidate.profiles[candidate.selected].mappings;
                    for (auto i = mappings.begin(); i != mappings.end();) { if (i->second == to) i = mappings.erase(i); else ++i; }
                    if (from == to) mappings.erase(from);
                    else candidate.profiles[candidate.selected].mappings[from] = to;
                    commit(std::move(candidate)); rowsProfile.clear(); refresh(); return 0;
                }
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
    RECT bounds{0, 0, 900, 794};
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
        beginUpdateCheck(true);
        if (invalidTheme) error(L"The saved theme could not be loaded. Ivory is being used. Your profiles are unchanged.");
    }
    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!(quickChatWindow && IsDialogMessageW(quickChatWindow, &msg)) && !IsDialogMessageW(window, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    if (hookThread) { WaitForSingleObject(hookThread, 5000); CloseHandle(hookThread); }
    if (hookReady) CloseHandle(hookReady);
    CloseHandle(single);
    return 0;
}
