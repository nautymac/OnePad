// OnePad - enable/disable HID collection device nodes of the controller (needs administrator)
#pragma once
#include "common.h"
#include <setupapi.h>
#include <cfgmgr32.h>

namespace onepad {

struct DevNodeState { bool found = false; bool disabled = false; std::wstring instanceId; };

// Matches "HID\VID_2993&PID_0012&MI_01&COL0x" instance id prefixes (case-insensitive).
inline std::wstring HidCollectionPrefix(const std::string& col) { return L"HID\\VID_2993&PID_0012&MI_01&" + Widen(col); }

inline DevNodeState QueryDevNode(const std::wstring& prefix) {
    DevNodeState r;
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, _wcsnicmp(prefix.c_str(), L"USB\\", 4) == 0 ? L"USB" : L"HID", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return r;
    SP_DEVINFO_DATA dd{}; dd.cbSize = sizeof dd;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dd); i++) {
        wchar_t id[512]; if (!SetupDiGetDeviceInstanceIdW(set, &dd, id, 512, nullptr)) continue;
        if (_wcsnicmp(id, prefix.c_str(), prefix.size()) != 0) continue;
        r.found = true; r.instanceId = id;
        ULONG status = 0, problem = 0;
        if (CM_Get_DevNode_Status(&status, &problem, dd.DevInst, 0) == CR_SUCCESS) r.disabled = (status & DN_HAS_PROBLEM) && problem == CM_PROB_DISABLED;
        break;
    }
    SetupDiDestroyDeviceInfoList(set);
    return r;
}

// Returns true when the node is in the requested state afterwards.
inline bool SetDevNodeEnabled(const std::wstring& prefix, bool enable) {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, _wcsnicmp(prefix.c_str(), L"USB\\", 4) == 0 ? L"USB" : L"HID", nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) { LOGE("SetupDiGetClassDevs failed %lu", GetLastError()); return false; }
    bool ok = false, found = false;
    SP_DEVINFO_DATA dd{}; dd.cbSize = sizeof dd;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dd); i++) {
        wchar_t id[512]; if (!SetupDiGetDeviceInstanceIdW(set, &dd, id, 512, nullptr)) continue;
        if (_wcsnicmp(id, prefix.c_str(), prefix.size()) != 0) continue;
        found = true;
        ULONG status = 0, problem = 0;
        CM_Get_DevNode_Status(&status, &problem, dd.DevInst, 0);
        bool disabledNow = (status & DN_HAS_PROBLEM) && problem == CM_PROB_DISABLED;
        if (disabledNow == !enable) { ok = true; break; }
        SP_PROPCHANGE_PARAMS pc{}; pc.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER); pc.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pc.StateChange = enable ? DICS_ENABLE : DICS_DISABLE; pc.Scope = DICS_FLAG_GLOBAL; pc.HwProfile = 0;
        if (!SetupDiSetClassInstallParamsW(set, &dd, &pc.ClassInstallHeader, sizeof pc)) { LOGE("SetClassInstallParams failed %lu", GetLastError()); break; }
        if (!SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, set, &dd)) { LOGE("CallClassInstaller(%s) failed %lu for %ls", enable ? "enable" : "disable", GetLastError(), id); break; }
        LOGI("device node %s: %ls", enable ? "enabled" : "disabled", id);
        ok = true; break;
    }
    SetupDiDestroyDeviceInfoList(set);
    if (!found) LOGW("device node not found: %ls", prefix.c_str());
    return ok;
}

inline bool IsElevated() {
    HANDLE t; BOOL elevated = FALSE;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) { TOKEN_ELEVATION e{}; DWORD n; if (GetTokenInformation(t, TokenElevation, &e, sizeof e, &n)) elevated = e.TokenIsElevated; CloseHandle(t); }
    return elevated != FALSE;
}

} // namespace onepad
