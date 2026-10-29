Portable MagicKey for Windows 10/11 x64. End users do not need Visual Studio or an installer.

New in this version:

- **Alt + 1 … Alt + 9, Alt + 0** select and enable the first ten profiles in list order. Shortcuts appear beside profile names. Top-row digits are distinct from the numpad; repeats are consumed once per press.
- When Warcraft III is foreground, profile shortcuts send **Configuration [profile name] enabled.** in normal game chat.
- **Ctrl + Alt + F12** also sends **magicKey enabled.** or **magicKey disabled.** while Warcraft is foreground.
- Chat notifications wait for shortcut release, support Unicode names, bypass mappings, and cancel on focus changes. A pending notification is replaced by the newest status.

Notifications use the current/default chat audience and can be seen by other players. They reset chat with Escape first, which can discard a draft or cancel an action. Use during gameplay; game-specific acceptance still needs verification on your installation. Existing profiles and themes are preserved.

Download **MagicKey-windows-x64-portable.zip**, extract it into a writable folder, and run **MagicKey.exe**. The executable is also available separately for existing installations and the in-app updater. The source-code archives are intended for developers.

- Defaults: Numpad 7 → Q, Numpad 8 → W, Numpad 5 → R; top-row numbers remain unchanged.
- Direct key capture, multiple profiles, individual-key disabling, and Windows-key blocking.
- English, Simplified Chinese, Japanese, and Korean interface.
- Scan-code output for game compatibility; Ctrl + Alt + F12 global toggle.
- Version display and user-triggered GitHub update checks, SHA-256 verification, replacement after exit, and restart with profiles preserved.

For a manual upgrade, close MagicKey and replace only the executable; keep `mywarkey.profiles` beside it. SHA-256 checksum files are attached. Future updates can be installed through **Settings → Check for updates** in the app.
