<#
.SYNOPSIS
  3D One PC 재설치용 OnePad 원샷 설치 스크립트. 관리자 PowerShell에서 이 파일을 실행한다.
  (이 폴더 D:\OneDrive\AI\ClaudeCode\OnePad 가 OneDrive로 동기화돼 있어야 bin\onepad.exe 경로가 그대로 유지된다)

.DESCRIPTION
  1. HidHide 설치 (winget, 실패 시 installer\HidHide_1.5.230_x64.exe)
  2. usbip-win2 설치 (installer\USBip-0.9.8.1-x64.exe, Inno Setup 무인 설치)
  3. OnePad 설정 복원 (%LOCALAPPDATA%\OnePad\onepad.ini, firmware-backup.ini)
  4. OnePad 첫 실행 → 로그온 작업 "OnePad" 자기 등록 (최고 권한, 로그온 8초 뒤)
  5. (옵션) Azahar 2126.1 포터블 설치 (D:\Azahar) + 교착 우회 래퍼 + 바로가기 + Steam 비Steam 항목
  6. 재부팅 안내 (HidHide 필터는 재부팅 후에야 Xbox 스택에 붙는다)

.PARAMETER SkipAzahar      Azahar 단계 건너뜀
.PARAMETER SkipSteamShortcut  Steam 비Steam 항목 추가 건너뜀 (Steam을 종료해야 하므로)
.PARAMETER ForceConfig     이미 있는 onepad.ini를 패키지 것으로 덮어씀
.PARAMETER NoReboot        마지막 재부팅 질문 생략
#>
[CmdletBinding()]
param([switch]$SkipAzahar, [switch]$SkipSteamShortcut, [switch]$ForceConfig, [switch]$NoReboot)
$ErrorActionPreference = "Stop"
$pkg = Split-Path -Parent $MyInvocation.MyCommand.Path
function Step($msg) { Write-Host "`n== $msg" -ForegroundColor Cyan }
function Ok($msg)   { Write-Host "   $msg" -ForegroundColor Green }
function Warn($msg) { Write-Host "   $msg" -ForegroundColor Yellow }

if (-not ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "관리자 PowerShell에서 실행하세요."
}
if (-not (Test-Path "$pkg\bin\onepad.exe")) { throw "bin\onepad.exe 가 없습니다. 패키지 폴더에서 실행하세요: $pkg" }

Step "0. 환경 확인"
$pad = Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -match '^USB\\VID_2993&PID_0012\\' }
if ($pad) { Ok "3D One 컨트롤러 연결됨" } else { Warn "3D One 컨트롤러가 안 보입니다(절전으로 USB에서 사라졌을 수 있음). 설치는 계속하지만 펌웨어 백업은 다음 연결 때 됩니다." }
if (Get-Process steam -ErrorAction SilentlyContinue) { Warn "Steam이 실행 중입니다. 마지막에 재부팅하면 순서가 정리됩니다." }

Step "1. HidHide (실제 Xbox 패드를 Steam/게임에서 숨김)"
$hhCli = "C:\Program Files\Nefarius Software Solutions\HidHide\x64\HidHideCLI.exe"
if (Test-Path $hhCli) { Ok "이미 설치됨" }
else {
    $done = $false
    try { winget install --id Nefarius.HidHide --exact --silent --accept-package-agreements --accept-source-agreements | Out-Null; $done = Test-Path $hhCli } catch {}
    if (-not $done -and (Test-Path "$pkg\installer\HidHide_1.5.230_x64.exe")) {
        Warn "winget 실패 → 로컬 설치 파일 실행 (설치 마법사가 뜨면 기본값으로 진행)"
        Start-Process "$pkg\installer\HidHide_1.5.230_x64.exe" -Wait
        $done = Test-Path $hhCli
    }
    if ($done) { Ok "설치됨 (재부팅 후 필터 활성화)" } else { throw "HidHide 설치 실패" }
}

Step "2. usbip-win2 (가상 Steam Deck 컨트롤러 전송 드라이버)"
$usbip = "C:\Program Files\USBip\usbip.exe"
if (Test-Path $usbip) { Ok "이미 설치됨" }
else {
    $inst = "$pkg\installer\USBip-0.9.8.1-x64.exe"
    if (-not (Test-Path $inst)) { throw "installer\USBip-0.9.8.1-x64.exe 가 없습니다" }
    Start-Process $inst -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART" -Wait
    if (Test-Path $usbip) { Ok "설치됨" } else { Warn "무인 설치 실패 → 설치 파일을 직접 실행하세요: $inst"; Start-Process $inst -Wait }
    if (-not (Test-Path $usbip)) { throw "usbip-win2 설치 실패" }
}

Step "3. OnePad 설정 복원"
$data = "$env:LOCALAPPDATA\OnePad"
New-Item -ItemType Directory -Force $data | Out-Null
if ($ForceConfig -or -not (Test-Path "$data\onepad.ini")) { Copy-Item "$pkg\config\onepad.ini" "$data\onepad.ini" -Force; Ok "onepad.ini 복원" } else { Ok "onepad.ini 이미 있음(유지, 덮어쓰려면 -ForceConfig)" }
if (-not (Test-Path "$data\firmware-backup.ini") -and (Test-Path "$pkg\config\firmware-backup.ini")) { Copy-Item "$pkg\config\firmware-backup.ini" "$data\firmware-backup.ini"; Ok "firmware-backup.ini 복원(원본 펌웨어 값)" }

Step "4. OnePad 첫 실행 (로그온 작업 자기 등록)"
Get-Process onepad -ErrorAction SilentlyContinue | ForEach-Object { & "$pkg\bin\onepad.exe" --stop; Start-Sleep 5 }
Start-Process "$pkg\bin\onepad.exe" -WorkingDirectory "$pkg\bin"
Start-Sleep 8
$task = schtasks /Query /TN OnePad 2>$null
if ($LASTEXITCODE -eq 0) { Ok "작업 스케줄러 'OnePad' 등록됨 (로그온 8초 뒤, 최고 권한)" } else { Warn "작업 등록이 안 보입니다. 트레이의 OnePad → '로그온 시 자동 시작'을 켜거나 로그를 확인: $data\onepad.log" }

if (-not $SkipAzahar) {
    Step "5. Azahar 2126.1 (D:\Azahar, SDL 교착 우회 래퍼로 실행)"
    $az = "D:\Azahar"
    if (-not (Test-Path "$az\azahar.exe")) {
        $zip = "$pkg\installer\azahar-windows-msys2-2126.1.zip"
        if (-not (Test-Path $zip)) { throw "installer\azahar-windows-msys2-2126.1.zip 이 없습니다 (https://github.com/azahar-emu/azahar/releases/tag/2126.1)" }
        $tmp = Join-Path $env:TEMP "azahar-2126.1-extract"; if (Test-Path $tmp) { Remove-Item $tmp -Recurse -Force }
        Expand-Archive -Path $zip -DestinationPath $tmp -Force
        $root = Get-ChildItem $tmp -Directory | Select-Object -First 1
        New-Item -ItemType Directory -Force $az | Out-Null
        Copy-Item "$($root.FullName)\*" $az -Recurse -Force
        Ok "Azahar 압축 해제 → $az"
    } else { Ok "Azahar 이미 있음: $az" }
    Copy-Item "$pkg\bin\azahar-launch.exe" "$az\azahar-launch.exe" -Force
    $sh = New-Object -ComObject WScript.Shell
    foreach ($lnk in "C:\ProgramData\Microsoft\Windows\Start Menu\Programs\Azahar.lnk", "C:\Users\Public\Desktop\Azahar.lnk") {
        $l = $sh.CreateShortcut($lnk); $l.TargetPath = "$az\azahar-launch.exe"; $l.WorkingDirectory = $az; $l.IconLocation = "$az\azahar.exe,0"; $l.Description = "Azahar (azahar-launch.exe: SDL WGI/Steam Deck hidapi 끔)"; $l.Save()
    }
    Ok "바로가기(시작 메뉴, 공용 바탕화면) → azahar-launch.exe"
    if (-not $SkipSteamShortcut) {
        if (Test-Path "D:\Steam\steam.exe") {
            & "$pkg\tools\Add-SteamShortcut.ps1" -Name "Azahar" -Exe "$az\azahar-launch.exe" -StartDir "$az\" -Icon "$az\azahar.exe" -SteamDir "D:\Steam"
            Ok "Steam 라이브러리에 Azahar(비Steam 게임) 추가 — Azahar는 Steam에서 실행할 것"
        } else { Warn "D:\Steam\steam.exe 가 없어 Steam 항목은 건너뜀. 나중에 tools\Add-SteamShortcut.ps1 실행" }
    }
}

Step "완료"
Write-Host @"
   남은 일:
   - 재부팅 (HidHide 필터가 Xbox 스택에 붙으려면 필수). 재부팅 후 Steam 컨트롤러 설정에 'Steam Deck 컨트롤러'만 보여야 정상.
   - Steam은 반드시 일반 권한으로 실행 (관리자 셸에서 steam.exe를 직접 띄우지 말 것). 재부팅 후 자동 시작이면 OK.
   - Azahar는 Steam 라이브러리에서 실행. 바로가기 직접 실행은 Steam이 켜진 동안 실제 패드가 숨겨져 컨트롤러가 안 잡힘.
   - 검증은 재설치-가이드.md의 '검증' 절 참고.
"@
if (-not $NoReboot) {
    $a = Read-Host "지금 재부팅할까요? (y/N)"
    if ($a -match '^[yY]') { Restart-Computer -Force }
}
