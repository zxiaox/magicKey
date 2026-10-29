Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version: corrected the clipboard encoding used by classic Warcraft notifications. Classic targets now receive **UTF-8 bytes in CF_TEXT**, rather than depending on Windows to convert UTF-16 into the system ANSI code page. Modern recognized Warcraft executables retain Unicode clipboard text.

English messages now paste as complete text too, avoiding dropped letters and keyboard-layout/Caps Lock effects from rapid scan-code typing. Translations and names are tested against exact expected messages in all four languages, including **诸葛亮**, using a classic-style narrow UTF-8 clipboard reader. The earlier receiver tested only Unicode clipboard reads.

The shorter timings, clipboard preservation, profiles, themes and shortcuts are preserved. Diagnostics reports the selected transport. Actual Warcraft acceptance still needs verification in the game.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
