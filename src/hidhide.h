// OnePad - HidHide integration: hide the controller's real Xbox 360 (XUSB + HID) nodes from every process but
// OnePad while Steam is running, so Steam and games only see the emulated Steam Deck controller.
#pragma once
#include "common.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <tlhelp32.h>

namespace onepad {

inline std::vector<std::wstring> FindInstanceIds(const wchar_t* enumerator, const std::wstring& prefix, bool presentOnly = true) {
    std::vector<std::wstring> out;
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, enumerator, nullptr, (presentOnly ? DIGCF_PRESENT : 0) | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return out;
    SP_DEVINFO_DATA dd{}; dd.cbSize = sizeof dd;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dd); i++) {
        wchar_t id[512]; if (!SetupDiGetDeviceInstanceIdW(set, &dd, id, 512, nullptr)) continue;
        if (_wcsnicmp(id, prefix.c_str(), prefix.size()) == 0) out.push_back(id);
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

inline bool ProcessRunning(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{}; pe.dwSize = sizeof pe; bool found = false;
    if (Process32FirstW(snap, &pe)) do { if (_wcsicmp(pe.szExeFile, exeName) == 0) { found = true; break; } } while (Process32NextW(snap, &pe));
    CloseHandle(snap); return found;
}

// Restart a device node (DICS_PROPCHANGE) so processes that already hold it re-open through the filter.
inline bool RestartDevNode(const std::wstring& instanceId) {
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return false;
    bool ok = false; SP_DEVINFO_DATA dd{}; dd.cbSize = sizeof dd;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &dd); i++) {
        wchar_t id[512]; if (!SetupDiGetDeviceInstanceIdW(set, &dd, id, 512, nullptr)) continue;
        if (_wcsicmp(id, instanceId.c_str()) != 0) continue;
        SP_PROPCHANGE_PARAMS pc{}; pc.ClassInstallHeader.cbSize = sizeof(SP_CLASSINSTALL_HEADER); pc.ClassInstallHeader.InstallFunction = DIF_PROPERTYCHANGE;
        pc.StateChange = DICS_PROPCHANGE; pc.Scope = DICS_FLAG_GLOBAL;
        ok = SetupDiSetClassInstallParamsW(set, &dd, &pc.ClassInstallHeader, sizeof pc) && SetupDiCallClassInstaller(DIF_PROPERTYCHANGE, set, &dd);
        if (!ok) LOGW("hidhide: restart of %ls failed (%lu)", id, GetLastError());
        break;
    }
    SetupDiDestroyDeviceInfoList(set);
    return ok;
}

class HidHide {
public:
    bool Locate(const std::string& override) {
        std::wstring p = Widen(override);
        if (p.empty()) {
            wchar_t buf[MAX_PATH] = {}; DWORD n = sizeof buf; HKEY k;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Nefarius Software Solutions e.U.\\HidHide", 0, KEY_READ | KEY_WOW64_64KEY, &k) == ERROR_SUCCESS) {
                if (RegQueryValueExW(k, L"Path", nullptr, nullptr, (BYTE*)buf, &n) == ERROR_SUCCESS) {
                    p = buf; if (!p.empty() && p.back() != L'\\') p += L'\\'; p += L"x64\\HidHideCLI.exe";
                }
                RegCloseKey(k);
            }
            if (p.empty() || GetFileAttributesW(p.c_str()) == INVALID_FILE_ATTRIBUTES) p = L"C:\\Program Files\\Nefarius Software Solutions\\HidHide\\x64\\HidHideCLI.exe";
        }
        cli_ = p; available_ = GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
        LOGI("hidhide: cli %ls -> %s", p.c_str(), available_ ? "found" : "missing");
        return available_;
    }
    bool Available() const { return available_; }
    bool Cloaked() const { return cloaked_; }

    // Whitelist OnePad and register the controller's Xbox nodes as hidden (the cloak itself is switched separately).
    std::vector<std::wstring> extraApps_;
    void SetExtraApps(const std::vector<std::wstring>& apps) { extraApps_ = apps; }
    // Inverse mode (HidHide "inverse application list"): the registered apps are the ONLY ones that cannot see the
    // hidden pad. Register just Steam, and every game/emulator/desktop app reads the real pad directly (lowest latency)
    // while Steam still sees only the virtual Deck. Nothing per app to configure.
    bool inverse_ = false, inverseActive_ = false; std::vector<std::wstring> blockApps_;
    void SetInverse(bool on, const std::vector<std::wstring>& block) { inverse_ = on; blockApps_ = block; }
    bool InverseActive() const { return inverseActive_; }
    // Make HidHide's application list exactly the wanted set:
    //   inverse  -> Steam processes only (they are the ones that must NOT see the pad)
    //   normal   -> OnePad (rumble via XInput) + direct_apps (they are the ones that MAY see the pad)
    // Everything else in the list is removed by its stored spelling (see ListApps).
    void RegisterApps(bool inverse) {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::vector<std::wstring> want;
        if (inverse) want = blockApps_; else { want.push_back(exe); for (auto& a : extraApps_) want.push_back(a); }
        // HidHideCLI silently ignores "d:\..." (exit 0, "volume is case sensitive"): the drive letter must be upper case.
        // Steam's registry SteamPath is lower case ("d:/steam"), so normalise every path here.
        for (auto& w : want) if (w.size() > 1 && w[1] == L':') w[0] = (wchar_t)towupper(w[0]);
        if (!inverse) Run(L"--inv-off");
        for (auto& cur : ListApps()) {
            bool keep = false;
            for (auto& w : want) if (_wcsicmp(cur.c_str(), w.c_str()) == 0) { keep = true; break; }
            if (!keep && cur.find(L"HidHideCLI.exe") == std::wstring::npos) Run(L"--app-unreg \"" + cur + L"\"");
        }
        for (auto& w : want) Run(L"--app-reg \"" + w + L"\"");
        if (inverse) Run(L"--inv-on");
        inverseActive_ = inverse;
    }
    // Runtime switch between "hidden from Steam only" (inverse) and "hidden from everyone but OnePad" (normal).
    // Used while a Steam game runs: the game must see only Steam Input's virtual pad, otherwise the real pad is a
    // second player. The Xbox node is re-plugged so whoever already holds the pad has to re-open it through the filter.
    void SwitchInverse(bool on) {
        if (!available_) return;
        RegisterApps(on);
        for (auto& id : hidden_) if (id.rfind(L"USB\\", 0) == 0) RestartDevNode(id);
        LOGI("hidhide: %s", on ? "inverse ON - real pad hidden from Steam only (apps read it directly)" : "inverse OFF - real pad hidden from every app (Steam game running)");
    }
    void Prepare() {
        if (!available_) return;
        RegisterApps(inverse_);
        hidden_.clear();
        for (auto& id : FindInstanceIds(L"USB", L"USB\\VID_2993&PID_0012&MI_00")) hidden_.push_back(id);
        // The XInput HID node alternates between IG_00 and IG_01 across re-enumerations: hide every instance ever seen
        // (present or not) plus the USB-level IG_ node, so Steam cannot list the pad as a DirectInput "Controller" either.
        for (auto& id : FindInstanceIds(L"HID", L"HID\\VID_2993&PID_0012&IG_", false)) hidden_.push_back(id);
        for (auto& id : FindInstanceIds(L"USB", L"USB\\VID_2993&PID_0012&IG_", false)) hidden_.push_back(id);
        for (auto& id : hidden_) Run(L"--dev-hide \"" + id + L"\"");
        prepared_ = !hidden_.empty();
        if (!prepared_) LOGW("hidhide: controller Xbox nodes not present yet");
    }
    void SetCloak(bool on) {
        if (!available_) return;
        if (on && !prepared_) Prepare();
        if (on == cloaked_ && cloakKnown_) return;
        Run(on ? L"--cloak-on" : L"--cloak-off");
        cloaked_ = on; cloakKnown_ = true;
        // processes that already hold the pad (Steam, games) must re-open it through the filter
        for (auto& id : hidden_) if (id.rfind(L"USB\\", 0) == 0) RestartDevNode(id);
        LOGI("hidhide: cloak %s (%zu nodes)", on ? "ON - real Xbox pad hidden" : "OFF - real Xbox pad visible", hidden_.size());
    }
    void Release() {   // uncloak and forget the nodes (on exit / --restore)
        if (!available_) return;
        Run(L"--cloak-off");
        if (inverse_) { Run(L"--inv-off"); for (auto& a : blockApps_) Run(L"--app-unreg \"" + a + L"\""); }
        for (auto& id : hidden_) Run(L"--dev-unhide \"" + id + L"\"");
        for (auto& id : hidden_) if (id.rfind(L"USB\\", 0) == 0) RestartDevNode(id);
        cloaked_ = false; cloakKnown_ = true; prepared_ = false;
    }

private:
    bool Run(const std::wstring& args) {
        std::wstring cmd = L"\"" + cli_ + L"\" " + args;
        STARTUPINFOW si{}; si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
        std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
        if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { LOGE("hidhide: launch failed %lu", GetLastError()); return false; }
        DWORD code = 1; if (WaitForSingleObject(pi.hProcess, 10000) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code); else TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        LOGI("hidhide: %ls -> %lu", args.c_str(), code);
        return code == 0;
    }
    // "--app-list" output, one registered path per entry, spelled exactly as HidHide stores it
    // (HidHide dedupes case-insensitively on --app-reg but --app-unreg must match the stored spelling).
    std::vector<std::wstring> ListApps() {
        std::vector<std::wstring> out;
        SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof sa; sa.bInheritHandle = TRUE;
        HANDLE rd = nullptr, wr = nullptr; if (!CreatePipe(&rd, &wr, &sa, 0)) return out;
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        std::wstring cmd = L"\"" + cli_ + L"\" --app-list";
        STARTUPINFOW si{}; si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES; si.wShowWindow = SW_HIDE; si.hStdOutput = wr; si.hStdError = wr;
        PROCESS_INFORMATION pi{}; std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
        bool ok = CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi) != 0;
        CloseHandle(wr);
        std::string text;
        if (ok) { char b[4096]; DWORD n; while (ReadFile(rd, b, sizeof b, &n, nullptr) && n) text.append(b, n); WaitForSingleObject(pi.hProcess, 10000); CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
        CloseHandle(rd);
        size_t p = 0;
        while ((p = text.find("--app-reg \"", p)) != std::string::npos) {
            p += 11; size_t q = text.find('"', p); if (q == std::string::npos) break;
            out.push_back(Widen(text.substr(p, q - p))); p = q + 1;
        }
        return out;
    }
    std::wstring cli_; bool available_ = false, prepared_ = false, cloaked_ = false, cloakKnown_ = false;
    std::vector<std::wstring> hidden_;
};

} // namespace onepad
