param([Parameter(Mandatory = $true)][string]$Executable, [string]$OutputDirectory = 'theme-previews')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ThemeWindow {
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
        [IO.File]::WriteAllText($profile, "MYWARKEY 2 0 1 $language 1 `"Game`" 0 3 81 103 87 104 82 101`n")
        [IO.File]::WriteAllText((Join-Path $root 'MagicKey.theme'), 'MAGICKEY_THEME 1 0')
        $profileHash = (Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash
        $process = Start-MagicKey
        $hwnd = $process.MainWindowHandle
        if ([ThemeWindow]::GetClassLongPtr($hwnd, -14) -eq [IntPtr]::Zero) { throw 'App icon missing' }
        $toggle = [ThemeWindow]::GetDlgItem($hwnd, 112)
        $activeLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText($toggle, $activeLabel, 256)
        if ($language -eq 0 -and $activeLabel.ToString() -cne 'Pause remapping') { throw 'App did not start enabled' }
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr](2010 + $theme), [IntPtr]::Zero)
        Start-Sleep -Milliseconds 200
        if ([IO.File]::ReadAllText((Join-Path $root 'MagicKey.theme')).Trim() -cne "MAGICKEY_THEME 1 $theme") { throw 'Theme was not saved' }
        if ((Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash -cne $profileHash) { throw 'Changing themes changed profiles' }
        # Pause before closing: every normal launch must still start enabled.
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr]112, [IntPtr]::Zero)
        $pausedLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText($toggle, $pausedLabel, 256)
        if ($pausedLabel.ToString() -ceq $activeLabel.ToString()) { throw 'Pause button did not change state' }
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr]112, [IntPtr]::Zero)
        $resumedLabel = New-Object Text.StringBuilder 256
        $null = [ThemeWindow]::GetWindowText($toggle, $resumedLabel, 256)
        if ($resumedLabel.ToString() -cne $activeLabel.ToString()) { throw 'Resume button did not enable remapping' }
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr]112, [IntPtr]::Zero)
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
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr]112, [IntPtr]::Zero) # Pause first.
        $null = [ThemeWindow]::SendMessage($hwnd, 0x8005, [IntPtr]$index, [IntPtr]::Zero)
        $contents = [IO.File]::ReadAllText($profile)
        if (!$contents.StartsWith("MYWARKEY 2 $index 11 0 1")) { throw "Profile slot $index did not persist" }
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
    Close-MagicKey $process
    $process = $null
    Write-Host 'PASS: ten profile shortcut slots enable and persist; invalid slot ignored.'
} finally {
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
