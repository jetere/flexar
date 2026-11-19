param([string]$BuildDirectory = 'build-pixel')
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (!(Test-Path 'third_party/imgui-1.91.8/imgui.cpp')) {
    New-Item -ItemType Directory -Force third_party | Out-Null
    Invoke-WebRequest 'https://github.com/ocornut/imgui/archive/refs/tags/v1.91.8.zip' -OutFile 'third_party/imgui.zip'
    Expand-Archive 'third_party/imgui.zip' third_party -Force
}
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) { $cmakePath = $cmake.Source } else {
    $cmakePath = Get-ChildItem "${env:ProgramFiles(x86)}/Microsoft Visual Studio" -Recurse -Filter cmake.exe | Select-Object -First 1 -ExpandProperty FullName
}
if (!$cmakePath) { throw 'Install Visual Studio Build Tools with Desktop development with C++ and CMake.' }
& $cmakePath -S . -B $BuildDirectory -A x64
if ($LASTEXITCODE) { throw 'Configure failed' }
& $cmakePath --build $BuildDirectory --config Release
if ($LASTEXITCODE) { throw 'Build failed' }
& $cmakePath --build $BuildDirectory --config Release --target RUN_TESTS
if ($LASTEXITCODE) { throw 'Tests failed' }
Write-Host "Ready: $BuildDirectory/Release/flexar-canvas.exe"
