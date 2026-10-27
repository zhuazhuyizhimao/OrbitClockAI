#pragma once
#include <lvgl.h>
#include <src/draw/lv_draw_private.h>
using lv_draw_ctx_t = lv_layer_t;
#define lv_event_get_draw_ctx lv_event_get_layer
inline void lv_draw_polygon(lv_layer_t* layer, const lv_draw_rect_dsc_t* style,
                            const lv_point_t* points, uint8_t count) {
    if (count == 4) {
        lv_area_t rectangle = {points[0].x, points[0].y, points[0].x, points[0].y};
        for (int i = 1; i < 4; ++i) {
            rectangle.x1 = LV_MIN(rectangle.x1, points[i].x);
            rectangle.x2 = LV_MAX(rectangle.x2, points[i].x);
            rectangle.y1 = LV_MIN(rectangle.y1, points[i].y);
            rectangle.y2 = LV_MAX(rectangle.y2, points[i].y);
        }
        bool axis_aligned = true;
        for (int i = 0; i < 4; ++i)
            axis_aligned &= (points[i].x == rectangle.x1 || points[i].x == rectangle.x2) &&
                            (points[i].y == rectangle.y1 || points[i].y == rectangle.y2);
        if (axis_aligned) {
            lv_draw_rect(layer, style, &rectangle);
            return;
        }
    }
    lv_draw_triangle_dsc_t triangle;
    lv_draw_triangle_dsc_init(&triangle);
    triangle.color = style->bg_color;
    triangle.opa = style->bg_opa;
    // Normalized polygons in dice_view are strictly convex with at most 8 vertices.
    for (int i = 1; i + 1 < count; ++i) {
        triangle.p[0] = {(lv_value_precise_t)points[0].x, (lv_value_precise_t)points[0].y};
        triangle.p[1] = {(lv_value_precise_t)points[i].x, (lv_value_precise_t)points[i].y};
        triangle.p[2] = {(lv_value_precise_t)points[i + 1].x, (lv_value_precise_t)points[i + 1].y};
        lv_draw_triangle(layer, &triangle);
    }
}
inline void orbit_draw_label(lv_layer_t* layer, const lv_draw_label_dsc_t* source,
                             const lv_area_t* area, const char* text, void*) {
    auto label = *source;
    label.text = text;
    label.text_local = 1;
    lv_draw_label(layer, &label, area);
}
#define lv_draw_label orbit_draw_label

#undef lv_obj_clear_flag
#define lv_obj_clear_flag(object, flags) \
    lv_obj_remove_flag(object, static_cast<lv_obj_flag_t>(flags))
