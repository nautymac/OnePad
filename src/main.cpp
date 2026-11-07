// OnePad - abxylute 3D One trackpad / grip / gyro translation service (tray application)
#include "common.h"
#include "config.h"
#include "vendor.h"
#include "devctl.h"
#include "output.h"
#include "engine.h"
#include "deck.h"
#include "hidhide.h"
#include "autostart.h"
#include <shellapi.h>
#include <commctrl.h>

using namespace onepad;

namespace {

const UINT WM_TRAY = WM_APP + 10;
const UINT WM_APPLY = WM_APP + 11;
const UINT WM_DEVICE = WM_APP + 12;
const UINT WM_SAVEBIAS = WM_APP + 13;
const UINT WM_DECK = WM_APP + 14;
const UINT ID_TIMER = 1;

enum MenuId {
    ID_ENABLED = 1, ID_PAD_R = 100, ID_PAD_L = 200, ID_HAPTIC_R = 300, ID_HAPTIC_L = 301,
    ID_GYRO = 400, ID_GYRO_CAL = 420, ID_OPEN_CFG = 500, ID_RELOAD = 501, ID_OPEN_LOG = 502, ID_RESTORE_FW = 503, ID_STATUS = 600, ID_DECK = 700, ID_DECK_STATUS = 701, ID_AUTOSTART = 710, ID_EXIT = 999
};

struct FirmwareBackup { bool valid = false; uint8_t tpMode[2] = {1, 1}; bool tpOn[2] = {true, true}; uint8_t keymap[2][2] = {{28, 32}, {28, 32}}; };

class App {
public:
    int Run(HINSTANCE hinst, const std::string& args) {
        dataDir_ = DataDir(); cfgPath_ = dataDir_ + L"onepad.ini"; backupPath_ = dataDir_ + L"firmware-backup.ini";
        Log::I().Open(dataDir_ + L"onepad.log");
        bool console = args.find("--console") != std::string::npos;
        if (console) { AllocConsole(); FILE* f; freopen_s(&f, "CONOUT$", "w", stdout); Log::I().SetConsole(true); }
        LOGI("==== OnePad start (elevated=%d) args='%s'", IsElevated() ? 1 : 0, args.c_str());
        if (args.find("--config") != std::string::npos) { EnsureConfig(); ShellExecuteW(nullptr, L"open", L"notepad.exe", cfgPath_.c_str(), nullptr, SW_SHOW); return 0; }
        if (!vendor_.Load()) { MessageBoxW(nullptr, L"sapjoyapis.dll 을 불러올 수 없습니다. bin 폴더에 DLL이 있어야 합니다.", L"OnePad", MB_ICONERROR); return 1; }
        if (args.find("--restore") != std::string::npos) { RestoreOnly(); return 0; }
        if (args.find("--uninstall") != std::string::npos) {   // stop a running instance, drop the logon task, restore the controller
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\OnePad.Stop"); if (ev) { SetEvent(ev); CloseHandle(ev); Sleep(4000); }
            Autostart::Uninstall(); RestoreOnly(); return 0;
        }
        if (args.find("--stop") != std::string::npos) {
            HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, L"Local\\OnePad.Stop");
            if (ev) { SetEvent(ev); CloseHandle(ev); LOGI("stop requested"); } else LOGI("stop: no running instance");
            return 0;
        }

        HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\OnePad.SingleInstance");
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (mutex) CloseHandle(mutex);   // do not keep the running instance's mutex alive from here
            LOGW("already running");
            MessageBoxW(nullptr, L"OnePad 가 이미 실행 중입니다. (트레이 아이콘 확인)", L"OnePad", MB_ICONINFORMATION | MB_TOPMOST);
            return 0;
        }
        (void)mutex;
        if (!IsElevated()) LOGW("not elevated: HID collection nodes cannot be disabled; pointer/wheel from the pads will leak to Windows");

        EnsureConfig();
        LoadConfig(cfgPath_, cfg_);
        engine_.SetConfig(cfg_);
        ApplyEnabled();
        engine_.onApplyNeeded = [this] { PostMessageW(hwnd_, WM_APPLY, 0, 0); };
        engine_.onBiasUpdated = [this] { PostMessageW(hwnd_, WM_SAVEBIAS, 0, 0); };

        WNDCLASSW wc{}; wc.lpfnWndProc = &App::WndProcThunk; wc.hInstance = hinst; wc.lpszClassName = L"OnePadTrayWindow";
        RegisterClassW(&wc);
        hwnd_ = CreateWindowExW(0, wc.lpszClassName, L"OnePad", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hinst, this);
        LOGI("window=%p (err %lu)", hwnd_, GetLastError());
        stopEvent_ = CreateEventW(nullptr, TRUE, FALSE, L"Local\\OnePad.Stop");
        AddTray(hinst);

        if (!vendor_.Create([this](const VendorState& s) { OnVendorState(s); },
                            [this](bool arrived) { PostMessageW(hwnd_, WM_DEVICE, arrived ? 1 : 0, 0); })) {
            LOGE("SAPJoy_Create failed");
        }
        hidhide_.Locate(cfg_.hidhide.cli);
        UpdateCloak(true);
        if (cfg_.deck.enabled) StartDeck();
        // Self-registering logon autostart: the first run installs the Task Scheduler entry, nothing else to do by hand.
        autostartOn_ = Autostart::Installed();
        if (cfg_.autostart && !autostartOn_) { autostartOn_ = Autostart::Install(); LOGI("autostart: %s", autostartOn_ ? "registered" : "registration failed"); }
        SetTimer(hwnd_, ID_TIMER, 500, nullptr);

        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Shutdown();
        return 0;
    }

private:
    static LRESULT CALLBACK WndProcThunk(HWND h, UINT m, WPARAM w, LPARAM l) {
        App* self;
        if (m == WM_NCCREATE) { self = (App*)((CREATESTRUCTW*)l)->lpCreateParams; SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)self); self->hwnd_ = h; }
        else self = (App*)GetWindowLongPtrW(h, GWLP_USERDATA);
        return self ? self->WndProc(h, m, w, l) : DefWindowProcW(h, m, w, l);
    }
    LRESULT WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        switch (m) {
            case WM_TRAY:
                if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_LBUTTONUP || LOWORD(l) == WM_CONTEXTMENU) ShowMenu();
                return 0;
            case WM_COMMAND: OnCommand(LOWORD(w)); return 0;
            case WM_APPLY: Apply(); return 0;
            case WM_DEVICE:
                if (w) {
                    devicePresent_ = true; lastApplied_ = FirmwareBackup(); keymapApplied_ = false; Backup();
                    if (!cfg_.enabled && PendingRestore()) RestoreFirmware();   // a previous exit could not reach the absent controller
                    Apply();
                    AttachDeck();          // the virtual Steam Deck exists only while the 3D One is connected
                    UpdateCloak(true); UpdateTip();
                }
                else {
                    devicePresent_ = false;
                    // No 3D One: unplug the virtual Deck only if it stays away (sleep/reconnect cycles must not churn the vhci).
                    detachDueTick_ = GetTickCount64() + (uint64_t)std::max(0, cfg_.deck.detachDelaySec) * 1000;
                    UpdateCloak(true); UpdateTip();
                }
                return 0;
            case WM_SAVEBIAS: { double b[2][3]; bool v[2]; engine_.GetBias(b, v); memcpy(cfg_.gyro.bias, b, sizeof b); cfg_.gyro.biasValid[0] = v[0]; cfg_.gyro.biasValid[1] = v[1]; SaveConfig(cfgPath_, cfg_); return 0; }
            case WM_DECK: deckClaimed_ = w != 0; ApplyEnabled(); UpdateTip(); return 0;
            case WM_TIMER:
                if (stopEvent_ && WaitForSingleObject(stopEvent_, 0) == WAIT_OBJECT_0) { LOGI("stop event received (restart expected: keeping HidHide cloak so Steam cannot re-adopt the real pad)"); keepCloak_ = true; PostQuitMessage(0); return 0; }
                if (++cloakTicks_ % 4 == 0) UpdateCloak(false);
                ApplyEnabled();   // foreground may have changed (game <-> desktop)
                if (++timerTicks_ % 4 == 0) UpdateTip();
                if (applyRetry_ && timerTicks_ % 4 == 0 && cfg_.enabled && vendor_.Present()) { applyRetry_ = false; Apply(); }
                if (!vendor_.Present() && deckAttached_ && detachDueTick_ && GetTickCount64() >= detachDueTick_) { detachDueTick_ = 0; LOGI("deck: controller absent for %d s, unplugging virtual controller", cfg_.deck.detachDelaySec); DetachDeck(); }
                if (deckAttached_ && timerTicks_ % 60 == 0 && !DeckEnumerated()) { LOGW("deck: health check: attached but not enumerated, recovering"); deckAttached_ = false; AttachDeck(); }
                return 0;
            case WM_CLOSE: case WM_QUERYENDSESSION: if (m == WM_QUERYENDSESSION) { Shutdown(); return TRUE; } PostQuitMessage(0); return 0;
            case WM_DESTROY: PostQuitMessage(0); return 0;
        }
        if (m == taskbarCreated_) { AddTray((HINSTANCE)GetModuleHandleW(nullptr)); return 0; }
        return DefWindowProcW(h, m, w, l);
    }

    void EnsureConfig() { if (GetFileAttributesW(cfgPath_.c_str()) == INVALID_FILE_ATTRIBUTES) { SaveConfig(cfgPath_, Config()); LOGI("default config written: %ls", cfgPath_.c_str()); } }

    void AddTray(HINSTANCE hinst) {
        taskbarCreated_ = RegisterWindowMessageW(L"TaskbarCreated");
        NOTIFYICONDATAW n{}; n.cbSize = sizeof n; n.hWnd = hwnd_; n.uID = 1; n.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; n.uCallbackMessage = WM_TRAY;
        n.hIcon = (HICON)LoadImageW(hinst, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        if (!n.hIcon) n.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
        wcscpy_s(n.szTip, L"OnePad"); BOOL ok = Shell_NotifyIconW(NIM_ADD, &n); trayAdded_ = true;
        LOGI("tray icon %s (icon=%p)", ok ? "added" : "FAILED", n.hIcon);
        UpdateTip();
    }
    void UpdateTip() {
        if (!trayAdded_) return;
        NOTIFYICONDATAW n{}; n.cbSize = sizeof n; n.hWnd = hwnd_; n.uID = 1; n.uFlags = NIF_TIP;
        std::wstring tip = L"OnePad - ";
        tip += !cfg_.enabled ? L"비활성" : !engine_.DevicePresent() ? L"컨트롤러 없음" : deckClaimed_ ? L"Steam이 Deck 컨트롤러 점유 중" : L"동작 중";
        if (cfg_.deck.enabled) tip += usbip_.Attached() ? L"\nDeck 에뮬: 연결됨" : L"\nDeck 에뮬: 미연결";
        if (cfg_.enabled && engine_.DevicePresent()) { tip += L"\nR: " + Widen(PadModeName(engine_.EffectiveMode(true))) + L"  L: " + Widen(PadModeName(engine_.EffectiveMode(false))); }
        wcsncpy_s(n.szTip, tip.c_str(), _TRUNCATE); Shell_NotifyIconW(NIM_MODIFY, &n);
    }
    void RemoveTray() { if (!trayAdded_) return; NOTIFYICONDATAW n{}; n.cbSize = sizeof n; n.hWnd = hwnd_; n.uID = 1; Shell_NotifyIconW(NIM_DELETE, &n); trayAdded_ = false; }

    static const wchar_t* PadModeLabel(PadMode m) {
        switch (m) {
            case PadMode::Mouse: return L"마우스 (터치패드)"; case PadMode::Scroll: return L"스크롤"; case PadMode::DPad: return L"D패드 (키)";
            case PadMode::NativeTouchpad: return L"펌웨어: 터치패드 (Windows 포인터)"; case PadMode::NativeWheel: return L"펌웨어: 휠";
            case PadMode::NativeLeftStick: return L"펌웨어: 왼쪽 스틱"; case PadMode::NativeRightStick: return L"펌웨어: 오른쪽 스틱"; case PadMode::Off: return L"끔";
        }
        return L"?";
    }
    HMENU PadMenu(int base, const PadConfig& pc) {
        HMENU m = CreatePopupMenu();
        for (int i = 0; i < 8; i++) AppendMenuW(m, MF_STRING | (pc.mode == (PadMode)i ? MF_CHECKED : 0), base + i, PadModeLabel((PadMode)i));
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        for (int i = 0; i < 8; i++) AppendMenuW(m, MF_STRING | (pc.shiftMode == (PadMode)i ? MF_CHECKED : 0), base + 10 + i, (std::wstring(L"시프트: ") + PadModeLabel((PadMode)i)).c_str());
        AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
        // 2026-11-07: re-centred on the value in use (0.8 = "보통"); the user wanted room below it.
        static const double sens[] = {0.4, 0.5, 0.65, 0.8, 1.0, 1.2, 1.6};
        static const int steps[] = {80, 64, 50, 40, 32, 25, 20};   // scroll mode: pad units per wheel notch (40 = "보통", bigger = slower)
        static const wchar_t* sensName[] = {L"가장 느림", L"매우 느림", L"느림", L"보통", L"빠름", L"매우 빠름", L"최대"};
        bool scroll = pc.mode == PadMode::Scroll;   // scroll ignores `sensitivity`; its speed is scroll_step
        for (int i = 0; i < 7; i++) {
            wchar_t lbl[64];
            const wchar_t* def = (scroll ? steps[i] == 50 : std::fabs(sens[i] - 0.8) < 0.01) ? L" — 기본" : L"";   // defaults chosen 2026-11-07
            if (scroll) swprintf(lbl, 64, L"스크롤 속도: %s (step %d)%s", sensName[i], steps[i], def);
            else swprintf(lbl, 64, L"민감도: %s (%.2f)%s", sensName[i], sens[i], def);
            bool on = scroll ? pc.scrollStep == steps[i] : std::fabs(pc.sensitivity - sens[i]) < 0.05;
            AppendMenuW(m, MF_STRING | (on ? MF_CHECKED : 0), base + 20 + i, lbl);
        }
        return m;
    }
    void ShowMenu() {
        HMENU menu = CreatePopupMenu();
        std::wstring status = engine_.DevicePresent() ? (L"컨트롤러 연결됨  펌웨어 " + Widen(vendor_.Version(SapRight))) : L"컨트롤러 없음";
        AppendMenuW(menu, MF_STRING | MF_DISABLED, ID_STATUS, status.c_str());
        AppendMenuW(menu, MF_STRING | (cfg_.enabled ? MF_CHECKED : 0), ID_ENABLED, L"활성");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)PadMenu(ID_PAD_R, cfg_.right), L"오른쪽 패드");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)PadMenu(ID_PAD_L, cfg_.left), L"왼쪽 패드");
        HMENU hap = CreatePopupMenu();
        AppendMenuW(hap, MF_STRING | (cfg_.right.haptics ? MF_CHECKED : 0), ID_HAPTIC_R, L"오른쪽 패드 진동");
        AppendMenuW(hap, MF_STRING | (cfg_.left.haptics ? MF_CHECKED : 0), ID_HAPTIC_L, L"왼쪽 패드 진동");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)hap, L"패드 진동 (햅틱)");
        HMENU gy = CreatePopupMenu();
        bool off = cfg_.gyro.mode == GyroMode::Off;
        AppendMenuW(gy, MF_STRING | (off ? MF_CHECKED : 0), ID_GYRO + 0, L"끔");
        AppendMenuW(gy, MF_STRING | (!off && cfg_.gyro.activate == GyroActivate::Always ? MF_CHECKED : 0), ID_GYRO + 1, L"마우스: 항상");
        AppendMenuW(gy, MF_STRING | (!off && cfg_.gyro.activate == GyroActivate::Grip ? MF_CHECKED : 0), ID_GYRO + 2, L"마우스: 그립(gyro_hold) 누르는 동안");
        AppendMenuW(gy, MF_STRING | (!off && cfg_.gyro.activate == GyroActivate::RightTouch ? MF_CHECKED : 0), ID_GYRO + 3, L"마우스: 오른쪽 패드 터치 중");
        AppendMenuW(gy, MF_STRING | (!off && cfg_.gyro.activate == GyroActivate::Toggle ? MF_CHECKED : 0), ID_GYRO + 4, L"마우스: 토글(gyro_toggle 그립)");
        AppendMenuW(gy, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(gy, MF_STRING, ID_GYRO_CAL, engine_.Calibrating() ? L"자이로 보정 중..." : L"자이로 보정 (기기를 가만히 두고 2초)");
        AppendMenuW(menu, MF_POPUP, (UINT_PTR)gy, L"자이로");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING | (cfg_.deck.enabled ? MF_CHECKED : 0), ID_DECK, L"Steam Deck 컨트롤러 에뮬레이션");
        if (cfg_.deck.enabled) {
            std::wstring ds = usbip_.Attached() ? (deckClaimed_ ? L"   상태: Steam 점유 중 (변환 정지)" : L"   상태: 연결됨, 변환 동작 중") : L"   상태: 가상 장치 미연결 (usbip-win2 설치/attach 필요)";
            AppendMenuW(menu, MF_STRING | MF_DISABLED, ID_DECK_STATUS, ds.c_str());
            AppendMenuW(menu, MF_STRING | MF_DISABLED, ID_DECK_STATUS + 1, hidhide_.Available() ? (hidhide_.Cloaked() ? L"   HidHide: 실제 Xbox 패드 숨김 중" : L"   HidHide: 실제 Xbox 패드 보임") : L"   HidHide 미설치 (Steam에서 패드 중복)");
        }
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_OPEN_CFG, L"설정 파일 열기");
        AppendMenuW(menu, MF_STRING, ID_RELOAD, L"설정 다시 읽기");
        AppendMenuW(menu, MF_STRING, ID_OPEN_LOG, L"로그 열기");
        AppendMenuW(menu, MF_STRING, ID_RESTORE_FW, L"펌웨어 설정 원복 (패드를 공장 동작으로)");
        AppendMenuW(menu, MF_STRING | (autostartOn_ ? MF_CHECKED : 0), ID_AUTOSTART, L"로그온 시 자동 시작");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_EXIT, L"종료");
        POINT pt; GetCursorPos(&pt); SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
        PostMessageW(hwnd_, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }
    void OnCommand(int id) {
        if (id == ID_EXIT) { PostQuitMessage(0); return; }
        if (id == ID_ENABLED) { cfg_.enabled = !cfg_.enabled; ApplyEnabled(); SaveConfig(cfgPath_, cfg_); if (cfg_.enabled) Apply(); else RestoreFirmware(); UpdateTip(); return; }
        if (id == ID_DECK) { cfg_.deck.enabled = !cfg_.deck.enabled; SaveConfig(cfgPath_, cfg_); if (cfg_.deck.enabled) StartDeck(); else StopDeck(); UpdateTip(); return; }
        if (id == ID_AUTOSTART) {
            cfg_.autostart = !autostartOn_; SaveConfig(cfgPath_, cfg_);
            autostartOn_ = cfg_.autostart ? Autostart::Install() : (Autostart::Uninstall(), false);
            return;
        }
        if (id >= ID_PAD_R && id < ID_PAD_R + 8) cfg_.right.mode = (PadMode)(id - ID_PAD_R);
        else if (id >= ID_PAD_R + 10 && id < ID_PAD_R + 18) cfg_.right.shiftMode = (PadMode)(id - ID_PAD_R - 10);
        else if (id >= ID_PAD_L && id < ID_PAD_L + 8) cfg_.left.mode = (PadMode)(id - ID_PAD_L);
        else if (id >= ID_PAD_L + 10 && id < ID_PAD_L + 18) cfg_.left.shiftMode = (PadMode)(id - ID_PAD_L - 10);
        else if ((id >= ID_PAD_R + 20 && id < ID_PAD_R + 27) || (id >= ID_PAD_L + 20 && id < ID_PAD_L + 27)) {
            static const double sens[] = {0.4, 0.5, 0.65, 0.8, 1.0, 1.2, 1.6};
            static const int steps[] = {80, 64, 50, 40, 32, 25, 20};
            bool r = id < ID_PAD_L; PadConfig& pc = r ? cfg_.right : cfg_.left; int i = id - (r ? ID_PAD_R : ID_PAD_L) - 20;
            if (pc.mode == PadMode::Scroll) pc.scrollStep = steps[i]; else pc.sensitivity = sens[i];
        }

        else if (id == ID_HAPTIC_R) cfg_.right.haptics = !cfg_.right.haptics;
        else if (id == ID_HAPTIC_L) cfg_.left.haptics = !cfg_.left.haptics;
        else if (id == ID_GYRO + 0) cfg_.gyro.mode = GyroMode::Off;
        else if (id >= ID_GYRO + 1 && id <= ID_GYRO + 4) { cfg_.gyro.mode = GyroMode::Mouse; cfg_.gyro.activate = id == ID_GYRO + 1 ? GyroActivate::Always : id == ID_GYRO + 2 ? GyroActivate::Grip : id == ID_GYRO + 3 ? GyroActivate::RightTouch : GyroActivate::Toggle; }
        else if (id == ID_GYRO_CAL) { engine_.RequestCalibration(); return; }
        else if (id == ID_OPEN_CFG) { ShellExecuteW(nullptr, L"open", L"notepad.exe", cfgPath_.c_str(), nullptr, SW_SHOW); return; }
        else if (id == ID_OPEN_LOG) { ShellExecuteW(nullptr, L"open", L"notepad.exe", (dataDir_ + L"onepad.log").c_str(), nullptr, SW_SHOW); return; }
        else if (id == ID_RELOAD) { Config c; LoadConfig(cfgPath_, c); c.gyro.bias[0][0] = c.gyro.bias[0][0]; cfg_ = c; }
        else if (id == ID_RESTORE_FW) { RestoreFirmware(); return; }
        else return;
        SaveConfig(cfgPath_, cfg_);
        engine_.SetConfig(cfg_);
        ApplyEnabled();
        deck_.Configure(cfg_.deck); deck_.SetPadFlip(cfg_.left.flipX, cfg_.left.flipY, cfg_.right.flipX, cfg_.right.flipY);
        Apply();
        UpdateTip();
    }

    // Routing: while Steam holds the Deck persona, pads/grips/gyro go to Steam only when a Steam game is in the
    // foreground (games_only); on the desktop OnePad's own trackpad engine keeps working (tap, drag, scroll, gyro).
    static bool ForegroundIsSteamClient() {   // steam.exe / steamwebhelper.exe window in front (Big Picture, library): buttons still go to Steam
        HWND fg = GetForegroundWindow(); if (!fg) return false;
        DWORD pid = 0; GetWindowThreadProcessId(fg, &pid); if (!pid) return false;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) return false;
        wchar_t path[MAX_PATH]; DWORD n = MAX_PATH; bool ok = QueryFullProcessImageNameW(h, 0, path, &n) != 0; CloseHandle(h);
        if (!ok) return false;
        const wchar_t* base = wcsrchr(path, L'\\'); base = base ? base + 1 : path;
        return _wcsicmp(base, L"steam.exe") == 0 || _wcsicmp(base, L"steamwebhelper.exe") == 0;
    }
    static bool ForegroundIsSteamGame() {
        HWND fg = GetForegroundWindow(); if (!fg) return false;
        DWORD pid = 0; GetWindowThreadProcessId(fg, &pid); if (!pid || pid == GetCurrentProcessId()) return false;
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snap == INVALID_HANDLE_VALUE) return false;
        MODULEENTRY32W me{}; me.dwSize = sizeof me; bool game = false;
        if (Module32FirstW(snap, &me)) do { if (_wcsnicmp(me.szModule, L"GameOverlayRenderer", 19) == 0) { game = true; break; } } while (Module32NextW(snap, &me));
        CloseHandle(snap);
        return game;
    }
    void ApplyEnabled() {
        bool toSteam = deckClaimed_ && cfg_.deck.padsToSteam != "never" && (cfg_.deck.padsToSteam != "games_only" || ForegroundIsSteamGame() || ForegroundIsSteamClient());
        if (toSteam != padsToSteam_) { padsToSteam_ = toSteam; LOGI("routing: pads/grips/gyro -> %s", toSteam ? "Steam (game in front)" : "OnePad engine"); }
        deck_.SetPassPads(toSteam);
        engine_.SetEnabled(cfg_.enabled && !toSteam);
        bool btn = deckClaimed_ && (cfg_.deck.buttonsToSteam == "always" || toSteam || ForegroundIsSteamGame() || ForegroundIsSteamClient());
        if (btn != buttonsToSteam_) { buttonsToSteam_ = btn; LOGI("routing: buttons/sticks/triggers -> %s", btn ? "Steam" : "idle (desktop: real XInput pad serves apps)"); }
        deck_.SetPassButtons(btn);
        deck_.SetPassGrips(btn);                       // L4/R4 to Steam in games (Steam config decides), OnePad mouse clicks elsewhere
        engine_.SetGripsEnabled(cfg_.enabled && !btn);
    }

    // Hide the real Xbox pad (HidHide) according to [hidhide] mode. auto = only while Steam runs and the Deck persona is attached.
    static const wchar_t* XboxNodePrefix() { return L"USB\\VID_2993&PID_0012&MI_00"; }
    bool WantHide(const std::string& mode) {
        bool want;
        if (mode == "always") want = true;
        else if (mode == "never") want = false;
        else {
            // auto: hide while Steam runs, and for a grace period after Steam was last seen (or since our own start).
            // Steam adopts the real pad within ~1 s of starting and then keeps it even after the cloak comes back,
            // so the cloak must already be ON when Steam (re)starts: Steam restarts/updates and the logon race stay covered.
            uint64_t now = GetTickCount64();
            if (ProcessRunning(L"steam.exe")) steamSeenTick_ = now;
            if (!steamSeenTick_) steamSeenTick_ = now;
            bool steamish = (now - steamSeenTick_) < (uint64_t)cfg_.hidhide.graceSec * 1000ULL;
            want = deckRunning_ && usbip_.Attached() && steamish;
        }
        if (!vendor_.Present() && mode != "always") want = false;   // auto/never: controller absent -> nothing to hide; always: keep the cloak so a re-plug cannot race Steam
        return want;
    }
    void UpdateCloak(bool force) {
        // 1. Real Xbox (XUSB) node: disabled while Steam runs so Steam/WGI/games see only the Deck persona.
        bool wantX = WantHide(cfg_.xbox.mode);
        if (!xboxHideFailed_ && (force || wantX != xboxHidden_)) {
            if (SetDevNodeEnabled(XboxNodePrefix(), !wantX)) { xboxHidden_ = wantX; LOGI("xbox node %s", wantX ? "DISABLED (Steam sees only the Deck persona)" : "enabled"); }
            else if (wantX) { xboxHideFailed_ = true; LOGW("xbox node cannot be disabled on this system (xusb reports NOT_DISABLEABLE); relying on Steam Xbox-support=off + HidHide"); }
        }
        // 2. HidHide cloak: hides the real pad from games (not from Steam itself, which uses another path)
        if (!hidhide_.Available()) return;
        bool want = WantHide(cfg_.hidhide.mode);
        // Startup with Steam running: never drop a cloak a previous instance left on just because the Deck is not attached yet
        // (Steam would adopt the real pad within a second and keep it until restarted).
        if (force && !want && ProcessRunning(L"steam.exe")) return;
        if (force || want != hidhide_.Cloaked()) hidhide_.SetCloak(want);
    }

    void OnVendorState(const VendorState& s) {
        engine_.OnState(s);
        if (deckRunning_) {
            XInputSnapshot x = xinput_.Read(0);
            deck_.Update(s, x, (s.buttons3 & cfg_.gripLeft.bit) != 0, (s.buttons3 & cfg_.gripRight.bit) != 0);
        }
    }

    // ---- Steam Deck persona over USB/IP ----
    bool RunUsbip(const std::string& args, DWORD timeoutMs = 15000) {
        std::wstring cmd = L"\"" + Widen(cfg_.deck.usbipExe) + L"\" " + Widen(args);
        if (GetFileAttributesW(Widen(cfg_.deck.usbipExe).c_str()) == INVALID_FILE_ATTRIBUTES) { LOGW("usbip.exe not found: %s (usbip-win2 not installed?)", cfg_.deck.usbipExe.c_str()); return false; }
        STARTUPINFOW si{}; si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
        std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
        if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { LOGE("usbip.exe launch failed %lu", GetLastError()); return false; }
        DWORD w = WaitForSingleObject(pi.hProcess, timeoutMs); DWORD code = 1; if (w == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code); else { TerminateProcess(pi.hProcess, 1); LOGW("usbip.exe timed out"); }
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        LOGI("usbip.exe %s -> exit %lu", args.c_str(), code);
        return code == 0;
    }
    void StartDeck() {
        if (deckRunning_) return;
        deck_.Configure(cfg_.deck); deck_.SetPadFlip(cfg_.left.flipX, cfg_.left.flipY, cfg_.right.flipX, cfg_.right.flipY);
        deck_.onRumble = [this](uint16_t l, uint16_t r) { xinput_.Rumble(0, l, r); };
        deck_.onClaimChanged = [this](bool c) { PostMessageW(hwnd_, WM_DECK, c ? 1 : 0, 0); };
        // Clean up what a previous instance may have left in the vhci: cancel re-attach attempts, plug out stale ports.
        if (cfg_.deck.autoAttach) { RunUsbip(cfg_.deck.stopAttemptsArgs, 8000); RunUsbip(cfg_.deck.detachArgs, 8000); }
        if (!usbip_.Start(cfg_.deck.port, cfg_.deck.busid, deck_.Model())) { LOGE("deck: usbip server start failed"); return; }
        deckRunning_ = true;
        AttachDeck();
    }
    // Plug the virtual Steam Deck in only while the 3D One is present.
    // True once Windows has actually enumerated the virtual Deck (the vhci can report "attached" without it).
    bool DeckEnumerated() { return !FindInstanceIds(L"USB", L"USB\\VID_28DE&PID_1205").empty(); }
    bool WaitDeckEnumerated(int ms) { for (int t = 0; t < ms; t += 250) { if (DeckEnumerated()) return true; Sleep(250); } return DeckEnumerated(); }
    void AttachDeck() {
        if (!deckRunning_ || !cfg_.deck.autoAttach || !vendor_.Present()) return;
        detachDueTick_ = 0;                     // the controller is back: cancel a pending unplug
        if (deckAttached_) return;
        Sleep(200);
        deckAttached_ = RunUsbip(cfg_.deck.attachArgs);
        if (!deckAttached_) { LOGW("deck: attach failed; attach manually: usbip.exe %s", cfg_.deck.attachArgs.c_str()); return; }
        if (WaitDeckEnumerated(6000)) { LOGI("deck: virtual controller enumerated"); return; }
        // The usbip host controller sometimes keeps a stale port after detach/re-attach cycles: cycle it and plug in again.
        LOGW("deck: attached but not enumerated; restarting the usbip host controller");
        RunUsbip(cfg_.deck.detachArgs, 8000);
        for (auto& id : FindInstanceIds(L"ROOT", L"ROOT\\USB\\")) RestartDevNode(id);
        Sleep(4000);
        deckAttached_ = RunUsbip(cfg_.deck.attachArgs);
        if (deckAttached_ && WaitDeckEnumerated(8000)) LOGI("deck: virtual controller enumerated after host controller restart");
        else LOGE("deck: virtual controller still not enumerated (usbip-win2 problem?)");
    }
    void DetachDeck() {
        detachDueTick_ = 0;
        if (!deckRunning_ || !cfg_.deck.autoAttach || !deckAttached_) return;
        RunUsbip(cfg_.deck.detachArgs, 8000);          // proper plug-out first ...
        RunUsbip(cfg_.deck.stopAttemptsArgs, 8000);    // ... then make sure the vhci will not retry against us
        deckAttached_ = false; deckClaimed_ = false; ApplyEnabled();
    }
    void StopDeck() {
        if (!deckRunning_) return;
        DetachDeck();          // plug out while the server is still answering, never the other way round
        usbip_.Stop();
        deckRunning_ = false; deckClaimed_ = false; ApplyEnabled();
    }

    // ---- firmware / device node management (main thread) ----
    void Backup() {
        if (backup_.valid) return;
        IniData d = ReadIni(backupPath_);
        if (!d.empty()) {
            backup_.valid = true;
            backup_.tpMode[0] = (uint8_t)IniGetI(d, "firmware", "tpmode_left", 1); backup_.tpMode[1] = (uint8_t)IniGetI(d, "firmware", "tpmode_right", 1);
            backup_.tpOn[0] = IniGetI(d, "firmware", "tpstatus_left", 2) == 2; backup_.tpOn[1] = IniGetI(d, "firmware", "tpstatus_right", 2) == 2;
            backup_.keymap[0][0] = (uint8_t)IniGetI(d, "firmware", "keymap_left_m1", 28); backup_.keymap[0][1] = (uint8_t)IniGetI(d, "firmware", "keymap_left_m2", 32);
            backup_.keymap[1][0] = (uint8_t)IniGetI(d, "firmware", "keymap_right_m1", 28); backup_.keymap[1][1] = (uint8_t)IniGetI(d, "firmware", "keymap_right_m2", 32);
            LOGI("firmware backup loaded from file");
            return;
        }
        if (!vendor_.Present()) return;
        FirmwareBackup b; bool ok = true;
        for (int i = 0; i < 2; i++) {
            uint8_t side = i ? SapRight : SapLeft, v = 0;
            if (vendor_.GetTouchpadMode(side, v) < 0) ok = false; else b.tpMode[i] = v;
            if (vendor_.GetTouchpadStatus(side, v) < 0) ok = false; else b.tpOn[i] = v == 2;
            if (vendor_.GetKeyMap(side, KeyM1, v) < 0) ok = false; else b.keymap[i][0] = v;
            if (vendor_.GetKeyMap(side, KeyM2, v) < 0) ok = false; else b.keymap[i][1] = v;
        }
        if (!ok) { LOGW("firmware backup: some reads failed, not saved"); return; }
        b.valid = true; backup_ = b;
        FILE* f = _wfopen(backupPath_.c_str(), L"wb");
        if (f) {
            fprintf(f, "; OnePad firmware backup (original controller settings before OnePad changed them)\n[firmware]\n"
                       "tpmode_left=%u\ntpmode_right=%u\ntpstatus_left=%u\ntpstatus_right=%u\nkeymap_left_m1=%u\nkeymap_left_m2=%u\nkeymap_right_m1=%u\nkeymap_right_m2=%u\n",
                    b.tpMode[0], b.tpMode[1], b.tpOn[0] ? 2 : 1, b.tpOn[1] ? 2 : 1, b.keymap[0][0], b.keymap[0][1], b.keymap[1][0], b.keymap[1][1]);
            fclose(f);
        }
        LOGI("firmware backup saved: tpmode L=%u R=%u status L=%d R=%d keymap L(%u,%u) R(%u,%u)", b.tpMode[0], b.tpMode[1], b.tpOn[0], b.tpOn[1], b.keymap[0][0], b.keymap[0][1], b.keymap[1][0], b.keymap[1][1]);
    }

    void Apply() {
        if (!cfg_.enabled || !vendor_.Present()) return;
        bool ptpNeeded[2] = {false, false}, mouseNeeded = false;
        for (int i = 0; i < 2; i++) {
            bool right = i == 1;
            const PadConfig& pc = right ? cfg_.right : cfg_.left;
            PadMode eff = engine_.EffectiveMode(right);
            uint8_t mode = lastApplied_.tpMode[i]; bool on = true;
            switch (eff) {
                case PadMode::Mouse: case PadMode::Scroll: case PadMode::DPad: mode = pc.haptics ? TpTouchpad : TpWheel; break;
                case PadMode::NativeTouchpad: mode = TpTouchpad; ptpNeeded[i] = true; break;
                case PadMode::NativeWheel: mode = TpWheel; mouseNeeded = true; break;
                case PadMode::NativeLeftStick: mode = TpLeftStick; break;
                case PadMode::NativeRightStick: mode = TpRightStick; break;
                case PadMode::Off: on = false; break;
            }
            uint8_t side = right ? SapRight : SapLeft;
            // Firmware writes occasionally fail (status -3); write, read back, retry, and leave the slot "unapplied" on failure
            // so the periodic timer retries it instead of silently keeping the old mode.
            if (!lastApplied_.valid || lastApplied_.tpMode[i] != mode) {
                bool ok = false;
                for (int t = 0; t < 3 && !ok; t++) { vendor_.SetTouchpadMode(side, mode); Sleep(15); uint8_t rb = 0xFF; ok = vendor_.GetTouchpadMode(side, rb) >= 0 && rb == mode; if (!ok) Sleep(40); }
                lastApplied_.tpMode[i] = ok ? mode : 0xFE; applyRetry_ = applyRetry_ || !ok;
                if (!ok) LOGW("touchpad mode side=%u -> %u not confirmed, will retry", side, mode);
            }
            if (!lastApplied_.valid || lastApplied_.tpOn[i] != on) {
                bool ok = false;
                for (int t = 0; t < 3 && !ok; t++) { vendor_.SetTouchpadStatus(side, on); Sleep(15); uint8_t rb = 0; ok = vendor_.GetTouchpadStatus(side, rb) >= 0 && ((rb == 2) == on); if (!ok) Sleep(40); }
                lastApplied_.tpOn[i] = ok ? on : !on; applyRetry_ = applyRetry_ || !ok;
                if (!ok) LOGW("touchpad status side=%u -> %d not confirmed, will retry", side, on ? 1 : 0);
            }
        }
        lastApplied_.valid = true;
        if (cfg_.unmapGrips && !keymapApplied_) {
            for (uint8_t side : {(uint8_t)SapLeft, (uint8_t)SapRight}) { vendor_.SetKeyMap(side, KeyM1, KeyM1); Sleep(6); vendor_.SetKeyMap(side, KeyM2, KeyM2); Sleep(6); }
            keymapApplied_ = true;
        }
        SetDevNodeEnabled(HidCollectionPrefix(cfg_.ptpRight), ptpNeeded[1]);
        SetDevNodeEnabled(HidCollectionPrefix(cfg_.ptpLeft), ptpNeeded[0]);
        SetDevNodeEnabled(HidCollectionPrefix(cfg_.mouseCol), mouseNeeded);
        LOGI("applied: R=%s L=%s shift=%d", PadModeName(engine_.EffectiveMode(true)), PadModeName(engine_.EffectiveMode(false)), engine_.ShiftHeld() ? 1 : 0);
    }

    void RestoreFirmware() {
        bool okR = SetDevNodeEnabled(HidCollectionPrefix(cfg_.ptpRight), true);
        bool okL = SetDevNodeEnabled(HidCollectionPrefix(cfg_.ptpLeft), true);
        bool okM = SetDevNodeEnabled(HidCollectionPrefix(cfg_.mouseCol), true);
        bool okX = SetDevNodeEnabled(XboxNodePrefix(), true); xboxHidden_ = false;
        // Nodes of an absent controller stay disabled in the registry; remember to enable them when it returns.
        SetPendingRestore(!(okR && okL && okM && okX) || !vendor_.Present());
        if (!vendor_.Present()) { LOGW("restore: device not present (will restore on next arrival / --restore)"); return; }
        FirmwareBackup b = backup_.valid ? backup_ : FirmwareBackup();
        for (int i = 0; i < 2; i++) {
            uint8_t side = i ? SapRight : SapLeft;
            vendor_.SetTouchpadStatus(side, b.tpOn[i]); Sleep(6);
            vendor_.SetTouchpadMode(side, b.tpMode[i]); Sleep(6);
            vendor_.SetKeyMap(side, KeyM1, b.keymap[i][0]); Sleep(6);
            vendor_.SetKeyMap(side, KeyM2, b.keymap[i][1]); Sleep(6);
        }
        lastApplied_ = FirmwareBackup(); keymapApplied_ = false;
        SetPendingRestore(false);
        LOGI("firmware restored (%s)", backup_.valid ? "from backup" : "defaults");
    }
    std::wstring PendingPath() { return dataDir_ + L"restore-pending.flag"; }
    void SetPendingRestore(bool on) {
        if (on) { FILE* f = _wfopen(PendingPath().c_str(), L"wb"); if (f) fclose(f); }
        else DeleteFileW(PendingPath().c_str());
    }
    bool PendingRestore() { return GetFileAttributesW(PendingPath().c_str()) != INVALID_FILE_ATTRIBUTES; }
    void RestoreOnly() {
        LoadConfig(cfgPath_, cfg_);
        vendor_.Create(nullptr, [this](bool arrived) { if (arrived) devicePresent_ = true; });
        for (int i = 0; i < 100 && !vendor_.Present(); i++) Sleep(100);   // wait up to 10 s for the controller
        Backup(); RestoreFirmware(); vendor_.Release();
        if (cfg_.deck.autoAttach) RunUsbip(cfg_.deck.detachArgs, 8000);
        hidhide_.Locate(cfg_.hidhide.cli); hidhide_.Prepare(); hidhide_.Release();
    }
    void Shutdown() {
        if (shutdown_) return; shutdown_ = true;
        LOGI("shutting down");
        engine_.SetEnabled(false);
        StopDeck();
        if (cfg_.hidhide.mode == "always") LOGI("hidhide: mode=always, cloak stays ON after exit (use --restore or --uninstall to lift it)");
        else if (keepCloak_ && hidhide_.Cloaked() && ProcessRunning(L"steam.exe")) LOGI("hidhide: cloak kept ON across restart"); else hidhide_.Release();
        if (cfg_.restoreOnExit) RestoreFirmware();
        vendor_.Release();
        RemoveTray();
    }

    std::wstring dataDir_, cfgPath_, backupPath_;
    Config cfg_;
    Output out_;
    Engine engine_{out_};
    Vendor vendor_;
    DeckDevice deck_; UsbipServer usbip_; XInputReader xinput_; bool deckRunning_ = false, deckClaimed_ = false;
    HidHide hidhide_; unsigned cloakTicks_ = 0; uint64_t steamSeenTick_ = 0; bool autostartOn_ = false, keepCloak_ = false, deckAttached_ = false, applyRetry_ = false, xboxHidden_ = false, xboxHideFailed_ = false, padsToSteam_ = true, buttonsToSteam_ = true; uint64_t detachDueTick_ = 0;
    FirmwareBackup backup_, lastApplied_;
    bool keymapApplied_ = false, devicePresent_ = false, trayAdded_ = false, shutdown_ = false;
    HWND hwnd_ = nullptr; UINT taskbarCreated_ = 0; HANDLE stopEvent_ = nullptr; unsigned timerTicks_ = 0;
};

} // namespace

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR cmd, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    App app;
    return app.Run(hinst, cmd ? cmd : "");
}
