// OnePad - common utilities (logging, paths, string helpers)
#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <vector>
#include <mutex>
#include <algorithm>
#include <cmath>

namespace onepad {

inline std::wstring ExeDir() {
    wchar_t buf[MAX_PATH]; GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring s(buf); return s.substr(0, s.find_last_of(L"\\/") + 1);
}

inline std::wstring DataDir() {
    wchar_t p[MAX_PATH]; std::wstring dir;
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, p))) dir = p;
    else { wchar_t t[MAX_PATH]; GetTempPathW(MAX_PATH, t); dir = t; }
    dir += L"\\OnePad\\";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir;
}

inline std::string Narrow(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0); WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, nullptr, nullptr); return s;
}
inline std::wstring Widen(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n); return w;
}
inline std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n");
    return a == std::string::npos ? "" : s.substr(a, b - a + 1);
}
inline std::string Lower(std::string s) { for (auto& c : s) c = (char)tolower((unsigned char)c); return s; }
inline std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out; std::string cur;
    for (char c : s) { if (c == sep) { out.push_back(Trim(cur)); cur.clear(); } else cur += c; }
    out.push_back(Trim(cur)); return out;
}

class Log {
public:
    static Log& I() { static Log l; return l; }
    void Open(const std::wstring& path) { std::lock_guard<std::mutex> g(m_); if (f_) fclose(f_); f_ = _wfopen(path.c_str(), L"a"); }
    void Write(const char* level, const char* fmt, ...) {
        char msg[1024]; va_list ap; va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
        SYSTEMTIME st; GetLocalTime(&st);
        char line[1200]; snprintf(line, sizeof line, "%04d-%02d-%02d %02d:%02d:%02d.%03d [%s] %s\n", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, level, msg);
        std::lock_guard<std::mutex> g(m_);
        if (f_) { fputs(line, f_); fflush(f_); }
        if (console_) fputs(line, stdout);
        OutputDebugStringA(line);
    }
    void SetConsole(bool on) { console_ = on; }
private:
    FILE* f_ = nullptr; bool console_ = false; std::mutex m_;
};
#define LOGI(...) ::onepad::Log::I().Write("info", __VA_ARGS__)
#define LOGW(...) ::onepad::Log::I().Write("warn", __VA_ARGS__)
#define LOGE(...) ::onepad::Log::I().Write("error", __VA_ARGS__)
#define LOGD(...) ::onepad::Log::I().Write("debug", __VA_ARGS__)

} // namespace onepad
