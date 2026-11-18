// Test the inline helpers with a simulated transport; do not link hid.cpp.
#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>

namespace {
std::vector<std::vector<BYTE>> reports;
int reads = 0;
}  // namespace

// Stand-ins for the Win32 HID transport, as in rapoo_driver_test.cpp.
bool HidDev::Open(const HidColl& c) {
    c_ = c;
    h_ = reinterpret_cast<HANDLE>(1);
    return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
int HidDev::Read(std::vector<BYTE>& out, int timeoutMs) {
    assert(timeoutMs > 0 && timeoutMs <= 4000);
    ++reads;
    if (reports.empty()) return -1;
    out = reports.front();
    reports.erase(reports.begin());
    return (int)out.size();
}

int main() {
    HidDev dev;
    HidColl c;
    c.path = L"scripted";
    assert(dev.Open(c));
    std::vector<BYTE> buf;
    int matches = 0;
    auto match = [&](const std::vector<BYTE>& report) {
        ++matches;
        return report.size() == 2 && report[0] == 7;
    };

    // Skip unrelated reports and stop at the first match, leaving the next one queued.
    reports = { { 1, 10 }, { 2, 20 }, { 7, 30 }, { 7, 40 } };
    assert(dev.ReadUntil(buf, 4000, match));
    assert(reads == 3 && matches == 3);
    assert((buf == std::vector<BYTE>{ 7, 30 }));
    assert(reports.size() == 1 && reports.front()[1] == 40);

    // An immediate read error must return false without evaluating the predicate.
    reports.clear();
    reads = matches = 0;
    assert(!dev.ReadUntil(buf, 4000, match));
    assert(reads == 1 && matches == 0);

    // A read error after a rejected report must also end the search.
    reports = { { 1, 10 } };
    reads = matches = 0;
    assert(!dev.ReadUntil(buf, 4000, match));
    assert(reads == 2 && matches == 1);

    // Nonpositive timeouts must not consume even an immediately matching report.
    for (int timeout : { 0, -1, -100 }) {
        reports = { { 7, 50 } };
        reads = matches = 0;
        assert(!dev.ReadUntil(buf, timeout, match));
        assert(reads == 0 && matches == 0 && reports.size() == 1);
    }

    // Load wobble while discharging (AJ139: 97, 94, 97, 100) must not climb back up.
    DischargeLatch latch;
    assert(latch.Filter(L"m", 97, false) == 97);
    assert(latch.Filter(L"m", 94, false) == 94);
    assert(latch.Filter(L"m", 97, false) == 94);
    assert(latch.Filter(L"m", 100, false) == 94);
    assert(latch.Filter(L"other", 60, false) == 60);
    assert(latch.Filter(L"m", -1, false) == -1);   // asleep: no reading, latch kept
    assert(latch.Filter(L"m", 96, false) == 94);
    assert(latch.Filter(L"m", 99, true) == 99);    // charging shows the raw value and resets
    assert(latch.Filter(L"m", 98, false) == 98);
    assert(latch.Filter(L"m", 80, false) == 80);
    assert(latch.Filter(L"m", 95, false) == 95);   // big rise: charged while unseen
    assert(latch.Filter(L"other", 59, false) == 59);

    Backoff backoff(50);
    assert(!backoff.Waiting(L"unknown"));
    backoff.Failed(L"first");
    assert(backoff.Waiting(L"first"));
    assert(!backoff.Waiting(L"second"));
    backoff.Failed(L"second");
    assert(backoff.Waiting(L"first") && backoff.Waiting(L"second"));
    backoff.Succeeded(L"first");
    assert(!backoff.Waiting(L"first"));
    assert(backoff.Waiting(L"second"));
    backoff.Succeeded(L"second");
    assert(!backoff.Waiting(L"second"));

    backoff.Failed(L"first");
    assert(backoff.Waiting(L"first"));
    Sleep(80);
    assert(!backoff.Waiting(L"first"));

    std::puts("ReadUntil and Backoff: ok (matching, errors, timeouts, retry, independent keys).");
    return 0;
}
