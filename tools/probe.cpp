// OnePad probe: experiments with the abxylute 3D One controller through the vendor
// sapjoyapis.dll API and the raw vendor HID report (usage page 0xFFA0).
//
//   probe info                      show versions / modes / mappings
//   probe tpmode L|R <1..4>         1=touchpad(pointer,haptic) 2=leftstick 3=rightstick 4=wheel
//   probe tpstatus L|R on|off
//   probe keymap L|R <src> <dst>    SAPJoyGamepadKey ids (M1=28, M2=32, L3=3, R3=8 ...)
//   probe devmode L|R 0|1           0=gamepad(xinput) 1=kb/mouse(dinput)
//   probe call <name> L|R [byte]    generic: Get* -> prints 16-byte out buffer, Set* -> sends byte
//   probe dump <seconds>            print decoded vendor reports as they change
#include <windows.h>
#include <cfgmgr32.h>
#include <hidsdi.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>

#pragma pack(push, 8)
struct SAPJOY_ATTRIBUTES {
    uint32_t size;
    const wchar_t* name;
    uint16_t vid;
    uint16_t pid;
    void* fnFirmwareUpdate;
    void* fnDeviceChange;
    void* fnStateNotify;
    void* ctx;
};
#pragma pack(pop)
#pragma pack(push, 1)
struct SAPJOY_VERSION { uint8_t engineer[3]; uint8_t protocol[3]; uint32_t firmware; uint8_t hardware[3]; };
#pragma pack(pop)

typedef int (__stdcall *PFN_Create)(SAPJOY_ATTRIBUTES*, void**);
typedef int (__stdcall *PFN_Release)(void*);
typedef int (__stdcall *PFN_DevIdPtr)(void*, uint8_t, void*);
typedef int (__stdcall *PFN_DevIdByte)(void*, uint8_t, uint8_t);
typedef int (__stdcall *PFN_DevIdByteByte)(void*, uint8_t, uint8_t, uint8_t);
typedef int (__stdcall *PFN_DevIdBytePtr)(void*, uint8_t, uint8_t, void*);

static HMODULE g_dll;
static void* g_sapjoy = nullptr;
static std::atomic<void*> g_dev{nullptr};
static std::atomic<int> g_devId{0};
static std::atomic<int> g_stateCount{0};

static void __stdcall OnFirmware(void*, uint32_t, int, void*) {}
static void __stdcall OnDeviceChange(void* dev, int status, uint8_t id, void*) {
    printf("[cb] device change: handle=%p status=%s id=%u\n", dev, status == 1 ? "Arrival" : "Removal", id);
    if (status == 1) { g_dev = dev; g_devId = id; }
}
static void __stdcall OnState(void*, uint32_t type, uint32_t size, void* state, void*) {
    int n = ++g_stateCount;
    if (n == 1) {
        printf("[cb] first state notification: type=%u size=%u\n", type, size);
        const uint8_t* b = (const uint8_t*)state;
        printf("     ");
        for (uint32_t i = 0; i < size && i < 64; i++) printf("%02X ", b[i]);
        printf("\n");
    }
}

template <class T> static T Fn(const char* name) {
    T f = (T)GetProcAddress(g_dll, name);
    if (!f) { printf("!! export %s not found\n", name); }
    return f;
}
static const char* StatusName(int s) {
    switch (s) {
        case 0: return "Success"; case 1: return "SuccessStart";
        case -1: return "InvalidParam"; case -2: return "Mismatch"; case -3: return "NotImpl"; case -4: return "ObjAllocFailed";
        case -5: return "NotSupported"; case -6: return "OpenFailed"; case -7: return "StartFailed"; case -8: return "GetInfoFailed";
        case -9: return "AlreadyInitialized"; case -10: return "InvalidDevice"; case -11: return "WriteFailed"; case -12: return "ReadFailed";
        case -13: return "UpdateFailed"; case -14: return "InitFailed"; case -15: return "NotEnough"; case -16: return "NotFound"; case -17: return "SetInfoFailed";
    }
    return "?";
}

static bool InitSdk() {
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/") + 1);
    std::wstring dll = dir + L"sapjoyapis.dll";
    g_dll = LoadLibraryW(dll.c_str());
    if (!g_dll) g_dll = LoadLibraryW(L"sapjoyapis.dll");
    if (!g_dll) { printf("cannot load sapjoyapis.dll (err %lu)\n", GetLastError()); return false; }
    auto create = Fn<PFN_Create>("SAPJoy_Create");
    SAPJOY_ATTRIBUTES a{};
    a.size = sizeof(a); a.name = L"OnePadProbe"; a.vid = 0x2993; a.pid = 0x0012;
    a.fnFirmwareUpdate = (void*)OnFirmware; a.fnDeviceChange = (void*)OnDeviceChange; a.fnStateNotify = (void*)OnState; a.ctx = nullptr;
    void* h = nullptr;
    int st = create(&a, &h);
    printf("SAPJoy_Create -> %d (%s), sapjoy=%p\n", st, StatusName(st), h);
    if (st < 0) return false;
    g_sapjoy = h;
    for (int i = 0; i < 40 && !g_dev.load(); i++) Sleep(100);
    if (!g_dev.load()) { printf("no device arrival within 4 s\n"); return false; }
    Sleep(200);
    return true;
}

static uint8_t SideId(const char* s) { return (s[0] == 'L' || s[0] == 'l') ? 3 : 4; } // GamepadLHotMoving=3, R=4

static void Info() {
    void* d = g_dev.load();
    auto getVer = Fn<PFN_DevIdPtr>("SAPJoyDevice_GetVersion");
    for (uint8_t id : {1, 2, 3, 4}) {
        SAPJOY_VERSION v{}; int st = getVer(d, id, &v);
        printf("version id=%u: %s fw=%u.%u.%u.%u proto=%u.%u.%u hw=%u.%u.%u\n", id, StatusName(st),
               v.firmware & 0xFF, (v.firmware >> 8) & 0xFF, (v.firmware >> 16) & 0xFF, v.firmware >> 24,
               v.protocol[0], v.protocol[1], v.protocol[2], v.hardware[0], v.hardware[1], v.hardware[2]);
    }
    struct Q { const char* name; } qs[] = {
        {"SAPJoyDevice_GetDeviceMode"}, {"SAPJoyDevice_GetTouchpadEmulationMode"}, {"SAPJoyDevice_GetTouchpadStatus"},
        {"SAPJoyDevice_GetAudioMotorOnOff"}, {"SAPJoyDevice_GetVibrateLevel"}, {"SAPJoyDevice_GetReportRate"},
        {"SAPJoyDevice_GetStickCenterDeadZone"},
    };
    for (auto& q : qs) {
        auto f = Fn<PFN_DevIdPtr>(q.name); if (!f) continue;
        for (uint8_t id : {3, 4}) {
            uint8_t buf[16] = {}; int st = f(d, id, buf);
            printf("%-42s id=%u -> %-14s [%02X %02X %02X %02X]\n", q.name + 13, id, StatusName(st), buf[0], buf[1], buf[2], buf[3]);
        }
    }
    auto getMap = Fn<PFN_DevIdBytePtr>("SAPJoyDevice_GetGamepadKeyMappingV1");
    for (uint8_t id : {3, 4}) for (uint8_t src : {28, 32}) {
        uint8_t dst = 0; int st = getMap(d, id, src, &dst);
        printf("keymap id=%u src=%u (M%c) -> %s dst=%u\n", id, src, src == 28 ? '1' : '2', StatusName(st), dst);
    }
}

static int16_t BE16(const uint8_t* p) { return (int16_t)((p[0] << 8) | p[1]); }

static void Dump(int seconds) {
    GUID g; HidD_GetHidGuid(&g);
    ULONG len = 0; CM_Get_Device_Interface_List_SizeW(&len, &g, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    std::vector<wchar_t> buf(len); CM_Get_Device_Interface_ListW(&g, nullptr, buf.data(), len, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    HANDLE h = INVALID_HANDLE_VALUE;
    for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) {
        std::wstring s(p); for (auto& c : s) c = towlower(c);
        if (s.find(L"vid_2993&pid_0012&mi_02") == std::wstring::npos) continue;
        h = CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        printf("open %ls -> %s\n", p, h == INVALID_HANDLE_VALUE ? "FAILED" : "ok");
        break;
    }
    if (h == INVALID_HANDLE_VALUE) { printf("vendor interface not found/opened (err %lu)\n", GetLastError()); return; }
    uint8_t prev[64] = {}; bool first = true;
    DWORD t0 = GetTickCount();
    uint8_t rep[64];
    HMODULE xi = LoadLibraryW(L"xinput1_4.dll");
    typedef DWORD (WINAPI *PFN_XIGetState)(DWORD, void*);
    PFN_XIGetState xiGet = xi ? (PFN_XIGetState)GetProcAddress(xi, "XInputGetState") : nullptr;
    struct XS { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } xprev{};
    while ((int)(GetTickCount() - t0) < seconds * 1000) {
        DWORD n = 0;
        if (!ReadFile(h, rep, sizeof rep, &n, nullptr)) { printf("read err %lu\n", GetLastError()); break; }
        if (xiGet) {
            XS xs{}; if (xiGet(0, &xs) == 0 && (xs.buttons != xprev.buttons || xs.lt != xprev.lt || xs.rt != xprev.rt
                || abs(xs.lx - xprev.lx) > 4000 || abs(xs.ly - xprev.ly) > 4000 || abs(xs.rx - xprev.rx) > 4000 || abs(xs.ry - xprev.ry) > 4000)) {
                printf("%6lu ms  XINPUT btn=%04X LT=%u RT=%u LS(%d,%d) RS(%d,%d)\n", GetTickCount() - t0, xs.buttons, xs.lt, xs.rt, xs.lx, xs.ly, xs.rx, xs.ry);
                xprev = xs;
            }
        }
        if (n < 64 || rep[0] != 4) continue;
        // fields of interest (indices into the raw report incl. report id)
        bool changed = first;
        static const int idx[] = {18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 60, 61, 62, 63};
        for (int i : idx) if (rep[i] != prev[i]) changed = true;
        if (changed) {
            printf("%6lu ms  btn[%02X %02X %02X %02X] trig[%3u %3u] tm=%02X wheel=%4d  Rabs(%4d,%4d) Ldelta(%4d,%4d) Rdelta(%4d,%4d) Labs(%4d,%4d)  LS(%3u,%3u) RS(%3u,%3u)\n",
                   GetTickCount() - t0, rep[18], rep[19], rep[20], rep[21], rep[22], rep[23], rep[24], (int)rep[25] - 128,
                   BE16(rep + 26), BE16(rep + 28), (int)rep[30] - 128, (int)rep[31] - 128, (int)rep[32] - 128, (int)rep[33] - 128,
                   BE16(rep + 60), BE16(rep + 62), rep[14], rep[15], rep[16], rep[17]);
            memcpy(prev, rep, 64); first = false;
        }
    }
    CloseHandle(h);
}

// ---- interactive button learning (appended to probe.cpp) ----
#include <conio.h>
static HANDLE OpenVendor() {
    GUID g; HidD_GetHidGuid(&g);
    ULONG len = 0; CM_Get_Device_Interface_List_SizeW(&len, &g, nullptr, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    std::vector<wchar_t> buf(len); CM_Get_Device_Interface_ListW(&g, nullptr, buf.data(), len, CM_GET_DEVICE_INTERFACE_LIST_PRESENT);
    for (wchar_t* p = buf.data(); *p; p += wcslen(p) + 1) {
        std::wstring s(p); for (auto& c : s) c = towlower(c);
        if (s.find(L"vid_2993&pid_0012&mi_02") == std::wstring::npos) continue;
        return CreateFileW(p, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    }
    return INVALID_HANDLE_VALUE;
}
static bool ReadState(HANDLE h, uint8_t rep[64]) { DWORD n = 0; return ReadFile(h, rep, 64, &n, nullptr) && n == 64 && rep[0] == 4; }
static bool Idle(const uint8_t* r) { for (int i = 18; i <= 23; i++) if (r[i]) return false; return true; }
static void Learn() {
    SetConsoleOutputCP(65001);
    HANDLE h = OpenVendor();
    if (h == INVALID_HANDLE_VALUE) { printf("컨트롤러(벤더 인터페이스)를 열 수 없습니다. 컨트롤러가 켜져 있는지 확인하세요.\n"); return; }
    const char* names[] = {"A", "B", "X", "Y", "LB (왼쪽 범퍼)", "RB (오른쪽 범퍼)", "LT 끝까지 당기기", "RT 끝까지 당기기",
                           "L3 (왼쪽 스틱 누르기)", "R3 (오른쪽 스틱 누르기)", "십자 위", "십자 아래", "십자 왼쪽", "십자 오른쪽",
                           "View / 뒤로 (왼쪽 작은 버튼)", "Menu / 시작 (오른쪽 작은 버튼)", "Home (Steam/집 모양)", "Setup / 설정 (… 또는 톱니)",
                           "왼쪽 백버튼", "오른쪽 백버튼"};
    const int N = sizeof names / sizeof names[0];
    FILE* out = nullptr; { wchar_t p[MAX_PATH]; ExpandEnvironmentStringsW(L"%LOCALAPPDATA%\OnePad\button-map.txt", p, MAX_PATH); out = _wfopen(p, L"w"); }
    printf("=== OnePad 버튼 학습 ===\n안내하는 버튼을 1초쯤 눌렀다 떼세요. 없는 버튼은 Enter 로 건너뜁니다. 종료: Esc\n\n");
    uint8_t rep[64];
    for (int i = 0; i < N; i++) {
        // wait idle
        for (int k = 0; k < 400; k++) { if (ReadState(h, rep) && Idle(rep)) break; }
        printf("[%2d/%d] 지금 [ %s ] 을(를) 누르세요... ", i + 1, N, names[i]); fflush(stdout);
        bool got = false, skip = false; uint8_t diff[6] = {}; int lt = 0, rt = 0;
        while (!got && !skip) {
            if (_kbhit()) { int c = _getch(); if (c == 27) { fclose(out); CloseHandle(h); return; } if (c == 13) skip = true; }
            if (!ReadState(h, rep)) continue;
            { static uint8_t last[6] = {}; bool ch = false; for (int b = 0; b < 6; b++) if (rep[18 + b] != last[b]) ch = true; if (ch) { memcpy(last, rep + 18, 6); printf("\n      [raw %02X %02X %02X %02X %02X %02X] ", rep[18], rep[19], rep[20], rep[21], rep[22], rep[23]); } }
            if (!Idle(rep)) {
                // hold 60 ms to settle, then sample
                for (int k = 0; k < 8; k++) ReadState(h, rep);
                for (int b = 0; b < 6; b++) diff[b] = rep[18 + b];
                lt = rep[22]; rt = rep[23]; got = true;
            }
        }
        if (skip) { printf("건너뜀\n"); if (out) fprintf(out, "%s = (none)\n", names[i]); continue; }
        char desc[128] = ""; int o = 0;
        for (int b = 0; b < 4; b++) if (diff[b]) o += snprintf(desc + o, sizeof desc - o, "buttons%d=0x%02X ", b + 1, diff[b]);
        if (lt) o += snprintf(desc + o, sizeof desc - o, "LT=%d ", lt);
        if (rt) o += snprintf(desc + o, sizeof desc - o, "RT=%d ", rt);
        printf("-> %s\n", desc); if (out) fprintf(out, "%s = %s\n", names[i], desc);
        // wait release
        for (int k = 0; k < 250; k++) { if (ReadState(h, rep) && Idle(rep)) break; }
    }
    if (out) fclose(out);
    CloseHandle(h);
    printf("\n완료. 결과: %%LOCALAPPDATA%%\OnePad\button-map.txt  (이 창은 닫아도 됩니다)\n");
    printf("아무 키나 누르면 닫힙니다..."); _getch();
}

int main(int argc, char** argv) {
    setvbuf(stdout, nullptr, _IONBF, 0);
    if (argc >= 2 && std::string(argv[1]) == "learn") { Learn(); return 0; }
    if (argc < 2) { printf("usage: info | tpmode L|R n | tpstatus L|R on|off | keymap L|R src dst | devmode L|R n | call Name L|R [byte] | dump secs\n"); return 1; }
    std::string cmd = argv[1];
    if (cmd == "dump") { Dump(argc > 2 ? atoi(argv[2]) : 20); return 0; }
    if (!InitSdk()) return 2;
    void* d = g_dev.load();
    if (cmd == "info") Info();
    else if (cmd == "rate") { int a = g_stateCount.load(); Sleep(2000); printf("state callbacks in 2 s: %d\n", g_stateCount.load() - a); }
    else if (cmd == "tpmode" && argc >= 4) {
        int st = Fn<PFN_DevIdByte>("SAPJoyDevice_SetTouchpadEmulationMode")(d, SideId(argv[2]), (uint8_t)atoi(argv[3]));
        printf("SetTouchpadEmulationMode(%s, %s) -> %s\n", argv[2], argv[3], StatusName(st));
        uint8_t m = 0; st = Fn<PFN_DevIdPtr>("SAPJoyDevice_GetTouchpadEmulationMode")(d, SideId(argv[2]), &m);
        printf("readback -> %s mode=%u\n", StatusName(st), m);
    } else if (cmd == "tpstatus" && argc >= 4) {
        uint8_t v = (strcmp(argv[3], "on") == 0) ? 2 : 1;
        int st = Fn<PFN_DevIdByte>("SAPJoyDevice_SetTouchpadStatus")(d, SideId(argv[2]), v);
        printf("SetTouchpadStatus(%s, %u) -> %s\n", argv[2], v, StatusName(st));
        uint8_t m = 0; st = Fn<PFN_DevIdPtr>("SAPJoyDevice_GetTouchpadStatus")(d, SideId(argv[2]), &m);
        printf("readback -> %s status=%u\n", StatusName(st), m);
    } else if (cmd == "keymap" && argc >= 5) {
        int st = Fn<PFN_DevIdByteByte>("SAPJoyDevice_SetGamepadKeyMappingV1")(d, SideId(argv[2]), (uint8_t)atoi(argv[3]), (uint8_t)atoi(argv[4]));
        printf("SetGamepadKeyMappingV1(%s, %s -> %s) -> %s\n", argv[2], argv[3], argv[4], StatusName(st));
        uint8_t m = 0; st = Fn<PFN_DevIdBytePtr>("SAPJoyDevice_GetGamepadKeyMappingV1")(d, SideId(argv[2]), (uint8_t)atoi(argv[3]), &m);
        printf("readback -> %s dst=%u\n", StatusName(st), m);
    } else if (cmd == "devmode" && argc >= 4) {
        int st = Fn<PFN_DevIdByte>("SAPJoyDevice_SetDeviceMode")(d, SideId(argv[2]), (uint8_t)atoi(argv[3]));
        printf("SetDeviceMode -> %s\n", StatusName(st));
    } else if (cmd == "call" && argc >= 4) {
        std::string name = std::string("SAPJoyDevice_") + argv[2];
        if (argc >= 5) {
            int st = Fn<PFN_DevIdByte>(name.c_str())(d, SideId(argv[3]), (uint8_t)atoi(argv[4]));
            printf("%s(%s, %s) -> %s\n", name.c_str(), argv[3], argv[4], StatusName(st));
        } else {
            uint8_t buf[16] = {}; int st = Fn<PFN_DevIdPtr>(name.c_str())(d, SideId(argv[3]), buf);
            printf("%s(%s) -> %s [", name.c_str(), argv[3], StatusName(st)); for (int i = 0; i < 16; i++) printf("%02X ", buf[i]); printf("]\n");
        }
    } else printf("bad args\n");
    Sleep(100);
    if (g_sapjoy) { int st = Fn<PFN_Release>("SAPJoy_Release")(g_sapjoy); printf("SAPJoy_Release -> %s\n", StatusName(st)); }
    return 0;
}
