#include <windows.h>
#include <filesystem>
#include <fstream>
int main() {
    wchar_t path[32768]{};
    if (!GetModuleFileNameW(nullptr, path, DWORD(std::size(path)))) return 1;
    std::ofstream marker(std::filesystem::path(path).parent_path() / L"installed.marker");
    marker << "MagicKey update restarted successfully\n";
    return marker ? 0 : 1;
}
