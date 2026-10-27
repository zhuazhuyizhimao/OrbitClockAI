#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "orbit_panel_orientation.h"

namespace orbit_touch {
struct Mapping {
    uint32_t magic = 0;
    float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    bool Valid() const {
        const float det = a * e - b * d;
        return magic == 0x54434831 && std::isfinite(a) && std::isfinite(b) && std::isfinite(c) &&
               std::isfinite(d) && std::isfinite(e) && std::isfinite(f) && std::abs(a) <= 4 &&
               std::abs(b) <= 4 && std::abs(d) <= 4 && std::abs(e) <= 4 && std::abs(det) > .05f &&
               std::abs(det) < 10.f && std::abs(c) < 2000 && std::abs(f) < 2000;
    }
    orbit_panel::Point Apply(int x, int y) const {
        return {std::clamp(int(std::lround(a * x + b * y + c)), 0, 479),
                std::clamp(int(std::lround(d * x + e * y + f)), 0, 479)};
    }
};

class Calibrator {
public:
    inline static constexpr orbit_panel::Point TARGETS[] = {{100, 100}, {380, 100}, {100, 380}};
    void Reset() { count_ = 0; }
    unsigned Count() const { return count_; }
    bool Add(int x, int y) {
        if (count_ >= 3 || x < 0 || x > 4095 || y < 0 || y > 4095)
            return false;
        points_[count_++] = {x, y};
        return true;
    }
    bool Solve(Mapping& out) const {
        if (count_ != 3)
            return false;
        const float ux = points_[1].x - points_[0].x, uy = points_[1].y - points_[0].y;
        const float vx = points_[2].x - points_[0].x, vy = points_[2].y - points_[0].y;
        const float det = ux * vy - vx * uy;
        if (std::abs(det) < 10000 || ux * ux + uy * uy < 10000 || vx * vx + vy * vy < 10000)
            return false;
        Mapping candidate;
        candidate.magic = 0x54434831;
        candidate.a = 280 * vy / det;
        candidate.b = -280 * vx / det;
        candidate.d = -280 * uy / det;
        candidate.e = 280 * ux / det;
        candidate.c = 100 - candidate.a * points_[0].x - candidate.b * points_[0].y;
        candidate.f = 100 - candidate.d * points_[0].x - candidate.e * points_[0].y;
        if (!candidate.Valid())
            return false;
        out = candidate;
        return true;
    }

private:
    orbit_panel::Point points_[3] = {};
    unsigned count_ = 0;
};
}  // namespace orbit_touch
