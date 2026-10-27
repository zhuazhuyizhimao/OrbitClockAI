#include "orbit_display.h"
#include <esp_heap_caps.h>
#include <esp_lcd_panel_ops.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <src/misc/cache/instance/lv_image_cache.h>
#include <atomic>
#include "application.h"
#include "orbit_flush.h"
#include "orbit_fonts.h"
#include "orbit_network_state.h"

namespace {
i2c_master_bus_handle_t shared_bus = nullptr;
std::atomic<bool> dismiss_requested{false}, open_requested{false};
}  // namespace
namespace orbit_clock_ai {
void SetI2cBus(i2c_master_bus_handle_t bus) { shared_bus = bus; }
i2c_master_bus_handle_t I2cBus() { return shared_bus; }
void SetRotation(uint8_t quarter) {
    lv_display_set_rotation(lv_display_get_default(), (lv_display_rotation_t)(quarter & 3));
}
void EndConversation() { dismiss_requested.store(true); }
void OpenConversation() { open_requested.store(true); }
}  // namespace orbit_clock_ai

OrbitLcdDisplay::OrbitLcdDisplay(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel,
                                 int width, int height, int offset_x, int offset_y)
    : LcdDisplay(io, panel, width, height) {
    lv_init();
    // Give the combined GUI a second bounded PSRAM pool. Keep image caching
    // disabled; indexed pixel art and voice streaming must share the 8MB PSRAM.
    void* pool = heap_caps_malloc(LV_MEM_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    ESP_ERROR_CHECK(pool && lv_mem_add_pool(pool, LV_MEM_SIZE) ? ESP_OK : ESP_ERR_NO_MEM);
    lv_image_cache_resize(0, true);
    lvgl_port_cfg_t port = ESP_LVGL_PORT_INIT_CONFIG();
    port.task_priority = 1;
    port.task_affinity = 1;
    ESP_ERROR_CHECK(lvgl_port_init(&port));
    DisplayLockGuard guard(this);
    constexpr size_t buffer_bytes = 480 * 24 * sizeof(uint16_t);
    draw_buffer_ = static_cast<uint16_t*>(
        heap_caps_malloc(buffer_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    transfer_buffer_ = static_cast<uint16_t*>(
        heap_caps_malloc(buffer_bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA));
    ESP_ERROR_CHECK(draw_buffer_ && transfer_buffer_ ? ESP_OK : ESP_ERR_NO_MEM);
    display_ = lv_display_create(width, height);
    ESP_ERROR_CHECK(display_ ? ESP_OK : ESP_FAIL);
    lv_display_set_color_format(display_, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(display_, draw_buffer_, nullptr, buffer_bytes,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(display_, this);
    lv_display_set_flush_cb(display_, Flush);
    lv_display_set_offset(display_, offset_x, offset_y);
    lv_display_add_event_cb(
        display_,
        [](lv_event_t* event) {
            auto* area = static_cast<lv_area_t*>(lv_event_get_param(event));
            area->x1 &= ~1;
            area->y1 &= ~1;
            area->x2 |= 1;
            area->y2 |= 1;
            lvgl_port_task_wake(LVGL_PORT_EVENT_DISPLAY, nullptr);
        },
        LV_EVENT_INVALIDATE_AREA, nullptr);
    esp_lcd_panel_io_callbacks_t callbacks = {};
    callbacks.on_color_trans_done = FlushDone;
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io, &callbacks, display_));
    // CO5300 stays in its original physical orientation; drawing and LVGL's
    // inverse touch mapping rotate together, using the clock's calibrated basis.
    // Keep the neutral MADCTL written by board initialization. Do not apply
    // generic esp_lcd mirror/swap operations to this CO5300 panel.
}
void OrbitLcdDisplay::Flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    auto* self = static_cast<OrbitLcdDisplay*>(lv_display_get_user_data(display));
    const auto panel_area =
        OrbitPrepareStrip(*area, reinterpret_cast<uint16_t*>(pixels), self->transfer_buffer_,
                          uint8_t(lv_display_get_rotation(display)));
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(self->panel_, panel_area.x1, panel_area.y1,
                                              panel_area.x2 + 1, panel_area.y2 + 1,
                                              self->transfer_buffer_));
}
bool OrbitLcdDisplay::FlushDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*,
                                void* user) {
    lv_display_flush_ready(static_cast<lv_display_t*>(user));
    return false;
}
void OrbitLcdDisplay::SetupUI() {
    LcdDisplay::SetupUI();
    DisplayLockGuard guard(this);
    orbit_clock_ai::InitializeUi();
    lv_timer_create(Tick, 20, this);
    Update();
}
void OrbitLcdDisplay::Tick(lv_timer_t* timer) {
    static_cast<OrbitLcdDisplay*>(lv_timer_get_user_data(timer))->Update();
}
void OrbitLcdDisplay::Update() {
    auto& app = Application::GetInstance();
    const auto state = app.GetDeviceState();
    if (orbit_clock_ai::TakeNetworkSetupUiRequest()) {
        hidden_by_user_ = false;
        closing_ = false;
    }
    if (int(state) != last_state_) {
        last_state_ = int(state);
        if (state != kDeviceStateIdle && !closing_)
            hidden_by_user_ = false;
    }
    if (open_requested.exchange(false)) {
        hidden_by_user_ = false;
        closing_ = false;
        if (state == kDeviceStateIdle)
            app.ToggleChatState();
    }
    if (dismiss_requested.exchange(false)) {
        hidden_by_user_ = true;
        closing_ = state == kDeviceStateSpeaking || state == kDeviceStateListening ||
                   state == kDeviceStateConnecting;
        app.CloseChat();
    }
    if (closing_ && state != kDeviceStateSpeaking && state != kDeviceStateListening &&
        state != kDeviceStateConnecting) {
        closing_ = false;
    }
    const bool show = state != kDeviceStateIdle && !hidden_by_user_;
    if (show != last_voice_) {
        orbit_clock_ai::SetVoiceVisible(show);
        last_voice_ = show;
    }
    // SetEmotion can replace the controller from the application task. Update
    // runs under the LVGL lock, so check its actual state on every UI tick.
    if (gif_controller_) {
        if (show && !gif_controller_->IsPlaying())
            gif_controller_->Resume();
        else if (!show && gif_controller_->IsPlaying())
            gif_controller_->Pause();
    }
    orbit_clock_ai::PollUi(show);
}
