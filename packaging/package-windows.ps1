param(
    [Parameter(Mandatory = $true)][string]$Executable,
    [string]$OutputDirectory = 'dist'
)
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$output = (Resolve-Path $OutputDirectory).Path
Copy-Item -LiteralPath $Executable -Destination (Join-Path $output 'MagicKey.exe') -Force
$bundle = Join-Path $output 'portable'
New-Item -ItemType Directory -Force -Path $bundle | Out-Null
Copy-Item -LiteralPath (Join-Path $output 'MagicKey.exe') -Destination $bundle -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'PORTABLE-README.txt') -Destination (Join-Path $bundle 'README.txt') -Force
Copy-Item -LiteralPath (Join-Path $PSScriptRoot '../THIRD-PARTY-NOTICES.txt') -Destination $bundle -Force
$hash = (Get-FileHash -LiteralPath (Join-Path $output 'MagicKey.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $output 'MagicKey.exe.sha256'), "$hash  MagicKey.exe`n", [Text.Encoding]::ASCII)
Copy-Item -LiteralPath (Join-Path $output 'MagicKey.exe.sha256') -Destination $bundle -Force
$archive = Join-Path $output 'MagicKey-windows-x64-portable.zip'
Compress-Archive -Path (Join-Path $bundle '*') -DestinationPath $archive -Force
$zipHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$archive.sha256", "$zipHash  MagicKey-windows-x64-portable.zip`n", [Text.Encoding]::ASCII)
Remove-Item -LiteralPath $bundle -Recurse -Force
