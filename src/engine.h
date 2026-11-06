// OnePad - Steam-Controller-style input logic: trackpads -> mouse/scroll/dpad, grips -> actions, gyro -> mouse
#pragma once
#include "common.h"
#include "config.h"
#include "vendor.h"
#include "output.h"
#include <functional>

namespace onepad {

inline bool IsOursMode(PadMode m) { return m == PadMode::Mouse || m == PadMode::Scroll || m == PadMode::DPad; }
// Sensor orientation fix; (0,0) stays (0,0) because it means "not touching".
inline int FlipX(const PadConfig& pc, int x, int y) { if ((x | y) == 0) return 0; return pc.flipX ? std::max(1, 1024 - x) : x; }
inline int FlipY(const PadConfig& pc, int x, int y) { if ((x | y) == 0) return 0; return pc.flipY ? std::max(1, 1024 - y) : y; }

class Engine {
public:
    std::function<void()> onApplyNeeded;   // effective pad mode changed -> firmware/device nodes must be re-applied (main thread)
    std::function<void()> onBiasUpdated;   // gyro bias calibrated -> persist config

    Engine(Output& out) : out_(out) { QueryPerformanceFrequency(&qpf_); }

    void SetConfig(const Config& c) {
        std::lock_guard<std::mutex> g(m_);
        cfg_ = c;
        ReleaseAll();
        calibrating_ = !(c.gyro.biasValid[0] && c.gyro.biasValid[1]);
        calSamples_ = 0;
    }
    Config GetConfig() { std::lock_guard<std::mutex> g(m_); return cfg_; }
    void SetEnabled(bool on) { std::lock_guard<std::mutex> g(m_); enabled_ = on; if (!on) ReleaseAll(); }
    // Grips (L4/R4) go to Steam while a Steam game is in front: stop acting on them here and release anything held.
    void SetGripsEnabled(bool on) { std::lock_guard<std::mutex> g(m_); if (gripsEnabled_ == on) return; gripsEnabled_ = on; if (!on) { if (gripLeftDown_) { GripAction(cfg_.gripLeft, false); gripLeftDown_ = false; } if (gripRightDown_) { GripAction(cfg_.gripRight, false); gripRightDown_ = false; } shift_ = false; gyroHold_ = false; } }
    bool ShiftHeld() { std::lock_guard<std::mutex> g(m_); return shift_; }
    bool GyroToggled() { std::lock_guard<std::mutex> g(m_); return gyroToggle_; }
    void RequestCalibration() { std::lock_guard<std::mutex> g(m_); calibrating_ = true; calSamples_ = 0; calTries_ = 0; }
    bool Calibrating() { std::lock_guard<std::mutex> g(m_); return calibrating_; }
    void GetBias(double out[2][3], bool valid[2]) { std::lock_guard<std::mutex> g(m_); memcpy(out, cfg_.gyro.bias, sizeof cfg_.gyro.bias); valid[0] = cfg_.gyro.biasValid[0]; valid[1] = cfg_.gyro.biasValid[1]; }
    bool DevicePresent() { return GetTickCount() - lastReportTick_ < 1500; }

    PadMode EffectiveMode(bool right) {
        std::lock_guard<std::mutex> g(m_);
        const PadConfig& pc = right ? cfg_.right : cfg_.left;
        return shift_ ? pc.shiftMode : pc.mode;
    }

    // Called from the SDK thread at ~125 Hz.
    void OnState(const VendorState& s) {
        std::lock_guard<std::mutex> g(m_);
        lastReportTick_ = GetTickCount();
        reports_++;
        if (!enabled_) return;
        if (gripsEnabled_) ProcessGrips(s.buttons3);
        PadMode effR = shift_ ? cfg_.right.shiftMode : cfg_.right.mode;
        PadMode effL = shift_ ? cfg_.left.shiftMode : cfg_.left.mode;
        bool applyNeeded = false;
        if (effR != right_.mode) { ReleasePadKeys(right_, cfg_.right); right_.mode = effR; applyNeeded = true; }
        if (effL != left_.mode) { ReleasePadKeys(left_, cfg_.left); left_.mode = effL; applyNeeded = true; }
        if (applyNeeded && onApplyNeeded) onApplyNeeded();
        ProcessPad(right_, cfg_.right, FlipX(cfg_.right, s.RPadX(), s.RPadY()), FlipY(cfg_.right, s.RPadX(), s.RPadY()));
        ProcessPad(left_, cfg_.left, FlipX(cfg_.left, s.LPadX(), s.LPadY()), FlipY(cfg_.left, s.LPadX(), s.LPadY()));
        ProcessGyro(s);
        if (cfg_.debug && (reports_ % 125) == 0)
            LOGD("state: R(%d,%d) L(%d,%d) b3=%02X shift=%d gyroHold=%d", s.RPadX(), s.RPadY(), s.LPadX(), s.LPadY(), s.buttons3, shift_ ? 1 : 0, gyroHold_ ? 1 : 0);
    }

private:
    struct Pad {
        PadMode mode = PadMode::Off;
        bool touching = false; int x = 0, y = 0; DWORD t0 = 0; double dist = 0;
        double vx = 0, vy = 0; bool rolling = false;
        double fx = 0, fy = 0;              // fractional mouse accumulators
        double accX = 0, accY = 0;          // scroll accumulators
        int dpadMask = 0;
        DWORD lastTapTick = 0; bool dragging = false, secondTouch = false;
    };

    void ReleasePad(Pad& p) {
        p.touching = false; p.rolling = false; p.vx = p.vy = 0; p.accX = p.accY = 0; p.dpadMask = 0;
    }
    void ReleaseAll() {
        ReleasePadKeys(right_, cfg_.right); ReleasePadKeys(left_, cfg_.left);
        if (gripLeftDown_) { GripAction(cfg_.gripLeft, false); gripLeftDown_ = false; }
        if (gripRightDown_) { GripAction(cfg_.gripRight, false); gripRightDown_ = false; }
        shift_ = false; gyroHold_ = false;
    }
    void ReleasePadKeys(Pad& p, const PadConfig& pc) {
        if (p.dpadMask) for (int i = 0; i < 4; i++) if (p.dpadMask & (1 << i)) out_.Key(pc.dpadKeys[i], false);
        if (p.dragging) { p.dragging = false; out_.MouseButton(pc.tap == TapAction::Left ? 1 : pc.tap == TapAction::Right ? 2 : 3, false); }
        ReleasePad(p);
    }

    void SetDpad(Pad& p, int mask, const PadConfig* pc) {
        if (mask == p.dpadMask) return;
        if (pc) {
            for (int i = 0; i < 4; i++) { bool was = p.dpadMask & (1 << i), now = mask & (1 << i); if (was && !now) out_.Key(pc->dpadKeys[i], false); }
            for (int i = 0; i < 4; i++) { bool was = p.dpadMask & (1 << i), now = mask & (1 << i); if (!was && now) out_.Key(pc->dpadKeys[i], true); }
        }
        p.dpadMask = mask;
    }

    void EmitMove(Pad& p, double dx, double dy) {
        p.fx += dx; p.fy += dy;
        int ix = (int)p.fx, iy = (int)p.fy; p.fx -= ix; p.fy -= iy;
        out_.MouseMove(ix, iy);
    }
    void EmitScroll(Pad& p, const PadConfig& pc, double dx, double dy) {
        double sgn = pc.scrollInvert ? 1.0 : -1.0;       // finger down -> wheel negative (scroll content down)
        p.accY += dy; p.accX += dx;
        while (p.accY >= pc.scrollStep) { p.accY -= pc.scrollStep; out_.Wheel((int)(120 * sgn)); }
        while (p.accY <= -pc.scrollStep) { p.accY += pc.scrollStep; out_.Wheel((int)(-120 * sgn)); }
        if (pc.scrollHorizontal) {
            while (p.accX >= pc.scrollStep) { p.accX -= pc.scrollStep; out_.Wheel(120, true); }
            while (p.accX <= -pc.scrollStep) { p.accX += pc.scrollStep; out_.Wheel(-120, true); }
        } else p.accX = 0;
    }

    void ProcessPad(Pad& p, const PadConfig& pc, int rawX, int rawY) {
        if (!IsOursMode(p.mode)) { p.touching = (rawX | rawY) != 0; return; }
        bool touching = (rawX | rawY) != 0;
        if (touching) {
            if (!p.touching) {
                p.touching = true; p.x = rawX; p.y = rawY; p.t0 = GetTickCount(); p.dist = 0; p.vx = p.vy = 0; p.rolling = false;
                p.accX = p.accY = 0;
                // second touch right after a tap: a short one is a second click (double click), a held/moving one is a drag
                p.secondTouch = p.mode == PadMode::Mouse && pc.tapDrag && pc.tap != TapAction::None && p.lastTapTick && p.t0 - p.lastTapTick <= (DWORD)std::max(450, pc.tapMs * 2);
                p.lastTapTick = 0;
            } else {
                int dx = rawX - p.x, dy = rawY - p.y; p.x = rawX; p.y = rawY;
                double sp = std::sqrt((double)dx * dx + (double)dy * dy); p.dist += sp;
                if (p.secondTouch && !p.dragging && (GetTickCount() - p.t0 > (DWORD)pc.tapMs || p.dist > pc.tapDist)) {
                    p.dragging = true;   // tap-then-hold -> drag from here on
                    out_.MouseButton(pc.tap == TapAction::Left ? 1 : pc.tap == TapAction::Right ? 2 : 3, true);
                }
                p.vx = 0.55 * p.vx + 0.45 * dx; p.vy = 0.55 * p.vy + 0.45 * dy;
                if (p.mode == PadMode::Mouse) {
                    double mult = pc.sensitivity * (1.0 + pc.accel * sp * 0.1);
                    EmitMove(p, dx * mult, dy * mult);
                } else if (p.mode == PadMode::Scroll) {
                    EmitScroll(p, pc, dx, dy);
                }
            }
            if (p.mode == PadMode::DPad) {
                double nx = (rawX - 512) / 512.0, ny = (rawY - 512) / 512.0, mag = std::sqrt(nx * nx + ny * ny);
                int mask = 0;
                if (mag >= pc.dpadDeadzone) {
                    double ax = std::fabs(nx), ay = std::fabs(ny);
                    bool useX, useY;
                    if (pc.dpad8way) { useX = ax >= mag * 0.383; useY = ay >= mag * 0.383; }
                    else { useX = ax >= ay; useY = !useX; }
                    if (useY) mask |= ny < 0 ? 1 : 2;
                    if (useX) mask |= nx < 0 ? 4 : 8;
                }
                SetDpad(p, mask, &pc);
            }
        } else {
            if (p.touching) {
                p.touching = false;
                DWORD held = GetTickCount() - p.t0;
                if (p.dragging) {   // end of a tap-and-drag: release the held button
                    p.dragging = false; p.secondTouch = false;
                    out_.MouseButton(pc.tap == TapAction::Left ? 1 : pc.tap == TapAction::Right ? 2 : 3, false);
                }
                else if (p.mode == PadMode::DPad) { SetDpad(p, 0, &pc); }
                else if (pc.tap != TapAction::None && held <= (DWORD)pc.tapMs && p.dist <= pc.tapDist && (p.mode == PadMode::Mouse)) {
                    out_.Click(pc.tap == TapAction::Left ? 1 : pc.tap == TapAction::Right ? 2 : 3);   // 1st tap = click, 2nd quick tap = double click
                    p.lastTapTick = p.secondTouch ? 0 : GetTickCount(); p.secondTouch = false;
                } else if (pc.friction > 0 && std::sqrt(p.vx * p.vx + p.vy * p.vy) >= pc.flickMin) {
                    p.rolling = true;
                }
            }
            if (p.rolling) {
                p.vx *= pc.friction; p.vy *= pc.friction;
                if (std::sqrt(p.vx * p.vx + p.vy * p.vy) < 0.25) { p.rolling = false; p.vx = p.vy = 0; }
                else if (p.mode == PadMode::Mouse) EmitMove(p, p.vx * pc.sensitivity, p.vy * pc.sensitivity);
                else if (p.mode == PadMode::Scroll) EmitScroll(p, pc, p.vx, p.vy);
            }
        }
    }

    void GripAction(const GripConfig& g, bool down) {
        switch (g.kind) {
            case GripKind::ModeShift: shift_ = down; break;
            case GripKind::GyroHold: gyroHold_ = down; break;
            case GripKind::GyroToggle: if (down) gyroToggle_ = !gyroToggle_; break;
            case GripKind::Key: out_.Combo(g.keys, down); break;
            case GripKind::MouseButton: out_.MouseButton(g.mouseButton, down); break;
            default: break;
        }
    }
    void ProcessGrips(uint8_t b3) {
        bool l = (b3 & cfg_.gripLeft.bit) != 0, r = (b3 & cfg_.gripRight.bit) != 0;
        if (l != gripLeftDown_) { gripLeftDown_ = l; GripAction(cfg_.gripLeft, l); if (cfg_.debug) LOGD("grip left %s", l ? "down" : "up"); }
        if (r != gripRightDown_) { gripRightDown_ = r; GripAction(cfg_.gripRight, r); if (cfg_.debug) LOGD("grip right %s", r ? "down" : "up"); }
    }

    void ProcessGyro(const VendorState& s) {
        const GyroConfig& gc = cfg_.gyro;
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        double dt = lastQpc_.QuadPart ? (double)(now.QuadPart - lastQpc_.QuadPart) / qpf_.QuadPart : 0.008; lastQpc_ = now;
        dt = std::min(0.05, std::max(0.002, dt));
        double g[2][3], a[3];
        s.Gyro(false, g[0]); s.Gyro(true, g[1]); s.Accel(gc.sideRight, a);
        if (calibrating_) {
            for (int side = 0; side < 2; side++) for (int i = 0; i < 3; i++) { calSum_[side][i] += g[side][i]; calSq_[side][i] += g[side][i] * g[side][i]; }
            if (++calSamples_ >= 250) {
                double worst = 0;
                for (int side = 0; side < 2; side++) for (int i = 0; i < 3; i++) {
                    double mean = calSum_[side][i] / calSamples_, var = calSq_[side][i] / calSamples_ - mean * mean;
                    worst = std::max(worst, std::sqrt(std::max(0.0, var)));
                }
                bool ok = worst < 40.0 || ++calTries_ >= 10;
                if (ok) {
                    for (int side = 0; side < 2; side++) { for (int i = 0; i < 3; i++) cfg_.gyro.bias[side][i] = calSum_[side][i] / calSamples_; cfg_.gyro.biasValid[side] = true; }
                    calibrating_ = false;
                    LOGI("gyro bias calibrated: L(%.1f %.1f %.1f) R(%.1f %.1f %.1f) noise=%.1f", cfg_.gyro.bias[0][0], cfg_.gyro.bias[0][1], cfg_.gyro.bias[0][2], cfg_.gyro.bias[1][0], cfg_.gyro.bias[1][1], cfg_.gyro.bias[1][2], worst);
                    if (onBiasUpdated) onBiasUpdated();
                } else LOGW("gyro calibration: device moving (noise %.1f), retrying", worst);
                calSamples_ = 0; memset(calSum_, 0, sizeof calSum_); memset(calSq_, 0, sizeof calSq_);
            }
        }
        if (gc.mode == GyroMode::Off) return;
        int side = gc.sideRight ? 1 : 0;
        bool active = false;
        switch (gc.activate) {
            case GyroActivate::Always: active = true; break;
            case GyroActivate::Grip: active = gyroHold_; break;
            case GyroActivate::Toggle: active = gyroToggle_; break;
            case GyroActivate::RightTouch: active = right_.touching; break;
            case GyroActivate::LeftTouch: active = left_.touching; break;
        }
        // gravity direction (low-pass filtered accelerometer)
        if (!gravInit_) { for (int i = 0; i < 3; i++) grav_[i] = a[i]; gravInit_ = true; }
        else for (int i = 0; i < 3; i++) grav_[i] = grav_[i] * 0.98 + a[i] * 0.02;
        if (!active) { gfx_ = gfy_ = 0; return; }
        double dps[3]; for (int i = 0; i < 3; i++) dps[i] = (g[side][i] - cfg_.gyro.bias[side][i]) / gc.scaleDps;
        double yaw, pitch;
        if (gc.yaw == Axis::World) {
            double n = std::sqrt(grav_[0] * grav_[0] + grav_[1] * grav_[1] + grav_[2] * grav_[2]);
            yaw = n > 1 ? (dps[0] * grav_[0] + dps[1] * grav_[1] + dps[2] * grav_[2]) / n : 0;
        } else yaw = dps[(int)gc.yaw];
        pitch = gc.pitch == Axis::World ? dps[1] : dps[(int)gc.pitch];
        if (std::fabs(yaw) < gc.deadzone) yaw = 0;
        if (std::fabs(pitch) < gc.deadzone) pitch = 0;
        double dx = yaw * dt * gc.sensitivity * (gc.invertX ? -1 : 1);
        double dy = pitch * dt * gc.sensitivity * (gc.invertY ? -1 : 1);
        gfx_ += dx; gfy_ += dy;
        int ix = (int)gfx_, iy = (int)gfy_; gfx_ -= ix; gfy_ -= iy;
        out_.MouseMove(ix, iy);
    }

    Output& out_;
    Config cfg_;
    std::mutex m_;
    bool enabled_ = true, gripsEnabled_ = true;
    Pad right_, left_;
    bool shift_ = false, gyroHold_ = false, gyroToggle_ = false;
    bool gripLeftDown_ = false, gripRightDown_ = false;
    bool calibrating_ = true; int calSamples_ = 0, calTries_ = 0; double calSum_[2][3] = {}, calSq_[2][3] = {};
    double grav_[3] = {0, 0, 0}; bool gravInit_ = false; double gfx_ = 0, gfy_ = 0;
    LARGE_INTEGER qpf_{}, lastQpc_{};
    volatile DWORD lastReportTick_ = 0; uint32_t reports_ = 0;
};

} // namespace onepad
