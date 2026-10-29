Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version:

- Notifications now follow the selected interface language: English, Simplified Chinese, Japanese, or Korean.
- Non-ASCII profile names and localized messages use Unicode clipboard paste, addressing classic Warcraft dropping Unicode key events. Existing clipboard formats are preserved when they can be safely copied; a newer user copy is never overwritten. If preservation fails, the notification stops with a diagnostic.
- Removed the long 300/250 ms waits. Chat-open/text-settle waits are now 50/50 ms, retaining 80 ms Enter holds for reliability. Typical sequences take about 0.3–0.4 seconds after shortcut release.
- Diagnostics includes the exact intended message text for checking the selected profile name.

Use notification shortcuts during gameplay with chat closed. Multilingual messages temporarily use the clipboard; clipboard history tools may observe that text. Actual game behavior still requires testing in your Warcraft installation. Profiles, themes and shortcuts are preserved.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
