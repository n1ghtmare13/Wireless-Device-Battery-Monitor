#pragma once
// Where the taskbar strip may sit without covering another icon. Pure logic, no Windows calls, so it is
// unit-tested (tests/freespot_test.cpp).
#include <algorithm>
#include <cstdlib>
#include <vector>

struct Span1D {
    int lo, hi;  // half-open [lo, hi) along the taskbar; spans that only touch do not overlap
};

// The start position nearest to `want` for a strip of length `len` that lies inside [minPos, maxPos]
// and overlaps none of `taken`, each widened by `gap` on both sides. Ties go to the larger position
// (closer to the tray). Returns `want` unchanged when no free place is long enough.
inline int FreeSpot(int want, int len, int minPos, int maxPos, std::vector<Span1D> taken, int gap) {
    for (auto& t : taken) t = { t.lo - gap, t.hi + gap };
    std::sort(taken.begin(), taken.end(), [](const Span1D& a, const Span1D& b) { return a.lo < b.lo; });
    int best = want, bestDist = -1;
    auto consider = [&](int a, int b) {  // free run [a, b)
        if (b - a < len) return;
        int c = std::clamp(want, a, b - len), d = std::abs(c - want);
        if (bestDist < 0 || d < bestDist || (d == bestDist && c > best)) best = c, bestDist = d;
    };
    int from = minPos;
    for (const auto& t : taken) {
        if (t.hi <= from) continue;
        consider(from, std::min(t.lo, maxPos));
        from = std::max(from, t.hi);
        if (from >= maxPos) break;
    }
    consider(from, maxPos);
    return best;
}
