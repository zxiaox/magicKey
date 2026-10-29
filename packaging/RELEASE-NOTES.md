Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version: corrected Warcraft chat notifications to use **Enter → message → Enter**, without an initial Escape. Enter is now held for 80 ms so a game's input polling can observe it. English text uses regular keyboard scan codes; non-Latin profile names retain Unicode fallback. Focus-change cancellation pairs any held Enter with its release.

Added **Settings → Notification diagnostics** to report unrecognized foreground processes, shortcut-release timeouts, focus changes, and blocked Windows input. A successful input submission does not prove that Warcraft accepted chat; actual game verification remains necessary.

Use notification shortcuts during gameplay with chat closed. An already-open chat draft can be submitted by the opening Enter. Profiles, themes and shortcuts are preserved.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
