MagicKey v1.12.0 - portable keyboard remapper for Windows 10/11 x64
https://github.com/zxiaox/magicKey

Run MagicKey.exe from a writable folder. No installation, Visual Studio, CMake,
Python, .NET, or additional C++ runtime installation is needed.

The app starts enabled. Ctrl + Alt + F12 toggles remapping; closing the app stops it.
Choose Settings > Language or Theme for four languages and four built-in themes.

MAPPING FIELDS
Profiles span the top. Keypad and standard mappings sit side by side;
combinations span the row below. Status stays at the bottom.
The keypad fields select the input which sends each numpad destination.
New profiles: Q -> Numpad 7, W -> Numpad 8, R -> Numpad 4, T -> Numpad 5.
Slots 1 and 2 are blank. Top-row digits and numpad digits are distinct.

Standard mappings: left = Original Key; right = New Key.
Click a field, then press and release a physical key. Completed edits save
immediately. Add row creates a blank draft; x removes a row. Right-click an
output to clear it or choose Disabled. Right-click a trigger to clear it.
Incomplete drafts are not saved and are discarded on profile switch or exit.

Combinations: the trigger comes first, followed by outputs: X = A+B+C.
Three output slots appear by default. Use + to keep adding slots without a fixed cap. Use the row arrows to navigate. Leave unused
slots blank; right-click a slot to clear it. Blank positions and slot count are saved.
Together sends the outputs simultaneously; Sequence sends them in order.
Sequential Input Speed sets a global nominal onset interval of 80-180 ms (default
80 ms). Keys are held for 60 ms, with a 20-120 ms gap; 100 ms means a 60 ms hold and a 40 ms gap. Actual timing depends on
Windows scheduling. The slider does not affect Together mode.
Legacy Input chord rules retain the reverse behavior: right-hand physical keys
trigger the first field's output when pressed within 150 ms.
One output macro runs at a time; repeats do not restart it. Pause/profile/focus
changes cancel generated input. Generated keys bypass remapping.

UPDATES AND SAVED SETTINGS
MagicKey checks this repository's public GitHub Releases at startup. Automatic
checks stay quiet when current or offline, and prompt before downloading a newer
version. Settings > Check for updates runs an explicit check and shows its result.
A GitHub account is not required. Verified updates preserve profiles and settings.
The previous executable is kept as MagicKey.exe.backup.

Profiles and language use mywarkey.profiles beside the executable (the legacy
filename is intentional). Appearance is saved separately in MagicKey.theme.
Existing settings versions 1-6 load; saving writes version 7. Keep a backup if
downgrading, since older executables cannot read the current format.
Disable Windows keys is global under Settings.

PROFILE SHORTCUTS AND QUICK CHAT
Ctrl + left Alt + top-row 1..9, then 0 selects/enables profiles 1..10.
Alt + top-row 1..9, then 0 sends global Quick Chat messages only in foreground
Warcraft, including when remapping is paused. Quick Chat is independent of the
selected profile. The editor autosaves and also provides Save and Clear buttons.

WARCRAFT CHAT
Profile switches send Configuration [actual profile name] enabled.
Ctrl + Alt + F12 sends magicKey enabled. / magicKey disabled.
Messages follow the app language. These are regular game chat messages visible
to the current/default audience. Use during gameplay with chat closed: an open
draft may otherwise be submitted. Messages use Enter -> paste -> Enter with timed
key presses. Classic Warcraft receives UTF-8 narrow clipboard text; modern
Warcraft receives Unicode. Previous clipboard formats are preserved where safe,
and newer user copies are kept. Clipboard history tools may observe the text.
Background games and other apps never receive these messages. If no message
appears, open Settings > Notification diagnostics. Actual compatibility depends
on Warcraft version, settings and privileges.

Leave Game-compatible input (scan codes) enabled. If the game runs as
administrator, MagicKey may need the same privilege level.
Third-party notices and Warcraft artwork attribution are in THIRD-PARTY-NOTICES.txt.

The Warcraft III row above status provides an executable path, Browse and Launch.
The selected path is global and is preserved after restart. Launch uses the
executable folder as its working directory.
