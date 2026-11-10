# MagicKey

A portable, native Windows keyboard remapper. No installer, Python, .NET, service, driver, registry remapping, or reboot is required to **run** the built executable. Supports Windows 10 and 11, x64.

## Features

- Remap individual keys, including letters, function keys, modifiers, arrows, and the numeric keypad.
- Set source and destination by clicking their key buttons and pressing the actual keys, with explicit top-row/numpad labels and no key-selection menus.
- Each new profile uses Q/W/R/T → Numpad 7/8/4/5 in its dedicated keypad area. Saved profiles retain their mappings.
- Switch the interface instantly between **English**, **简体中文**, **日本語**, and **한국어**. The language is saved automatically; profile names stay as entered.
- **Game-compatible input (scan codes)** is on by default, with an option to return to virtual-key output for typing applications.
- Save up to 100 named profiles and switch between them in the profile menu.
- Create, rename, and delete profiles; edit mappings directly in autosaving rows.
- Disable individual keys by selecting **Disabled** as their destination.
- Disable both Windows keys globally through Settings. This takes priority over mappings from those keys.
- Start enabled with the selected profile; pause or resume using the main button. **Ctrl + Alt + F12** toggles enabled/paused, including when those keys are remapped.
- Release held output keys on pause, profile changes, edits, and normal exit.
- Run minimized while remapping remains active. Closing the window stops remapping.

## Profile shortcuts and Warcraft chat

The first ten profiles in list order have **Ctrl + Alt + 1** through **Ctrl + Alt + 9**, then **Ctrl + Alt + 0**. Their shortcuts appear in the profile selector. Use top-row digits; numpad digits remain separate. Selecting a profile with its shortcut enables remapping, saves the selection, and releases previous held output keys. Renaming keeps its slot; deleting a profile shifts later slots. Profiles beyond ten remain available in the selector. Use left Alt for profile shortcuts; right Alt/AltGr remains available for keyboard-layout input. Shift combinations, plain digits, numpad digits, and unassigned slots keep their normal behavior. Held-key repeats do not switch repeatedly.

When Warcraft III is the foreground application, these shortcuts send **Configuration [profile name] enabled.**, with the actual name substituted. **Ctrl + Alt + F12** sends **magicKey enabled.** or **magicKey disabled.** Messages use the game's normal chat and its current/default audience, so other players can see them. They are not private local overlays. Notification text follows the software language: English, Simplified Chinese, Japanese, or Korean. Profile names retain their original characters.

The app recognizes `War3.exe`, `Warcraft III.exe`, and `Warcraft III Reforged.exe` by executable name. Classic builds also match the exact native window classes `Warcraft III` and `WarcraftIII`, even if Windows blocks process-image queries or the executable has been renamed. Window titles alone and generic Qt/OsWindow classes do not identify the game. It never activates a background game or types into another foreground app. Messages wait up to 2.5 seconds for all keys and mouse buttons to be released; focus changes, further typing, or a timeout cancel delivery. Rapid changes replace a pending notification with the latest one. Input bypasses MagicKey's mappings, even when remapping is disabled.

**Chat limitation:** no game-memory integration is used, so the app cannot reliably detect gameplay versus menus or whether chat is already open. The notification presses and holds Enter for 80 ms, releases it, waits for chat to open, types the text using regular keyboard scan codes where possible, then holds/releases Enter again to submit. All messages use a complete clipboard paste so keyboard layout, Caps Lock, or a rapid batch of key events cannot change individual letters. Classic `War3.exe` and classic-class fallback targets receive UTF-8 bytes in `CF_TEXT`; modern recognized Warcraft executables receive UTF-16 in `CF_UNICODETEXT`. Relying on Windows to convert Unicode to narrow clipboard text can corrupt classic Warcraft messages. The previous clipboard formats are restored afterward, unless a newer user copy took ownership. If the clipboard cannot be safely preserved or opened, sending stops with a diagnostic. No Escape is sent before a normal notification. Use the shortcut during gameplay with chat closed, outside an unfinished draft. If chat is already open, the opening Enter can submit that draft. Actual chat support depends on your Warcraft version, chat settings, and matching administrator privileges. The app cannot confirm that the game accepted a message. After a failed attempt, open **Settings → Notification diagnostics** to see whether game detection, shortcut release, focus, or Windows input delivery failed. It includes the last foreground window class, title, process ID, executable when readable, and numeric Windows query error. Test on your installation before relying on notifications.

## Overhead hero mana in Classic Warcraft III

**Hero Mana…** next to the Warcraft III launcher reads the local `Game.dll` version and reports whether the build is supported. This integration recognizes **1.24e** (1.24.4.6387), **1.26a** (1.26.0.6401), **1.27a** (1.27.0.52240), and **1.27b** (1.27.1.7085). Other builds are unavailable through this integration.

1. Download [Warcraft Feature Extender (WFE)](https://github.com/UnryzeC/WFE-Release/releases), open WFE, and save a profile. Close WFE and Warcraft III.
2. Select your Warcraft III executable in MagicKey. Click **Hero Mana…** and choose the saved `.ini` from WFE's `Profiles` folder. Confirm the change.
3. Reopen WFE, select the same profile, and use WFE's **Launch** with **Auto-inject** enabled. Start a match and look for a mana bar above a hero with mana. This setting applies to heroes across maps, including other visible heroes. To turn it off, repeat the steps and confirm disabling it.

WFE draws the bars inside the game and must load before the match. MagicKey checks the game version and edits the selected WFE profile locally. It does not bundle WFE or upload game files.

## Themes and refreshed interface

Choose **Settings → Theme** in the top menu to switch instantly between **Ivory**, **Midnight**, **Arcane**, and **Warcraft III**. The Warcraft theme uses the Frozen Throne cover artwork from the user-provided Wikipedia page, with bronze borders and solid panels for readable controls. The artwork and the original keyboard-remapping app icon are embedded in the executable, so no separate files or runtime downloads are needed. Artwork attribution is included in THIRD-PARTY-NOTICES.txt.

Profiles span the top. Keypad and standard mapping fields sit side by side, with full-width combination rows below and status at the bottom. Buttons, table rows, headers, checkboxes, input fields, and dropdown contents follow the selected palette. Keyboard focus indicators remain visible, and theme text/background pairs are checked for readable contrast. Windows title-bar colors follow the theme where supported by the OS; native scrollbars and standard system message boxes can retain the Windows appearance.

The theme is saved separately in `MagicKey.theme` beside the executable, using a temporary file and replacement. Theme changes do not rewrite `mywarkey.profiles` or change the profile format. Older profile files, saved languages, and mappings are preserved. Invalid theme preferences fall back to Ivory with a message and leave profiles untouched. In-app updates preserve both files.

## Download and run (end users)

Download **MagicKey-windows-x64-portable.zip** from [GitHub Releases](https://github.com/zxiaox/magicKey/releases), extract it into a writable folder, and double-click **MagicKey.exe**. No Visual Studio, compiler, CMake, installer, or extra C++ runtime installation is needed. The executable statically links its C++ runtime. The standalone **MagicKey.exe** asset is also available for existing installations. Source-code ZIPs are for developers.

A release must first be published by the workflow below before the download links or in-app checks can return an executable. This source tree alone is not a published GitHub release.

The app displays **MagicKey v1.12.1** in its heading. Windows file properties also contain the product name and version, generated from the single version in `CMakeLists.txt`.

## Build and publish (maintainers)

GitHub Actions builds and tests Windows x64 executables on every push and pull request, and packages the executable, portable ZIP, and SHA-256 checksum files as the **MagicKey-windows-x64-portable** artifact. The workflow also runs Windows integration tests using the real updater helper and application windows. It verifies replacement, backup, restart, profile preservation, Unicode paths, all four theme palettes, language combinations, and theme persistence after restarting. Actual window screenshots are saved in the MagicKey-theme-previews artifact for visual review. End users never run the build script.

To publish a stable release:

1. Set `project(MagicKey VERSION X.Y.Z ...)` in `CMakeLists.txt` and commit the change. Use three numeric components.
2. Push the commit and an exactly matching version tag, for example:

   ```sh
   git push origin main
   git tag v1.12.1
   git push origin v1.12.1
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

Click **Settings → Check for updates**. MagicKey requests `https://api.github.com/repos/zxiaox/magicKey/releases/latest` in a background worker, compares numeric stable versions (1.12.1 is newer than 1.9.0), and reports whether a newer release exists. The same check runs automatically at startup; current-version and failure results stay quiet for automatic checks. Explicit checks report a missing public release, invalid metadata, connection failure, or GitHub rate limit separately from being up to date.

When an update is available, approve **Download and install**. MagicKey downloads only this repository's `MagicKey.exe` release asset over HTTPS, accepts redirects only to GitHub's asset hosts, checks the exact file length and Windows x64 PE header, and verifies SHA-256 against GitHub's asset digest and the attached `MagicKey.exe.sha256` when available. A release must provide a digest or checksum. A checksum checks the asset against release metadata; it is not a publisher code signature or protection against a compromised repository account.

The verified file and a temporary copy of the current app are staged in `.MagicKey-update-*` beside the executable. The temporary helper inherits a handle to the exact running parent process, waits for it to exit, verifies the file again, copies the old executable to `MagicKey.exe.backup`, replaces the executable, and restarts it. If replacement fails the original remains available; if process creation fails after replacement, the helper restores the backup and tries to start the original. This does not monitor the health of an app that starts successfully and then crashes. Temporary staging files are removed after restart; an interrupted update may leave a staging folder that can be deleted after closing the app.

The helper never rewrites `mywarkey.profiles`. Language, profiles, and preferences survive; the app restarts enabled with the selected profile. The legacy profile filename and single-instance identity are intentional, allowing upgrades without data migration or two remappers running together. A writable app folder is required; update privileges match the running app. Closing MagicKey during a request cancels it and finishes closing after the current bounded network operation returns. No scripts, installers, administrator prompts, or installed updater services are used.

For manual recovery, close MagicKey, rename `MagicKey.exe.backup` to `MagicKey.exe`, and keep the profiles file beside it. If you renamed your executable, the backup uses that executable's name followed by `.backup`.

## Use

1. Open `MagicKey.exe`. The selected profile is enabled immediately. Use **Pause remapping** or **Ctrl + Alt + F12** if you want to pause it.
2. In **Key Mapping**, fill the left **Original Key** field and the right **New Key** field. For example, left Q and right A means pressing A sends Q. Click a field and press/release the desired physical key. Complete rows save automatically; editing either field updates that same mapping. **Add row** inserts a blank row; **×** removes it. Right-click an output field to clear it or choose **Disabled**; right-click a trigger to clear it. Incomplete rows remain drafts until completed and are not saved.
3. To create a profile, type a unique name in **Profile name** and click **New**. The numeric keypad section starts with **Q → Numpad 7**, **W → Numpad 8**, **R → Numpad 4**, and **T → Numpad 5**. Slots 1 and 2 start blank. Edit the field beside a numpad destination to choose the key that sends it. Rename applies to the selected profile; switching profiles applies its saved mappings immediately when enabled.
4. Use **Settings → Disable Windows keys** to apply Windows-key blocking globally while remapping is enabled.
5. Switch to the application where you want to use it. If you paused remapping, click **Enable remapping** first. Remapping deliberately bypasses the MagicKey editor so its controls stay usable.
6. Press **Ctrl + Alt + F12** to toggle remapping from another application, or close MagicKey to stop.

Changes save automatically to `mywarkey.profiles` beside the executable. Keep that file with the executable when moving between computers. Saving uses a temporary file followed by replacement; save failures are reported and the previous settings remain active. Invalid profile files are preserved and reported on startup rather than overwritten. Enable/pause state is never persisted.

Use the **Settings → Language** menu to change the interface. UI labels, key names, status messages, and application dialogs are translated. Standard message-box buttons follow the Windows display language. Letters, punctuation, function-key names, and your own profile names are unchanged. The Windows build uses UTF-8 source encoding for all four languages.

During capture, the active button says **Press a key...**. Click it again to cancel, click the other key button to switch sides, or switch away from MagicKey to cancel. The first newly pressed key is accepted when you release it; held-key repeats do not create additional captures. Enter, Tab, Escape, modifiers, and Windows keys can be captured without activating the interface. Ctrl + Alt + F12 keeps its toggle behavior. Captured keys and their releases are consumed, and remapping resumes its normal behavior in other apps after leaving the editor.

Number labels distinguish **Top row 7** from **Numpad 7**, and likewise for every digit 0–9. Capture recognizes the physical numpad digits and decimal key with Num Lock either on or off, separately from Home, End, Delete, and the arrow cluster. Numpad Enter still shares the regular Enter mapping.

## Upgrade from an older source build

1. Close the running MagicKey app. Back up `mywarkey.profiles` before upgrading.
2. Copy the updated source files into your Windows project folder and run `build-windows.bat` again.
3. Replace your previous `MagicKey.exe` with `build-windows\Release\MagicKey.exe`, leaving your profile file beside it. If you run the executable directly from the build folder, copy your profile file there too.
4. Choose your language in **Settings → Language** and leave **Game-compatible input (scan codes)** checked when testing Warcraft III.

The new version reads version-1 and version-2 profile files. An untouched legacy **Default** profile containing exactly A → Q and W → Numpad 7 (version 1), or top-row 7 → Q and 8 → W, is updated to the current default mappings. Other profiles, including a customized Default profile, are preserved. Each new profile uses Q/W/R/T → Numpad 7/8/4/5 in its dedicated keypad area. Existing empty profiles stay empty; the defaults are only applied on creation. Saving writes settings version 7.

The next settings change saves the version-7 format with global Windows-key blocking, Quick Chat entries, and combination modes; older executables cannot read that format. Keep your backup if you need to downgrade.

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
- The toggle chord uses physical Ctrl, Alt, and F12. It switches enabled/paused once per F12 press, ignores held-key repeats, and takes precedence over mappings. Both left and right Ctrl/Alt work.
- Apps running as administrator may require MagicKey to run as administrator too. Windows restricts `SendInput` to apps at the same or lower integrity level. A detectable injection failure pauses remapping and reports the problem.
- Secure screens, Ctrl + Alt + Delete, games that reject synthetic input, and anti-cheat protected applications are outside the app's control. Do not assume compatibility with every game.
- The app uses a dedicated low-level keyboard-hook thread and ignores injected events to prevent remapping loops. It does not log keystrokes or alter system keyboard settings. MagicKey checks GitHub for updates in the background at startup and when you choose Check for updates. Downloads begin only after you approve an available update.

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

Theme tests cover all four preference values, malformed appearance files, and text contrast. Windows UI tests switch each theme through its real menu, restart the app, verify startup is enabled, pause/resume works, saved menu selections persist, the app icon is present, and the background artwork renders, ensure profiles remain byte-identical, and capture screenshots for all four language/theme combinations.

Taskbar identity: MagicKey sets explicit large/small window icons and the stable `zxiaox.MagicKey` AppUserModelID. Taskbar group/pinning properties point to the embedded icon and the current portable executable. These are restored after Explorer restarts. If an older pinned shortcut still shows a cached icon, unpin it, launch the updated MagicKey.exe, and pin the running app again. No system icon-cache deletion is required.

Notification speed: Enter remains held for 80 ms; chat-open and text-settle waits are now 50 ms each instead of 300/250 ms. Multilingual paste adds a 50 ms Ctrl+V hold. With the UI timer, typical sequences take roughly 0.3–0.4 seconds after shortcut release. Actual game timing can differ. Diagnostics includes the exact intended message, helping distinguish name retrieval from text-input/display failures. Clipboard tools/history managers may observe the temporary notification text.

## Mapping layout and automatic saving (v1.10)

Below the profiles, the first mapping row has **Custom Numeric Keypad** on the left and **Key Mapping** on the right. **Multi-Key Combinations** spans the full width beneath them. The upper panels adapt to their contents: the standard area expands up to four editable rows and the combination area shows two; scrollbars reveal additional rows. The combination area and bottom bar move with the adaptive panel height. Combination rows show the trigger first: X = A+B+C. There is no column header row. Native list headers and row tooltips are no longer used.

All fields capture physical keys, distinguishing top-row digits from the numeric keypad. A blank field means unconfigured, with no placeholder. Keypad slots 7/8/4/5 default to input Q/W/R/T respectively; slots 1/2 remain blank. The obsolete three-mapping default is not added separately.

For a combination, put the trigger in the first field and two or more output keys after the equals sign: **X = A+B+C** makes X send A/B/C. New rows provide three output slots. Use the row's **+** button to keep adding slots; there is no fixed slot-count cap. Long rows show four slots at a time; use **‹ / ›** to navigate. Adding a slot automatically shows the new blank slot. Unused slots stay blank and are skipped when sending. Right-click any key field to clear it; blank positions and the visible slot count survive saving and reopening. Every completed edit saves immediately, including mode changes. **Add row** creates a blank draft and **×** removes a row. Clearing a required field deactivates the saved rule until the row is complete again. Incomplete drafts are discarded when switching profiles or closing the app. Duplicate triggers and invalid combinations are rejected without replacing saved data.

**Sequential Input Speed** is a global slider from **80 to 180 ms** between the starts of successive output presses, with 80 ms as the default. Each key is held for 60 ms, followed by a 20–120 ms gap. The slider changes only the gap: at 100 ms, the hold remains 60 ms and the gap becomes 40 ms. The current interval is shown beside the slider and saves when dragging finishes; keyboard adjustments save immediately. Windows scheduling can round or delay actual timing, so these are nominal intervals. Together mode does not use this slider.

**Together** holds the outputs together for 60 ms, pressing modifiers first and releasing in reverse order. **Sequence** taps them in field order using the speed slider; repeated keys are allowed. Releasing the trigger does not truncate the macro. Only one macro runs at a time; repeats/additional macro presses during it are consumed. Pause, profile/focus changes, and exit cancel it and release generated keys. Generated outputs bypass remapping.

Legacy **Input chord** rules remain available and preserve their opposite direction: the right-hand physical keys trigger the first field’s output. Press them within 150 ms. These rules require distinct keys; unmatched keys resume their ordinary mappings after release/timeout. Built-in shortcuts take priority.

**Quick Chat** is global and independent of the profile. Open it from Settings or the bottom bar, immediately left of Pause remapping. The ten **Alt + 1–9, 0** slots autosave after a 350 ms typing pause, on slot changes, and on close; Save writes immediately. Messages send only while Warcraft is foreground, including while remapping is paused. Use with chat closed. Profile shortcuts use **Ctrl + Alt + 1–9, 0**. Windows-key blocking is global in Settings.

At startup MagicKey automatically checks for a newer release in the background. Current-version and network-failure results stay quiet; an available update prompts before downloading/installing. **Settings → Check for updates** remains available for an explicit check and displays its result. Update checks do not change profiles or pause remapping.

Settings version 7 supports variable-length slot layouts, blank positions, the global sequence interval, and the global Warcraft executable path. Versions 1–6 still load; version 4 chords retain Input chord mode, and version 5 output macros keep their modes with the default 80 ms interval. Older executables cannot read version 7. Keep a backup when downgrading.

Quick Chat Settings and message dialogs are centered over the main app window and kept within the monitor work area. Dialogs shown before the main window exists, or after it closes, are centered on the monitor. Context menus stay at the pointer.

A **Warcraft III** launcher row sits between combinations and status. Use **Browse...** to select `war3.exe` (or type its executable path), then **Launch**. The path is a global setting and survives profile changes and restarts. Launch starts the selected executable directly, with its containing folder as the working directory. No command-line arguments or elevation are added.
