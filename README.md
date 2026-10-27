# MagicKey

A portable, native Windows keyboard remapper. No installer, Python, .NET, service, driver, registry remapping, or reboot is required to **run** the built executable. Supports Windows 10 and 11, x64.

## Features

- Remap individual keys, including letters, function keys, modifiers, arrows, and the numeric keypad.
- Set source and destination by clicking their key buttons and pressing the actual keys, with explicit top-row/numpad labels and no key-selection menus.
- The initial Default profile contains **Numpad 7 → Q**, **Numpad 8 → W**, and **Numpad 5 → R**. Top-row number keys are unchanged.
- Switch the interface instantly between **English**, **简体中文**, **日本語**, and **한국어**. The language is saved automatically; profile names stay as entered.
- **Game-compatible input (scan codes)** is on by default, with an option to return to virtual-key output for typing applications.
- Save up to 100 named profiles and switch between them in the profile menu.
- Create, rename, and delete profiles; add, update, and remove mappings.
- Disable individual keys by selecting **Disabled** as their destination.
- Disable both Windows keys with a per-profile checkbox. This takes priority over mappings from those keys.
- Start paused; enable or pause using the main button. **Ctrl + Alt + F12** always pauses, including when those keys are remapped.
- Release held output keys on pause, profile changes, edits, and normal exit.
- Run minimized while remapping remains active. Closing the window stops remapping.

## Download and run (end users)

Download **MagicKey-windows-x64-portable.zip** from [GitHub Releases](https://github.com/zxiaox/magicKey/releases), extract it into a writable folder, and double-click **MagicKey.exe**. No Visual Studio, compiler, CMake, installer, or extra C++ runtime installation is needed. The executable statically links its C++ runtime. The standalone **MagicKey.exe** asset is also available for existing installations. Source-code ZIPs are for developers.

A release must first be published by the workflow below before the download links or in-app checks can return an executable. This source tree alone is not a published GitHub release.

The app displays **MagicKey v1.2.0** in its heading. Windows file properties also contain the product name and version, generated from the single version in `CMakeLists.txt`.

## Build and publish (maintainers)

GitHub Actions builds and tests Windows x64 executables on every push and pull request, and packages the executable, portable ZIP, and SHA-256 checksum files as the **MagicKey-windows-x64-portable** artifact. The workflow also runs a Windows integration test using the real updater helper to verify replacement, backup, restart, profile preservation, and Unicode paths. End users never run the build script.

To publish a stable release:

1. Set `project(MagicKey VERSION X.Y.Z ...)` in `CMakeLists.txt` and commit the change. Use three numeric components.
2. Push the commit and an exactly matching version tag, for example:

   ```sh
   git push origin main
   git tag v1.2.0
   git push origin v1.2.0
   ```

3. Open **Actions → Build and release MagicKey**. After compilation, tests, and packaging succeed, the tag run creates a draft release with all four download assets, then publishes it as the latest stable release. A tag that differs from the compiled version fails before publication. Branch and pull-request runs never publish releases.
4. Open [Releases](https://github.com/zxiaox/magicKey/releases) and download the portable ZIP to verify the end-user experience. Do not reuse or overwrite an already published tag; increment the version for corrections.

The workflow uses GitHub's built-in `GITHUB_TOKEN`, scoped to `contents: write` only in the release job. No personal access token is stored in the app or source. The GitHub repository must be public for unauthenticated end-user update checks.

**Optional local build:** Install Visual Studio 2022 Build Tools with **Desktop development with C++**, **C++ CMake tools for Windows**, and a Windows SDK. Run `build-windows.bat`. It finds CMake on PATH or inside Visual Studio, keeps the console open, and saves diagnostics to `build-windows.log`. The executable is `build-windows\Release\MagicKey.exe`. Build tools are needed only on the compiling machine. To package locally after building:

```powershell
powershell -ExecutionPolicy Bypass -File packaging/package-windows.ps1 -Executable build-windows/Release/MagicKey.exe -OutputDirectory dist
```

If double-clicking an older script seems to do nothing, run it from Command Prompt in the project folder to retain the error. The separate `build-windows` directory avoids caches copied from macOS/Linux. The JSON parser is vendored and pinned to nlohmann/json 3.12.0, so normal builds need no dependency downloads; its license is in `THIRD-PARTY-NOTICES.txt`.

## In-app updates

Click **Check for updates**. MagicKey requests `https://api.github.com/repos/zxiaox/magicKey/releases/latest` in a background worker, compares numeric stable versions (1.10.0 is newer than 1.9.0), and reports whether a newer release exists. Nothing is checked automatically at startup. A missing public release, invalid metadata, connection failure, or GitHub rate limit is reported separately from being up to date.

When an update is available, approve **Download and install**. MagicKey downloads only this repository's `MagicKey.exe` release asset over HTTPS, accepts redirects only to GitHub's asset hosts, checks the exact file length and Windows x64 PE header, and verifies SHA-256 against GitHub's asset digest and the attached `MagicKey.exe.sha256` when available. A release must provide a digest or checksum. A checksum checks the asset against release metadata; it is not a publisher code signature or protection against a compromised repository account.

The verified file and a temporary copy of the current app are staged in `.MagicKey-update-*` beside the executable. The temporary helper inherits a handle to the exact running parent process, waits for it to exit, verifies the file again, copies the old executable to `MagicKey.exe.backup`, replaces the executable, and restarts it. If replacement fails the original remains available; if process creation fails after replacement, the helper restores the backup and tries to start the original. This does not monitor the health of an app that starts successfully and then crashes. Temporary staging files are removed after restart; an interrupted update may leave a staging folder that can be deleted after closing the app.

The helper never rewrites `mywarkey.profiles`. Language, profiles, and preferences survive; the app restarts paused as usual. The legacy profile filename and single-instance identity are intentional, allowing upgrades without data migration or two remappers running together. A writable app folder is required; update privileges match the running app. Closing MagicKey during a request cancels it and finishes closing after the current bounded network operation returns. No scripts, installers, administrator prompts, or installed updater services are used.

For manual recovery, close MagicKey, rename `MagicKey.exe.backup` to `MagicKey.exe`, and keep the profiles file beside it. If you renamed your executable, the backup uses that executable's name followed by `.backup`.

## Use

1. Open `MagicKey.exe`. The app starts paused so you can review the selected profile.
2. Click the button under **From key**, then press and release the source key. Click the button under **To key**, then press and release the destination. Click **Add / update** to save the mapping. To block the source, click **Disabled** instead of capturing a destination. Selecting an existing row fills both buttons for editing. To remove a mapping, select its row and click **Remove** above the list.
3. To create an empty profile, type a unique name in **Profile name** and click **New**. To rename the selected profile, edit its name and click **Rename**. Switching profiles applies the saved mappings immediately if remapping is enabled.
4. Check **Disable both Windows keys** if desired.
5. Click **Enable remapping**, then switch to the application where you want to use it. Remapping deliberately bypasses the MagicKey editor so its controls stay usable.
6. Press **Ctrl + Alt + F12** to pause from another application, or close MagicKey to stop.

Changes save automatically to `mywarkey.profiles` beside the executable. Keep that file with the executable when moving between computers. Saving uses a temporary file followed by replacement; save failures are reported and the previous settings remain active. Invalid profile files are preserved and reported on startup rather than overwritten. Enable/pause state is never persisted.

Use the **Language** menu at the upper right to change the interface. UI labels, key names, status messages, and application dialogs are translated. Standard message-box buttons follow the Windows display language. Letters, punctuation, function-key names, and your own profile names are unchanged. The Windows build uses UTF-8 source encoding for all four languages.

During capture, the active button says **Press a key...**. Click it again to cancel, click the other key button to switch sides, or switch away from MagicKey to cancel. The first newly pressed key is accepted when you release it; held-key repeats do not create additional captures. Enter, Tab, Escape, modifiers, and Windows keys can be captured without activating the interface. Ctrl + Alt + F12 keeps its emergency-pause behavior. Captured keys and their releases are consumed, and remapping resumes its normal behavior in other apps after leaving the editor.

Number labels distinguish **Top row 7** from **Numpad 7**, and likewise for every digit 0–9. Capture recognizes the physical numpad digits and decimal key with Num Lock either on or off, separately from Home, End, Delete, and the arrow cluster. Numpad Enter still shares the regular Enter mapping.

## Upgrade from an older source build

1. Close the running MagicKey app. Back up `mywarkey.profiles` before upgrading.
2. Copy the updated source files into your Windows project folder and run `build-windows.bat` again.
3. Replace your previous `MagicKey.exe` with `build-windows\Release\MagicKey.exe`, leaving your profile file beside it. If you run the executable directly from the build folder, copy your profile file there too.
4. Choose your language in the upper-right menu and leave **Game-compatible input (scan codes)** checked when testing Warcraft III.

The new version reads version-1 and version-2 profile files. An untouched legacy **Default** profile containing exactly A → Q and W → Numpad 7 (version 1), or top-row 7 → Q and 8 → W, is updated to **Numpad 7 → Q**, **Numpad 8 → W**, and **Numpad 5 → R**. Other profiles, including a customized Default profile, are preserved. To change an existing custom profile, remove any unwanted old mappings and add the three new mappings in the editor. Newly created profiles remain empty. These updates do not change the profile file format.

The next settings change saves the version-2 format with language and output-mode preferences; older executables cannot read that format. Keep your backup if you need to downgrade.

## Warcraft III: typing works but abilities do not

Successful chat input does not prove that the gameplay input path accepts the same events. The previous version sent virtual-key `SendInput` events. The new **Game-compatible input** mode uses `KEYEVENTF_SCANCODE`, clears `wVk`, and sends matching physical scan-code down/up events (with extended-key flags where needed). The target application's keyboard layout is used to resolve scan codes, and each held output keeps the same scan code until release. This is an input-compatibility improvement, not a confirmed diagnosis of your Warcraft III version or a guarantee that the game accepts injected input.

To troubleshoot in a local game:

1. With MagicKey paused, press the real **Q**, **W**, and **R** keys. Verify they trigger the intended abilities for the selected unit. A remapper does not assign Warcraft abilities to Q/W/R; the game's hotkeys must already use those keys. Reforged 2.0 added an in-game hotkey customization screen; older game versions and custom maps may differ.
2. Enable **Game-compatible input (scan codes)** and **Enable remapping**. Close the game's chat box, select the unit, and press **Numpad 7**, **Numpad 8**, and **Numpad 5**. Compare with real Q/W/R. Top-row **7/8/5** remain unchanged in the Default profile.
3. If Warcraft III runs as administrator, run MagicKey at the same privilege level. Try the English keyboard layout during diagnosis. The UI language does not change your Windows/game keyboard layout.
4. Compare game mode on and off. If both type correctly but neither triggers an ability that real Q/W triggers, the game/client may be ignoring synthetic input for gameplay. Use the game's native hotkey settings where available; this app cannot force a client to accept injected input.
5. Pause using **Ctrl + Alt + F12** before chat if you want to type the original numpad 7/8/5. This app does not detect Warcraft's chat state automatically.

References: Microsoft's [KEYBDINPUT scan-code behavior](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-keybdinput) and [SendInput privilege restrictions](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput), and Blizzard's [Reforged 2.0 patch notes](https://news.blizzard.com/en-us/article/24167122/warcraft-iii-reforged-patch-notes-patch-2-0-0).

## Behavior and limitations

- Keep **Num Lock on** when sending numeric keypad digits. Top-row `7` and `Numpad 7` are separate choices. Numpad input keys are recognized independently of Num Lock. Numpad Enter shares the regular Enter setting.
- Mappings send key events, not fixed text. The active keyboard layout, Shift, Caps Lock, and the receiving app determine the resulting character. Punctuation labels describe a US keyboard.
- Mappings apply globally across applications and keyboards, except in the editor. They are direct: with A → Q and Q → Z, pressing A sends Q. Repeated presses and key-up events are forwarded; keys mapped to the same destination share its held state.
- The emergency chord uses physical Ctrl, Alt, and F12. It is reserved for pausing and takes precedence over mappings. Both left and right Ctrl/Alt work.
- Apps running as administrator may require MagicKey to run as administrator too. Windows restricts `SendInput` to apps at the same or lower integrity level. A detectable injection failure pauses remapping and reports the problem.
- Secure screens, Ctrl + Alt + Delete, games that reject synthetic input, and anti-cheat protected applications are outside the app's control. Do not assume compatibility with every game.
- The app uses a dedicated low-level keyboard-hook thread and ignores injected events to prevent remapping loops. It does not log keystrokes or alter system keyboard settings. Network access happens only when you click Check for updates or approve a download.

## Validation

The portable core tests can also run on macOS/Linux:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Core tests cover profile round-trips and malformed files, legacy migration, language/output-mode preferences, all translation entries, scan-code/virtual-key output flags, capture completion/cancellation/repeats/special keys, top-row and numpad distinction with Num Lock on/off, key repeat, shared destinations, disabled keys, Windows-key blocking, held-key cleanup, pause, and key-up pairing across profile edits. The Windows workflow builds the native GUI and runs these tests; actual keyboard injection and translated layouts still need a Windows smoke test.

Windows smoke test: capture top-row 7 and Numpad 7 separately with Num Lock both on and off, and check that the displayed identities are correct. Capture Enter, Tab, Escape, Windows, and right Ctrl without triggering UI actions. Cancel by clicking again or switching apps while a key is held; verify no key-up leaks into the editor. In Notepad, check Numpad 7 → Q, Numpad 8 → W, and Numpad 5 → R in both output modes, confirm top-row 7/8/5 are unchanged, hold/release a mapped key, test both Windows keys, switch profiles, pause with physical Ctrl + Alt + F12, and verify that closing MagicKey restores the keyboard. Switch through all four languages and restart to check persistence. Test right Ctrl, arrow keys, and changes of output mode or keyboard layout while holding a mapped key. Repeat at the privilege level of the target application. For Warcraft III, verify actual abilities outside chat using the diagnostic steps above.

Implementation references: [LowLevelKeyboardProc](https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelkeyboardproc), [SendInput](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput).

Updater tests additionally cover strict numeric versions, malformed release metadata, draft/prerelease rejection, trusted repository URLs, exact asset selection, size limits, checksum parsing, x64 PE identification, and Windows command-line quoting. The Windows helper integration test runs in CI; game compatibility and translated interface layout still require a real Windows smoke test.
