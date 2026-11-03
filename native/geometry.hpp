#pragma once

#include <algorithm>
#include <cmath>
#include <string_view>

namespace omasnap {
struct Point { double x, y; };
struct Box {
    double x, y, w, h;
    bool operator==(const Box&) const = default;
};
enum class Zone { none, left, right, top, bottom, topLeft, topRight, bottomLeft, bottomRight };

inline Zone zoneAt(Point p, Box screen, double edge = 24, double corner = 160) {
    if (p.x < screen.x || p.y < screen.y || p.x >= screen.x + screen.w || p.y >= screen.y + screen.h)
        return Zone::none;
    const double x = p.x - screen.x, y = p.y - screen.y;
    edge = std::min({edge, screen.w / 4, screen.h / 4});
    const double cx = std::min(corner, screen.w / 4), cy = std::min(corner, screen.h / 4);
    const bool l = x < edge, r = x >= screen.w - edge;
    const bool t = y < edge, b = y >= screen.h - edge;
    if ((l && y < cy) || (t && x < cx)) return Zone::topLeft;
    if ((r && y < cy) || (t && x >= screen.w - cx)) return Zone::topRight;
    if ((l && y >= screen.h - cy) || (b && x < cx)) return Zone::bottomLeft;
    if ((r && y >= screen.h - cy) || (b && x >= screen.w - cx)) return Zone::bottomRight;
    if (l) return Zone::left;
    if (r) return Zone::right;
    if (t) return Zone::top;
    if (b) return Zone::bottom;
    return Zone::none;
}

inline std::string_view zoneName(Zone zone) {
    switch (zone) {
        case Zone::left: return "left";
        case Zone::right: return "right";
        case Zone::top: return "top";
        case Zone::bottom: return "bottom";
        case Zone::topLeft: return "top-left";
        case Zone::topRight: return "top-right";
        case Zone::bottomLeft: return "bottom-left";
        case Zone::bottomRight: return "bottom-right";
        default: return "none";
    }
}

inline Zone parseZone(std::string_view name) {
    for (int i = 1; i <= int(Zone::bottomRight); ++i)
        if (zoneName(Zone(i)) == name) return Zone(i);
    return Zone::none;
}

// Split the usable area before adding gaps: odd and fractional monitor sizes
// share the same dividing pixel, including on monitors left of the origin.
inline Box placement(Zone zone, Box area, double gap) {
    const bool left = zone == Zone::left || zone == Zone::topLeft || zone == Zone::bottomLeft;
    const bool right = zone == Zone::right || zone == Zone::topRight || zone == Zone::bottomRight;
    const bool upper = zone == Zone::top || zone == Zone::topLeft || zone == Zone::topRight;
    const bool lower = zone == Zone::bottom || zone == Zone::bottomLeft || zone == Zone::bottomRight;
    double x1 = area.x, y1 = area.y, x2 = area.x + area.w, y2 = area.y + area.h;
    if (left) x2 = std::floor(area.x + area.w / 2);
    if (right) x1 = std::floor(area.x + area.w / 2);
    if (upper) y2 = std::floor(area.y + area.h / 2);
    if (lower) y1 = std::floor(area.y + area.h / 2);
    gap = std::min({gap, (x2 - x1) / 4, (y2 - y1) / 4});
    return {std::ceil(x1 + gap), std::ceil(y1 + gap),
            std::max(1.0, std::floor(x2 - gap) - std::ceil(x1 + gap)),
            std::max(1.0, std::floor(y2 - gap) - std::ceil(y1 + gap))};
}

// Preserve enough of the top of the frame to grab it again. Oversized windows
// remain usable, and partial off-screen placement is still allowed.
inline Box keepVisible(Box frame, Box area) {
    const double gripX = std::min({64.0, frame.w, area.w});
    const double gripY = std::min({30.0, frame.h, area.h});
    frame.x = std::clamp(frame.x, area.x + gripX - frame.w, area.x + area.w - gripX);
    frame.y = std::clamp(frame.y, area.y, area.y + area.h - gripY);
    return frame;
}
} // namespace omasnap
