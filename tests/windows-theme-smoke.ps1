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
    [DllImport("user32.dll")] public static extern bool UpdateWindow(IntPtr h);
}
'@
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
    for ($theme = 0; $theme -lt 4; $theme++) {
        $profile = Join-Path $root 'mywarkey.profiles'
        # All four interface languages are exercised across the four themes.
        [IO.File]::WriteAllText($profile, "MYWARKEY 2 0 1 $theme 1 `"Game`" 0 3 81 103 87 104 82 101`n")
        [IO.File]::WriteAllText((Join-Path $root 'MagicKey.theme'), 'MAGICKEY_THEME 1 0')
        $profileHash = (Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash
        $process = Start-MagicKey
        $hwnd = $process.MainWindowHandle
        $selector = [ThemeWindow]::GetDlgItem($hwnd, 117)
        if ($selector -eq [IntPtr]::Zero) { throw 'Theme selector missing' }
        $null = [ThemeWindow]::SendMessage($selector, 0x14E, [IntPtr]$theme, [IntPtr]::Zero) # CB_SETCURSEL
        $null = [ThemeWindow]::SendMessage($hwnd, 0x111, [IntPtr](117 -bor (1 -shl 16)), $selector)
        Start-Sleep -Milliseconds 200
        if ([IO.File]::ReadAllText((Join-Path $root 'MagicKey.theme')).Trim() -cne "MAGICKEY_THEME 1 $theme") { throw 'Theme was not saved' }
        if ((Get-FileHash -LiteralPath $profile -Algorithm SHA256).Hash -cne $profileHash) { throw 'Changing themes changed profiles' }
        Close-MagicKey $process
        $process = Start-MagicKey
        $hwnd = $process.MainWindowHandle
        $selector = [ThemeWindow]::GetDlgItem($hwnd, 117)
        if ([ThemeWindow]::SendMessage($selector, 0x147, [IntPtr]::Zero, [IntPtr]::Zero).ToInt32() -ne $theme) { throw 'Theme did not persist after restart' }
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
            $rgb = ($pixel.R -shl 16) -bor ($pixel.G -shl 8) -bor $pixel.B
            if ($rgb -ne $backgrounds[$theme]) { throw "Theme background did not render: $($names[$theme])" }
            $bitmap.Save((Join-Path $output ($names[$theme] + '.png')), [Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
        Close-MagicKey $process
        $process = $null
        Write-Host "PASS: $($names[$theme]) in language $theme persisted, rendered, and preserved profiles."
    }
} finally {
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
