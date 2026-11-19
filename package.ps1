# Assemble a folder (and zip) you can hand to someone for testing.
#   powershell -ExecutionPolicy Bypass -File package.ps1
param(
    [string]$BuildDirectory = 'build-controls',
    [string]$Output = 'dist'
)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$release = Join-Path $BuildDirectory 'Release'
$exe = Join-Path $release 'flexar-canvas.exe'
if (!(Test-Path $exe)) { throw "Missing $exe - build the Release target first." }

$stage = Join-Path $Output 'Flexar'
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null

# Runtime binary + effect shaders (required for the shader nodes).
Copy-Item $exe $stage
if (Test-Path (Join-Path $release 'shaders')) {
    Copy-Item (Join-Path $release 'shaders') (Join-Path $stage 'shaders') -Recurse
}

# Music track + the four static wallpapers, looked up next to the executable.
$musicCandidates = @(
    (Join-Path $BuildDirectory 'Music'),
    (Join-Path $PSScriptRoot 'Music'),
    (Join-Path $PSScriptRoot 'assets\music')
)
$music = $musicCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($music) {
    Copy-Item $music (Join-Path $stage 'Music') -Recurse
} else {
    Write-Warning "No Music folder found - the app will start silent and without wallpapers."
}

$zip = Join-Path $Output 'Flexar-share.zip'
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -CompressionLevel Optimal

$size = [math]::Round((Get-ChildItem $stage -Recurse -File | Measure-Object Length -Sum).Sum / 1MB, 1)
Write-Output "Folder : $stage"
Write-Output "Archive: $zip ($size MB)"
Write-Output "Recipient: unzip anywhere, run flexar-canvas.exe (Windows 10/11 x64)."
Write-Output "Keep the 'shaders' and 'Music' folders beside the exe."
