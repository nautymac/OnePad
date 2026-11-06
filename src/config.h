// OnePad - configuration (INI file) and key-name parsing
#pragma once
#include "common.h"
#include <map>

namespace onepad {

enum class PadMode { Mouse, Scroll, DPad, NativeTouchpad, NativeWheel, NativeLeftStick, NativeRightStick, Off };
enum class TapAction { None, Left, Right, Middle };
enum class GripKind { None, ModeShift, GyroHold, GyroToggle, Key, MouseButton };
enum class GyroMode { Off, Mouse };
enum class GyroActivate { Always, Grip, RightTouch, LeftTouch, Toggle };
enum class Axis { X, Y, Z, World };

inline const char* PadModeName(PadMode m) {
    switch (m) {
        case PadMode::Mouse: return "mouse"; case PadMode::Scroll: return "scroll"; case PadMode::DPad: return "dpad";
        case PadMode::NativeTouchpad: return "native_touchpad"; case PadMode::NativeWheel: return "native_wheel";
        case PadMode::NativeLeftStick: return "native_leftstick"; case PadMode::NativeRightStick: return "native_rightstick";
        case PadMode::Off: return "off";
    }
    return "mouse";
}
inline bool ParsePadMode(const std::string& s0, PadMode& out) {
    std::string s = Lower(Trim(s0));
    static const std::pair<const char*, PadMode> t[] = {
        {"mouse", PadMode::Mouse}, {"scroll", PadMode::Scroll}, {"dpad", PadMode::DPad},
        {"native_touchpad", PadMode::NativeTouchpad}, {"native_wheel", PadMode::NativeWheel},
        {"native_leftstick", PadMode::NativeLeftStick}, {"native_rightstick", PadMode::NativeRightStick}, {"off", PadMode::Off}};
    for (auto& p : t) if (s == p.first) { out = p.second; return true; }
    return false;
}

// Key names -> virtual key codes
inline int KeyFromName(const std::string& n0) {
    std::string n = Lower(Trim(n0));
    if (n.size() == 1) { char c = (char)toupper((unsigned char)n[0]); if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return c; }
    static const std::map<std::string, int> t = {
        {"up", VK_UP}, {"down", VK_DOWN}, {"left", VK_LEFT}, {"right", VK_RIGHT}, {"space", VK_SPACE}, {"enter", VK_RETURN}, {"return", VK_RETURN},
        {"esc", VK_ESCAPE}, {"escape", VK_ESCAPE}, {"tab", VK_TAB}, {"shift", VK_SHIFT}, {"lshift", VK_LSHIFT}, {"rshift", VK_RSHIFT},
        {"ctrl", VK_CONTROL}, {"control", VK_CONTROL}, {"lctrl", VK_LCONTROL}, {"rctrl", VK_RCONTROL}, {"alt", VK_MENU}, {"lalt", VK_LMENU}, {"ralt", VK_RMENU},
        {"win", VK_LWIN}, {"lwin", VK_LWIN}, {"backspace", VK_BACK}, {"delete", VK_DELETE}, {"del", VK_DELETE}, {"insert", VK_INSERT}, {"home", VK_HOME}, {"end", VK_END},
        {"pageup", VK_PRIOR}, {"pgup", VK_PRIOR}, {"pagedown", VK_NEXT}, {"pgdn", VK_NEXT}, {"capslock", VK_CAPITAL}, {"printscreen", VK_SNAPSHOT},
        {"f1", VK_F1}, {"f2", VK_F2}, {"f3", VK_F3}, {"f4", VK_F4}, {"f5", VK_F5}, {"f6", VK_F6}, {"f7", VK_F7}, {"f8", VK_F8}, {"f9", VK_F9}, {"f10", VK_F10},
        {"f11", VK_F11}, {"f12", VK_F12}, {"f13", VK_F13}, {"f14", VK_F14}, {"f15", VK_F15}, {"f16", VK_F16},
        {"numpad0", VK_NUMPAD0}, {"numpad1", VK_NUMPAD1}, {"numpad2", VK_NUMPAD2}, {"numpad3", VK_NUMPAD3}, {"numpad4", VK_NUMPAD4},
        {"numpad5", VK_NUMPAD5}, {"numpad6", VK_NUMPAD6}, {"numpad7", VK_NUMPAD7}, {"numpad8", VK_NUMPAD8}, {"numpad9", VK_NUMPAD9},
        {"comma", VK_OEM_COMMA}, {"period", VK_OEM_PERIOD}, {"minus", VK_OEM_MINUS}, {"plus", VK_OEM_PLUS}, {"grave", VK_OEM_3}, {"tilde", VK_OEM_3},
        {"hangul", 0x15}, {"hanja", 0x19}, {"apps", VK_APPS}, {"menu", VK_APPS},
        {"volumeup", VK_VOLUME_UP}, {"volumedown", VK_VOLUME_DOWN}, {"mute", VK_VOLUME_MUTE}, {"mediaplay", VK_MEDIA_PLAY_PAUSE},
    };
    auto it = t.find(n); if (it != t.end()) return it->second;
    if (n.rfind("vk", 0) == 0 || n.rfind("0x", 0) == 0) { return (int)strtol(n.c_str() + 2, nullptr, 16); }
    return 0;
}
inline std::vector<int> KeysFromCombo(const std::string& s) {
    std::vector<int> v; for (auto& k : Split(s, '+')) { int vk = KeyFromName(k); if (vk) v.push_back(vk); } return v;
}

struct PadConfig {
    PadMode mode = PadMode::Mouse;
    PadMode shiftMode = PadMode::Scroll;   // mode while the mode-shift grip is held
    bool haptics = false;                  // firmware touchpad mode (vibration on move) instead of wheel mode
    double sensitivity = 1.6;              // mouse pixels per pad unit
    double accel = 0.0;                    // extra multiplier per unit of speed (0 = linear)
    double friction = 0.0;                 // 0 = plain trackpad; 0.85-0.95 = trackball roll after lift
    bool flipX = false, flipY = false;     // sensor orientation fix (the left half is mounted rotated 180 degrees)
    double flickMin = 6.0;                 // min speed (units/report) to keep rolling after lift
    TapAction tap = TapAction::Left;
    int tapMs = 180;
    int tapDist = 24;
    bool tapDrag = false;                  // tap, then touch again and hold -> left button held until lift (off: misfires without a real pad button)
    int scrollStep = 40;                   // pad units per wheel notch
    bool scrollInvert = false;
    bool scrollHorizontal = true;
    int dpadKeys[4] = {VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT};
    double dpadDeadzone = 0.25;
    bool dpad8way = false;
};
struct GripConfig {
    GripKind kind = GripKind::None;
    std::vector<int> keys;                 // for Key
    int mouseButton = 0;                   // 1=left 2=right 3=middle 4=x1 5=x2
    uint8_t bit = 0;                       // raw bit in ButtonType3
};
struct GyroConfig {
    GyroMode mode = GyroMode::Mouse;
    GyroActivate activate = GyroActivate::Grip;
    bool sideRight = true;                 // which half's IMU
    double sensitivity = 10.0;             // pixels per degree
    bool invertX = false, invertY = false;
    double scaleDps = 16.4;                // raw LSB per deg/s
    Axis yaw = Axis::World;
    Axis pitch = Axis::Y;
    double deadzone = 0.4;                 // deg/s
    double bias[2][3] = {{0, 0, 0}, {0, 0, 0}}; // [left/right][x,y,z]
    bool biasValid[2] = {false, false};
};
struct DeckConfig {
    bool enabled = false;
    uint16_t port = 3240;
    std::string busid = "1-1";
    std::string usbipExe = "C:\\Program Files\\USBip\\usbip.exe";
    bool autoAttach = true;
    std::string attachArgs = "attach -r 127.0.0.1 -b 1-1 --once --receive-mode low-latency";
    std::string detachArgs = "detach --all";
    std::string stopAttemptsArgs = "attach -r 127.0.0.1 -b 1-1 --stop";   // cancel the vhci's automatic re-attach attempts
    int detachDelaySec = 90;               // keep the virtual Deck plugged in this long after the 3D One disappears
    std::string serial = "FX1A00000001";
    std::string axisX = "-y", axisY = "-x", axisZ = "-z";   // Deck IMU axis <- 3D One axis
    double accelScale = 4.0;
    double gyroScale = 1.0;
    bool homeIsGuide = true;
    double rumbleGain = 1.0;
    int viewByte = 3, viewMask = 0x02, menuByte = 3, menuMask = 0x01;   // vendor button byte (1-4) and mask for View / Menu
    int padPressure = 0;                   // pressure reported while a pad is touched (0 = touch only, never a click)
    int tapPressure = 32767;               // pressure sent during the tap click pulse (Steam judges trackpad clicks by pressure)
    std::string padsToSteam = "games_only"; // games_only: pads/grips/gyro go to Steam only while a Steam game is in front; always
    std::string buttonsToSteam = "games_only"; // games_only: buttons/sticks/triggers go to Steam only while a Steam game or the Steam client is in front; always
    bool tapClick = true, tapDrag = false;  // tap -> Deck trackpad click pulse; tap-then-hold drag off by default
    int tapMs = 180, tapDist = 24;
};

struct HidHideConfig { std::string mode = "auto"; std::string cli; int graceSec = 45; };   // HidHide cloak: auto (while steam.exe runs) | always | never
struct XboxHideConfig { std::string mode = "never"; };                  // XUSB node is CR_NOT_DISABLEABLE on this controller; kept for experiments only

struct Config {
    DeckConfig deck;
    HidHideConfig hidhide;
    XboxHideConfig xbox;
    bool enabled = true;
    bool unmapGrips = true;                // set firmware M1/M2 mapping to "no function" while running
    bool restoreOnExit = true;
    bool autostart = true;                 // register the logon task on first run
    PadConfig right, left;
    GripConfig gripLeft, gripRight;
    GyroConfig gyro;
    std::string ptpRight = "COL03", ptpLeft = "COL05", mouseCol = "COL02";
    bool debug = false;

    Config() {
        left.mode = PadMode::Scroll; left.shiftMode = PadMode::DPad; left.flipX = true; left.flipY = true;
        right.mode = PadMode::Mouse; right.shiftMode = PadMode::Scroll;
        gripLeft.kind = GripKind::MouseButton; gripLeft.mouseButton = 1; gripLeft.bit = 0x10;
        gripRight.kind = GripKind::MouseButton; gripRight.mouseButton = 2; gripRight.bit = 0x80;
    }
};

inline std::string TapName(TapAction t) { return t == TapAction::Left ? "left" : t == TapAction::Right ? "right" : t == TapAction::Middle ? "middle" : "none"; }
inline std::string AxisName(Axis a) { return a == Axis::X ? "x" : a == Axis::Y ? "y" : a == Axis::Z ? "z" : "world"; }
inline Axis AxisFrom(const std::string& s0, Axis def) { std::string s = Lower(Trim(s0)); return s == "x" ? Axis::X : s == "y" ? Axis::Y : s == "z" ? Axis::Z : s == "world" ? Axis::World : def; }
inline std::string KeyName(int vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, (char)vk);
    switch (vk) {
        case VK_UP: return "UP"; case VK_DOWN: return "DOWN"; case VK_LEFT: return "LEFT"; case VK_RIGHT: return "RIGHT"; case VK_SPACE: return "SPACE";
        case VK_RETURN: return "ENTER"; case VK_ESCAPE: return "ESC"; case VK_TAB: return "TAB"; case VK_SHIFT: return "SHIFT"; case VK_CONTROL: return "CTRL";
        case VK_MENU: return "ALT"; case VK_LWIN: return "WIN"; case VK_BACK: return "BACKSPACE"; case VK_DELETE: return "DELETE";
    }
    if (vk >= VK_F1 && vk <= VK_F16) return "F" + std::to_string(vk - VK_F1 + 1);
    char b[16]; snprintf(b, sizeof b, "0x%02X", vk); return b;
}
inline std::string GripString(const GripConfig& g) {
    switch (g.kind) {
        case GripKind::None: return "none"; case GripKind::ModeShift: return "modeshift"; case GripKind::GyroHold: return "gyro_hold"; case GripKind::GyroToggle: return "gyro_toggle";
        case GripKind::Key: { std::string s = "key:"; for (size_t i = 0; i < g.keys.size(); i++) { if (i) s += "+"; s += KeyName(g.keys[i]); } return s; }
        case GripKind::MouseButton: { static const char* n[] = {"", "left", "right", "middle", "x1", "x2"}; return std::string("mouse:") + n[g.mouseButton]; }
    }
    return "none";
}
inline void ParseGrip(const std::string& s0, GripConfig& g) {
    std::string s = Lower(Trim(s0));
    g.kind = GripKind::None; g.keys.clear(); g.mouseButton = 0;
    if (s == "modeshift") g.kind = GripKind::ModeShift;
    else if (s == "gyro_hold") g.kind = GripKind::GyroHold;
    else if (s == "gyro_toggle") g.kind = GripKind::GyroToggle;
    else if (s.rfind("key:", 0) == 0) { g.keys = KeysFromCombo(s0.substr(4)); if (!g.keys.empty()) g.kind = GripKind::Key; }
    else if (s.rfind("mouse:", 0) == 0) {
        std::string b = s.substr(6); g.mouseButton = b == "left" ? 1 : b == "right" ? 2 : b == "middle" ? 3 : b == "x1" ? 4 : b == "x2" ? 5 : 0;
        if (g.mouseButton) g.kind = GripKind::MouseButton;
    }
}

typedef std::map<std::string, std::map<std::string, std::string>> IniData;

inline IniData ReadIni(const std::wstring& path) {
    IniData d; FILE* f = _wfopen(path.c_str(), L"rb"); if (!f) return d;
    std::string content; char buf[4096]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) content.append(buf, n); fclose(f);
    if (content.size() >= 3 && (unsigned char)content[0] == 0xEF) content = content.substr(3);
    std::string section;
    for (auto& raw : Split(content, '\n')) {
        std::string line = Trim(raw); if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        size_t c = line.find(" ;"); if (c != std::string::npos) line = Trim(line.substr(0, c));
        if (line[0] == '[') { section = Lower(Trim(line.substr(1, line.find(']') - 1))); continue; }
        size_t eq = line.find('='); if (eq == std::string::npos) continue;
        d[section][Lower(Trim(line.substr(0, eq)))] = Trim(line.substr(eq + 1));
    }
    return d;
}
inline std::string IniGet(const IniData& d, const std::string& s, const std::string& k, const std::string& def) {
    auto a = d.find(s); if (a == d.end()) return def; auto b = a->second.find(k); return b == a->second.end() ? def : b->second;
}
inline double IniGetD(const IniData& d, const std::string& s, const std::string& k, double def) { std::string v = IniGet(d, s, k, ""); return v.empty() ? def : atof(v.c_str()); }
inline int IniGetI(const IniData& d, const std::string& s, const std::string& k, int def) { std::string v = IniGet(d, s, k, ""); return v.empty() ? def : atoi(v.c_str()); }
inline bool IniGetB(const IniData& d, const std::string& s, const std::string& k, bool def) { std::string v = Lower(IniGet(d, s, k, "")); return v.empty() ? def : (v == "1" || v == "true" || v == "on" || v == "yes"); }

inline void LoadPad(const IniData& d, const std::string& sec, PadConfig& p) {
    PadMode m; if (ParsePadMode(IniGet(d, sec, "mode", ""), m)) p.mode = m;
    if (ParsePadMode(IniGet(d, sec, "shift_mode", ""), m)) p.shiftMode = m;
    p.haptics = IniGetB(d, sec, "haptics", p.haptics);
    p.sensitivity = IniGetD(d, sec, "sensitivity", p.sensitivity);
    p.accel = IniGetD(d, sec, "accel", p.accel);
    p.friction = IniGetD(d, sec, "friction", p.friction);
    p.flipX = IniGetB(d, sec, "flip_x", p.flipX);
    p.flipY = IniGetB(d, sec, "flip_y", p.flipY);
    p.flickMin = IniGetD(d, sec, "flick_min", p.flickMin);
    std::string tap = Lower(IniGet(d, sec, "tap", TapName(p.tap)));
    p.tap = tap == "left" ? TapAction::Left : tap == "right" ? TapAction::Right : tap == "middle" ? TapAction::Middle : TapAction::None;
    p.tapMs = IniGetI(d, sec, "tap_ms", p.tapMs);
    p.tapDist = IniGetI(d, sec, "tap_dist", p.tapDist);
    p.tapDrag = IniGetB(d, sec, "tap_drag", p.tapDrag);
    p.scrollStep = std::max(4, IniGetI(d, sec, "scroll_step", p.scrollStep));
    p.scrollInvert = IniGetB(d, sec, "scroll_invert", p.scrollInvert);
    p.scrollHorizontal = IniGetB(d, sec, "scroll_horizontal", p.scrollHorizontal);
    auto keys = Split(IniGet(d, sec, "dpad_keys", ""), ',');
    if (keys.size() == 4) for (int i = 0; i < 4; i++) { int vk = KeyFromName(keys[i]); if (vk) p.dpadKeys[i] = vk; }
    p.dpadDeadzone = IniGetD(d, sec, "dpad_deadzone", p.dpadDeadzone);
    p.dpad8way = IniGetB(d, sec, "dpad_8way", p.dpad8way);
}

inline void LoadConfig(const std::wstring& path, Config& c) {
    IniData d = ReadIni(path);
    if (d.empty()) return;
    c.enabled = IniGetB(d, "general", "enabled", c.enabled);
    c.unmapGrips = IniGetB(d, "general", "unmap_grips", c.unmapGrips);
    c.restoreOnExit = IniGetB(d, "general", "restore_on_exit", c.restoreOnExit);
    c.autostart = IniGetB(d, "general", "autostart", c.autostart);
    c.debug = IniGetB(d, "general", "debug", c.debug);
    LoadPad(d, "pad.right", c.right); LoadPad(d, "pad.left", c.left);
    ParseGrip(IniGet(d, "grip.left", "action", GripString(c.gripLeft)), c.gripLeft);
    ParseGrip(IniGet(d, "grip.right", "action", GripString(c.gripRight)), c.gripRight);
    c.gripLeft.bit = (uint8_t)strtol(IniGet(d, "grip.left", "bit", "0x10").c_str(), nullptr, 0);
    c.gripRight.bit = (uint8_t)strtol(IniGet(d, "grip.right", "bit", "0x80").c_str(), nullptr, 0);
    std::string gm = Lower(IniGet(d, "gyro", "mode", "mouse")); c.gyro.mode = gm == "off" ? GyroMode::Off : GyroMode::Mouse;
    std::string ga = Lower(IniGet(d, "gyro", "activate", "grip"));
    c.gyro.activate = ga == "always" ? GyroActivate::Always : ga == "right_touch" ? GyroActivate::RightTouch : ga == "left_touch" ? GyroActivate::LeftTouch : ga == "toggle" ? GyroActivate::Toggle : GyroActivate::Grip;
    c.gyro.sideRight = Lower(IniGet(d, "gyro", "side", "right")) != "left";
    c.gyro.sensitivity = IniGetD(d, "gyro", "sensitivity", c.gyro.sensitivity);
    c.gyro.invertX = IniGetB(d, "gyro", "invert_x", false); c.gyro.invertY = IniGetB(d, "gyro", "invert_y", false);
    c.gyro.scaleDps = IniGetD(d, "gyro", "scale_dps", c.gyro.scaleDps);
    c.gyro.yaw = AxisFrom(IniGet(d, "gyro", "yaw", "world"), Axis::World);
    c.gyro.pitch = AxisFrom(IniGet(d, "gyro", "pitch", "y"), Axis::Y);
    c.gyro.deadzone = IniGetD(d, "gyro", "deadzone", c.gyro.deadzone);
    for (int s = 0; s < 2; s++) {
        std::string pre = s == 0 ? "bias_left_" : "bias_right_";
        if (!IniGet(d, "gyro", pre + "x", "").empty()) { c.gyro.biasValid[s] = true; const char* ax[] = {"x", "y", "z"}; for (int i = 0; i < 3; i++) c.gyro.bias[s][i] = IniGetD(d, "gyro", pre + ax[i], 0); }
    }
    c.ptpRight = IniGet(d, "devices", "ptp_right", c.ptpRight); c.ptpLeft = IniGet(d, "devices", "ptp_left", c.ptpLeft); c.mouseCol = IniGet(d, "devices", "mouse", c.mouseCol);
    DeckConfig& k = c.deck;
    k.enabled = IniGetB(d, "deck", "enabled", k.enabled);
    k.port = (uint16_t)IniGetI(d, "deck", "port", k.port);
    k.busid = IniGet(d, "deck", "busid", k.busid);
    k.usbipExe = IniGet(d, "deck", "usbip_exe", k.usbipExe);
    k.autoAttach = IniGetB(d, "deck", "auto_attach", k.autoAttach);
    k.attachArgs = IniGet(d, "deck", "attach_args", k.attachArgs);
    k.detachArgs = IniGet(d, "deck", "detach_args", k.detachArgs);
    k.stopAttemptsArgs = IniGet(d, "deck", "stop_attempts_args", k.stopAttemptsArgs);
    k.detachDelaySec = IniGetI(d, "deck", "detach_delay_s", k.detachDelaySec);
    k.serial = IniGet(d, "deck", "serial", k.serial);
    k.axisX = IniGet(d, "deck", "axis_x", k.axisX); k.axisY = IniGet(d, "deck", "axis_y", k.axisY); k.axisZ = IniGet(d, "deck", "axis_z", k.axisZ);
    k.accelScale = IniGetD(d, "deck", "accel_scale", k.accelScale);
    k.gyroScale = IniGetD(d, "deck", "gyro_scale", k.gyroScale);
    k.homeIsGuide = IniGetB(d, "deck", "home_is_guide", k.homeIsGuide);
    k.rumbleGain = IniGetD(d, "deck", "rumble_gain", k.rumbleGain);
    c.hidhide.mode = Lower(IniGet(d, "hidhide", "mode", c.hidhide.mode));
    c.hidhide.graceSec = IniGetI(d, "hidhide", "grace_s", c.hidhide.graceSec);
    c.xbox.mode = Lower(IniGet(d, "xbox", "hide", c.xbox.mode));
    c.deck.viewByte = IniGetI(d, "deck.buttons", "view_byte", c.deck.viewByte); c.deck.viewMask = (int)strtol(IniGet(d, "deck.buttons", "view_mask", "0x02").c_str(), nullptr, 0);
    c.deck.menuByte = IniGetI(d, "deck.buttons", "menu_byte", c.deck.menuByte); c.deck.menuMask = (int)strtol(IniGet(d, "deck.buttons", "menu_mask", "0x01").c_str(), nullptr, 0);
    c.deck.padPressure = IniGetI(d, "deck", "pad_pressure", c.deck.padPressure);
    c.deck.tapPressure = IniGetI(d, "deck", "tap_pressure", c.deck.tapPressure);
    c.deck.padsToSteam = Lower(IniGet(d, "deck", "pads_to_steam", c.deck.padsToSteam));
    c.deck.buttonsToSteam = Lower(IniGet(d, "deck", "buttons_to_steam", c.deck.buttonsToSteam));
    c.deck.tapClick = IniGetB(d, "deck", "tap_click", c.deck.tapClick);
    c.deck.tapDrag = IniGetB(d, "deck", "tap_drag", c.deck.tapDrag);
    c.deck.tapMs = IniGetI(d, "deck", "tap_ms", c.deck.tapMs);
    c.deck.tapDist = IniGetI(d, "deck", "tap_dist", c.deck.tapDist);
    c.hidhide.cli = IniGet(d, "hidhide", "cli", c.hidhide.cli);
}

inline std::string PadSection(const char* name, const PadConfig& p) {
    char b[2048];
    snprintf(b, sizeof b,
        "[%s]\n"
        "; mode: mouse | scroll | dpad | native_touchpad | native_wheel | native_leftstick | native_rightstick | off\n"
        "mode=%s\n"
        "; 모드시프트 그립을 누르는 동안의 모드\n"
        "shift_mode=%s\n"
        "; 1 = 펌웨어 터치패드 모드(손가락 움직일 때 진동), 0 = 휠 모드(진동 없음)\n"
        "haptics=%d\n"
        "; mouse 모드\n"
        "sensitivity=%.2f\n"
        "accel=%.2f\n"
        "friction=%.2f\n"
        "flick_min=%.1f\n"
        "tap=%s\n"
        "tap_ms=%d\n"
        "tap_dist=%d\n"
        "; 탭 직후 다시 대고 끌면 왼쪽 버튼 유지(드래그)\n"
        "tap_drag=%d\n"
        "; scroll 모드\n"
        "scroll_step=%d\n"
        "scroll_invert=%d\n"
        "scroll_horizontal=%d\n"
        "; dpad 모드 (위,아래,왼쪽,오른쪽 키)\n"
        "dpad_keys=%s,%s,%s,%s\n"
        "dpad_deadzone=%.2f\n"
        "dpad_8way=%d\n"
        "; 센서 좌표 뒤집기 (왼쪽 패드는 180도 돌아가 있어 기본 1,1)\n"
        "flip_x=%d\n"
        "flip_y=%d\n\n",
        name, PadModeName(p.mode), PadModeName(p.shiftMode), p.haptics ? 1 : 0, p.sensitivity, p.accel, p.friction, p.flickMin,
        TapName(p.tap).c_str(), p.tapMs, p.tapDist, p.tapDrag ? 1 : 0, p.scrollStep, p.scrollInvert ? 1 : 0, p.scrollHorizontal ? 1 : 0,
        KeyName(p.dpadKeys[0]).c_str(), KeyName(p.dpadKeys[1]).c_str(), KeyName(p.dpadKeys[2]).c_str(), KeyName(p.dpadKeys[3]).c_str(),
        p.dpadDeadzone, p.dpad8way ? 1 : 0, p.flipX ? 1 : 0, p.flipY ? 1 : 0);
    return b;
}

inline bool SaveConfig(const std::wstring& path, const Config& c) {
    std::string s = "; OnePad 설정 - abxylute 3D One 터치패드/백버튼/자이로 변환 서비스\n; 저장 후 트레이 메뉴의 '설정 다시 읽기' 또는 서비스 재시작\n\n";
    char b[2048];
    snprintf(b, sizeof b, "[general]\nenabled=%d\n; 실행 중 백버튼(M1/M2)의 펌웨어 매핑을 해제해 L3/R3 입력이 섞이지 않게 함\nunmap_grips=%d\n; 종료 시 펌웨어 설정을 원래대로 복원\nrestore_on_exit=%d\n; 로그온 시 자동 시작 작업(OnePad)을 첫 실행 때 스스로 등록\nautostart=%d\ndebug=%d\n\n",
             c.enabled ? 1 : 0, c.unmapGrips ? 1 : 0, c.restoreOnExit ? 1 : 0, c.autostart ? 1 : 0, c.debug ? 1 : 0);
    s += b;
    s += PadSection("pad.right", c.right);
    s += PadSection("pad.left", c.left);
    snprintf(b, sizeof b, "[grip.left]\n; none | modeshift | gyro_hold | gyro_toggle | key:CTRL+C | mouse:left|right|middle|x1|x2\naction=%s\nbit=0x%02X\n\n[grip.right]\naction=%s\nbit=0x%02X\n\n",
             GripString(c.gripLeft).c_str(), c.gripLeft.bit, GripString(c.gripRight).c_str(), c.gripRight.bit);
    s += b;
    const char* act = c.gyro.activate == GyroActivate::Always ? "always" : c.gyro.activate == GyroActivate::RightTouch ? "right_touch" : c.gyro.activate == GyroActivate::LeftTouch ? "left_touch" : c.gyro.activate == GyroActivate::Toggle ? "toggle" : "grip";
    snprintf(b, sizeof b, "[gyro]\n; mode: off | mouse\nmode=%s\n; activate: always | grip (gyro_hold 그립) | right_touch (오른쪽 패드 터치 중) | left_touch | toggle (gyro_toggle 그립)\nactivate=%s\n; 사용할 IMU: right | left\nside=%s\n; 1도 회전당 마우스 픽셀\nsensitivity=%.2f\ninvert_x=%d\ninvert_y=%d\n; 자이로 원시값 -> deg/s 변환 계수\nscale_dps=%.2f\n; yaw: world (중력 기준 수평 회전) | x | y | z,  pitch: x | y | z\nyaw=%s\npitch=%s\ndeadzone=%.2f\n",
             c.gyro.mode == GyroMode::Off ? "off" : "mouse", act, c.gyro.sideRight ? "right" : "left", c.gyro.sensitivity, c.gyro.invertX ? 1 : 0, c.gyro.invertY ? 1 : 0,
             c.gyro.scaleDps, AxisName(c.gyro.yaw).c_str(), AxisName(c.gyro.pitch).c_str(), c.gyro.deadzone);
    s += b;
    for (int i = 0; i < 2; i++) if (c.gyro.biasValid[i]) {
        snprintf(b, sizeof b, "bias_%s_x=%.3f\nbias_%s_y=%.3f\nbias_%s_z=%.3f\n", i ? "right" : "left", c.gyro.bias[i][0], i ? "right" : "left", c.gyro.bias[i][1], i ? "right" : "left", c.gyro.bias[i][2]);
        s += b;
    }
    snprintf(b, sizeof b, "\n[devices]\n; 서비스 실행 중 비활성화할 HID 컬렉션 (정밀 터치패드 2개, 마우스 컬렉션)\nptp_right=%s\nptp_left=%s\nmouse=%s\n", c.ptpRight.c_str(), c.ptpLeft.c_str(), c.mouseCol.c_str());
    s += b;
    const DeckConfig& k = c.deck;
    snprintf(b, sizeof b, "\n[deck]\n; Steam Deck 컨트롤러(28DE:1205) 에뮬레이션. usbip-win2 드라이버 필요. Steam이 점유하면 위 변환은 자동 정지.\nenabled=%d\nport=%u\nbusid=%s\nusbip_exe=%s\nauto_attach=%d\nattach_args=%s\ndetach_args=%s\nstop_attempts_args=%s\n; 3D One이 사라진 뒤 가상 장치를 뽑기까지 기다리는 초 (절전/재접속 때 뽑았다 꽂지 않도록)\ndetach_delay_s=%d\nserial=%s\n; Deck IMU 축 <- 3D One 축 (+x/-x/+y/-y/+z/-z)\naxis_x=%s\naxis_y=%s\naxis_z=%s\naccel_scale=%.2f\ngyro_scale=%.3f\n; 1 = Home 버튼을 Steam 버튼, Setup 버튼을 QAM(...)으로; 0 = 반대\nhome_is_guide=%d\nrumble_gain=%.2f\n; 패드 터치 중 보고하는 압력 (0 = 터치만, 클릭으로 보지 않음)\npad_pressure=%d\n; 탭 클릭 펄스 동안 보내는 압력 (Steam은 트랙패드 클릭을 압력으로 판단)\ntap_pressure=%d\n; 패드 라우팅: games_only = Steam 게임이 앞에 있을 때만 패드/그립/자이로를 Steam으로, always = 점유 중 항상, never = 게임에서도 OnePad가 처리(Steam 트랙볼/자이로 기능 미사용)\npads_to_steam=%s\n; 버튼·스틱·트리거 라우팅: games_only = Steam 게임 또는 Steam 창이 앞에 있을 때만 (데스크톱에서 Steam 데스크톱 레이아웃이 B=Space, X=PageUp 같은 키 입력을 만드는 것을 막음), always = 점유 중 항상. Steam/QAM 버튼은 항상 전달\nbuttons_to_steam=%s\n; 게임에서 탭 = Deck 트랙패드 클릭, 탭 후 다시 대고 끌기 = 클릭 유지\ntap_click=%d\ntap_drag=%d\ntap_ms=%d\ntap_dist=%d\n",
             k.enabled ? 1 : 0, k.port, k.busid.c_str(), k.usbipExe.c_str(), k.autoAttach ? 1 : 0, k.attachArgs.c_str(), k.detachArgs.c_str(), k.stopAttemptsArgs.c_str(), k.detachDelaySec, k.serial.c_str(),
             k.axisX.c_str(), k.axisY.c_str(), k.axisZ.c_str(), k.accelScale, k.gyroScale, k.homeIsGuide ? 1 : 0, k.rumbleGain, k.padPressure, k.tapPressure,
             k.padsToSteam.c_str(), k.buttonsToSteam.c_str(), k.tapClick ? 1 : 0, k.tapDrag ? 1 : 0, k.tapMs, k.tapDist);
    s += b;
    snprintf(b, sizeof b, "\n[hidhide]\n; HidHide로 실제 Xbox 패드를 Steam/게임에서 숨김: auto(기본) = Steam 실행 중 + Deck 연결 중, always, never\nmode=%s\ncli=%s\n; auto일 때 Steam이 사라진 뒤에도 숨김을 유지하는 초 (OnePad 시작 직후에도 적용). Steam은 시작 1초 안에 실제 패드를 잡고 놓지 않으므로 Steam 재시작/로그온 경쟁을 덮는다\ngrace_s=%d\n", c.hidhide.mode.c_str(), c.hidhide.cli.c_str(), c.hidhide.graceSec);
    s += b;
    snprintf(b, sizeof b, "\n[xbox]\n; 실제 Xbox(XUSB) 장치 노드 비활성화: auto = Steam 실행 중에만(권장), always, never. Steam이 Deck 에뮬만 1번 컨트롤러로 보게 함\nhide=%s\n", c.xbox.mode.c_str());
    s += b;
    snprintf(b, sizeof b, "\n; [deck] View/Menu 버튼의 벤더 바이트(1-4)와 비트 마스크 (probe learn 으로 확인)\n[deck.buttons]\nview_byte=%d\nview_mask=0x%02X\nmenu_byte=%d\nmenu_mask=0x%02X\n", c.deck.viewByte, c.deck.viewMask, c.deck.menuByte, c.deck.menuMask);
    s += b;
    FILE* f = _wfopen(path.c_str(), L"wb"); if (!f) return false;
    fputs("\xEF\xBB\xBF", f); fputs(s.c_str(), f); fclose(f); return true;
}

} // namespace onepad
