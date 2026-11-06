// Self-test client for OnePad's USB/IP device server: speaks the USB/IP wire protocol like a VHCI driver would,
// enumerates descriptors, exercises the Valve feature-report channel and times interrupt IN completions.
//   usbip-selftest [port]
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>

static void Put32(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
static void Put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x >> 8); v.push_back(x); }
static uint32_t Get32(const uint8_t* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
static bool RecvAll(SOCKET s, uint8_t* b, int n) { while (n > 0) { int k = recv(s, (char*)b, n, 0); if (k <= 0) return false; b += k; n -= k; } return true; }
static bool SendAll(SOCKET s, const std::vector<uint8_t>& v) { int n = (int)v.size(); const char* p = (const char*)v.data(); while (n > 0) { int k = send(s, p, n, 0); if (k <= 0) return false; p += k; n -= k; } return true; }
static SOCKET Connect(uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(s, (sockaddr*)&a, sizeof a) != 0) { printf("connect failed %d\n", WSAGetLastError()); return INVALID_SOCKET; }
    BOOL nd = TRUE; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof nd); return s;
}
static uint32_t g_seq = 1;
struct Ret { int status; std::vector<uint8_t> data; };
static Ret Control(SOCKET s, uint8_t rt, uint8_t req, uint16_t val, uint16_t idx, uint16_t len, const std::vector<uint8_t>& out = {}) {
    std::vector<uint8_t> v; uint32_t seq = g_seq++;
    Put32(v, 1); Put32(v, seq); Put32(v, 0x00010001); Put32(v, (rt & 0x80) ? 1 : 0); Put32(v, 0);
    Put32(v, 0); Put32(v, (rt & 0x80) ? len : (uint32_t)out.size()); Put32(v, 0); Put32(v, 0xffffffff); Put32(v, 0);
    uint8_t su[8] = {rt, req, (uint8_t)val, (uint8_t)(val >> 8), (uint8_t)idx, (uint8_t)(idx >> 8), (uint8_t)len, (uint8_t)(len >> 8)};
    v.insert(v.end(), su, su + 8);
    if (!(rt & 0x80)) v.insert(v.end(), out.begin(), out.end());
    SendAll(s, v);
    uint8_t h[48]; Ret r{-1, {}}; if (!RecvAll(s, h, 48)) return r;
    if (Get32(h) != 3 || Get32(h + 4) != seq) { printf("  !! bad reply cmd=%u seq=%u (expected %u)\n", Get32(h), Get32(h + 4), seq); return r; }
    r.status = (int)Get32(h + 0x14); uint32_t actual = Get32(h + 0x18);
    if ((rt & 0x80) && actual) { r.data.resize(actual); RecvAll(s, r.data.data(), (int)actual); }
    return r;
}
static void Hex(const std::vector<uint8_t>& d, size_t max = 64) { for (size_t i = 0; i < d.size() && i < max; i++) printf("%02x", d[i]); if (d.size() > max) printf("..(%zu)", d.size()); printf("\n"); }

int main(int argc, char** argv) {
    uint16_t port = argc > 1 ? (uint16_t)atoi(argv[1]) : 3240;
    WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
    int fails = 0;
    // 1. devlist
    { SOCKET s = Connect(port); if (s == INVALID_SOCKET) return 2;
      std::vector<uint8_t> v; Put16(v, 0x0111); Put16(v, 0x8005); Put32(v, 0); SendAll(s, v);
      uint8_t h[12]; RecvAll(s, h, 12); uint32_t n = Get32(h + 8); printf("devlist: status=%u devices=%u\n", Get32(h + 4), n);
      if (n == 1) { std::vector<uint8_t> d(0x138 + 5 * 4); RecvAll(s, d.data(), 0x138); uint8_t ni = d[0x137]; RecvAll(s, d.data() + 0x138, ni * 4);
        printf("  busid=%s vid=%02x%02x pid=%02x%02x bcd=%02x%02x ifaces=%u speed=%u\n", (char*)d.data() + 256, d[0x12c], d[0x12d], d[0x12e], d[0x12f], d[0x130], d[0x131], ni, Get32(d.data() + 0x128)); }
      else fails++;
      closesocket(s); }
    // 2. import + control transfers
    SOCKET s = Connect(port); if (s == INVALID_SOCKET) return 2;
    { std::vector<uint8_t> v; Put16(v, 0x0111); Put16(v, 0x8003); Put32(v, 0); char bus[32] = "1-1"; v.insert(v.end(), (uint8_t*)bus, (uint8_t*)bus + 32); SendAll(s, v);
      uint8_t h[8]; RecvAll(s, h, 8); uint32_t st = Get32(h + 4); printf("import: status=%u\n", st); if (st != 0) return 3;
      std::vector<uint8_t> d(0x138); RecvAll(s, d.data(), 0x138); }
    auto r = Control(s, 0x80, 6, 0x0100, 0, 18); printf("device descriptor: st=%d ", r.status); Hex(r.data); if (r.data.size() != 18) fails++;
    r = Control(s, 0x80, 6, 0x0200, 0, 9); printf("config header: st=%d ", r.status); Hex(r.data);
    uint16_t total = r.data.size() >= 4 ? r.data[2] | (r.data[3] << 8) : 0;
    r = Control(s, 0x80, 6, 0x0200, 0, total); printf("config full (%u): st=%d len=%zu\n", total, r.status, r.data.size()); if (r.data.size() != total) fails++;
    for (int i = 0; i <= 4; i++) { r = Control(s, 0x80, 6, 0x0300 + i, i ? 0x0409 : 0, 255); printf("string %d: st=%d ", i, r.status); if (i) { std::string t; for (size_t k = 2; k + 1 < r.data.size(); k += 2) t += (char)r.data[k]; printf("'%s'\n", t.c_str()); } else Hex(r.data); }
    for (int i = 0; i < 3; i++) { r = Control(s, 0x81, 6, 0x2200, i, 255); printf("report desc iface %d: st=%d len=%zu ", i, r.status, r.data.size()); Hex(r.data, 16); if (r.status != 0) fails++; }
    r = Control(s, 0x81, 6, 0x2100, 2, 9); printf("hid desc iface 2: st=%d ", r.status); Hex(r.data);
    r = Control(s, 0x80, 6, 0x0600, 0, 10); printf("device qualifier: st=%d (expect -32 stall)\n", r.status);
    r = Control(s, 0x00, 9, 1, 0, 0); printf("set configuration: st=%d\n", r.status);
    r = Control(s, 0x21, 0x0A, 0, 2, 0); printf("set idle: st=%d\n", r.status);
    // Valve channel: GET_ATTRIBUTES
    { std::vector<uint8_t> m(64, 0); m[0] = 0x83; r = Control(s, 0x21, 9, 0x0300, 2, 64, m); printf("SET 0x83: st=%d\n", r.status);
      r = Control(s, 0xA1, 1, 0x0300, 2, 64); printf("GET 0x83: st=%d ", r.status); Hex(r.data, 48); if (r.data.size() != 64 || r.data[0] != 0x83) fails++; }
    for (int idx = 0; idx < 3; idx++) { std::vector<uint8_t> m(64, 0); m[0] = 0xAE; m[1] = 0x15; m[2] = (uint8_t)idx; Control(s, 0x21, 9, 0x0300, 2, 64, m);
      r = Control(s, 0xA1, 1, 0x0300, 2, 64); printf("GET 0xAE idx %d: st=%d ", idx, r.status); Hex(r.data, 16); std::string t((char*)r.data.data() + 3, 12); printf("   serial='%s'\n", t.c_str()); }
    { std::vector<uint8_t> m(64, 0); m[0] = 0x87; m[1] = 3; m[2] = 9; m[3] = 0; m[4] = 0; r = Control(s, 0x21, 9, 0x0300, 2, 64, m); printf("SET 0x87 lizard=0: st=%d\n", r.status);
      r = Control(s, 0xA1, 1, 0x0300, 2, 64); printf("GET 0x87 echo: st=%d ", r.status); Hex(r.data, 8); if (r.data.size() < 5 || r.data[0] != 0x87 || r.data[2] != 9) fails++; }
    { std::vector<uint8_t> m(64, 0); m[0] = 0x85; Control(s, 0x21, 9, 0x0300, 2, 64, m); m[0] = 0x8E; Control(s, 0x21, 9, 0x0300, 2, 64, m); printf("lizard re-enabled (0x85, 0x8E)\n"); }
    { std::vector<uint8_t> m(64, 0); m[0] = 0xEB; m[1] = 9; m[5] = 0x00; m[6] = 0x40; m[7] = 0x00; m[8] = 0x20; r = Control(s, 0x21, 9, 0x0300, 2, 64, m); printf("rumble cmd: st=%d\n", r.status); }
    { std::vector<uint8_t> m(64, 0); m[0] = 0x55; Control(s, 0x21, 9, 0x0300, 2, 64, m); r = Control(s, 0xA1, 1, 0x0300, 2, 64); printf("GET after unknown 0x55: st=%d (expect -32)\n", r.status); if (r.status != -32) fails++; }
    // 3. interrupt IN pacing
    { LARGE_INTEGER f, t0, t1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
      const int N = 25; uint32_t first = g_seq;
      for (int i = 0; i < N; i++) { std::vector<uint8_t> v; Put32(v, 1); Put32(v, g_seq++); Put32(v, 0x00010001); Put32(v, 1); Put32(v, 3); Put32(v, 0); Put32(v, 64); Put32(v, 0); Put32(v, 0xffffffff); Put32(v, 1); v.resize(48, 0); SendAll(s, v); }
      uint32_t pkt0 = 0; int got = 0; bool seqOk = true;
      for (int i = 0; i < N; i++) { uint8_t h[48]; if (!RecvAll(s, h, 48)) break; uint32_t a = Get32(h + 0x18); std::vector<uint8_t> d(a); RecvAll(s, d.data(), (int)a);
        if (Get32(h + 4) != first + i) seqOk = false;
        if (a == 64) { uint32_t p = d[4] | (d[5] << 8) | (d[6] << 16) | (d[7] << 24); if (i == 0) pkt0 = p; if (i == N - 1) printf("  frame hdr %02x %02x %02x %02x pkt %u..%u\n", d[0], d[1], d[2], d[3], pkt0, p); got++; } }
      QueryPerformanceCounter(&t1); double ms = (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart;
      printf("interrupt IN: %d/%d frames in %.1f ms (%.2f ms/frame, expect ~4) seqOrder=%s\n", got, N, ms, ms / N, seqOk ? "ok" : "BAD"); if (got != N || ms / N > 8 || ms / N < 2) fails++; }
    // 4. unlink of a parked URB (ep1, never completed)
    { uint32_t seq = g_seq++; std::vector<uint8_t> v; Put32(v, 1); Put32(v, seq); Put32(v, 0x00010001); Put32(v, 1); Put32(v, 1); Put32(v, 0); Put32(v, 8); Put32(v, 0); Put32(v, 0xffffffff); Put32(v, 1); v.resize(48, 0); SendAll(s, v);
      Sleep(20); std::vector<uint8_t> u; uint32_t useq = g_seq++; Put32(u, 2); Put32(u, useq); Put32(u, 0x00010001); Put32(u, 0); Put32(u, 0); Put32(u, seq); u.resize(48, 0); SendAll(s, u);
      uint8_t h[48]; RecvAll(s, h, 48); printf("unlink: cmd=%u seq=%u status=%d (expect 4, %u, -104)\n", Get32(h), Get32(h + 4), (int)Get32(h + 0x14), useq); if (Get32(h) != 4 || (int)Get32(h + 0x14) != -104) fails++; }
    closesocket(s);
    printf("RESULT: %s (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
