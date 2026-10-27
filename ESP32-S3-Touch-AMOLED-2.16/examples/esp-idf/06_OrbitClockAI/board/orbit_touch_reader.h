#pragma once
#include <esp_lcd_touch.h>
#include <esp_timer.h>
#include <lvgl.h>
#include "orbit_touch_filter.h"
#include "orbit_touch_mapping.h"

namespace orbit_touch {
using ReadFunction = esp_err_t (*)(esp_lcd_touch_handle_t);
inline ReadFunction controller_read = nullptr;
inline unsigned read_errors = 0;
inline unsigned read_count = 0, contact_reports = 0;
inline esp_err_t last_error = ESP_OK;
inline uint16_t raw_x = 0, raw_y = 0;
inline bool raw_pressed = false;
inline ContactFilter contact_filter;
inline Mapping mapping;
inline orbit_panel::Point MapToDisplay(uint16_t x, uint16_t y) {
    return mapping.Valid() ? mapping.Apply(x, y) : orbit_panel::TouchToDisplay(x, y);
}

// The pinned CST9217 component parses reports but never acknowledges D000.
// SensorLib's working CST92xx driver sends D0 00 AB after every report read.
// Keep the vendor initialization/parser; add that missing handshake locally.
inline esp_err_t ReadAndAcknowledge(esp_lcd_touch_handle_t touch) {
    ++read_count;
    const auto read_result = controller_read(touch);
    const uint8_t acknowledgement[] = {0x00, 0xAB};
    const auto ack_result =
        esp_lcd_panel_io_tx_param(touch->io, 0xD0, acknowledgement, sizeof(acknowledgement));
    const bool valid = read_result == ESP_OK && ack_result == ESP_OK;
    const bool contact = valid && touch->data.points;
    if (!valid) {
        ++read_errors;
        last_error = read_result != ESP_OK ? read_result : ack_result;
    } else if (contact) {
        ++contact_reports;
        raw_x = touch->data.coords[0].x;
        raw_y = touch->data.coords[0].y;
        last_error = ESP_OK;
    }
    const auto filtered =
        contact_filter.Update(!valid    ? Report::Missing
                              : contact ? Report::Contact
                                        : Report::Released,
                              raw_x, raw_y, uint32_t(esp_timer_get_time() / 1000));
    raw_pressed = filtered.down;
    portENTER_CRITICAL(&touch->data.lock);
    touch->data.points = filtered.down ? 1 : 0;
    touch->data.coords[0].x = filtered.x;
    touch->data.coords[0].y = filtered.y;
    portEXIT_CRITICAL(&touch->data.lock);
    return ESP_OK;
}

inline void Install(esp_lcd_touch_handle_t touch) {
    if (touch->read_data != ReadAndAcknowledge) {
        controller_read = touch->read_data;
        contact_filter.Reset();
        touch->read_data = ReadAndAcknowledge;
    }
}

inline void ConfigureInput(lv_indev_t* input) {
    // Poll as in the working Arduino clock. Interrupts may still wake the GUI,
    // but a missed edge cannot leave the previous position or press latched.
    lv_indev_set_mode(input, LV_INDEV_MODE_TIMER);
    lv_timer_set_period(lv_indev_get_read_timer(input), 20);
}
}  // namespace orbit_touch
