// sdl-launch: tiny wrapper that starts <own-dir>\azahar.exe with SDL's Steam Deck hidapi driver disabled.
// Why: SDL2/SDL3 apps (Azahar, Dolphin, ...) can deadlock at startup when they open OnePad's virtual Steam Deck
// with their own hidapi Steam Deck driver (WGI + hidapi + SDL joystick lock race). Per-app env var avoids it
// without touching Steam, which must keep seeing the Deck persona.
// Build: clang++ -std=c++17 -O1 -static -mwindows -o azahar-launch.exe sdl-launch.cpp
#include <windows.h>
#include <string>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir(self); dir.resize(dir.find_last_of(L"\\/"));
    std::wstring exe = dir + L"\\azahar.exe";
    SetEnvironmentVariableW(L"SDL_JOYSTICK_HIDAPI_STEAMDECK", L"0");   // no SDL<->OnePad Deck persona fight
    SetEnvironmentVariableW(L"SDL_JOYSTICK_WGI", L"0");               // SDL WGI init deadlocks (inputhost CoreMessaging) under Steam overlay hooks; XInput/DirectInput remain
    std::wstring cmd = L"\"" + exe + L"\"";
    int argc = 0; LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; i++) { cmd += L" \""; cmd += argv[i]; cmd += L"\""; }
    if (argv) LocalFree(argv);
    STARTUPINFOW si{}; si.cb = sizeof si; PROCESS_INFORMATION pi{};
    std::wstring buf = cmd;
    if (!CreateProcessW(exe.c_str(), &buf[0], nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
        MessageBoxW(nullptr, (L"azahar.exe 실행 실패:\n" + exe).c_str(), L"azahar-launch", MB_ICONERROR);
        return 1;
    }
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 0;
}
