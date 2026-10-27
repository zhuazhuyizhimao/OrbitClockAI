#pragma once
#include <lvgl.h>
#include <algorithm>
#include "orbit_panel_orientation.h"
#include "pixel_clock_model.h"

// Both firmware and host checks use this strip transformation. Keep the
// calibrated four-way mapping identical to the existing clock firmware.
inline lv_area_t OrbitPrepareStrip(const lv_area_t& source_area, const uint16_t* source,
                                   uint16_t* destination, uint8_t quarter) {
    quarter = orbit_panel::PanelQuarter(quarter);
    const int width = lv_area_get_width(&source_area), height = lv_area_get_height(&source_area);
    orbit_pixels::rotatePixelBuffer(source, destination, width, height, quarter);
    for (int i = 0; i < width * height; ++i)
        destination[i] = __builtin_bswap16(destination[i]);
    const auto a = orbit_pixels::rotateToPanel(source_area.x1, source_area.y1, quarter);
    const auto b = orbit_pixels::rotateToPanel(source_area.x2, source_area.y2, quarter);
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x), std::max(a.y, b.y)};
}
