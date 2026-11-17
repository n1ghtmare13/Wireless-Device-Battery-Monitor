#include "../src/hid.h"

#include <cassert>
#include <cstdio>

namespace {
HidColl Collection(USHORT vid, USHORT pid, USHORT page, USHORT usage) {
    HidColl c;
    c.vid = vid;
    c.pid = pid;
    c.caps.UsagePage = page;
    c.caps.Usage = usage;
    return c;
}
}  // namespace

int main() {
    assert(Clamp100(-5) == 0);
    assert(Clamp100(0) == 0);
    assert(Clamp100(50) == 50);
    assert(Clamp100(100) == 100);
    assert(Clamp100(150) == 100);

    assert(MapTo100(100, 200, 400) == 0);
    assert(MapTo100(200, 200, 400) == 0);
    assert(MapTo100(300, 200, 400) == 50);
    assert(MapTo100(400, 200, 400) == 100);
    assert(MapTo100(500, 200, 400) == 100);
    for (int hi : { 200, 100 }) {
        assert(MapTo100(-5, 200, hi) == 0);
        assert(MapTo100(50, 200, hi) == 50);
        assert(MapTo100(150, 200, hi) == 100);
    }

    const int mv[] = { 4200, 3800, 3400 };
    const int pct[] = { 100, 60, 0 };
    assert(Interpolate(4300, mv, pct, 3) == 100);
    assert(Interpolate(3300, mv, pct, 3) == 0);
    assert(Interpolate(4200, mv, pct, 3) == 100);
    assert(Interpolate(3800, mv, pct, 3) == 60);
    assert(Interpolate(3400, mv, pct, 3) == 0);
    assert(Interpolate(4000, mv, pct, 3) == 80);
    assert(Interpolate(3600, mv, pct, 3) == 30);

    assert(Hex4(0x0) == L"0000");
    assert(Hex4(0xABCD) == L"ABCD");
    assert(GuessKind(L"Gaming MOUSE") == DeviceKind::Mouse);
    assert(GuessKind(L"Wireless Keyboard") == DeviceKind::Keyboard);
    assert(GuessKind(L"Wireless Controller") == DeviceKind::Gamepad);
    assert(GuessKind(L"Gaming Headset") == DeviceKind::Headset);
    assert(GuessKind(L"Wireless device") == DeviceKind::Other);

    const USHORT vid = 0x1234, pid = 0x5678;
    const auto mouse = Collection(vid, pid, 1, 2);
    const auto joystick = Collection(vid, pid, 1, 4);
    const auto gamepad = Collection(vid, pid, 1, 5);
    const auto keyboard = Collection(vid, pid, 1, 6);
    assert(KindFromCollections({}, vid, pid) == DeviceKind::Other);
    assert(KindFromCollections({ keyboard }, vid, pid) == DeviceKind::Keyboard);
    assert(KindFromCollections({ keyboard, gamepad }, vid, pid) == DeviceKind::Gamepad);
    assert(KindFromCollections({ joystick, keyboard }, vid, pid) == DeviceKind::Gamepad);
    assert(KindFromCollections({ keyboard, gamepad, mouse }, vid, pid) == DeviceKind::Mouse);
    assert(KindFromCollections({ mouse, gamepad, keyboard }, vid, pid) == DeviceKind::Mouse);

    // Higher-priority collections from other devices/pages must not affect this keyboard.
    const std::vector<HidColl> ignored = {
        Collection(0x9999, pid, 1, 2),
        Collection(vid, 0x9999, 1, 2),
        Collection(vid, pid, 0xFF00, 2),
        Collection(vid, pid, 0xFF00, 5)
    };
    assert(KindFromCollections(ignored, vid, pid) == DeviceKind::Other);
    auto colls = ignored;
    colls.push_back(keyboard);
    assert(KindFromCollections(colls, vid, pid) == DeviceKind::Keyboard);

    std::puts("HID helpers: ok (mapping, interpolation, formatting, device kinds).");
    return 0;
}
