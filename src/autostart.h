// OnePad - logon autostart through a Task Scheduler entry (highest privileges, no time limit, restart on failure).
// Registered by onepad.exe itself on first run so the user never has to touch an installer script.
#pragma once
#include "common.h"
#include <sddl.h>

namespace onepad {

class Autostart {
public:
    static const wchar_t* TaskName() { return L"OnePad"; }

    static bool Installed() {
        DWORD code = Run(L"schtasks.exe /Query /TN OnePad");
        return code == 0;
    }

    static bool Install() {
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring dir(exe); dir = dir.substr(0, dir.find_last_of(L"\\/"));
        std::wstring sid = CurrentUserSid();
        if (sid.empty()) { LOGE("autostart: cannot resolve user SID"); return false; }
        std::wstring xml =
            L"<?xml version=\"1.0\" encoding=\"UTF-16\"?>\n"
            L"<Task version=\"1.4\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">\n"
            L"  <RegistrationInfo><Description>abxylute 3D One 터치패드/백버튼/자이로 변환 + Steam Deck 컨트롤러 에뮬레이션 (OnePad)</Description></RegistrationInfo>\n"
            L"  <Triggers><LogonTrigger><Enabled>true</Enabled><UserId>" + sid + L"</UserId><Delay>PT8S</Delay></LogonTrigger></Triggers>\n"
            L"  <Principals><Principal id=\"Author\"><UserId>" + sid + L"</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>\n"
            L"  <Settings>\n"
            L"    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>\n"
            L"    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>\n"
            L"    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>\n"
            L"    <AllowHardTerminate>false</AllowHardTerminate>\n"
            L"    <StartWhenAvailable>true</StartWhenAvailable>\n"
            L"    <RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable>\n"
            L"    <IdleSettings><StopOnIdleEnd>false</StopOnIdleEnd><RestartOnIdle>false</RestartOnIdle></IdleSettings>\n"
            L"    <AllowStartOnDemand>true</AllowStartOnDemand>\n"
            L"    <Enabled>true</Enabled>\n"
            L"    <Hidden>false</Hidden>\n"
            L"    <RunOnlyIfIdle>false</RunOnlyIfIdle>\n"
            L"    <DisallowStartOnRemoteAppSession>false</DisallowStartOnRemoteAppSession>\n"
            L"    <UseUnifiedSchedulingEngine>true</UseUnifiedSchedulingEngine>\n"
            L"    <WakeToRun>false</WakeToRun>\n"
            L"    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>\n"
            L"    <Priority>4</Priority>\n"
            L"    <RestartOnFailure><Interval>PT1M</Interval><Count>3</Count></RestartOnFailure>\n"
            L"  </Settings>\n"
            L"  <Actions Context=\"Author\"><Exec><Command>" + Escape(exe) + L"</Command><WorkingDirectory>" + Escape(dir) + L"</WorkingDirectory></Exec></Actions>\n"
            L"</Task>\n";
        std::wstring path = DataDir() + L"onepad-task.xml";
        FILE* f = _wfopen(path.c_str(), L"wb");
        if (!f) { LOGE("autostart: cannot write %ls", path.c_str()); return false; }
        unsigned char bom[2] = {0xFF, 0xFE}; fwrite(bom, 1, 2, f);
        fwrite(xml.data(), sizeof(wchar_t), xml.size(), f); fclose(f);
        DWORD code = Run(L"schtasks.exe /Create /TN OnePad /XML \"" + path + L"\" /F");
        LOGI("autostart: register task -> %lu", code);
        return code == 0;
    }

    static bool Uninstall() {
        DWORD code = Run(L"schtasks.exe /Delete /TN OnePad /F");
        LOGI("autostart: delete task -> %lu", code);
        return code == 0;
    }

private:
    static std::wstring Escape(const std::wstring& s) {
        std::wstring o; for (wchar_t c : s) { if (c == L'&') o += L"&amp;"; else if (c == L'<') o += L"&lt;"; else if (c == L'>') o += L"&gt;"; else o += c; } return o;
    }
    static std::wstring CurrentUserSid() {
        HANDLE t; std::wstring out;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &t)) return out;
        DWORD n = 0; GetTokenInformation(t, TokenUser, nullptr, 0, &n);
        std::vector<uint8_t> buf(n);
        if (GetTokenInformation(t, TokenUser, buf.data(), n, &n)) {
            LPWSTR s = nullptr; if (ConvertSidToStringSidW(((TOKEN_USER*)buf.data())->User.Sid, &s)) { out = s; LocalFree(s); }
        }
        CloseHandle(t); return out;
    }
    static DWORD Run(const std::wstring& cmd) {
        STARTUPINFOW si{}; si.cb = sizeof si; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
        std::vector<wchar_t> buf(cmd.begin(), cmd.end()); buf.push_back(0);
        if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return (DWORD)-1;
        DWORD code = (DWORD)-1; if (WaitForSingleObject(pi.hProcess, 15000) == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code); else TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return code;
    }
};

} // namespace onepad
