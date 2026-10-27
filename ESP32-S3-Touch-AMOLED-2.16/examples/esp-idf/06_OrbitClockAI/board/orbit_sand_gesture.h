#pragma once
#include <cstdint>

namespace orbit_sand {
enum class Target { Page, Reset };
enum class Action { None, Toggle, Reset, Settings };
class Gesture {
public:
    static constexpr uint32_t HOLD_MS = 2000, TOGGLE_GAP_MS = 500;
    void Begin(Target target, int x, int y, uint32_t now) {
        active_ = true;
        consumed_ = moved_ = false;
        target_ = target;
        x_ = x;
        y_ = y;
        pressed_at_ = now;
        eligible_ =
            target != Target::Page || !have_toggle_ || uint32_t(now - toggled_at_) >= TOGGLE_GAP_MS;
    }
    Action Move(int x, int y, uint32_t now) {
        if (!active_ || consumed_)
            return Action::None;
        const int dx = x - x_, dy = y - y_;
        moved_ = moved_ || dx * dx + dy * dy > 24 * 24;
        if (!moved_ && uint32_t(now - pressed_at_) >= HOLD_MS) {
            consumed_ = true;
            return Action::Settings;
        }
        return Action::None;
    }
    Action End(int x, int y, uint32_t now, bool same_region) {
        if (!active_)
            return Action::None;
        const auto hold = Move(x, y, now);
        active_ = false;
        if (hold == Action::Settings)
            return hold;
        if (consumed_ || moved_ || !eligible_ || !same_region)
            return Action::None;
        if (target_ == Target::Reset)
            return Action::Reset;
        have_toggle_ = true;
        toggled_at_ = now;
        return Action::Toggle;
    }
    void Cancel() { active_ = false; }
    void Reset() { *this = {}; }

private:
    bool active_ = false, consumed_ = false, moved_ = false, eligible_ = false,
         have_toggle_ = false;
    Target target_ = Target::Page;
    int x_ = 0, y_ = 0;
    uint32_t pressed_at_ = 0, toggled_at_ = 0;
};
}  // namespace orbit_sand
