// AULA F75 / F87 Pro / F99 wireless keyboards on the Compx 2.4 GHz receiver (VID 3554, PID FA09).
// The receiver exposes a 20-byte vendor collection on usage page FF02 / usage 02. Commands are
// 20-byte output reports 0x13 carrying a command byte and a trailing sum checksum. The reply is
// another 0x13 frame whose command byte, with its top bit masked, matches the request.
//   0x07 link status -> reply [5] = 1 when a keyboard is linked to this receiver, 0 when it is
//        empty (verified on two FA09 receivers, one paired and one not; the OEM app sends it first)
//   0x4A battery     -> payload starts at byte 5: the level (1..100), then the state byte
//        (0x01 on battery, 0x10 with the cable in)
// The receiver also pushes unrelated 0x13 frames (a 0x0A status among them), so the report id
// alone does not identify the reply.
// Every AULA keyboard ships with its own receiver and they all enumerate as 3554:FA09, so each
// receiver is queried on its own and keyed by its device instance.
// From github.com/PyFlat/Device-Battery-Info (PR 6), github.com/deepan-alve/womier-l65-linux and
// OpenRGB issue 5166 (frame layout, 0x07 sent first by the OEM software).
#include "drivers.h"

#include <cwctype>

namespace {

constexpr BYTE kLinkStatus = 0x07;
constexpr BYTE kBattery = 0x4A;
constexpr int kReplyTimeoutMs = 1000;

// A receiver whose keyboard does not answer is retried after this long instead of every poll.
Backoff g_retry(3 * 60 * 1000);

BYTE Sum(const std::vector<BYTE>& b, size_t n) {
    unsigned s = 0;
    for (size_t i = 0; i < n; i++) s += b[i];
    return (BYTE)(s & 0xFF);
}

bool Command(HidDev& d, BYTE cmd, BYTE arg, std::vector<BYTE>& r) {
    std::vector<BYTE> req(20, 0);
    req[0] = 0x13;
    req[1] = cmd;
    req[2] = arg;
    req[19] = Sum(req, 19);
    d.Flush();
    if (!d.Write(req)) return false;
    return d.ReadUntil(r, kReplyTimeoutMs, [cmd](const std::vector<BYTE>& r) {
        return r.size() >= 20 && r[0] == 0x13 && (r[1] & 0x7F) == cmd && r[19] == Sum(r, 19);
    });
}

// "8&decd760" from "\\?\hid#vid_3554&pid_fa09&mi_01&col01#8&decd760&0&0000#{...}": the parent
// device instance, shared by the collections of one receiver and distinct between receivers.
std::wstring InstanceTag(const std::wstring& path) {
    size_t start = path.find(L'#');
    if (start != std::wstring::npos) start = path.find(L'#', start + 1);
    if (start == std::wstring::npos) return L"";
    size_t amp = path.find(L'&', start + 1);
    if (amp != std::wstring::npos) amp = path.find(L'&', amp + 1);
    size_t end = amp != std::wstring::npos ? amp : path.find(L'#', start + 1);
    std::wstring tag = path.substr(start + 1, end == std::wstring::npos ? std::wstring::npos : end - start - 1);
    for (auto& ch : tag) ch = towlower(ch);
    return tag;
}

}  // namespace

void PollAula(PollContext& ctx) {
    const USHORT vid = 0x3554, pid = 0xFA09;  // other 3554 products are mice on a different protocol
    if (ctx.IsClaimed(vid, pid)) return;
    bool any = false;
    for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) {
             return c.page() == 0xFF02 && c.usage() == 0x0002 && c.outLen() >= 20 && c.inLen() >= 20;
         })) {
        if (g_retry.Waiting(c->path)) continue;
        HidDev d;
        if (!d.Open(*c)) continue;
        std::vector<BYTE> r;
        if (Command(d, kLinkStatus, 0x01, r) && r[5] == 0) {  // receiver without a keyboard
            g_retry.Succeeded(c->path);
            continue;
        }
        // The first request can go unanswered while the 2.4 GHz link wakes up; retry once.
        if (!Command(d, kBattery, 0x00, r) && !Command(d, kBattery, 0x00, r)) {
            g_retry.Failed(c->path);
            continue;
        }
        g_retry.Succeeded(c->path);
        int level = r[5];
        if (level < 1 || level > 100) continue;
        DeviceStatus s;
        s.key = L"aula:" + Hex4(vid) + L":" + Hex4(pid) + L":" + InstanceTag(c->path);
        s.name = L"AULA wireless keyboard";
        s.kind = DeviceKind::Keyboard;
        s.via = L"2.4 GHz receiver";
        s.percent = level;
        s.charging = r[6] == 0x10;
        ctx.out.push_back(s);
        any = true;
    }
    if (any) ctx.Claim(vid, pid);
}
