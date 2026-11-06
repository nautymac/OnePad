# Builds OnePad (onepad.exe + probe.exe) with LLVM-MinGW clang.
param([switch]$Debug)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$llvm = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Directory -Filter 'MartinStorsjo.LLVM-MinGW*' | Select-Object -First 1
if (-not $llvm) { throw "LLVM-MinGW not found. Install: winget install MartinStorsjo.LLVM-MinGW.UCRT" }
$bin = (Get-ChildItem $llvm.FullName -Directory -Filter 'llvm-mingw-*' | Select-Object -First 1).FullName + '\bin'
$cxx = "$bin\clang++.exe"; $rc = "$bin\llvm-windres.exe"
New-Item -ItemType Directory -Force "$root\bin" | Out-Null
$opt = if ($Debug) { '-O0 -g' } else { '-O2' }

# icon (generated once by make-icon.ps1)
if (-not (Test-Path "$root\src\onepad.ico")) { & "$root\make-icon.ps1" }
& $rc -I "$root\src" "$root\src\onepad.rc" -O coff -o "$root\bin\onepad.res"
if ($LASTEXITCODE -ne 0) { throw "windres failed" }

$args1 = @('-std=c++17', '-mwindows', '-static') + $opt.Split(' ') + @('-Wall', '-Wno-unused-function', '-o', "$root\bin\onepad.exe", "$root\src\main.cpp", "$root\bin\onepad.res", '-lsetupapi', '-lcfgmgr32', '-lshell32', '-luser32', '-lgdi32', '-ladvapi32', '-lole32', '-lcomctl32', '-lws2_32')
& $cxx @args1
if ($LASTEXITCODE -ne 0) { throw "onepad.exe build failed" }
& $cxx -std=c++17 -O1 -static -o "$root\bin\usbip-selftest.exe" "$root\tools\usbip_selftest.cpp" -lws2_32
if ($LASTEXITCODE -ne 0) { throw "usbip-selftest.exe build failed" }
& $cxx -std=c++17 -O1 -static -o "$root\bin\probe.exe" "$root\tools\probe.cpp" -lhid -lcfgmgr32 -lsetupapi
if ($LASTEXITCODE -ne 0) { throw "probe.exe build failed" }
if (-not (Test-Path "$root\bin\sapjoyapis.dll")) { Copy-Item 'C:\Program Files (x86)\Gear\Settings\sapjoyapis.dll' "$root\bin\sapjoyapis.dll" }
Write-Host "built: $root\bin\onepad.exe"
