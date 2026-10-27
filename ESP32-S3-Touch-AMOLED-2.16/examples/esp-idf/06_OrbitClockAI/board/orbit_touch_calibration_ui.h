#pragma once
#include <lvgl.h>
#include "orbit_ai_font.h"
#include "orbit_touch_filter.h"
#include "orbit_touch_mapping.h"

namespace orbit_touch_calibration {
using Read = orbit_touch::Sample (*)();
using Save = bool (*)(const orbit_touch::Mapping&);
inline Read read_sample = nullptr;
inline Save save_mapping = nullptr;
inline lv_obj_t *panel = nullptr, *hint = nullptr, *dot = nullptr;
inline orbit_touch::Calibrator calibrator;
inline lv_display_rotation_t previous_rotation;
inline bool was_down = false, finished = false;
inline uint32_t pressed_at = 0, finished_at = 0;
inline int sum_x = 0, sum_y = 0, origin_x = 0, origin_y = 0;
inline unsigned samples = 0;
inline bool moved = false;
inline bool wait_release = false;

inline bool Active() { return panel != nullptr; }
inline void Configure(Read read, Save save) {
    read_sample = read;
    save_mapping = save;
}
inline void Cancel() {
    if (!panel)
        return;
    lv_display_set_rotation(lv_display_get_default(), previous_rotation);
    lv_obj_delete_async(panel);
    panel = nullptr;
}
inline void PlaceTarget() {
    const auto p = orbit_touch::Calibrator::TARGETS[calibrator.Count()];
    lv_obj_set_pos(dot, p.x - 14, p.y - 14);
    char text[100];
    lv_snprintf(text, sizeof(text), "请点击圆点  %u / 3\n按 Key3 取消", calibrator.Count() + 1);
    lv_label_set_text(hint, text);
}
inline void Open() {
    if (Active())
        return;
    previous_rotation = lv_display_get_rotation(lv_display_get_default());
    lv_display_set_rotation(lv_display_get_default(), LV_DISPLAY_ROTATION_0);
    calibrator.Reset();
    was_down = finished = false;
    samples = 0;
    wait_release = read_sample && read_sample().down;
    panel = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 480, 480);
    lv_obj_set_pos(panel, 0, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x111C2B), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    auto* title = lv_label_create(panel);
    lv_obj_set_style_text_font(title, &orbit_font_ai_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xF0CF72), 0);
    lv_label_set_text(title, "触摸校准");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 36);
    hint = lv_label_create(panel);
    lv_obj_set_style_text_font(hint, &orbit_font_ai_20, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
    dot = lv_obj_create(panel);
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 28, 28);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xF0CF72), 0);
    PlaceTarget();
}
inline void Poll(uint32_t now) {
    if (!Active())
        return;
    if (finished) {
        if (uint32_t(now - finished_at) >= 1200)
            Cancel();
        return;
    }
    if (!read_sample)
        return;
    const auto sample = read_sample();
    if (wait_release) {
        if (!sample.down)
            wait_release = false;
        return;
    }
    if (sample.down) {
        if (!was_down) {
            pressed_at = now;
            samples = 0;
            sum_x = sum_y = 0;
            moved = false;
            origin_x = sample.x;
            origin_y = sample.y;
        }
        const int dx = sample.x - origin_x, dy = sample.y - origin_y;
        moved = moved || dx * dx + dy * dy > 24 * 24;
        if (samples < 1000) {
            sum_x += sample.x;
            sum_y += sample.y;
            ++samples;
        }
    } else if (was_down && !moved && samples >= 2 && uint32_t(now - pressed_at) >= 40) {
        calibrator.Add(sum_x / int(samples), sum_y / int(samples));
        if (calibrator.Count() < 3)
            PlaceTarget();
        else {
            orbit_touch::Mapping candidate;
            if (!calibrator.Solve(candidate)) {
                calibrator.Reset();
                PlaceTarget();
                lv_label_set_text(hint, "校准失败，请重试\n请点击圆点  1 / 3");
            } else if (!save_mapping || !save_mapping(candidate)) {
                calibrator.Reset();
                PlaceTarget();
                lv_label_set_text(hint, "保存失败，请重试\n请点击圆点  1 / 3");
            } else {
                finished = true;
                finished_at = now;
                lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
                lv_label_set_text(hint, "校准完成");
            }
        }
        lv_obj_align(hint, LV_ALIGN_CENTER, 0, 0);
    }
    was_down = sample.down;
}
}  // namespace orbit_touch_calibration
