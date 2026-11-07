#include "../native/geometry.hpp"
#include <cstdlib>
#include <iostream>

void check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}

int main() {
    using namespace omasnap;
    const Box screen{-1920, 100, 1920, 1080}, work{-1920, 140, 1920, 1040};
    check(zoneAt({-1919, 600}, screen) == Zone::left, "negative-origin monitor left edge");
    check(zoneAt({-1, 600}, screen) == Zone::right, "negative-origin monitor right edge");
    check(zoneAt({-1919, 101}, screen) == Zone::topLeft, "top-left corner");
    check(zoneAt({-1, 101}, screen) == Zone::topRight, "top-right corner");
    check(zoneAt({-1919, 1179}, screen) == Zone::bottomLeft, "bottom-left corner");
    check(zoneAt({-1, 1179}, screen) == Zone::bottomRight, "bottom-right corner");
    check(zoneAt({-960, 101}, screen) == Zone::top, "top edge");
    check(zoneAt({-960, 1179}, screen) == Zone::bottom, "bottom edge");
    check(zoneAt({-960, 600}, screen) == Zone::none, "no snapping in the middle");
    check(zoneAt({0, 600}, screen) == Zone::none, "adjacent monitor does not belong to this screen");
    check(placement(Zone::topLeft, work, 8) == Box{-1912, 148, 944, 504}, "quarter respects reserved top bar");
    check(placement(Zone::top, work, 8) == Box{-1912, 148, 1904, 504}, "top half respects reserved top bar");
    check(placement(Zone::bottom, work, 8) == Box{-1912, 668, 1904, 504}, "bottom half mirrors top half");
    check(placement(Zone::full, work, 8) == Box{-1912, 148, 1904, 1024}, "full placement respects reserved top bar and gap");
    const Gaps inner{3, 5, 7, 9}, outer{11, 13, 17, 19};
    check(placement(Zone::left, work, inner, outer) == Box{-1901, 151, 936, 1012}, "left half separates inner right gap from outer edges");
    check(placement(Zone::topRight, work, inner, outer) == Box{-951, 151, 938, 502}, "quarter uses asymmetric inner and outer gaps");
    check(placement(Zone::full, work, inner, outer) == Box{-1901, 151, 1888, 1012}, "full placement uses only outer gaps");
    check(placement(Zone::right, work, Gaps{0, 0, 0, 0}, Gaps{0, 0, 0, 0}) == Box{-960, 140, 960, 1040}, "zero gaps are flush with monitor and split");
    check(placement(Zone::full, {0, 0, 100, 1000}, 100) == Box{25, 25, 50, 950}, "explicit gap keeps the legacy cap on narrow areas");
    const Box fractional{37.5, -100.25, 1537.5, 863.5};
    const auto l = placement(Zone::left, fractional, 8), r = placement(Zone::right, fractional, 8);
    check(l.x + l.w + 16 == r.x, "fractional scale has consistent seam");
    const auto t = placement(Zone::top, fractional, 8), b = placement(Zone::bottom, fractional, 8);
    check(t.y + t.h + 16 == b.y && t.x == b.x && t.w == b.w, "fractional top and bottom halves share a seam");
    check(keepVisible({-4000, -1000, 800, 600}, work) == Box{-2656, 140, 800, 600}, "off-screen titlebar remains reachable");
    check(keepVisible({-1900, 200, 800, 600}, work) == Box{-1900, 200, 800, 600}, "normal placement unchanged");
    const auto huge = keepVisible({3000, 3000, 4000, 3000}, work);
    check(huge.x == -64 && huge.y == 1150, "oversized window keeps a grip on screen");
    for (int i = 1; i <= int(Zone::full); ++i)
        check(parseZone(zoneName(Zone(i))) == Zone(i), "zone roundtrip");
    std::cout << "Geometry checks passed\n";
}
