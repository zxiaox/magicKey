Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version:

- Each profile has a **Custom Numeric Keypad** grid for numpad 7/8, 4/5, 1/2 and a separate **Key Mapping** table. Click a keypad destination to capture/save it; right-click to disable or restore the key.
- Existing mapping selection defaults to **Update**. **New mapping** starts a separate entry; the action label changes dynamically between **Add** and **Update**. Editing the source replaces the selected entry.
- **Disable Windows keys** moved to global Settings. Migration preserves the selected profile's previous blocking preference as the global value.
- Profile shortcuts are now **Ctrl + Alt + 1–9, 0** (left Alt).
- **Quick Chat** provides ten saved custom messages on **Alt + 1–9, 0**, sent verbatim only while Warcraft is foreground. Open its editor from the sidebar or Settings.
- Refined button and dropdown painting to address hover artifacts across themes.

Existing profiles and mappings are preserved. Saving upgrades the settings file to version 3, including global Windows-key blocking and Quick Chat; older executables cannot read this version. Quick Chat uses the existing game chat/clipboard transport. Use it during gameplay with chat closed; game-specific length/font/input limitations still apply.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
