// OnePad - minimal USB/IP device server (protocol v1.1.1) exporting one emulated full-speed USB device
// on loopback, for attachment through the usbip-win2 virtual host controller.
#pragma once
#include "common.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <atomic>
#include <deque>
#include <functional>

namespace onepad {

struct UsbDeviceModel {
    std::vector<uint8_t> deviceDesc;                 // 18 bytes
    std::vector<uint8_t> configDesc;                 // full configuration descriptor (wTotalLength bytes)
    std::vector<std::vector<uint8_t>> reportDescs;   // HID report descriptor per interface number (empty = none)
    std::vector<std::wstring> strings;               // string descriptors, index 1.. (index 0 = language id, implicit)
    uint8_t numInterfaces = 0;
    struct Iface { uint8_t cls, sub, proto; };
    std::vector<Iface> ifaces;
    int interruptInEp = 3;                           // endpoint number whose IN URBs are completed on the frame clock
    int frameIntervalMs = 4;
    // Class requests on the HID/vendor interface.
    std::function<int(uint8_t iface, uint8_t reportType, uint8_t reportId, const uint8_t* data, int len)> onSetReport;   // return 0 ok, -32 stall
    std::function<bool(uint8_t iface, uint8_t reportType, uint8_t reportId, std::vector<uint8_t>& out)> onGetReport;   // false = stall
    std::function<void(uint8_t* frame64)> fillFrame; // produce the next interrupt IN report (64 bytes)
};

class UsbipServer {
public:
    ~UsbipServer() { Stop(); }
    bool Start(uint16_t port, const std::string& busid, const UsbDeviceModel& model) {
        WSADATA w; WSAStartup(MAKEWORD(2, 2), &w);
        port_ = port; busid_ = busid; model_ = model;
        listen_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (listen_ == INVALID_SOCKET) { LOGE("usbip: socket failed %d", WSAGetLastError()); return false; }
        BOOL yes = TRUE; setsockopt(listen_, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof yes);
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(port); a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (bind(listen_, (sockaddr*)&a, sizeof a) != 0) { LOGE("usbip: bind 127.0.0.1:%u failed %d", port, WSAGetLastError()); closesocket(listen_); listen_ = INVALID_SOCKET; return false; }
        if (listen(listen_, 4) != 0) { LOGE("usbip: listen failed %d", WSAGetLastError()); closesocket(listen_); listen_ = INVALID_SOCKET; return false; }
        running_ = true;
        acceptThread_ = std::thread([this] { AcceptLoop(); });
        pacerThread_ = std::thread([this] { PacerLoop(); });
        LOGI("usbip: server listening on 127.0.0.1:%u busid=%s", port, busid.c_str());
        return true;
    }
    void Stop() {
        if (!running_) return;
        running_ = false;
        if (listen_ != INVALID_SOCKET) { closesocket(listen_); listen_ = INVALID_SOCKET; }
        CloseClient();
        if (acceptThread_.joinable()) acceptThread_.join();
        if (pacerThread_.joinable()) pacerThread_.join();
    }
    bool Attached() const { return attached_; }
    uint64_t UrbsCompleted() const { return urbsDone_; }

private:
    // ---- byte helpers ----
    static void Put32(std::vector<uint8_t>& v, uint32_t x) { v.push_back(x >> 24); v.push_back(x >> 16); v.push_back(x >> 8); v.push_back(x); }
    static void Put16(std::vector<uint8_t>& v, uint16_t x) { v.push_back(x >> 8); v.push_back(x); }
    static uint32_t Get32(const uint8_t* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }
    static uint16_t Get16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

    bool RecvAll(SOCKET s, uint8_t* buf, int len) {
        while (len > 0) { int n = recv(s, (char*)buf, len, 0); if (n <= 0) return false; buf += n; len -= n; }
        return true;
    }
    bool SendAll(const std::vector<uint8_t>& v) {
        std::lock_guard<std::mutex> g(sendMutex_);
        SOCKET s = client_; if (s == INVALID_SOCKET) return false;
        const char* p = (const char*)v.data(); int len = (int)v.size();
        while (len > 0) { int n = send(s, p, len, 0); if (n <= 0) return false; p += n; len -= n; }
        return true;
    }
    void CloseClient() {
        SOCKET s = client_.exchange(INVALID_SOCKET);
        if (s != INVALID_SOCKET) { shutdown(s, SD_BOTH); closesocket(s); }
        attached_ = false;
        std::lock_guard<std::mutex> g(queueMutex_); intrQueue_.clear(); parked_.clear();
    }

    void AppendDeviceInfo(std::vector<uint8_t>& v, bool withInterfaces) {
        char path[256] = {}; snprintf(path, sizeof path, "/sys/devices/pci0000:00/0000:00:01.0/usb1/%s", busid_.c_str());
        char bus[32] = {}; snprintf(bus, sizeof bus, "%s", busid_.c_str());
        v.insert(v.end(), (uint8_t*)path, (uint8_t*)path + 256);
        v.insert(v.end(), (uint8_t*)bus, (uint8_t*)bus + 32);
        Put32(v, 1); Put32(v, 2); Put32(v, 2); // busnum, devnum, speed = USB_SPEED_FULL
        const auto& d = model_.deviceDesc;
        Put16(v, (uint16_t)(d[8] | (d[9] << 8)));   // idVendor
        Put16(v, (uint16_t)(d[10] | (d[11] << 8))); // idProduct
        Put16(v, (uint16_t)(d[12] | (d[13] << 8))); // bcdDevice
        v.push_back(d[4]); v.push_back(d[5]); v.push_back(d[6]); // class, subclass, protocol
        v.push_back(1);                      // bConfigurationValue
        v.push_back(d[17]);                  // bNumConfigurations
        v.push_back(model_.numInterfaces);
        if (withInterfaces) for (auto& i : model_.ifaces) { v.push_back(i.cls); v.push_back(i.sub); v.push_back(i.proto); v.push_back(0); }
    }

    void AcceptLoop() {
        while (running_) {
            sockaddr_in a{}; int al = sizeof a;
            SOCKET s = accept(listen_, (sockaddr*)&a, &al);
            if (s == INVALID_SOCKET) { if (running_) Sleep(50); continue; }
            BOOL nd = TRUE; setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char*)&nd, sizeof nd);
            HandleConnection(s);
        }
    }

    void HandleConnection(SOCKET s) {
        uint8_t op[8];
        if (!RecvAll(s, op, 8)) { closesocket(s); return; }
        uint16_t version = Get16(op), code = Get16(op + 2);
        if (code == 0x8005) { // OP_REQ_DEVLIST
            std::vector<uint8_t> r; Put16(r, 0x0111); Put16(r, 0x0005); Put32(r, 0); Put32(r, 1); AppendDeviceInfo(r, true);
            send(s, (const char*)r.data(), (int)r.size(), 0);
            LOGI("usbip: devlist served (client version 0x%04X)", version);
            closesocket(s); return;
        }
        if (code != 0x8003) { LOGW("usbip: unknown op 0x%04X", code); closesocket(s); return; }
        uint8_t bus[32]; if (!RecvAll(s, bus, 32)) { closesocket(s); return; }
        std::string want((const char*)bus, strnlen((const char*)bus, 32));
        std::vector<uint8_t> r; Put16(r, 0x0111); Put16(r, 0x0003);
        if (want != busid_) { Put32(r, 1); send(s, (const char*)r.data(), (int)r.size(), 0); LOGW("usbip: import of unknown busid '%s' refused", want.c_str()); closesocket(s); return; }
        Put32(r, 0); AppendDeviceInfo(r, false);
        send(s, (const char*)r.data(), (int)r.size(), 0);
        CloseClient();
        client_ = s; attached_ = true;
        LOGI("usbip: device imported by client (attached)");
        UrbLoop(s);
        LOGI("usbip: client connection closed");
        if (client_ == s) CloseClient();
    }

    void UrbLoop(SOCKET s) {
        std::vector<uint8_t> buf;
        while (running_) {
            uint8_t h[48];
            if (!RecvAll(s, h, 48)) return;
            uint32_t cmd = Get32(h), seq = Get32(h + 4), dir = Get32(h + 12), ep = Get32(h + 16);
            if (cmd == 1) { // CMD_SUBMIT
                uint32_t tlen = Get32(h + 0x18);
                buf.assign(tlen, 0);
                if (dir == 0 && tlen > 0 && !RecvAll(s, buf.data(), (int)tlen)) return;
                uint8_t setup[8]; memcpy(setup, h + 0x28, 8);
                if (ep == 0) HandleControl(seq, setup, buf, tlen);
                else if (dir == 1 && (int)ep == model_.interruptInEp) { std::lock_guard<std::mutex> g(queueMutex_); intrQueue_.push_back({seq, tlen}); }
                else if (dir == 1) { std::lock_guard<std::mutex> g(queueMutex_); parked_.push_back(seq); }
                else SendRet(seq, 0, (int)tlen, nullptr, 0); // OUT on other endpoints: swallow
            } else if (cmd == 2) { // CMD_UNLINK
                uint32_t target = Get32(h + 0x14);
                bool found = false;
                { std::lock_guard<std::mutex> g(queueMutex_);
                  for (auto it = intrQueue_.begin(); it != intrQueue_.end(); ++it) if (it->seq == target) { intrQueue_.erase(it); found = true; break; }
                  for (auto it = parked_.begin(); !found && it != parked_.end(); ++it) if (*it == target) { parked_.erase(it); found = true; break; } }
                std::vector<uint8_t> r; Put32(r, 4); Put32(r, seq); Put32(r, 0); Put32(r, 0); Put32(r, 0);
                Put32(r, found ? (uint32_t)-104 : 0); r.resize(48, 0);
                SendAll(r);
            } else { LOGW("usbip: unexpected command %u", cmd); return; }
        }
    }

    void SendRet(uint32_t seq, int status, int actual, const uint8_t* data, int len) {
        std::vector<uint8_t> r; r.reserve(48 + len);
        Put32(r, 3); Put32(r, seq); Put32(r, 0); Put32(r, 0); Put32(r, 0);
        Put32(r, (uint32_t)status); Put32(r, (uint32_t)actual); Put32(r, 0); Put32(r, 0xffffffff); Put32(r, 0); r.resize(48, 0);
        if (data && len > 0) r.insert(r.end(), data, data + len);
        SendAll(r);
        urbsDone_++;
    }

    const uint8_t* FindHidDescriptor(uint8_t iface) {
        const auto& c = model_.configDesc; size_t i = 0; int cur = -1;
        while (i + 2 <= c.size()) {
            uint8_t len = c[i], type = c[i + 1]; if (len < 2) break;
            if (type == 4) cur = c[i + 2];
            if (type == 0x21 && cur == iface) return &c[i];
            i += len;
        }
        return nullptr;
    }

    void HandleControl(uint32_t seq, const uint8_t* su, std::vector<uint8_t>& out, uint32_t tlen) {
        uint8_t rt = su[0], req = su[1]; uint16_t wValue = su[2] | (su[3] << 8), wIndex = su[4] | (su[5] << 8), wLength = su[6] | (su[7] << 8);
        bool in = (rt & 0x80) != 0; uint8_t type = (rt >> 5) & 3, recip = rt & 0x1F;
        std::vector<uint8_t> data; bool stall = false;
        if (type == 0) { // standard
            switch (req) {
                case 6: { uint8_t dt = wValue >> 8, idx = wValue & 0xFF;
                    if (dt == 1) data = model_.deviceDesc;
                    else if (dt == 2) data = model_.configDesc;
                    else if (dt == 3) {
                        if (idx == 0) data = {4, 3, 0x09, 0x04};
                        else if (idx <= model_.strings.size()) { const auto& s = model_.strings[idx - 1]; data.push_back((uint8_t)(2 + 2 * s.size())); data.push_back(3); for (wchar_t ch : s) { data.push_back(ch & 0xFF); data.push_back(ch >> 8); } }
                        else stall = true;
                    }
                    else if (dt == 0x22 && recip == 1 && wIndex < model_.reportDescs.size() && !model_.reportDescs[wIndex].empty()) data = model_.reportDescs[wIndex];
                    else if (dt == 0x21 && recip == 1) { const uint8_t* h = FindHidDescriptor((uint8_t)wIndex); if (h) data.assign(h, h + h[0]); else stall = true; }
                    else stall = true;
                    break; }
                case 0: data = {0, 0}; break;            // GET_STATUS
                case 8: data = {1}; break;               // GET_CONFIGURATION
                case 10: data = {0}; break;              // GET_INTERFACE
                case 1: case 3: case 5: case 9: case 11: break; // CLEAR/SET_FEATURE, SET_ADDRESS, SET_CONFIGURATION, SET_INTERFACE
                default: stall = true; break;
            }
        } else if (type == 1) { // class
            uint8_t iface = wIndex & 0xFF;
            if (iface <= 2) { // HID interfaces
                switch (req) {
                    case 1: { // GET_REPORT
                        if (!model_.onGetReport || !model_.onGetReport(iface, wValue >> 8, wValue & 0xFF, data)) stall = true; break; }
                    case 9: { // SET_REPORT
                        int st = model_.onSetReport ? model_.onSetReport(iface, wValue >> 8, wValue & 0xFF, out.data(), (int)tlen) : 0; if (st != 0) stall = true; break; }
                    case 2: data = {0}; break;           // GET_IDLE
                    case 3: data = {1}; break;           // GET_PROTOCOL
                    case 10: case 11: break;             // SET_IDLE, SET_PROTOCOL
                    default: stall = true; break;
                }
            } else { // CDC ACM
                switch (req) {
                    case 0x21: data = {0x80, 0x25, 0, 0, 0, 0, 8}; break; // GET_LINE_CODING 9600 8N1
                    case 0x20: case 0x22: case 0x23: break;             // SET_LINE_CODING, SET_CONTROL_LINE_STATE, SEND_BREAK
                    default: stall = true; break;
                }
            }
        } else stall = true;
        if (stall) { SendRet(seq, -32, 0, nullptr, 0); return; }
        if (in) { int n = (int)std::min<size_t>(data.size(), wLength); SendRet(seq, 0, n, data.data(), n); }
        else SendRet(seq, 0, (int)tlen, nullptr, 0);
    }

    void PacerLoop() {
        HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!timer) timer = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        LARGE_INTEGER due; due.QuadPart = -10000LL * model_.frameIntervalMs;
        SetWaitableTimer(timer, &due, model_.frameIntervalMs, nullptr, nullptr, FALSE);
        uint8_t frame[64];
        while (running_) {
            if (WaitForSingleObject(timer, 100) != WAIT_OBJECT_0) continue;
            if (!attached_) continue;
            PendingUrb u{0, 0}; bool have = false;
            { std::lock_guard<std::mutex> g(queueMutex_); if (!intrQueue_.empty()) { u = intrQueue_.front(); intrQueue_.pop_front(); have = true; } }
            if (!have) continue;
            memset(frame, 0, sizeof frame);
            if (model_.fillFrame) model_.fillFrame(frame);
            int n = (int)std::min<uint32_t>(64, u.len);
            SendRet(u.seq, 0, n, frame, n);
        }
        CloseHandle(timer);
    }

    struct PendingUrb { uint32_t seq; uint32_t len; };
    uint16_t port_ = 0; std::string busid_; UsbDeviceModel model_;
    SOCKET listen_ = INVALID_SOCKET; std::atomic<SOCKET> client_{INVALID_SOCKET};
    std::atomic<bool> running_{false}, attached_{false}; std::atomic<uint64_t> urbsDone_{0};
    std::thread acceptThread_, pacerThread_;
    std::mutex sendMutex_, queueMutex_;
    std::deque<PendingUrb> intrQueue_; std::deque<uint32_t> parked_;
};

} // namespace onepad
