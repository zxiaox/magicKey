Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version: fixed classic Warcraft detection when Windows blocks reading the foreground process name or a launcher uses a renamed executable. MagicKey can now recognize the exact classic Warcraft window classes **Warcraft III** and **WarcraftIII** without opening the process. A matching window title alone is insufficient.

**Settings → Notification diagnostics** now includes the last foreground window class, title, process ID, readable executable name, and numeric Windows query error. Query failure is no longer reported as an assumed administrator mismatch. Actual Windows input restrictions remain separate from game recognition.

The Enter → message → Enter sequence, timed presses, profiles, themes, and shortcuts are preserved. Use notification shortcuts during gameplay with chat closed.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
