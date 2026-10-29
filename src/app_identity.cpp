#include "app_identity.hpp"
#include <shobjidl.h>
#include <shellapi.h>
#include <propkey.h>
#include <propvarutil.h>
#include <string>
#include <iterator>
namespace magickey {
HICON appIcon(bool compactIcon) {
    return static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(101), IMAGE_ICON,
        GetSystemMetrics(compactIcon ? SM_CXSMICON : SM_CXICON),
        GetSystemMetrics(compactIcon ? SM_CYSMICON : SM_CYICON), LR_SHARED));
}
void initializeAppIdentity() {
    // Stable across portable updates, file moves and version numbers.
    SetCurrentProcessExplicitAppUserModelID(AppUserModelId);
}
void setWindowIdentity(HWND window) {
    SendMessageW(window, WM_SETICON, ICON_BIG, LPARAM(appIcon(false)));
    SendMessageW(window, WM_SETICON, ICON_SMALL, LPARAM(appIcon(true)));
    wchar_t path[32768]{};
    DWORD length = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
    if (!length || length >= std::size(path)) return;
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IPropertyStore* store = nullptr;
    if (SUCCEEDED(SHGetPropertyStoreForWindow(window, IID_PPV_ARGS(&store)))) {
        auto set = [store](REFPROPERTYKEY key, const std::wstring& text) {
            PROPVARIANT value{};
            if (SUCCEEDED(InitPropVariantFromString(text.c_str(), &value))) {
                store->SetValue(key, value);
                PropVariantClear(&value);
            }
        };
        const std::wstring executable(path, length);
        set(PKEY_AppUserModel_RelaunchIconResource, executable + L",-101");
        set(PKEY_AppUserModel_RelaunchCommand, L"\"" + executable + L"\"");
        set(PKEY_AppUserModel_RelaunchDisplayNameResource, L"@" + executable + L",-201");
        // Set the window ID last: this notifies Explorer to refresh the group.
        set(PKEY_AppUserModel_ID, AppUserModelId);
        store->Commit();
        store->Release();
    }
    if (SUCCEEDED(initialized)) CoUninitialize();
}
}
