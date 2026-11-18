// Exercise the actual driver with a simulated HID transport; no USB writes or hardware needed.
#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>
#include <map>
#include <thread>

namespace {
std::map<std::wstring, std::map<BYTE, std::vector<BYTE>>> reports;
std::map<std::wstring, std::vector<std::vector<BYTE>>> requests;
std::map<std::wstring, int> opens;

BYTE Sum(const std::vector<BYTE>& frame) {
    unsigned sum = 0;
    for (size_t i = 0; i < 19; ++i) sum += frame[i];
    return (BYTE)(sum & 0xFF);
}

HidColl Collection(const wchar_t* instance) {
    HidColl c;
    c.path = LR"(\\?\hid#vid_3554&pid_fa09&mi_01&col01#8&)" + std::wstring(instance) + L"&0&0000#{x}";
    c.vid = 0x3554;
    c.pid = 0xFA09;
    c.caps.UsagePage = 0xFF02;
    c.caps.Usage = 2;
    c.caps.InputReportByteLength = 20;
    c.caps.OutputReportByteLength = 20;
    return c;
}

std::vector<BYTE> Reply(BYTE command, BYTE value, BYTE state = 0) {
    std::vector<BYTE> frame(20, 0);
    frame[0] = 0x13;
    frame[1] = (BYTE)(command | 0x80);
    frame[5] = value;
    frame[6] = state;
    frame[19] = Sum(frame);
    return frame;
}

int RequestCount(const HidColl& c, BYTE command) {
    int count = 0;
    for (const auto& request : requests[c.path])
        if (request[1] == command) ++count;
    return count;
}
}  // namespace

// Stand-ins for the Win32 HID transport. The production driver is linked unchanged.
bool HidDev::Open(const HidColl& c) {
    c_ = c;
    h_ = reinterpret_cast<HANDLE>(1);
    ++opens[c.path];
    return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
void HidDev::Flush() { assert(ok()); }
bool HidDev::Write(const std::vector<BYTE>& data, int timeoutMs) {
    assert(ok() && timeoutMs > 0);
    assert(data.size() == 20 && data[0] == 0x13 && data[19] == Sum(data));
    requests[c_.path].push_back(data);
    return true;
}
int HidDev::Read(std::vector<BYTE>& out, int timeoutMs) {
    assert(ok() && timeoutMs > 0 && timeoutMs <= 1300);
    const auto& sent = requests[c_.path];
    if (sent.empty()) return -1;
    auto& scripted = reports[c_.path];
    auto reply = scripted.find(sent.back()[1]);
    if (reply == scripted.end()) {
        // Behave like a real timeout: the driver tells an instant failure (unplugged) from silence.
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
        return -1;
    }
    out = reply->second;
    scripted.erase(reply);
    return (int)out.size();
}
// Not a generic "2.4G Wireless Receiver" name, so no MCHOSE model probe.
std::wstring HidProduct(const HidColl&) { return L"AULA 2.4G receiver"; }
std::wstring Hex4(USHORT v) {
    wchar_t text[8];
    swprintf(text, 8, L"%04X", v);
    return text;
}

int main() {
    auto receiverA = Collection(L"aaaa");
    auto receiverB = Collection(L"bbbb");
    PollContext ctx;
    ctx.colls = { receiverA, receiverB };
    reports = {
        { receiverA.path, { { 0x07, Reply(0x07, 1) }, { 0x4A, Reply(0x4A, 88, 0x01) } } },
        { receiverB.path, { { 0x07, Reply(0x07, 0) } } }
    };
    PollAula(ctx);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].percent == 88 && !ctx.out[0].charging);
    assert(ctx.out[0].kind == DeviceKind::Keyboard);
    assert(ctx.out[0].key == L"aula:3554:FA09:8&aaaa");
    assert(RequestCount(receiverA, 0x07) == 1 && RequestCount(receiverA, 0x4A) == 1);
    assert(RequestCount(receiverB, 0x07) == 1 && RequestCount(receiverB, 0x4A) == 0);
    assert(ctx.IsClaimed(0x3554, 0xFA09));

    // Both receivers share VID/PID, but each linked keyboard must keep its own identity.
    receiverA = Collection(L"cccc");
    receiverB = Collection(L"dddd");
    ctx = {};
    ctx.colls = { receiverA, receiverB };
    reports = {
        { receiverA.path, { { 0x07, Reply(0x07, 1) }, { 0x4A, Reply(0x4A, 50, 0x01) } } },
        { receiverB.path, { { 0x07, Reply(0x07, 1) }, { 0x4A, Reply(0x4A, 70, 0x10) } } }
    };
    PollAula(ctx);
    assert(ctx.out.size() == 2);
    assert(ctx.out[0].key == L"aula:3554:FA09:8&cccc");
    assert(ctx.out[1].key == L"aula:3554:FA09:8&dddd");
    assert(ctx.out[0].key != ctx.out[1].key);
    assert(ctx.out[0].percent == 50 && !ctx.out[0].charging);
    assert(ctx.out[1].percent == 70 && ctx.out[1].charging);
    assert(ctx.out[0].kind == DeviceKind::Keyboard && ctx.out[1].kind == DeviceKind::Keyboard);
    assert(ctx.IsClaimed(0x3554, 0xFA09));

    // Silence resends the battery request every ~1.3 s within the 4 s deadline, then skips the path
    // on the next poll.
    auto silent = Collection(L"eeee");
    ctx = {};
    ctx.colls = { silent };
    reports.clear();
    PollAula(ctx);
    assert(ctx.out.empty() && ctx.claimed.empty());
    assert(opens[silent.path] == 1);
    const int attempts = RequestCount(silent, 0x4A);
    assert(RequestCount(silent, 0x07) == 1 && attempts >= 3 && attempts <= 4);
    ctx = {};
    ctx.colls = { silent };
    PollAula(ctx);
    assert(ctx.out.empty() && ctx.claimed.empty());
    assert(opens[silent.path] == 1);
    assert(RequestCount(silent, 0x07) == 1 && RequestCount(silent, 0x4A) == attempts);

    // Older firmware can ignore link status while still reporting a valid battery level.
    auto legacy = Collection(L"ffff");
    ctx = {};
    ctx.colls = { legacy };
    reports = { { legacy.path, { { 0x4A, Reply(0x4A, 61, 0x01) } } } };
    PollAula(ctx);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"aula:3554:FA09:8&ffff");
    assert(ctx.out[0].percent == 61 && !ctx.out[0].charging);
    assert(RequestCount(legacy, 0x07) == 1 && RequestCount(legacy, 0x4A) == 1);
    assert(ctx.IsClaimed(0x3554, 0xFA09));

    // Invalid levels must not invent a device or claim the receiver's VID/PID.
    for (BYTE level : { (BYTE)0, (BYTE)101 }) {
        auto invalid = Collection(level == 0 ? L"0000" : L"0101");
        ctx = {};
        ctx.colls = { invalid };
        reports = { { invalid.path, { { 0x07, Reply(0x07, 1) }, { 0x4A, Reply(0x4A, level, 0x01) } } } };
        PollAula(ctx);
        assert(RequestCount(invalid, 0x4A) == 1);
        assert(ctx.out.empty() && ctx.claimed.empty());
    }
    std::puts("AULA driver tests passed (multiple receivers, link status, charging, backoff, legacy, invalid levels).");
}
