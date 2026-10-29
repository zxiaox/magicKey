Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version: fixed the Windows taskbar icon. MagicKey explicitly supplies large and small window icons, a stable application identity, and taskbar group/pinning icon and relaunch metadata. The taskbar identity is reapplied when Explorer restarts.

If an older pinned shortcut still shows the old icon after updating, unpin it, launch the updated MagicKey.exe, then pin the running app again. Profiles, themes, shortcuts and game notifications are unchanged.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
