// OnePad - binding to the vendor sapjoyapis.dll (Sapientia joystick SDK shipped with the Gear/Prd apps)
#pragma once
#include "common.h"
#include <atomic>
#include <functional>

namespace onepad {

// Raw gamepad state delivered by the SDK (59 bytes = vendor HID report bytes 5..63). Big-endian 16-bit fields.
#pragma pack(push, 1)
struct VendorState {
    uint8_t lBatteryLevel, lBatteryStatus, rBatteryLevel, rBatteryStatus;
    uint8_t fpsStatus, lGyroStatus, rGyroStatus, lStatus, rStatus;
    uint8_t lStickX, lStickY, rStickX, rStickY;
    uint8_t buttons1, buttons2, buttons3, buttons4;   // buttons1: Setup 0x80 Home 0x40 Up 0x08 Down 0x04 Left 0x02 Right 0x01
                                                      // buttons2: A 0x80 B 0x40 LB 0x08 LT 0x04 RB 0x02 RT 0x01
                                                      // buttons3: left grip 0x10, right grip 0x80 (observed)
    uint8_t lTrigger, rTrigger;
    uint8_t testMode, mouseWheel;                     // mouseWheel: 0x80 idle, else signed delta
    uint8_t rPadXH, rPadXL, rPadYH, rPadYL;           // right pad absolute (0..1024), 0 = not touching
    uint8_t lPadDX, lPadDY, rPadDX, rPadDY;           // relative deltas (0x80 center), only in touchpad mode
    uint8_t lImuTs; uint8_t lAccel[6]; uint8_t lGyro[6];
    uint8_t rImuTs; uint8_t rAccel[6]; uint8_t rGyro[6];
    uint8_t lPadXH, lPadXL, lPadYH, lPadYL;           // left pad absolute

    int RPadX() const { return (rPadXH << 8) | rPadXL; }
    int RPadY() const { return (rPadYH << 8) | rPadYL; }
    int LPadX() const { return (lPadXH << 8) | lPadXL; }
    int LPadY() const { return (lPadYH << 8) | lPadYL; }
    static int16_t BE(const uint8_t* p) { return (int16_t)((p[0] << 8) | p[1]); }
    void Accel(bool right, double out[3]) const { const uint8_t* p = right ? rAccel : lAccel; for (int i = 0; i < 3; i++) out[i] = BE(p + 2 * i); }
    void Gyro(bool right, double out[3]) const { const uint8_t* p = right ? rGyro : lGyro; for (int i = 0; i < 3; i++) out[i] = BE(p + 2 * i); }
};
static_assert(sizeof(VendorState) == 59, "VendorState layout");
#pragma pack(pop)

enum SapDeviceId : uint8_t { SapRx = 1, SapDongle = 2, SapLeft = 3, SapRight = 4 };
enum SapTouchpadMode : uint8_t { TpNone = 0, TpTouchpad = 1, TpLeftStick = 2, TpRightStick = 3, TpWheel = 4 };
enum SapKey : uint8_t { KeyL3 = 3, KeyR3 = 8, KeyM1 = 28, KeyM2 = 32 };

class Vendor {
public:
    typedef std::function<void(const VendorState&)> StateFn;
    typedef std::function<void(bool arrived)> DeviceFn;

    bool Load() {
        std::wstring p = ExeDir() + L"sapjoyapis.dll";
        dll_ = LoadLibraryW(p.c_str());
        if (!dll_) dll_ = LoadLibraryW(L"C:\\Program Files (x86)\\Gear\\Settings\\sapjoyapis.dll");
        if (!dll_) { LOGE("sapjoyapis.dll load failed (%lu)", GetLastError()); return false; }
#define OP_FN(var, name) var = (decltype(var))GetProcAddress(dll_, name); if (!var) { LOGE("export missing: %s", name); return false; }
        OP_FN(fnCreate_, "SAPJoy_Create"); OP_FN(fnRelease_, "SAPJoy_Release");
        OP_FN(fnGetTpMode_, "SAPJoyDevice_GetTouchpadEmulationMode"); OP_FN(fnSetTpMode_, "SAPJoyDevice_SetTouchpadEmulationMode");
        OP_FN(fnGetTpStatus_, "SAPJoyDevice_GetTouchpadStatus"); OP_FN(fnSetTpStatus_, "SAPJoyDevice_SetTouchpadStatus");
        OP_FN(fnGetKeyMap_, "SAPJoyDevice_GetGamepadKeyMappingV1"); OP_FN(fnSetKeyMap_, "SAPJoyDevice_SetGamepadKeyMappingV1");
        OP_FN(fnGetVersion_, "SAPJoyDevice_GetVersion");
#undef OP_FN
        return true;
    }
    bool Create(StateFn onState, DeviceFn onDevice) {
        onState_ = onState; onDevice_ = onDevice;
        Attributes a{}; a.size = sizeof(a); a.name = L"OnePad"; a.vid = 0x2993; a.pid = 0x0012;
        a.fnFirmware = (void*)&Vendor::CbFirmware; a.fnDevice = (void*)&Vendor::CbDevice; a.fnState = (void*)&Vendor::CbState; a.ctx = this;
        int st = fnCreate_(&a, &sap_);
        LOGI("SAPJoy_Create -> %d", st);
        return st >= 0;
    }
    // SAPJoy_Release has been seen to block when the controller is absent; give it a bounded wait.
    void Release() {
        if (!sap_ || !fnRelease_) return;
        void* h = sap_; sap_ = nullptr; PfnRelease fn = fnRelease_;
        HANDLE t = CreateThread(nullptr, 0, [](void* p) -> DWORD { auto* pr = (std::pair<PfnRelease, void*>*)p; pr->first(pr->second); delete pr; return 0; }, new std::pair<PfnRelease, void*>(fn, h), 0, nullptr);
        if (t) { if (WaitForSingleObject(t, 3000) != WAIT_OBJECT_0) LOGW("SAPJoy_Release did not return within 3 s; abandoning"); CloseHandle(t); }
    }
    void* Device() const { return dev_.load(); }
    bool Present() const { return dev_.load() != nullptr; }

    int GetTouchpadMode(uint8_t side, uint8_t& mode) { return Call(fnGetTpMode_, side, &mode); }
    int SetTouchpadMode(uint8_t side, uint8_t mode) { int r = CallB(fnSetTpMode_, side, mode); LOGI("SetTouchpadMode side=%u mode=%u -> %d", side, mode, r); return r; }
    int GetTouchpadStatus(uint8_t side, uint8_t& status) { return Call(fnGetTpStatus_, side, &status); }
    int SetTouchpadStatus(uint8_t side, bool on) { int r = CallB(fnSetTpStatus_, side, on ? 2 : 1); LOGI("SetTouchpadStatus side=%u on=%d -> %d", side, on ? 1 : 0, r); return r; }
    int GetKeyMap(uint8_t side, uint8_t src, uint8_t& dst) { void* d = dev_.load(); if (!d) return -10; return fnGetKeyMap_(d, side, src, &dst); }
    int SetKeyMap(uint8_t side, uint8_t src, uint8_t dst) { void* d = dev_.load(); if (!d) return -10; int r = fnSetKeyMap_(d, side, src, dst); LOGI("SetKeyMap side=%u %u->%u -> %d", side, src, dst, r); return r; }
    std::string Version(uint8_t side) {
        struct V { uint8_t e[3], p[3]; uint32_t fw; uint8_t h[3]; } __attribute__((packed)) v{};
        void* d = dev_.load(); if (!d) return "?";
        if (fnGetVersion_(d, side, &v) < 0) return "?";
        char b[64]; snprintf(b, sizeof b, "%u.%u.%u.%u", v.fw & 0xFF, (v.fw >> 8) & 0xFF, (v.fw >> 16) & 0xFF, v.fw >> 24); return b;
    }

private:
#pragma pack(push, 8)
    struct Attributes { uint32_t size; const wchar_t* name; uint16_t vid; uint16_t pid; void* fnFirmware; void* fnDevice; void* fnState; void* ctx; };
#pragma pack(pop)
    typedef int (__stdcall* PfnCreate)(Attributes*, void**);
    typedef int (__stdcall* PfnRelease)(void*);
    typedef int (__stdcall* PfnIdPtr)(void*, uint8_t, void*);
    typedef int (__stdcall* PfnIdByte)(void*, uint8_t, uint8_t);
    typedef int (__stdcall* PfnIdBytePtr)(void*, uint8_t, uint8_t, void*);
    typedef int (__stdcall* PfnIdByteByte)(void*, uint8_t, uint8_t, uint8_t);

    int Call(PfnIdPtr f, uint8_t side, void* out) { void* d = dev_.load(); if (!d) return -10; return f(d, side, out); }
    int CallB(PfnIdByte f, uint8_t side, uint8_t v) { void* d = dev_.load(); if (!d) return -10; return f(d, side, v); }

    static void __stdcall CbFirmware(void*, uint32_t, int, void*) {}
    static void __stdcall CbDevice(void* dev, int status, uint8_t id, void* ctx) {
        Vendor* self = (Vendor*)ctx;
        LOGI("device %s handle=%p id=%u", status == 1 ? "arrival" : "removal", dev, id);
        if (status == 1) self->dev_ = dev; else self->dev_ = nullptr;
        if (self->onDevice_) self->onDevice_(status == 1);
    }
    static void __stdcall CbState(void*, uint32_t type, uint32_t size, void* state, void* ctx) {
        Vendor* self = (Vendor*)ctx;
        if (type != 1 || size < sizeof(VendorState) || !self->onState_) return;
        self->onState_(*(const VendorState*)state);
    }

    HMODULE dll_ = nullptr; void* sap_ = nullptr; std::atomic<void*> dev_{nullptr};
    StateFn onState_; DeviceFn onDevice_;
    PfnCreate fnCreate_ = nullptr; PfnRelease fnRelease_ = nullptr;
    PfnIdPtr fnGetTpMode_ = nullptr, fnGetTpStatus_ = nullptr, fnGetVersion_ = nullptr;
    PfnIdByte fnSetTpMode_ = nullptr, fnSetTpStatus_ = nullptr;
    PfnIdBytePtr fnGetKeyMap_ = nullptr; PfnIdByteByte fnSetKeyMap_ = nullptr;
};

} // namespace onepad
