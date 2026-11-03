Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version:

- New row layout: Configuration Schemes across the top; numeric keypad left; Key Mapping and Multi-Key Combinations stacked at right; software status unchanged at the bottom.
- Removed native mapping-header/tooltips and replaced them with static labels to eliminate stray hover text.
- Per-profile two- and three-key combinations, such as **A+B → C** and **A+B+C → D**, with order-independent matching, longest-combination preference, and held-output cleanup. Only chord members may wait up to 150 ms for recognition; unmatched presses retain normal behavior.
- Quick Chat is global across profiles. It autosaves, remembers its editor slot, previews assigned messages, and displays **Saving… / Saved / Not saved**. Save remains available for immediate persistence. Profile switches preserve an open draft.

Profile shortcuts remain **Ctrl + Alt + 1–9, 0**; Quick Chat remains **Alt + 1–9, 0** while Warcraft is foreground. Existing data is preserved; saving upgrades settings to version 4. The scan-code option is in Settings. Older executables cannot read the new format.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
