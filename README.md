# OnePad

**abxylute 3D One 컨트롤러를 Steam Deck 컨트롤러처럼 — Use the abxylute 3D One's controller like a Steam Deck controller**

[한국어](#한국어) · [English](#english) · [Releases](https://github.com/nautymac/OnePad/releases)

---

## 한국어

### 목적

3D One 핸드헬드에 내장된 컨트롤러(터치패드 2개, 백버튼(L4, R4) 2개, 자이로)를 **최대한 Steam Deck 컨트롤러처럼** 쓰기 위한 프로그램입니다. 목적은 편의성입니다.
Steam에서는 Steam Input이 이 컨트롤러를 진짜 Steam Deck 컨트롤러로 인식하므로 트랙패드·자이로·백버튼을 Steam 컨트롤러 설정 화면에서 게임별로 매핑할 수 있고,
Steam 밖(데스크톱, 일반 프로그램)에서는 터치패드가 마우스, 백버튼(L4, R4)이 마우스 버튼으로 동작합니다. 가상 패드 에뮬레이터(ViGEm 등)는 쓰지 않습니다.

### Steam 컨트롤러와 다른 점 (꼭 읽어 주세요)

- **L5 / R5 백버튼이 없습니다.** 3D One에는 백버튼이 좌우 하나씩뿐이라 L4 / R4로만 잡힙니다. Steam 설정에서 L5 / R5에 넣은 동작은 발동하지 않습니다.
- **터치패드에 물리 클릭이 없습니다.** 그래서 OnePad가 **짧은 탭(톡 치기)을 트랙패드 클릭으로** 바꿔 보냅니다. Steam 설정의 "트랙패드 클릭"에 넣은 동작은 탭으로 발동하고, 두 번 탭은 더블클릭이 됩니다. 손가락을 올려 두거나 움직이는 것은 "터치"일 뿐 클릭이 아니며, 꾹 누르는 압력 클릭도 없습니다.
- 그 밖의 것(버튼, 스틱, 트리거, 트랙패드 터치·이동, 자이로, Steam/QAM 버튼)은 Steam Deck 컨트롤러와 같이 동작합니다.

### 용어와 예시

- **백버튼(L4, R4)**: 3D One 뒷면의 버튼 두 개. 왼쪽이 L4, 오른쪽이 R4이고 Steam에는 Steam Deck의 L4 / R4로 전달됩니다(펌웨어 이름은 M1/M2).
  - 예: Steam의 Azahar 설정에서 L4 = Tab → Azahar에서 L4를 누르면 Tab이 들어감.
  - 예: 데스크톱에서는 L4 = 왼쪽 클릭, R4 = 오른쪽 클릭(onepad.ini `[grip.left]`/`[grip.right]`의 `action=mouse:left` / `mouse:right`). `key:ALT+TAB`처럼 키 조합도 가능.
- **패드 모드 "마우스 / 스크롤 / D패드"**: OnePad가 패드 좌표를 읽어 직접 변환하는 모드. 기본은 오른쪽 패드 = 마우스, 왼쪽 패드 = 스크롤.
- **패드 모드 "펌웨어: …"**: 패드 처리를 OnePad가 아니라 **컨트롤러 펌웨어**에 맡기는 모드. Gear 설정 앱에서 고르던 것과 같습니다.
  - 펌웨어: 터치패드 — Windows 정밀 터치패드로 잡힘(두 손가락 스크롤 등 Windows 제스처, 움직일 때 진동). OnePad의 탭 클릭·민감도는 적용 안 됨.
  - 펌웨어: 휠 — 마우스 휠로 잡힘.
  - 펌웨어: 왼쪽 스틱 / 오른쪽 스틱 — Xbox 패드의 스틱으로 잡힘. 예: Steam 없이 돌리는 게임에서 오른쪽 패드를 조준 스틱으로.
- **시프트**: `action=modeshift`로 둔 백버튼(L4 또는 R4)을 **누르고 있는 동안만** 바뀌는 패드 모드. 키보드 Shift처럼 "누른 채로 다른 기능".
  - 예: 오른쪽 패드가 평소 마우스, 시프트가 스크롤이면 R4를 누른 채 패드를 밀 때만 스크롤. 기본 설정(백버튼 = 마우스 클릭)에서는 쓰이지 않음.

### 동작하는 기능

| 기능 | 상태 |
|---|---|
| Steam이 3D One을 **Steam Deck 컨트롤러**로 인식 (USB/IP로 꽂는 가상 USB 장치, 28DE:1205) | ✅ |
| Steam 게임·Steam 창에서는 버튼·스틱·트리거·트랙패드·백버튼(L4/R4)·자이로가 전부 Steam Input 설정을 따름 | ✅ |
| 트랙패드 **탭 = 트랙패드 클릭**, 두 번 탭 = 더블클릭 (Steam 안팎 모두) | ✅ |
| 데스크톱·비Steam 프로그램: 오른쪽 패드 = 마우스 포인터, 왼쪽 패드 = 스크롤 휠, 탭 = 왼쪽 클릭. 민감도·스크롤 속도·축 반전 설정 | ✅ |
| 데스크톱: 왼쪽 백버튼 = 왼쪽 클릭, 오른쪽 백버튼 = 오른쪽 클릭 (키 조합, 모드 전환, 자이로 홀드로 바꿀 수 있음) | ✅ |
| 실제 Xbox 패드 숨김(HidHide) — Steam과 게임에는 Steam Deck 컨트롤러 하나만 보여 2P 중복 입력이 없음 | ✅ |
| 패드별 펌웨어 진동(햅틱) 켜기/끄기, 펌웨어 백버튼 매핑 해제·종료 시 원복 | ✅ |
| 자이로 → 마우스 (데스크톱, 선택) | ✅ |
| 로그온 자동 시작, 트레이 아이콘, 컨트롤러가 없으면 아무것도 하지 않음, 종료 시 펌웨어·장치 원복 | ✅ |
| Azahar(3DS 에뮬레이터) 교착 우회 실행기 + Steam 라이브러리 등록 | ✅ |
| 3D One 단축키(선택, AutoHotkey v2 자동 설치): ① AstraDepth3D가 Ctrl+F4를 전역 종료 단축키로 잡아 두는데 geo-11 같은 3D 픽스 도구의 단축키와 겹치면 게임 중 앱이 꺼지므로 Ctrl+F4를 먼저 선점해 차단 ② 다국어 키보드 사용자가 입력 언어 전환(한/영 등)을 오른쪽 Alt 한 키로 하도록 변환 | ✅ |
| L5 / R5 백버튼 | ✖ 하드웨어에 없음 |
| 트랙패드 물리 클릭·압력 클릭 | ✖ 탭으로 대체 |
| Steam 없이 Steam Deck 기능(Steam의 트랙볼·자이로 조준) | ✖ Steam Input이 필요 |

### 설치

1. [Releases](https://github.com/nautymac/OnePad/releases)에서 `OnePad-Setup-x.y.z.exe`를 받아 실행합니다(관리자 권한 요청). 설치 중 HidHide, usbip-win2, (선택) AutoHotkey v2, Azahar를 각 공식 릴리스에서 내려받습니다.
2. 재부팅합니다. HidHide 필터는 재부팅 뒤에 Xbox 입력 스택에 붙습니다.
3. Steam → 설정 → 컨트롤러에 **Steam Deck 컨트롤러 하나만** 보이면 정상입니다. 3D One이 같이 보이면 Steam을 한 번 재시작하세요.
4. Steam을 관리자 권한으로 실행하지 마세요(Steam Input이 깨집니다).

요구 사항: Windows 11, abxylute 3D One(컨트롤러 VID 2993 / PID 0012), Steam. 나머지는 설치기가 내려받습니다.

### 어떻게 동작하나

```
3D One 컨트롤러 (USB 복합 장치)
 ├─ XInput (Xbox 360)  ──HidHide로 숨김──▶ (Steam/게임에는 안 보임)
 └─ 벤더 HID (패드 좌표, 백버튼, IMU, 125 Hz) ──▶ onepad.exe
                                                  ├─ Steam 게임/Steam 창이 앞: 가상 Steam Deck 프레임 (USB/IP → usbip-win2)
                                                  └─ 그 외: 마우스 이동·클릭·스크롤·키 입력
```

- `onepad.exe` 안에 USB/IP 장치 서버가 있고, usbip-win2(서명된 드라이버)가 그것을 로컬 USB 장치로 꽂습니다. Steam은 이 장치를 Steam Deck(Neptune)으로 인식해 점유하고 설정을 보냅니다.
- 라우팅은 앞 창으로 결정합니다. Steam 게임(오버레이가 붙은 프로세스)이나 Steam 클라이언트 창이면 Steam, 아니면 OnePad가 처리합니다.
- HidHide는 OnePad만 화이트리스트에 넣고 실제 Xbox 패드 노드를 숨깁니다. Steam이 (재)시작하는 순간 이미 숨겨져 있어야 하므로 컨트롤러가 꽂혀 있는 한 항상 숨깁니다.
- 펌웨어 제어(터치패드 모드, 햅틱, 백버튼 매핑)는 기기에 들어 있는 벤더 SDK(`sapjoyapis.dll`, Gear 설정 앱의 것)를 그대로 호출합니다.

### 설정

`%LOCALAPPDATA%\OnePad\onepad.ini`. 자주 쓰는 항목(패드 모드, 민감도, 스크롤 속도, 햅틱, 자이로, Deck 에뮬 켜기/끄기)은 트레이 메뉴에서 바꿀 수 있습니다.
전체 키 설명은 [docs/설계-설정.md](docs/설계-설정.md), 재설치 절차와 함정 모음은 [docs/재설치-가이드.md](docs/재설치-가이드.md)에 있습니다.

### 빌드

LLVM-MinGW(`winget install MartinStorsjo.LLVM-MinGW.UCRT`) → `build.ps1`. 설치 파일은 Inno Setup 6 → `ISCC.exe installer\OnePad-Setup.iss`. 빌드 전 실행 중인 `onepad.exe --stop`이 필요합니다.

---

## English

### Purpose

OnePad makes the controller built into the abxylute 3D One handheld (two trackpads, two back buttons L4/R4, gyro) behave **as close to a Steam Deck controller as possible**. The goal is convenience.
Inside Steam, Steam Input sees a genuine Steam Deck controller, so trackpads, gyro and grips can be mapped per game in the Steam controller configurator.
Outside Steam (desktop, ordinary programs) the trackpads act as a mouse and the grips as mouse buttons. No virtual-gamepad emulator (ViGEm etc.) is involved.

### How it differs from a real Steam Deck / Steam Controller (please read)

- **There are no L5 / R5 grips.** The 3D One has one back button per side, exposed as L4 / R4. Anything bound to L5 / R5 in Steam will never fire.
- **The trackpads have no physical click.** OnePad therefore turns a **short tap into a trackpad click**. Whatever you bind to "trackpad click" in Steam fires on a tap, and a double tap is a double click. Resting or moving a finger is only a "touch", not a click, and there is no pressure click.
- Everything else (buttons, sticks, triggers, trackpad touch and movement, gyro, Steam/QAM buttons) behaves like a Steam Deck controller.

### Terms and examples

- **Back buttons (L4, R4)**: the two buttons on the back of the 3D One. Left is L4, right is R4, and Steam sees them as the Steam Deck's L4 / R4 (the firmware calls them M1/M2).
  - Example: in Steam's Azahar configuration L4 = Tab → pressing L4 in Azahar sends Tab.
  - Example: on the desktop L4 = left click, R4 = right click (`action=mouse:left` / `mouse:right` under `[grip.left]` / `[grip.right]` in onepad.ini). Key combos such as `key:ALT+TAB` work too.
- **Pad modes "mouse / scroll / dpad"**: OnePad reads the pad coordinates and translates them itself. Default: right pad = mouse, left pad = scroll.
- **Pad modes "firmware: …"**: the pad is handled by the **controller firmware** instead of OnePad, exactly as the Gear settings app offers.
  - firmware: touchpad — appears as a Windows precision touchpad (two-finger scroll and other Windows gestures, haptics while moving). OnePad's tap-click and sensitivity do not apply.
  - firmware: wheel — appears as a mouse wheel.
  - firmware: left stick / right stick — appears as an Xbox stick. Example: use the right pad as an aiming stick in a game launched without Steam.
- **Shift**: the pad mode used **only while** a back button set to `action=modeshift` (L4 or R4) is held, like the Shift key on a keyboard.
  - Example: right pad is mouse normally and scroll in shift mode → it scrolls only while R4 is held. Unused with the default setup (back buttons = mouse clicks).

### What works

| Feature | Status |
|---|---|
| Steam recognizes the 3D One as a **Steam Deck controller** (virtual USB device over USB/IP, 28DE:1205) | ✅ |
| In Steam games and the Steam client every input (buttons, sticks, triggers, trackpads, L4/R4 grips, gyro) follows the Steam Input configuration | ✅ |
| Trackpad **tap = trackpad click**, double tap = double click (inside and outside Steam) | ✅ |
| Desktop / non-Steam apps: right pad = mouse pointer, left pad = scroll wheel, tap = left click; sensitivity, scroll speed and axis flip are configurable | ✅ |
| Desktop: left grip = left click, right grip = right click (configurable: key combos, mode shift, gyro hold) | ✅ |
| Hides the real Xbox pad (HidHide) so Steam and games see exactly one controller, no duplicate player-2 input | ✅ |
| Per-pad firmware haptics on/off; firmware grip mapping unmapped while running and restored on exit | ✅ |
| Gyro → mouse on the desktop (optional) | ✅ |
| Starts at logon, tray icon, does nothing when the controller is absent, restores firmware and device nodes on exit | ✅ |
| Deadlock-free launcher for Azahar (3DS emulator) and registration in the Steam library | ✅ |
| Optional 3D One hotkeys (installs AutoHotkey v2 automatically): ① AstraDepth3D registers Ctrl+F4 as its global quit hotkey, which collides with hotkeys of 3D-fix tools such as geo-11 and kills the app mid-game, so Ctrl+F4 is pre-empted and blocked ② for multilingual keyboards, Right Alt becomes the input-language toggle (Korean/English etc.) | ✅ |
| L5 / R5 grips | ✖ not present on the hardware |
| Physical or pressure trackpad click | ✖ replaced by tap |
| Steam Deck features without Steam (Steam's trackball / gyro aim) | ✖ needs Steam Input itself |

### Install

1. Download `OnePad-Setup-x.y.z.exe` from [Releases](https://github.com/nautymac/OnePad/releases) and run it (asks for administrator rights). The installer downloads HidHide, usbip-win2, optional AutoHotkey v2 and Azahar from their official releases.
2. Reboot. The HidHide filter attaches to the Xbox input stack only after a reboot.
3. Steam → Settings → Controller should list **only one Steam Deck controller**. If the 3D One shows up as well, restart Steam once.
4. Never run Steam as administrator (it breaks Steam Input).

Requirements: Windows 11, abxylute 3D One (controller VID 2993 / PID 0012), Steam. Everything else is downloaded by the installer.

### How it works

```
3D One controller (USB composite device)
 ├─ XInput (Xbox 360)  ──hidden by HidHide──▶ (invisible to Steam and games)
 └─ vendor HID (pad coordinates, grips, IMU, 125 Hz) ──▶ onepad.exe
                                                        ├─ Steam game / Steam window in front: virtual Steam Deck frames (USB/IP → usbip-win2)
                                                        └─ otherwise: mouse movement, clicks, scroll, key input
```

- `onepad.exe` contains a USB/IP device server; usbip-win2 (signed driver) attaches it as a local USB device. Steam recognizes it as a Steam Deck (Neptune), claims it and sends its settings.
- Routing is decided by the foreground window: a Steam game (a process with the Steam overlay) or the Steam client → Steam; anything else → OnePad.
- HidHide whitelists only OnePad and hides the real Xbox pad's device nodes. Steam adopts the real pad within a second of starting and never lets go, so the pad stays hidden whenever the controller is connected.
- Firmware control (trackpad mode, haptics, grip mapping) goes through the vendor SDK that ships on the device (`sapjoyapis.dll` from the Gear settings app).

### Configuration

`%LOCALAPPDATA%\OnePad\onepad.ini`. The tray menu covers the common options (pad modes, sensitivity, scroll speed, haptics, gyro, Deck emulation on/off).
Full key reference (Korean): [docs/설계-설정.md](docs/설계-설정.md). Reinstall guide and pitfalls (Korean): [docs/재설치-가이드.md](docs/재설치-가이드.md).

### Build

LLVM-MinGW (`winget install MartinStorsjo.LLVM-MinGW.UCRT`) → `build.ps1`. Installer: Inno Setup 6 → `ISCC.exe installer\OnePad-Setup.iss`. Stop a running instance first with `onepad.exe --stop`.

---

## License / 라이선스

MIT for OnePad itself. HidHide (BSD-3), usbip-win2 (GPL-3), AutoHotkey (GPL-2) and Azahar (GPL-2) are downloaded at install time under their own licenses.
`sapjoyapis.dll` is the vendor SDK shipped on the 3D One and is not part of this repository (OnePad loads it from `C:\Program Files (x86)\Gear\Settings`).
The Steam Deck descriptor / feature-report stubs follow HIDMaestro's MIT-licensed profile.
