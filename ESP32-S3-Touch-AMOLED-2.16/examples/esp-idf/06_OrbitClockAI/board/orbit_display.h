#pragma once
#include <driver/i2c_master.h>
#include "lcd_display.h"

namespace orbit_clock_ai {
void SetI2cBus(i2c_master_bus_handle_t bus);
i2c_master_bus_handle_t I2cBus();
void SetRotation(uint8_t quarter);
void InitializeUi();
void SetVoiceVisible(bool visible);
void PollUi(bool voice_visible);
void EndConversation();
void OpenConversation();
}  // namespace orbit_clock_ai

class OrbitLcdDisplay : public LcdDisplay {
public:
    OrbitLcdDisplay(esp_lcd_panel_io_handle_t io, esp_lcd_panel_handle_t panel, int width,
                    int height, int offset_x, int offset_y);
    void SetupUI() override;

private:
    bool hidden_by_user_ = false, last_voice_ = false, closing_ = false;
    int last_state_ = -1;
    uint16_t *draw_buffer_ = nullptr, *transfer_buffer_ = nullptr;
    static void Flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels);
    static bool FlushDone(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t*, void* user);
    static void Tick(lv_timer_t* timer);
    void Update();
};
