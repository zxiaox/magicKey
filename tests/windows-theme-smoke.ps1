param([Parameter(Mandatory = $true)][string]$Executable, [string]$OutputDirectory = 'theme-previews')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
[StructLayout(LayoutKind.Sequential)] public struct PropertyKey { public Guid Format; public uint Id; }
[StructLayout(LayoutKind.Explicit, Size=24)] public struct PropertyValue {
    [FieldOffset(0)] public ushort Type;
    [FieldOffset(8)] public IntPtr Text;
}
[ComImport, Guid("886d8eeb-8cf2-4446-8d02-cdba1dbdcf99"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
public interface WindowPropertyStore {
    [PreserveSig] int GetCount(out uint count);
    [PreserveSig] int GetAt(uint index, out PropertyKey key);
    [PreserveSig] int GetValue(ref PropertyKey key, out PropertyValue value);
    [PreserveSig] int SetValue(ref PropertyKey key, ref PropertyValue value);
    [PreserveSig] int Commit();
}
public static class ThemeWindow {
    [DllImport("shell32.dll")] private static extern int SHGetPropertyStoreForWindow(IntPtr h, ref Guid iid, out WindowPropertyStore store);
    [DllImport("ole32.dll")] private static extern int PropVariantClear(ref PropertyValue value);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
    public static void ClearTaskbarProperty(IntPtr h, uint id) {
        Guid iid = new Guid("886d8eeb-8cf2-4446-8d02-cdba1dbdcf99");
        WindowPropertyStore store;
        Marshal.ThrowExceptionForHR(SHGetPropertyStoreForWindow(h, ref iid, out store));
        try {
            var key = new PropertyKey { Format = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3"), Id = id };
            var empty = new PropertyValue();
            Marshal.ThrowExceptionForHR(store.SetValue(ref key, ref empty));
        } finally { Marshal.ReleaseComObject(store); }
    }
    public static string TaskbarProperty(IntPtr h, uint id) {
        Guid iid = new Guid("886d8eeb-8cf2-4446-8d02-cdba1dbdcf99");
        WindowPropertyStore store;
        Marshal.ThrowExceptionForHR(SHGetPropertyStoreForWindow(h, ref iid, out store));
        try {
            var key = new PropertyKey { Format = new Guid("9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3"), Id = id };
            PropertyValue value;
            Marshal.ThrowExceptionForHR(store.GetValue(ref key, out value));
            try { return value.Type == 31 ? Marshal.PtrToStringUni(value.Text) : null; }
            finally { PropVariantClear(ref value); }
        } finally { Marshal.ReleaseComObject(store); }
    }
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out Rect r);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref Point p);
    [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint flags);
    [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
    [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetSubMenu(IntPtr h, int index);
    [DllImport("user32.dll")] public static extern uint GetMenuState(IntPtr h, uint item, uint flags);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h, System.Text.StringBuilder text, int count);
    [DllImport("user32.dll", EntryPoint="GetClassLongPtrW")] public static extern IntPtr GetClassLongPtr(IntPtr h, int index);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string kind, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool SetWindowText(IntPtr h, string text);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr value);
}
'@
$null = [ThemeWindow]::SetThreadDpiAwarenessContext([IntPtr](-4))
$source = (Resolve-Path $Executable).Path
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = (Resolve-Path $OutputDirectory).Path
$root = Join-Path $env:TEMP ('MagicKey themes ' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$app = Join-Path $root 'MagicKey.exe'
Copy-Item -LiteralPath $source -Destination $app
$names = @('ivory', 'midnight', 'arcane', 'warcraft-iii')
$backgrounds = @(0xF2F5FA, 0x0D1520, 0x171426, 0x141714)
$process = $null
function Start-MagicKey {
    $p = Start-Process -FilePath $app -PassThru
    $deadline = [DateTime]::UtcNow.AddSeconds(15)
    do {
        Start-Sleep -Milliseconds 100
        $p.Refresh()
        if ($p.HasExited) { throw 'MagicKey exited before its interface appeared' }
        if ([DateTime]::UtcNow -gt $deadline) { throw 'MagicKey window did not appear' }
    } while ($p.MainWindowHandle -eq [IntPtr]::Zero)
    return $p
}
function Close-MagicKey($p) {
    $null = $p.CloseMainWindow()
    if (!$p.WaitForExit(10000) -or $p.ExitCode -ne 0) { throw 'MagicKey did not close cleanly' }
}
try {
    $cases = @(@(0,0,'ivory'), @(1,0,'midnight'), @(2,0,'arcane'), @(3,0,'warcraft-iii'), @(3,1,'warcraft-iii-zh'), @(3,2,'warcraft-iii-ja'), @(3,3,'warcraft-iii-ko'))
    foreach ($case in $cases) {
        $theme = $case[0]
        $language = $case[1]
        $name = $case[2]
        $profile = Join-Path $root 'mywarkey.profiles'
        [IO.File]::WriteAllText($profile, "MYWARKEY 2 0 1 $language 1 `"Game`" 0 5 103 81 104 87 101 82 65 90 83 88`n")
        [IO.File]::WriteAllText((Join-Path $root 'MagicKey.theme'), 'MAGICKEY_THEME 1 0')
        $profileHash = (Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash
        $process = Start-MagicKey
        $hwnd = $process.MainWindowHandle
        if ([ThemeWindow]::GetClassLongPtr($hwnd, -14) -eq [IntPtr]::Zero) { throw 'App icon missing' }
        foreach ($iconKind in 0,1) {
            if ([ThemeWindow]::SendMessage($hwnd, 0x7F, [IntPtr]$iconKind, [IntPtr]::Zero) -eq [IntPtr]::Zero) { throw 'Explicit small/large window icon missing' }
        }
        if ([ThemeWindow]::GetClassLongPtr($hwnd, -34) -eq [IntPtr]::Zero) { throw 'Small window class icon missing' }
        if ([ThemeWindow]::TaskbarProperty($hwnd, 5) -cne 'zxiaox.MagicKey') { throw 'Taskbar application identity missing' }
        if ([ThemeWindow]::TaskbarProperty($hwnd, 3) -cne ($app + ',-101')) { throw 'Taskbar group icon does not point to embedded icon' }
        if ([ThemeWindow]::TaskbarProperty($hwnd, 2) -cne ('"' + $app + '"')) { throw 'Taskbar relaunch path incorrect' }
        if ([ThemeWindow]::TaskbarProperty($hwnd, 4) -cne ('@' + $app + ',-201')) { throw 'Taskbar display name missing' }
        # Simulate Explorer restarting; metadata and explicit icons must survive.
        [ThemeWindow]::ClearTaskbarProperty($hwnd, 3)
        if ([ThemeWindow]::TaskbarProperty($hwnd, 3)) { throw 'Could not reset taskbar property for restart test' }
        $created = [ThemeWindow]::RegisterWindowMessage('TaskbarCreated')
        $null = [ThemeWindow]::SendMessage($hwnd, $created, [IntPtr]::Zero, [IntPtr]::Zero)
        if ([ThemeWindow]::TaskbarProperty($hwnd, 3) -cne ($app + ',-101')) { throw 'Taskbar icon not restored' }
        if ([ThemeWindow]::GetDlgItem($hwnd, 300) -eq [IntPtr]::Zero -or [ThemeWindow]::GetDlgItem($hwnd, 305) -eq [IntPtr]::Zero) { throw 'Dedicated numpad editor missing' }
        if ([ThemeWindow]::GetDlgItem($hwnd, 111) -ne [IntPtr]::Zero) { throw 'Windows-key setting still lives in a profile control' }
        if ([ThemeWindow]::SendMessage([ThemeWindow]::GetDlgItem($hwnd,106),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 2) { throw 'Dedicated keypad mappings leaked into general list' }
        $toggle = [ThemeWindow]::GetDlgItem($hwnd, 112)
        $activeLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText($toggle, $activeLabel, 256)
        if ($language -eq 0 -and $activeLabel.ToString() -cne 'Pause remapping') { throw 'App did not start enabled' }
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr](2010 + $theme), [IntPtr]::Zero)
        Start-Sleep -Milliseconds 200
        if ([IO.File]::ReadAllText((Join-Path $root 'MagicKey.theme')).Trim() -cne "MAGICKEY_THEME 1 $theme") { throw 'Theme was not saved' }
        if ((Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash -cne $profileHash) { throw 'Changing themes changed profiles' }
        # Pause before closing: every normal launch must still start enabled.
        $null = [ThemeWindow]::SendMessage($hwnd, 0x8001, [IntPtr]::Zero, [IntPtr]::Zero)
        $pausedLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText($toggle, $pausedLabel, 256)
        if ($pausedLabel.ToString() -ceq $activeLabel.ToString()) { throw 'Toggle shortcut did not pause' }
        $null = [ThemeWindow]::SendMessage($hwnd, 0x8001, [IntPtr]::Zero, [IntPtr]::Zero)
        $resumedLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText($toggle, $resumedLabel, 256)
        if ($resumedLabel.ToString() -cne $activeLabel.ToString()) { throw 'Toggle shortcut did not resume' }
        $null = [ThemeWindow]::SendMessage($hwnd, 0x8001, [IntPtr]::Zero, [IntPtr]::Zero)
        Close-MagicKey $process
        $process = Start-MagicKey
        $hwnd = $process.MainWindowHandle
        $restartedLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText([ThemeWindow]::GetDlgItem($hwnd, 112), $restartedLabel, 256)
        if ($restartedLabel.ToString() -cne $activeLabel.ToString()) { throw 'Restart did not enable remapping' }
        $options = [ThemeWindow]::GetSubMenu([ThemeWindow]::GetMenu($hwnd), 0)
        $themes = [ThemeWindow]::GetSubMenu($options, 1)
        if (([ThemeWindow]::GetMenuState($themes, [uint32](2010 + $theme), 0) -band 8) -eq 0) { throw 'Theme menu selection did not persist' }
        $languages = [ThemeWindow]::GetSubMenu($options, 0)
        if (([ThemeWindow]::GetMenuState($languages, [uint32](2000 + $language), 0) -band 8) -eq 0) { throw 'Language menu selection did not persist' }
        # Exercise hover/leave repainting of a keycap, action button, checkbox
        # and combo before every palette/language screenshot.
        foreach ($control in 107,109,114,101,300) {
            $item = [ThemeWindow]::GetDlgItem($hwnd,$control)
            $null = [ThemeWindow]::SendMessage($item,0x200,[IntPtr]::Zero,[IntPtr]0x00100010)
            $null = [ThemeWindow]::UpdateWindow($item)
            $null = [ThemeWindow]::SendMessage($item,0x2A3,[IntPtr]::Zero,[IntPtr]::Zero)
            $null = [ThemeWindow]::UpdateWindow($item)
        }
        $null = [ThemeWindow]::UpdateWindow($hwnd)
        Start-Sleep -Milliseconds 200
        $rect = New-Object ThemeWindow+Rect
        $null = [ThemeWindow]::GetWindowRect($hwnd, [ref]$rect)
        $bitmap = New-Object Drawing.Bitmap ($rect.Right - $rect.Left), ($rect.Bottom - $rect.Top)
        $graphics = [Drawing.Graphics]::FromImage($bitmap)
        $dc = $graphics.GetHdc()
        try { if (![ThemeWindow]::PrintWindow($hwnd, $dc, 2)) { throw 'Window capture failed' } }
        finally { $graphics.ReleaseHdc($dc); $graphics.Dispose() }
        try {
            $origin = New-Object ThemeWindow+Point
            $null = [ThemeWindow]::ClientToScreen($hwnd, [ref]$origin)
            $pixel = $bitmap.GetPixel($origin.X - $rect.Left + 8, $origin.Y - $rect.Top + 110)
            $rgb = ([int]$pixel.R -shl 16) -bor ([int]$pixel.G -shl 8) -bor [int]$pixel.B
            if ($theme -eq 3) {
                $colors = [Collections.Generic.HashSet[int]]::new()
                for ($x = 540; $x -lt 840; $x += 5) {
                    for ($y = 10; $y -lt 90; $y += 5) {
                        $null = $colors.Add($bitmap.GetPixel($origin.X-$rect.Left+$x, $origin.Y-$rect.Top+$y).ToArgb())
                    }
                }
                if ($colors.Count -lt 100) { throw 'Warcraft artwork was not rendered' }
            }
            $bitmap.Save((Join-Path $output ($name + '.png')), [Drawing.Imaging.ImageFormat]::Png)
            if ($theme -ne 3 -and $rgb -ne $backgrounds[$theme]) { throw ('Theme background mismatch for {0}: actual {1:X6}, expected {2:X6}, origin {3},{4}, bitmap {5}x{6}' -f $name,$rgb,$backgrounds[$theme],($origin.X-$rect.Left),($origin.Y-$rect.Top),$bitmap.Width,$bitmap.Height) }
        } finally { $bitmap.Dispose() }
        $nextLanguage = ($language + 1) % 4
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr](2000 + $nextLanguage), [IntPtr]::Zero)
        Close-MagicKey $process
        $process = Start-MagicKey
        $options = [ThemeWindow]::GetSubMenu([ThemeWindow]::GetMenu($process.MainWindowHandle), 0)
        $languages = [ThemeWindow]::GetSubMenu($options, 0)
        if (([ThemeWindow]::GetMenuState($languages, [uint32](2000 + $nextLanguage), 0) -band 8) -eq 0) { throw 'Language change did not persist' }
        Close-MagicKey $process
        $process = $null
        Write-Host "PASS: $name in language $language persisted, rendered, and preserved profiles."
    }
    # Exercise the production profile-switch command with all ten digit slots,
    # including a paused app, an unavailable slot and persistence after restart.
    $profile = Join-Path $root 'mywarkey.profiles'
    $profiles = (0..10 | ForEach-Object { '"Profile' + $_ + '" 0 0' }) -join ' '
    [IO.File]::WriteAllText($profile, "MYWARKEY 2 0 11 0 1 $profiles`n")
    $process = Start-MagicKey
    $hwnd = $process.MainWindowHandle
    foreach ($index in 0..9) {
        $null = [ThemeWindow]::SendMessage($hwnd, 0x8001, [IntPtr]::Zero, [IntPtr]::Zero) # Pause first.
        $null = [ThemeWindow]::SendMessage($hwnd, 0x8005, [IntPtr]$index, [IntPtr]::Zero)
        $contents = [IO.File]::ReadAllText($profile)
        if (!$contents.StartsWith("MYWARKEY 3 $index 11 0 1")) { throw "Profile slot $index did not persist" }
        $label = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText([ThemeWindow]::GetDlgItem($hwnd, 112), $label, 256)
        if ($label.ToString() -cne 'Pause remapping') { throw 'Profile shortcut did not enable remapping' }
    }
    $before = [IO.File]::ReadAllText($profile)
    $null = [ThemeWindow]::SendMessage($hwnd, 0x8005, [IntPtr]10, [IntPtr]::Zero)
    if ([IO.File]::ReadAllText($profile) -cne $before) { throw 'Nonexistent hotkey slot changed selection' }
    Close-MagicKey $process
    $process = Start-MagicKey
    $selection = [ThemeWindow]::SendMessage([ThemeWindow]::GetDlgItem($process.MainWindowHandle, 101), 0x147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32()
    if ($selection -ne 9) { throw 'Shortcut profile selection did not survive restart' }
    # Empty profile starts at Add; after saving it becomes Update. Editing the
    # selected row must replace its destination rather than append another row.
    $hwnd = $process.MainWindowHandle
    $button = [ThemeWindow]::GetDlgItem($hwnd,109)
    $caption = New-Object Text.StringBuilder 256
    $null = [ThemeWindow]::GetWindowText($button,$caption,256)
    if ($caption.ToString() -cne 'Add') { throw 'Empty profile did not default to Add' }
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]109,[IntPtr]::Zero)
    $null = [ThemeWindow]::GetWindowText($button,$caption.Clear(),256)
    if ($caption.ToString() -cne 'Update') { throw 'Existing mapping did not default to Update' }
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]115,[IntPtr]::Zero) # Disabled target.
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]109,[IntPtr]::Zero)
    if ([ThemeWindow]::SendMessage([ThemeWindow]::GetDlgItem($hwnd,106),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 1) { throw 'Update appended a duplicate mapping' }
    if (![IO.File]::ReadAllText($profile).Contains("65 0")) { throw 'Update did not replace the selected mapping' }
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]120,[IntPtr]::Zero)
    $null = [ThemeWindow]::GetWindowText($button,$caption.Clear(),256)
    if ($caption.ToString() -cne 'Add') { throw 'New mapping did not reset to Add' }
    # Windows-key blocking is global across profile changes.
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]111,[IntPtr]::Zero)
    $null = [ThemeWindow]::SendMessage($hwnd,0x8005,[IntPtr]::Zero,[IntPtr]::Zero)
    if (![IO.File]::ReadAllText($profile).StartsWith("MYWARKEY 3 0 11 0 1 1")) { throw 'Global Windows-key setting lost on profile switch' }
    # Route a captured replacement into the dedicated numpad mapping and
    # verify it stays out of the general table. Generation is intentionally
    # opaque; stale capture messages must be ignored until the current token.
    $null = [ThemeWindow]::SetForegroundWindow($hwnd)
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]300,[IntPtr]::Zero)
    $pad = [ThemeWindow]::GetDlgItem($hwnd,300)
    foreach ($generation in 0..64) {
        $null = [ThemeWindow]::SendMessage($hwnd,0x8003,[IntPtr]$generation,[IntPtr](84 -bor (2 -shl 16)))
        $null = [ThemeWindow]::GetWindowText($pad,$caption.Clear(),256)
        if ($caption.ToString() -ceq 'T') { break }
    }
    if ($caption.ToString() -cne 'T' -or ![IO.File]::ReadAllText($profile).Contains('103 84')) { throw 'Numpad destination capture did not save' }
    if ([ThemeWindow]::SendMessage([ThemeWindow]::GetDlgItem($hwnd,106),0x1004,[IntPtr]::Zero,[IntPtr]::Zero).ToInt32() -ne 0) { throw 'Captured numpad mapping appeared in general table' }
    # Save two independent Quick Chat slots and reopen to check persistence.
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]121,[IntPtr]::Zero)
    $chat = [ThemeWindow]::FindWindow('MagicKey.QuickChat','Quick Chat')
    if ($chat -eq [IntPtr]::Zero) { throw 'Quick Chat editor missing' }
    $slots = [ThemeWindow]::GetDlgItem($chat,401)
    $edit = [ThemeWindow]::GetDlgItem($chat,402)
    $null = [ThemeWindow]::SetWindowText($edit,'你好 诸葛亮')
    $null = [ThemeWindow]::SendMessage($chat,0x111,[IntPtr]403,[IntPtr]::Zero)
    $null = [ThemeWindow]::SendMessage($slots,0x14E,[IntPtr]9,[IntPtr]::Zero)
    $null = [ThemeWindow]::SendMessage($chat,0x111,[IntPtr](401 -bor (1 -shl 16)),$slots)
    $null = [ThemeWindow]::SetWindowText($edit,'Ready team')
    $null = [ThemeWindow]::SendMessage($chat,0x111,[IntPtr]403,[IntPtr]::Zero)
    $null = [ThemeWindow]::SendMessage($chat,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Close-MagicKey $process
    $process = Start-MagicKey
    $hwnd = $process.MainWindowHandle
    $null = [ThemeWindow]::SendMessage($hwnd,0x111,[IntPtr]121,[IntPtr]::Zero)
    $chat = [ThemeWindow]::FindWindow('MagicKey.QuickChat','Quick Chat')
    $edit = [ThemeWindow]::GetDlgItem($chat,402)
    $null = [ThemeWindow]::GetWindowText($edit,$caption.Clear(),256)
    if ($caption.ToString() -cne '你好 诸葛亮') { throw 'Quick Chat slot 1 failed round trip' }
    $slots = [ThemeWindow]::GetDlgItem($chat,401)
    $null = [ThemeWindow]::SendMessage($slots,0x14E,[IntPtr]9,[IntPtr]::Zero)
    $null = [ThemeWindow]::SendMessage($chat,0x111,[IntPtr](401 -bor (1 -shl 16)),$slots)
    $null = [ThemeWindow]::GetWindowText($edit,$caption.Clear(),256)
    if ($caption.ToString() -cne 'Ready team') { throw 'Quick Chat slot 0 failed round trip' }
    $null = [ThemeWindow]::SendMessage($chat,0x111,[IntPtr]404,[IntPtr]::Zero)
    $null = [ThemeWindow]::GetWindowText($edit,$caption.Clear(),256)
    if ($caption.Length -ne 0) { throw 'Quick Chat clear did not clear slot' }
    $null = [ThemeWindow]::SendMessage($chat,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    Close-MagicKey $process
    $process = $null
    Write-Host 'PASS: ten profile shortcut slots enable and persist; invalid slot ignored.'
} finally {
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
