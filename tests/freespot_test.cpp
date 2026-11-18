// Unit tests for FreeSpot (taskbar free-gap picker).
#include <cstdio>
#include <vector>

#if __has_include("freespot.h")
#include "freespot.h"
#else
#include "../src/freespot.h"
#endif

static int failures = 0;

static void Expect(const char* what, int want, int len, int minPos, int maxPos,
                   const std::vector<Span1D>& taken, int gap, int expected) {
    int got = FreeSpot(want, len, minPos, maxPos, taken, gap);
    if (got != expected) {
        failures++;
        printf("FAIL %s\n  want=%d len=%d [%d, %d] gap=%d -> got %d, expected %d\n",
               what, want, len, minPos, maxPos, gap, got, expected);
    }
}

int main() {
    // 1. No obstacles: want inside range returns want; want below minPos clamps to minPos;
    //    want too far right clamps to maxPos - len.
    Expect("no obstacles: inside range returns want", 250, 50, 100, 500, {}, 0, 250);
    Expect("no obstacles: exact minPos returns want", 100, 50, 100, 500, {}, 0, 100);
    Expect("no obstacles: exact maxPos - len returns want", 450, 50, 100, 500, {}, 0, 450);
    Expect("no obstacles: want below minPos clamps to minPos", 50, 50, 100, 500, {}, 0, 100);
    Expect("no obstacles: want negative clamps to minPos", -20, 50, 100, 500, {}, 0, 100);
    Expect("no obstacles: want too far right clamps to maxPos - len", 460, 50, 100, 500, {}, 0, 450);
    Expect("no obstacles: want far past maxPos clamps to maxPos - len", 999, 50, 100, 500, {}, 0, 450);

    // 2. want already in a free run returns want unchanged.
    const std::vector<Span1D> multiTaken = { {100, 200}, {500, 600} };
    Expect("want in first free run unchanged", 30, 40, 0, 1000, multiTaken, 0, 30);
    Expect("want at left edge of first free run unchanged", 0, 40, 0, 1000, multiTaken, 0, 0);
    Expect("want at right edge of first free run unchanged", 60, 40, 0, 1000, multiTaken, 0, 60);
    Expect("want in middle free run unchanged", 350, 40, 0, 1000, multiTaken, 0, 350);
    Expect("want at left edge of middle free run unchanged", 200, 40, 0, 1000, multiTaken, 0, 200);
    Expect("want at right edge of middle free run unchanged", 460, 40, 0, 1000, multiTaken, 0, 460);
    Expect("want in last free run unchanged", 800, 40, 0, 1000, multiTaken, 0, 800);
    Expect("want at right edge of last free run unchanged", 960, 40, 0, 1000, multiTaken, 0, 960);

    // 3. Overlapping an obstacle snaps to the nearer side; equal distance picks the larger position.
    Expect("overlapping obstacle nearer left side snaps to left", 180, 50, 0, 600, { {200, 300} }, 0, 150);
    Expect("overlapping obstacle at obstacle start snaps to left", 200, 50, 0, 600, { {200, 300} }, 0, 150);
    Expect("overlapping obstacle slightly left of midpoint snaps to left", 224, 50, 0, 600, { {200, 300} }, 0, 150);
    Expect("overlapping obstacle nearer right side snaps to right", 270, 50, 0, 600, { {200, 300} }, 0, 300);
    Expect("overlapping obstacle slightly right of midpoint snaps to right", 226, 50, 0, 600, { {200, 300} }, 0, 300);
    Expect("overlapping obstacle equal distance picks larger position", 225, 50, 0, 600, { {200, 300} }, 0, 300);
    Expect("equal distance tie breaker picks larger position (closer to tray)", 210, 20, 100, 500, { {200, 240} }, 0, 240);

    // 4. gap widens obstacles (strip must keep gap px away); touching spans (hi == lo) count as free.
    Expect("gap widens obstacle on left: strip kept gap px away", 150, 50, 0, 500, { {200, 300} }, 10, 140);
    Expect("gap widens obstacle on right: strip kept gap px away", 300, 50, 0, 500, { {200, 300} }, 10, 310);
    Expect("want at widened boundary on left", 140, 50, 0, 500, { {200, 300} }, 10, 140);
    Expect("want at widened boundary on right", 310, 50, 0, 500, { {200, 300} }, 10, 310);
    Expect("touching span on left (strip.hi == obstacle.lo)", 60, 40, 0, 400, { {100, 200} }, 0, 60);
    Expect("touching span on right (strip.lo == obstacle.hi)", 200, 40, 0, 400, { {100, 200} }, 0, 200);
    Expect("two adjacent touching obstacles merge without gap", 150, 40, 0, 500, { {100, 200}, {200, 300} }, 0, 60);
    Expect("strip touching right edge of merged adjacent obstacles", 300, 40, 0, 500, { {100, 200}, {200, 300} }, 0, 300);
    Expect("strip touching widened obstacle on left", 55, 40, 0, 400, { {100, 200} }, 5, 55);
    Expect("strip touching widened obstacle on right", 205, 40, 0, 400, { {100, 200} }, 5, 205);

    // 5. Overlapping / unsorted obstacles are merged correctly (pass them out of order).
    Expect("unsorted obstacles: inside free run between them", 250, 50, 0, 1000, { {600, 700}, {100, 200}, {350, 450} }, 0, 250);
    Expect("unsorted obstacles: snapping left from inside obstacle", 370, 50, 0, 1000, { {600, 700}, {100, 200}, {350, 450} }, 0, 300);
    Expect("unsorted obstacles: snapping right from inside obstacle", 380, 50, 0, 1000, { {600, 700}, {100, 200}, {350, 450} }, 0, 450);
    Expect("overlapping out of order obstacles merge: snaps left", 200, 50, 0, 600, { {200, 350}, {100, 250}, {300, 400} }, 0, 50);
    Expect("overlapping out of order obstacles merge: snaps right", 300, 50, 0, 600, { {200, 350}, {100, 250}, {300, 400} }, 0, 400);
    Expect("contained obstacle passed out of order merges correctly", 120, 50, 0, 500, { {150, 200}, {100, 300} }, 0, 50);
    Expect("obstacles merge after gap widening, tie-break to larger", 200, 50, 0, 600, { {250, 350}, {100, 210} }, 25, 375);

    // 6. Gap too short for len is skipped in favour of a farther run that fits.
    Expect("gap too short for len is skipped: nearer to right fit", 320, 100, 0, 1000, { {150, 300}, {350, 500} }, 0, 500);
    Expect("gap too short for len is skipped: nearer to left fit", 250, 100, 0, 1000, { {150, 300}, {350, 500} }, 0, 50);
    Expect("multiple short gaps skipped in favour of only fitting run", 90, 60, 0, 500, { {50, 80}, {100, 130}, {150, 180} }, 0, 180);

    // 7. No run fits at all -> returns want unchanged.
    Expect("no run fits because gaps are too small returns want", 50, 80, 0, 100, { {40, 60} }, 0, 50);
    Expect("no run fits: want at other position returns want unchanged", 20, 80, 0, 100, { {40, 60} }, 0, 20);
    Expect("len exceeds entire range returns want unchanged", 120, 150, 100, 200, {}, 0, 120);
    Expect("obstacle covers entire range returns want unchanged", 150, 50, 100, 300, { {50, 350} }, 0, 150);
    Expect("gap expansion eliminates all fitting space returns want", 50, 40, 0, 100, { {45, 55} }, 20, 50);

    // 8. Obstacles partly outside [minPos, maxPos] are handled (no free run invented outside the range).
    Expect("obstacle partly before minPos: no run invented below minPos", 80, 50, 100, 500, { {50, 200} }, 0, 200);
    Expect("obstacle partly before minPos: want inside obstacle", 120, 50, 100, 500, { {50, 200} }, 0, 200);
    Expect("obstacle partly after maxPos: no run invented above maxPos", 450, 50, 100, 500, { {400, 600} }, 0, 350);
    Expect("obstacle partly after maxPos: want past maxPos", 550, 50, 100, 500, { {400, 600} }, 0, 350);
    Expect("obstacles partly outside both ends: want near left", 100, 50, 100, 500, { {50, 200}, {400, 600} }, 0, 200);
    Expect("obstacles partly outside both ends: want near right", 480, 50, 100, 500, { {50, 200}, {400, 600} }, 0, 350);
    Expect("obstacles completely outside range do not affect free space", 50, 50, 100, 500, { {10, 80}, {550, 650} }, 0, 100);
    Expect("obstacles completely outside range: want inside range", 250, 50, 100, 500, { {10, 80}, {550, 650} }, 0, 250);
    Expect("obstacles completely outside range: want past maxPos", 520, 50, 100, 500, { {10, 80}, {550, 650} }, 0, 450);
    Expect("obstacle covering entire range and extending outside returns want", 300, 50, 100, 500, { {50, 550} }, 0, 300);

    if (!failures) printf("freespot_test: all passed\n");
    return failures ? 1 : 0;
}
