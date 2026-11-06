// OnePad - synthesized mouse/keyboard output (SendInput)
#pragma once
#include "common.h"

namespace onepad {

class Output {
public:
    void MouseMove(int dx, int dy) {
        if (!dx && !dy) return;
        INPUT in{}; in.type = INPUT_MOUSE; in.mi.dx = dx; in.mi.dy = dy; in.mi.dwFlags = MOUSEEVENTF_MOVE; SendInput(1, &in, sizeof in);
    }
    void Wheel(int delta, bool horizontal = false) {
        if (!delta) return;
        INPUT in{}; in.type = INPUT_MOUSE; in.mi.mouseData = (DWORD)delta; in.mi.dwFlags = horizontal ? MOUSEEVENTF_HWHEEL : MOUSEEVENTF_WHEEL; SendInput(1, &in, sizeof in);
    }
    // button: 1=left 2=right 3=middle 4=x1 5=x2
    void MouseButton(int button, bool down) {
        INPUT in{}; in.type = INPUT_MOUSE;
        switch (button) {
            case 1: in.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
            case 2: in.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
            case 3: in.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
            case 4: in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; in.mi.mouseData = XBUTTON1; break;
            case 5: in.mi.dwFlags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; in.mi.mouseData = XBUTTON2; break;
            default: return;
        }
        SendInput(1, &in, sizeof in);
    }
    void Click(int button) { MouseButton(button, true); MouseButton(button, false); }
    void Key(int vk, bool down) {
        if (!vk) return;
        INPUT in{}; in.type = INPUT_KEYBOARD; in.ki.wVk = (WORD)vk; in.ki.wScan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
        in.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP);
        switch (vk) { case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT: case VK_INSERT: case VK_DELETE: case VK_RCONTROL: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_APPS: in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY; }
        SendInput(1, &in, sizeof in);
    }
    void Combo(const std::vector<int>& keys, bool down) {
        if (down) for (int k : keys) Key(k, true);
        else for (auto it = keys.rbegin(); it != keys.rend(); ++it) Key(*it, false);
    }
};

} // namespace onepad
