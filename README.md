# OnePad — abxylute 3D One 컨트롤러를 Steam Deck 컨트롤러처럼

**목적**: 3D One 핸드헬드의 내장 컨트롤러(터치패드 2개, 백버튼 2개, 자이로)를 **최대한 Steam Deck 컨트롤러처럼** 쓰게 해서 편의성을 높인다.
Steam에서는 Steam Input이 진짜 Steam Deck 컨트롤러로 인식해 트랙패드·자이로·백버튼을 Steam 설정으로 매핑하고,
Steam 밖(데스크톱, 일반 앱)에서는 터치패드가 마우스, 백버튼이 마우스 버튼이 된다. 가상 패드 에뮬레이터(ViGEm 등)를 쓰지 않는다.

**Purpose**: Make the built-in controller of the abxylute 3D One handheld (two trackpads, two back grips, gyro) behave
**as close to a Steam Deck controller as possible**. Inside Steam, Steam Input sees a genuine Steam Deck controller and maps
trackpads, gyro and grips through its own configurator; outside Steam the trackpads become a mouse and the grips become mouse buttons.
No virtual-gamepad emulator (ViGEm etc.) is involved.

> 설치: [Releases](https://github.com/nautymac/OnePad/releases)에서 `OnePad-Setup-x.y.z.exe` 를 받아 실행 → 재부팅.
> Install: download `OnePad-Setup-x.y.z.exe` from Releases, run it, reboot.

---

## 동작하는 기능 / What works

| 기능 | Feature | 상태 |
|---|---|---|
| Steam이 3D One을 **Steam Deck 컨트롤러**로 인식 (USB/IP로 꽂는 가상 장치, 28DE:1205) | Steam recognizes the 3D One as a **Steam Deck controller** (virtual USB device over USB/IP) | ✅ |
| Steam 게임·Steam 창에서는 버튼·스틱·트리거·트랙패드·백버튼(L4/R4)·자이로가 전부 Steam Input 설정을 따름 | In Steam games and the Steam client every input (buttons, sticks, triggers, trackpads, L4/R4 grips, gyro) follows the Steam Input configuration | ✅ |
| 트랙패드 **탭 = 트랙패드 클릭** (3D One 패드에 물리 클릭이 없어서 짧은 탭을 압력 포함 클릭으로 변환, 두 번 탭 = 더블클릭) | Trackpad **tap = trackpad click** (the 3D One pads have no physical click; a short tap is sent as a click with pressure, double tap = double click) | ✅ |
| 데스크톱·비Steam 앱: 오른쪽 패드 = 마우스 포인터, 왼쪽 패드 = 스크롤, 탭 = 왼쪽 클릭, 민감도·반전 설정 | Desktop / non-Steam apps: right pad = mouse pointer, left pad = scroll, tap = left click, sensitivity and axis flip | ✅ |
| 데스크톱·비Steam 앱: 왼쪽 백버튼 = 왼쪽 클릭, 오른쪽 백버튼 = 오른쪽 클릭 (키 조합·모드 전환으로 바꿀 수 있음) | Desktop: left grip = left click, right grip = right click (configurable: key combos, mode shift, gyro hold) | ✅ |
| 실제 Xbox 패드 숨김 (HidHide) — Steam과 게임에는 Steam Deck 하나만 보여 2P 중복 입력이 없음. Steam이 꺼지면 자동으로 실제 패드 복귀 | Hides the real Xbox pad (HidHide) so Steam and games see exactly one controller; the real pad returns automatically when Steam is not running | ✅ |
| 패드별 펌웨어 진동(햅틱) 켜기/끄기, 펌웨어 백버튼 매핑 해제·원복 | Per-pad firmware haptics on/off, firmware grip mapping unmapped and restored on exit | ✅ |
| 자이로 → 마우스 (데스크톱, 옵션) | Gyro → mouse on the desktop (optional) | ✅ |
| 로그온 자동 시작, 트레이 아이콘, 컨트롤러가 없으면 아무것도 하지 않음, 종료 시 펌웨어·장치 원복 | Starts at logon, tray icon, does nothing when the controller is absent, restores firmware and device nodes on exit | ✅ |
| Azahar(3DS 에뮬레이터) 교착 우회 실행기 — Steam 라이브러리에서 실행 | Deadlock-free launcher for Azahar (3DS emulator), registered in the Steam library | ✅ |
| 3D One 단축키(옵션): AstraDepth3D Ctrl+F4 종료 차단, 오른쪽 Alt = 한/영 | Optional hotkeys: block Ctrl+F4 killing AstraDepth3D, Right Alt = Korean/English toggle | ✅ |
| Steam 없이 Steam Deck 기능(Steam 트랙볼·자이로 조준) | Steam Deck features without Steam (Steam's trackball/gyro aim) | ✖ Steam Input 자체가 필요 / needs Steam Input itself |
| L5/R5 백버튼 | L5/R5 grips | ✖ 3D One에는 없음 / not on the 3D One |

## 어떻게 동작하나 / How it works

```
3D One 컨트롤러 (USB 복합 장치)
 ├─ XInput (Xbox 360)  ──HidHide로 숨김──▶ (Steam/게임에는 안 보임)
 └─ 벤더 HID (패드 좌표, 백버튼, IMU, 125 Hz) ──▶ onepad.exe
                                                  ├─ Steam 게임/Steam 창이 앞: 가상 Steam Deck 프레임 (USB/IP → usbip-win2 vhci)
                                                  └─ 그 외: SendInput (마우스 이동·클릭·스크롤·키)
```

- `onepad.exe` 안에 USB/IP 장치 서버가 있고, usbip-win2(서명된 vhci 드라이버)가 그것을 로컬 USB 장치로 꽂는다. Steam은 이 장치를 Steam Deck(Neptune)으로 인식해 점유하고 설정을 보낸다.
- 라우팅은 앞 창을 보고 결정한다: Steam 게임(오버레이가 붙은 프로세스) 또는 Steam 클라이언트 창이면 Steam, 아니면 OnePad 엔진.
- HidHide는 OnePad만 화이트리스트에 넣고 실제 Xbox 패드 노드를 숨긴다. Steam이 (재)시작하는 순간 숨김이 켜져 있어야 하므로 Steam 종료 후 45초, OnePad 재시작 동안에도 숨김을 유지한다.
- 펌웨어 제어(터치패드 모드, 햅틱, 백버튼 매핑)는 기기에 들어 있는 벤더 SDK(`sapjoyapis.dll`, Gear 설정 앱의 것)를 그대로 호출한다.

## 설치 / Install

1. Releases에서 `OnePad-Setup-x.y.z.exe` 실행(관리자 권한 요청). 설치 중 HidHide, usbip-win2, (선택) AutoHotkey v2, Azahar를 공식 릴리스에서 내려받는다.
2. 재부팅. HidHide 필터는 재부팅 뒤에 Xbox 스택에 붙는다.
3. Steam → 설정 → 컨트롤러에 **Steam Deck 컨트롤러 하나만** 보이면 정상. 둘 다 보이면 Steam을 한 번 재시작.
4. Steam을 관리자 권한으로 실행하지 말 것(Steam Input이 깨진다).

Requirements: Windows 11, abxylute 3D One (controller VID 2993 / PID 0012), Steam. Everything else is downloaded by the installer.

## 설정 / Configuration

`%LOCALAPPDATA%\OnePad\onepad.ini` (트레이 메뉴에서도 주요 항목 변경 가능). 키 설명은 파일 안 주석과 [docs/설계-설정.md](docs/설계-설정.md) 참고.
Config lives in `%LOCALAPPDATA%\OnePad\onepad.ini`; the tray menu covers the common options. Full key reference (Korean) in [docs/설계-설정.md](docs/설계-설정.md).

## 문서 / Docs

- [docs/설계-설정.md](docs/설계-설정.md) — 설계, 모든 ini 키, 벤더 리포트 구조, 문제 해결 메모 (Korean)
- [docs/재설치-가이드.md](docs/재설치-가이드.md) — 재설치 절차, 검증 명령, 함정 모음 (Korean)
- `tools/probe.cpp` — 진단 CLI (`probe.exe info | dump | keymap ...`)

## 빌드 / Build

LLVM-MinGW (`winget install MartinStorsjo.LLVM-MinGW.UCRT`) → `build.ps1`. 설치 파일: Inno Setup 6 → `ISCC.exe installer\OnePad-Setup.iss`.
빌드 전 실행 중인 `onepad.exe --stop` 필수.

## 라이선스 / License

MIT (OnePad 소스). 설치 시 내려받는 HidHide(BSD-3), usbip-win2(GPL-3), AutoHotkey(GPL-2), Azahar(GPL-2)는 각자의 라이선스를 따른다.
`sapjoyapis.dll`은 3D One에 포함된 벤더 SDK이며 이 저장소에 포함하지 않는다(OnePad가 기기의 `C:\Program Files (x86)\Gear\Settings`에서 로드).
Steam Deck 디스크립터/feature 스텁은 HIDMaestro의 MIT 프로필을 참고했다.
