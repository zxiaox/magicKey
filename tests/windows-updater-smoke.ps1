param([Parameter(Mandatory = $true)][string]$Executable)
$ErrorActionPreference = 'Stop'
$app = (Resolve-Path $Executable).Path
$binaries = Split-Path -Parent $app
$root = Join-Path $env:TEMP ("MagicKey update & test_日本語 " + [guid]::NewGuid().ToString('N'))
$staging = Join-Path $root ('.MagicKey-update-' + [guid]::NewGuid().ToString('N').Substring(0, 24))
New-Item -ItemType Directory -Path $staging -Force | Out-Null
try {
    $target = Join-Path $root 'MagicKey.exe'
    Copy-Item -LiteralPath (Join-Path $binaries 'update_test_launcher.exe') -Destination $target
    $oldHash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    Copy-Item -LiteralPath $app -Destination (Join-Path $staging 'MagicKey.updater.exe')
    Copy-Item -LiteralPath (Join-Path $binaries 'update_test_payload.exe') -Destination (Join-Path $staging 'MagicKey.new.exe')
    $newHash = (Get-FileHash -LiteralPath (Join-Path $staging 'MagicKey.new.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    $profiles = Join-Path $root 'mywarkey.profiles'
    [IO.File]::WriteAllText($profiles, 'MYWARKEY 2 0 1 3 1 "Custom" 0 1 103 81')
    $profileHash = (Get-FileHash -LiteralPath $profiles -Algorithm SHA256).Hash
    $launcher = Start-Process -FilePath $target -ArgumentList ('"' + $staging + '" ' + $newHash) -PassThru
    if (!$launcher.WaitForExit(15000) -or $launcher.ExitCode -ne 0) { throw 'Updater launcher failed' }
    $deadline = [DateTime]::UtcNow.AddSeconds(45)
    while (!(Test-Path -LiteralPath (Join-Path $root 'installed.marker'))) {
        if ([DateTime]::UtcNow -gt $deadline) { throw 'Updated executable did not restart' }
        Start-Sleep -Milliseconds 100
    }
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant() -cne $newHash) { throw 'Executable was not replaced' }
    if ((Get-FileHash -LiteralPath "$target.backup" -Algorithm SHA256).Hash -cne $oldHash) { throw 'Old executable backup is missing or changed' }
    if ((Get-FileHash -LiteralPath $profiles -Algorithm SHA256).Hash -cne $profileHash) { throw 'Profiles were changed during update' }
    Write-Host 'PASS: actual MagicKey helper verified SHA-256, waited for parent, replaced executable, kept backup, restarted payload, and preserved profiles in a Unicode path.'
} finally {
    # Only terminate processes executing this test's temporary files.
    Get-Process -Name 'MagicKey','MagicKey.updater' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) } | Stop-Process -Force -ErrorAction SilentlyContinue
    Remove-Item -LiteralPath $root -Recurse -Force -ErrorAction SilentlyContinue
}
