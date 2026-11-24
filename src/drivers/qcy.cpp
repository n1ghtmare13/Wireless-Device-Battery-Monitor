// QCY TWS earbuds (MeloBuds Pro and other Wuqi/HT-chip models) broadcast their left / right /
// case battery in a clear manufacturer-specific BLE advertisement, company id 0x521C (also seen:
// 0x522C, 0x071E). Windows has no standard Battery Service level for them, so the Bluetooth
// fallback in generic.cpp picks them up from here. Payload after the 2-byte company id
// (layout from hyc1743/qcy-audio-windows, crates/qcy-protocol/src/adv.rs):
//   [0..1] modelID big-endian (MeloBuds Pro = 0x4D4A), [2] reserved, [3] side/color bits,
//   [4] flags, [5] left battery, [6] right battery, [7] case battery — each packed as
//   bit7 = charging, bits0-6 = percent (0x00 / 0x7F = unknown),
//   [11..16] swapped classic Bluetooth MAC, [18..23] peer classic MAC.
// The BLE address rotates, so entries are keyed by the embedded classic MAC, which is the
// address Windows paired (BTHENUM\DEV_xxxxxxxxxxxx).
#include "drivers.h"
#include "ble_adverts.h"

#include <map>

namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
std::map<UINT64, FastPairBattery> g_byClassic;  // classic (public) MAC -> latest battery
std::map<UINT64, FastPairBattery> g_byBle;      // rotating BLE advert address -> latest battery

int Component(BYTE b, bool& charging) {
    int level = b & 0x7F;
    charging = (b & 0x80) != 0;
    return (level == 0 || level == 0x7F || level > 100) ? -1 : level;
}

UINT64 UnswapMac(const BYTE* raw) {
    // advert order [b1,b0,b2,b5,b4,b3] -> canonical big-endian MAC
    const BYTE m[6] = { raw[1], raw[0], raw[2], raw[5], raw[4], raw[3] };
    UINT64 a = 0;
    for (int i = 0; i < 6; i++) a = (a << 8) | m[i];
    return a;
}

}  // namespace

void QcyOnAdvert(UINT64 addr, const BYTE* d, UINT32 n) {
    if (n < 8) return;
    FastPairBattery b;
    b.left  = Component(d[5], b.leftChg);
    b.right = Component(d[6], b.rightChg);
    b.box   = Component(d[7], b.boxChg);
    if (b.left < 0 && b.right < 0 && b.box < 0) return;
    b.tick = GetTickCount64();
    AcquireSRWLockExclusive(&g_lock);
    g_byBle[addr] = b;
    if (n >= 17) g_byClassic[UnswapMac(d + 11)] = b;
    ReleaseSRWLockExclusive(&g_lock);
}

bool QcyLookup(UINT64 addr, FastPairBattery& out) {
    ULONGLONG now = GetTickCount64();
    AcquireSRWLockShared(&g_lock);
    const FastPairBattery* hit = nullptr;
    auto it = g_byClassic.find(addr);
    if (it != g_byClassic.end()) hit = &it->second;
    else {
        auto it2 = g_byBle.find(addr);
        if (it2 != g_byBle.end()) hit = &it2->second;
    }
    bool ok = hit && now - hit->tick < 5 * 60 * 1000;
    if (ok) out = *hit;
    ReleaseSRWLockShared(&g_lock);
    return ok;
}
