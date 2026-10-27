#include "../src/updater.hpp"
int wmain(int count, wchar_t** arguments) {
    if (count != 3) return 2;
    try {
        magickey::updates::Result result;
        result.outcome = magickey::updates::Outcome::Downloaded;
        result.staging = arguments[1];
        std::wstring hash(arguments[2]);
        result.hash.assign(hash.begin(), hash.end());
        magickey::updates::launchInstaller(result, 0);
        return 0; // The helper must wait for this process to exit before replacing it.
    } catch (...) { return 3; }
}
