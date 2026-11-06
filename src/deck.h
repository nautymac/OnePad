// OnePad - Steam Deck controller (28DE:1205) persona: USB identity, Neptune input frames, Valve feature-report
// command channel (attributes, serials, settings, lizard mode, rumble).
#pragma once
#include "common.h"
#include "config.h"
#include "usbip.h"
#include "vendor.h"
#include <atomic>

namespace onepad {

struct XInputSnapshot { bool ok = false; uint16_t buttons = 0; uint8_t lt = 0, rt = 0; int16_t lx = 0, ly = 0, rx = 0, ry = 0; };

inline std::vector<uint8_t> HexBytes(const char* hex) {
    std::vector<uint8_t> v; for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) { char b[3] = {hex[i], hex[i + 1], 0}; v.push_back((uint8_t)strtol(b, nullptr, 16)); } return v;
}

class DeckDevice {
public:
    std::function<void(uint16_t left, uint16_t right)> onRumble;
    std::function<void(bool claimed)> onClaimChanged;

    void Configure(const DeckConfig& c) { cfg_ = c; }
    void SetPadFlip(bool leftX, bool leftY, bool rightX, bool rightY) { std::lock_guard<std::mutex> g(m_); flipLX_ = leftX; flipLY_ = leftY; flipRX_ = rightX; flipRY_ = rightY; }

    UsbDeviceModel Model() {
        UsbDeviceModel m;
        m.deviceDesc = HexBytes("1201000200000040de280512000301020301");
        m.configDesc = HexBytes("0902960005010480fa090400000103000200092111010001224100070581030800010904010001030101000921110100012227000705820308000109040200010300000009211101000122260007058303400001080b030202020100090403000102020100052400100105240103010424020205240603040705860310001009040400020a0000000705840240000007050502400000");
        m.reportDescs = {
            HexBytes("05010902a1010901a100050919012902150025017501950281027506950181010501093009311581257f750895028106950109388106050c0a380295018106c0c0"),
            HexBytes("05010906a101050719e029e71500250175019508810281011900296515002565750895068100c0"),
            HexBytes("06ffff0901a10109020903150026ff0075089540810209060907150026ff0075089540b102c0"),
        };
        m.strings = {L"Valve Software", L"Steam Controller", Widen(cfg_.serial), L"Full-Speed"};
        m.numInterfaces = 5;
        m.ifaces = {{3, 0, 2}, {3, 1, 1}, {3, 0, 0}, {2, 2, 1}, {10, 0, 0}};
        m.interruptInEp = 3; m.frameIntervalMs = 4;
        m.onSetReport = [this](uint8_t iface, uint8_t type, uint8_t id, const uint8_t* d, int n) { return SetReport(iface, type, id, d, n); };
        m.onGetReport = [this](uint8_t iface, uint8_t type, uint8_t id, std::vector<uint8_t>& out) { return GetReport(iface, type, id, out); };
        m.fillFrame = [this](uint8_t* f) { Fill(f); };
        return m;
    }

    bool Claimed() const { return claimed_; }

    // Called per vendor report (~125 Hz) with the latest controller state.
    // When false, the frame carries buttons/sticks/triggers only: pads, grips and IMU stay with OnePad's own engine
    // (used while Steam holds the device but no Steam game is in the foreground).
    void SetPassPads(bool on) { passPads_ = on; }
    // When false, buttons/sticks/triggers are left idle too (only Steam/QAM pass): used on the desktop so Steam's desktop layout cannot turn the pad into keyboard/mouse input.
    void SetPassButtons(bool on) { passButtons_ = on; }
    void SetPassGrips(bool on) { passGrips_ = on; }     // L4/R4 bits follow the button routing, not the pad routing
    bool PassPads() const { return passPads_; }

    void Update(const VendorState& s0, const XInputSnapshot& x, bool gripLeft, bool gripRight) {
        std::lock_guard<std::mutex> g(m_);
        VendorState s = s0;
        if (!passPads_) {
            s.rPadXH = s.rPadXL = s.rPadYH = s.rPadYL = 0; s.lPadXH = s.lPadXL = s.lPadYH = s.lPadYL = 0;
        }
        if (!passGrips_) gripLeft = gripRight = false;
        uint64_t b = 0;
        auto set = [&](int bit, bool on) { if (on) b |= (1ULL << bit); };
        (void)x;   // XInput is not used: the real Xbox node is disabled while Steam runs, everything comes from the vendor report
        // buttons2: A 0x80  B 0x40  X 0x20  Y 0x10  LB 0x08  LT 0x04  RB 0x02  RT 0x01   (A/B/X/Y measured, rest per Prd)
        set(7, s.buttons2 & 0x80); set(5, s.buttons2 & 0x40); set(6, s.buttons2 & 0x20); set(4, s.buttons2 & 0x10);
        set(3, s.buttons2 & 0x08); set(2, s.buttons2 & 0x02);
        // buttons1: Setup 0x80  Home 0x40  L3 0x20  R3 0x10  Up 0x08  Down 0x04  Left 0x02  Right 0x01
        set(8, s.buttons1 & 0x08); set(11, s.buttons1 & 0x04); set(10, s.buttons1 & 0x02); set(9, s.buttons1 & 0x01);
        set(22, s.buttons1 & 0x20); set(26, s.buttons1 & 0x10);
        // View / Menu: configurable byte:mask (default buttons3 0x02 / 0x01, as captured)
        set(12, Bit(s, cfg_.viewByte, cfg_.viewMask)); set(14, Bit(s, cfg_.menuByte, cfg_.menuMask));
        set(0, s.rTrigger >= 230 || (s.buttons2 & 0x01)); set(1, s.lTrigger >= 230 || (s.buttons2 & 0x04));
        set(cfg_.homeIsGuide ? 13 : 50, s.buttons1 & 0x40);   // Home
        set(cfg_.homeIsGuide ? 50 : 13, s.buttons1 & 0x80);   // Setup
        set(41, gripLeft); set(42, gripRight);                 // L4, R4
        int lx = s.LPadX(), ly = s.LPadY(), rx = s.RPadX(), ry = s.RPadY();
        bool lt = (lx | ly) != 0, rt = (rx | ry) != 0;
        if (lt) { if (flipLX_) lx = std::max(1, 1024 - lx); if (flipLY_) ly = std::max(1, 1024 - ly); }
        if (rt) { if (flipRX_) rx = std::max(1, 1024 - rx); if (flipRY_) ry = std::max(1, 1024 - ry); }
        // The 3D One pads have no physical click: a tap becomes a short Deck trackpad click, tap-then-hold becomes a held click (drag).
        bool clickL = false, clickR = false;
        if (cfg_.tapClick) {
            clickL = TapLogic(tapL_, lt, lx, ly); clickR = TapLogic(tapR_, rt, rx, ry);
            if (!lt && tapL_.holdTouch) { lt = true; lx = tapL_.lastX; ly = tapL_.lastY; }   // keep the contact alive during the click pulse
            if (!rt && tapR_.holdTouch) { rt = true; rx = tapR_.lastX; ry = tapR_.lastY; }
            set(17, clickL); set(18, clickR);
        }
        set(19, lt); set(20, rt);
        if ((b & ~0x180000ULL) != (lastLogged_ & ~0x180000ULL)) {   // log button changes (ignore pad-touch bits)
            LOGI("deck: buttons=%016llX  raw b1=%02X b2=%02X b3=%02X b4=%02X trig=%u/%u", (unsigned long long)b, s.buttons1, s.buttons2, s.buttons3, s.buttons4, s.lTrigger, s.rTrigger);
            lastLogged_ = b;
        }
        buttons_ = passButtons_ ? b : (b & ((1ULL << 13) | (1ULL << 50)));
        lpadX_ = lt ? (int16_t)std::clamp((lx - 512) * 64, -32767, 32767) : 0; lpadY_ = lt ? (int16_t)std::clamp((512 - ly) * 64, -32767, 32767) : 0;
        rpadX_ = rt ? (int16_t)std::clamp((rx - 512) * 64, -32767, 32767) : 0; rpadY_ = rt ? (int16_t)std::clamp((512 - ry) * 64, -32767, 32767) : 0;
        // Steam judges a Deck trackpad click by pressure, not only by the click bit (pressure 12000 alone reads as a held click),
        // so the tap pulse carries full pressure too.
        lpress_ = lt ? (uint16_t)(clickL ? cfg_.tapPressure : cfg_.padPressure) : 0; rpress_ = rt ? (uint16_t)(clickR ? cfg_.tapPressure : cfg_.padPressure) : 0;
        auto ax = [](int v) { return (int16_t)std::clamp((v - 128) * 258, -32767, 32767); };
        if (passButtons_) { lsX_ = ax(s.lStickX); lsY_ = ax(256 - s.lStickY); rsX_ = ax(s.rStickX); rsY_ = ax(256 - s.rStickY); } else { lsX_ = lsY_ = rsX_ = rsY_ = 0; }
        if (passButtons_) { ltrig_ = (uint16_t)std::min(32767, s.lTrigger * 129); rtrig_ = (uint16_t)std::min(32767, s.rTrigger * 129); } else { ltrig_ = rtrig_ = 0; }
        double a[3], gy[3]; s.Accel(true, a); s.Gyro(true, gy);
        if (!passPads_) { gy[0] = gy[1] = gy[2] = 0; }
        for (int i = 0; i < 3; i++) { accel_[i] = (int16_t)std::clamp(MapAxis(a, i) * cfg_.accelScale, -32767.0, 32767.0); gyro_[i] = (int16_t)std::clamp(MapAxis(gy, i) * cfg_.gyroScale, -32767.0, 32767.0); }
    }

private:
    static bool Bit(const VendorState& s, int byteIdx, int mask) {
        const uint8_t* bb[] = {&s.buttons1, &s.buttons2, &s.buttons3, &s.buttons4};
        return byteIdx >= 1 && byteIdx <= 4 && mask && (*bb[byteIdx - 1] & mask) != 0;
    }
    double MapAxis(const double src[3], int deckAxis) const {
        const std::string& m = deckAxis == 0 ? cfg_.axisX : deckAxis == 1 ? cfg_.axisY : cfg_.axisZ;
        if (m.size() < 2) return 0;
        double sgn = m[0] == '-' ? -1 : 1; int idx = m[1] == 'x' ? 0 : m[1] == 'y' ? 1 : 2;
        return sgn * src[idx];
    }
    static void P16(uint8_t* f, int o, int v) { f[o] = (uint8_t)(v & 0xFF); f[o + 1] = (uint8_t)((v >> 8) & 0xFF); }

    void Fill(uint8_t* f) {
        std::lock_guard<std::mutex> g(m_);
        uint32_t n = ++packet_;
        f[0] = 1; f[1] = 0; f[2] = 9; f[3] = 64;
        f[4] = n & 0xFF; f[5] = (n >> 8) & 0xFF; f[6] = (n >> 16) & 0xFF; f[7] = (n >> 24) & 0xFF;
        for (int i = 0; i < 8; i++) f[8 + i] = (uint8_t)((buttons_ >> (8 * i)) & 0xFF);
        P16(f, 16, lpadX_); P16(f, 18, lpadY_); P16(f, 20, rpadX_); P16(f, 22, rpadY_);
        // wire order: accel X, Z(neg), Y ; gyro X, Z(neg), Y   (SDL_hidapi_steamdeck reads (X, Z, -Y))
        P16(f, 24, accel_[0]); P16(f, 26, -accel_[2]); P16(f, 28, accel_[1]);
        P16(f, 30, gyro_[0]); P16(f, 32, -gyro_[2]); P16(f, 34, gyro_[1]);
        P16(f, 44, ltrig_); P16(f, 46, rtrig_);
        P16(f, 48, lsX_); P16(f, 50, lsY_); P16(f, 52, rsX_); P16(f, 54, rsY_);
        P16(f, 56, lpress_); P16(f, 58, rpress_);
    }

    void SetClaimed(bool c, const char* why) {
        if (claimed_ == c) return;
        claimed_ = c; LOGI("deck: %s (%s)", c ? "claimed by host (lizard off)" : "released (lizard on)", why);
        if (onClaimChanged) onClaimChanged(c);
    }

    int SetReport(uint8_t iface, uint8_t type, uint8_t id, const uint8_t* d, int n) {
        if (iface != 2) return 0;                       // keyboard/mouse LEDs etc: accept
        if (type != 3 || n < 2) return 0;
        std::lock_guard<std::mutex> g(m_);
        lastSet_.assign(d, d + n); lastSet_.resize(64, 0);
        uint8_t cmd = d[0], len = d[1];
        { char hx[80]; int o = 0; for (int i = 0; i < n && i < 20 && o < 70; i++) o += snprintf(hx + o, sizeof hx - o, "%02x", d[i]); LOGI("deck: SET cmd=%02x len=%u [%s]", cmd, len, hx); }
        switch (cmd) {
            case 0x81: SetClaimed(true, "CLEAR_DIGITAL_MAPPINGS"); break;
            case 0x85: case 0x8E: SetClaimed(false, cmd == 0x85 ? "SET_DEFAULT_DIGITAL_MAPPINGS" : "LOAD_DEFAULT_SETTINGS"); break;
            case 0x87: { // settings: (id u8, value u16) triples
                for (int i = 2; i + 2 < 2 + len && i + 2 < n; i += 3) {
                    uint8_t sid = d[i]; uint16_t val = d[i + 1] | (d[i + 2] << 8);
                    if (sid == 9) SetClaimed(val == 0, "SETTING_LIZARD_MODE");
                }
                break; }
            case 0xEB: { // rumble: [2] type [3-4] intensity [5-6] left speed [7-8] right speed [9] gainL [10] gainR
                if (n >= 9) {
                    uint16_t intensity = d[3] | (d[4] << 8), ls = d[5] | (d[6] << 8), rs = d[7] | (d[8] << 8);
                    uint32_t l = std::min<uint32_t>(65535, (uint32_t)((ls ? ls : intensity) * cfg_.rumbleGain));
                    uint32_t r = std::min<uint32_t>(65535, (uint32_t)((rs ? rs : intensity) * cfg_.rumbleGain));
                    if (onRumble) onRumble((uint16_t)l, (uint16_t)r);
                }
                break; }
            case 0x8F: break; // haptic pulse on a trackpad: no actuator exposed, ignore
            default: break;
        }
        return 0;
    }

    bool GetReport(uint8_t iface, uint8_t type, uint8_t id, std::vector<uint8_t>& out) {
        if (iface != 2 || type != 3) { out.assign(iface == 2 ? 64 : 8, 0); return true; }
        std::lock_guard<std::mutex> g(m_);
        if (lastSet_.empty()) { out.assign(64, 0); return true; }
        uint8_t cmd = lastSet_[0];
        LOGI("deck: GET after cmd=%02x", cmd);
        out.assign(64, 0);
        if (cmd == 0x83) {
            auto a = HexBytes("832d010512000002000000000a2b12a96204b7617c67092e0000000ba00f00000d000000000c000000000e00000000");
            memcpy(out.data(), a.data(), a.size()); return true;
        }
        if (cmd == 0xAE) {
            uint8_t idx = lastSet_[2];
            std::string serial = idx == 0 ? std::string("MHDA") + cfg_.serial.substr(4) : cfg_.serial;
            if (idx > 1) { out[0] = 0xAE; out[1] = 0x14; out[2] = 0xFF; return true; }
            out[0] = 0xAE; out[1] = 0x14; out[2] = idx; memcpy(out.data() + 3, serial.data(), std::min<size_t>(12, serial.size())); return true;
        }
        if (cmd == 0x87) { memcpy(out.data(), lastSet_.data(), 64); return true; }
        return false; // stall for unknown messages, as a real device does
    }

    DeckConfig cfg_;
    std::mutex m_;
    std::atomic<bool> claimed_{false};
    std::vector<uint8_t> lastSet_;
    uint32_t packet_ = 0; uint64_t buttons_ = 0, lastLogged_ = 0;
    int16_t lpadX_ = 0, lpadY_ = 0, rpadX_ = 0, rpadY_ = 0, lsX_ = 0, lsY_ = 0, rsX_ = 0, rsY_ = 0;
    uint16_t ltrig_ = 0, rtrig_ = 0, lpress_ = 0, rpress_ = 0;
    int16_t accel_[3] = {0, 0, 0}, gyro_[3] = {0, 0, 0};
    bool flipLX_ = true, flipLY_ = true, flipRX_ = false, flipRY_ = false;
    std::atomic<bool> passPads_{true}, passButtons_{true}, passGrips_{true};

    struct TapState { bool touching = false, dragging = false, second = false, holdTouch = false; DWORD t0 = 0, clickUntil = 0, lastTap = 0; int lastX = 0, lastY = 0; double dist = 0; };
    TapState tapL_, tapR_;
    // Click-bit state for this report. Rules: a short touch = one click pulse; a second short touch right after a tap = a
    // second click (double click); a second touch that is held past tap_ms or moves = held click (drag) until lift.
    bool TapLogic(TapState& t, bool touching, int x, int y) {
        DWORD now = GetTickCount();
        if (touching) {
            if (!t.touching) {
                t.touching = true; t.t0 = now; t.dist = 0; t.dragging = false;
                t.second = cfg_.tapDrag && t.lastTap && now - t.lastTap <= (DWORD)std::max(450, cfg_.tapMs * 2);
                t.lastTap = 0;
            } else t.dist += std::sqrt((double)(x - t.lastX) * (x - t.lastX) + (double)(y - t.lastY) * (y - t.lastY));
            t.lastX = x; t.lastY = y;
            if (t.second && !t.dragging && (now - t.t0 > (DWORD)cfg_.tapMs || t.dist > cfg_.tapDist)) t.dragging = true;   // tap-then-hold -> drag
        } else if (t.touching) {
            t.touching = false;
            if (t.dragging) { t.dragging = false; t.second = false; }
            else if (now - t.t0 <= (DWORD)cfg_.tapMs && t.dist <= cfg_.tapDist) { t.clickUntil = now + 70; t.lastTap = t.second ? 0 : now; t.second = false; }
            else t.second = false;
        }
        bool pulse = !t.touching && now < t.clickUntil;
        t.holdTouch = pulse;
        return t.dragging || pulse;
    }
};

// XInput polling (dynamically loaded, so a missing DLL never breaks the build or start-up)
class XInputReader {
public:
    XInputReader() {
        dll_ = LoadLibraryW(L"xinput1_4.dll"); if (!dll_) dll_ = LoadLibraryW(L"xinput9_1_0.dll");
        if (dll_) { get_ = (PfnGet)GetProcAddress(dll_, "XInputGetState"); set_ = (PfnSet)GetProcAddress(dll_, "XInputSetState"); }
    }
    XInputSnapshot Read(int idx = 0) {
        XInputSnapshot s; if (!get_) return s;
        struct { DWORD packet; WORD buttons; BYTE lt, rt; SHORT lx, ly, rx, ry; } st{};
        if (get_(idx, &st) != 0) return s;
        s.ok = true; s.buttons = st.buttons; s.lt = st.lt; s.rt = st.rt; s.lx = st.lx; s.ly = st.ly; s.rx = st.rx; s.ry = st.ry; return s;
    }
    void Rumble(int idx, uint16_t left, uint16_t right) { if (!set_) return; struct { WORD l, r; } v{left, right}; set_(idx, &v); }
private:
    typedef DWORD (WINAPI* PfnGet)(DWORD, void*); typedef DWORD (WINAPI* PfnSet)(DWORD, void*);
    HMODULE dll_ = nullptr; PfnGet get_ = nullptr; PfnSet set_ = nullptr;
};

} // namespace onepad
