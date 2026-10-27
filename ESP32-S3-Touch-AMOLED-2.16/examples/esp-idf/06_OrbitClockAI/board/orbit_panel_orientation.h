#pragma once
#include <algorithm>
#include <cstdint>

namespace orbit_panel {
// Match Arduino_CO5300::setRotation(0), used by the working clock firmware.
// All four UI orientations are handled by software; retain no MADCTL axis bits.
inline constexpr uint8_t MADCTL = 0x00;
inline constexpr int SIDE = 480;
// Looking at the screen with all three physical buttons along the top edge,
// the previous panel frame was 90 degrees clockwise. Correct it once at the
// panel boundary, after the saved/automatic logical page rotation.
inline constexpr uint8_t UPRIGHT_QUARTER = 1;
inline uint8_t PanelQuarter(uint8_t page_quarter) { return (page_quarter + UPRIGHT_QUARTER) & 3; }

struct Point {
    int x, y;
};

inline Point TouchToPanel(uint16_t raw_x, uint16_t raw_y) {
    // SensorLib applies swapXY before mirrorX. The ESP-IDF touch framework
    // applies mirrors before swapXY, so copying the flags changes the mapping.
    // Preserve the working clock's (480 - raw_y, raw_x), bounded to the panel.
    return {std::clamp(SIDE - int(raw_y), 0, SIDE - 1), std::clamp(int(raw_x), 0, SIDE - 1)};
}

inline Point PanelToDisplay(Point native) {
    // Inverse of the fixed physical mounting rotation, before LVGL applies
    // the inverse of the user's logical page rotation.
    switch (UPRIGHT_QUARTER) {
        case 1:
            return {SIDE - 1 - native.y, native.x};
        case 2:
            return {SIDE - 1 - native.x, SIDE - 1 - native.y};
        case 3:
            return {native.y, SIDE - 1 - native.x};
        default:
            return native;
    }
}

inline Point TouchToDisplay(uint16_t raw_x, uint16_t raw_y) {
    return PanelToDisplay(TouchToPanel(raw_x, raw_y));
}
}  // namespace orbit_panel
