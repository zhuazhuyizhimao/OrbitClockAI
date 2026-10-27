// Orbit Clock: a custom application based on the official LVGL Widgets example.
// Arduino-ESP32 3.3.11 + bundled LVGL 8.4.0 / GFX / SensorLib / Mylibrary.
#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TouchDrvCSTXXX.hpp>
#include <pin_config.h>
#include <HWCDC.h>
#include <math.h>
#include <initializer_list>
#include <SensorQMI8658.hpp>
#include <Preferences.h>
#include "clock_config.h"
#include "orbit_fonts.h"
#include "pixel_clock_model.h"
#include "retro_clock_face.h"
#include "retro_assets.h"
#include "clock_audio.h"
#include "dice_game_model.h"
#include "sand_timer_model.h"
#include "player_game_model.h"
#include <esp_random.h>

HWCDC USBSerial;
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_CO5300 *gfx = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 0, 0, 0, 0);
TouchDrvCST92xx touch;
static bool touchOnline = false;
static bool touchDown = false;
static lv_disp_draw_buf_t drawBuffer;
static lv_color_t *rotationBuffer = nullptr;
static SensorQMI8658 motionSensor;
static bool motionOnline = false;
static bool motionGyroOnline = false;
static constexpr uint8_t USER_BUTTON_PIN=18;

static lv_obj_t *screen,*timeButton,*timeLabel,*dateLabel,*characterImage,*characterButton;
static lv_obj_t *orbit,*focusPanel,*focusLabel,*focusTitle,*startButton,*startLabel,*resetButton;
static lv_obj_t *minusButton,*plusButton,*durationLabel,*focusHint,*modeButton,*modeLabel;
static lv_obj_t *secondsLabel,*formatLabel,*statusLabel,*directionHint=nullptr;
static lv_obj_t *settingsPanel=nullptr;
static lv_obj_t *calibrationPanel=nullptr,*calibrationTitle,*calibrationHint;
static lv_obj_t *calibrationActionLabel,*calibrationCancelButton,*calibrationLockButton;
static bool orientationCalibrating=false,automaticOrientation=false,clockTouchMoved=false;
static uint8_t pageRotation=0,calibrationStep=0,rotationBeforeCalibration=0,themeIndex=0;
static bool (*storeOrientationSettings)()=nullptr;
static orbit_pixels::Orientation pageOrientation;
static const uint32_t accentColors[]={0x8C1B24};
static uint16_t focusMinutes=25;
static uint64_t focusDurationUs=25ULL*60*1000000,remainingUs=focusDurationUs,focusDeadlineUs=0;
static bool hour24=true,focusMode=false,focusRunning=false,usbTimeSet=false,ntpStarted=false;
static uint32_t lastConnectAttempt=0,lastRenderMs=0,lastRenderPixels=0;
static uint32_t bounceStarted=0;
static bool bouncing=false,classicCharacter=false;
static lv_point_t clockTouchOrigin;
static retro_clock::Dot timeDots[retro_clock::MAX_DOTS];
static int timeDotCount=0;
static char dotTime[6]={};
static void refreshUi(lv_timer_t *);
static bool diceMode=false;
static bool sandMode=false,playerMode=false;
static player_game::Game playerGame;
static bool (*storePlayerCount)(uint8_t)=nullptr;
static void openPlayerMode();
static void closePlayerMode();
static void playerAnimationTick();
static bool sandSettingsOpen();
static sand_timer::Countdown sandTimer;
static bool (*storeSandSeconds)(uint16_t)=nullptr;
static void openSandMode();
static void closeSandMode();
static void sandAnimationTick();
static dice_game::Game diceGame;
static uint32_t motionSampleAt=0,motionSampleCount=0,motionReadErrors=0;
static float motionX=0,motionY=0,motionZ=0;
static dice_game::Button userButton;
static dice_game::DoubleClick diceClicks;
static uint32_t (*diceRandomSource)()=nullptr;
static bool (*storeDiceSettings)(uint8_t,uint8_t)=nullptr;

// Enlarge only the clock glyphs, without LVGL's full text-layer transform buffer.
// The bundled Montserrat 48 font is uncompressed 4 bpp. Render callbacks are
// synchronous on the UI thread, so one reusable bitmap is sufficient.
static lv_font_t clockFont;
static uint8_t clockGlyph[96 * 96 / 2];
static bool clockGlyphDescriptor(const lv_font_t *, lv_font_glyph_dsc_t *out,
                                 uint32_t letter, uint32_t next) {
  if (letter < 32 || letter > 127 || !strchr("0123456789:-", (char)letter)) return false;
  if (!lv_font_get_glyph_dsc(&lv_font_montserrat_48, out, letter, next)) return false;
  out->adv_w *= 2; out->box_w *= 2; out->box_h *= 2;
  out->ofs_x *= 2; out->ofs_y *= 2;
  return out->bpp == 4 && out->box_w <= 96 && out->box_h <= 96;
}
static const uint8_t *clockGlyphBitmap(const lv_font_t *, uint32_t letter) {
  lv_font_glyph_dsc_t glyph = {};
  if (!lv_font_get_glyph_dsc(&lv_font_montserrat_48, &glyph, letter, 0)) return nullptr;
  const uint8_t *source = lv_font_get_glyph_bitmap(&lv_font_montserrat_48, letter);
  if (!source || glyph.bpp != 4 || glyph.box_w > 48 || glyph.box_h > 48) return nullptr;
  const unsigned width = glyph.box_w * 2, height = glyph.box_h * 2;
  memset(clockGlyph, 0, (width * height + 1) / 2);
  for (unsigned y = 0; y < height; ++y) {
    for (unsigned x = 0; x < width; ++x) {
      const unsigned src = (y / 2) * glyph.box_w + x / 2;
      const uint8_t alpha = (source[src / 2] >> ((src & 1) ? 0 : 4)) & 15;
      const unsigned dst = y * width + x;
      clockGlyph[dst / 2] |= alpha << ((dst & 1) ? 0 : 4);
    }
  }
  return clockGlyph;
}


static lv_color_t color(uint32_t value){return lv_color_hex(value);}
static void text(lv_obj_t *label,const char *value){
  if(strcmp(lv_label_get_text(label),value)!=0)lv_label_set_text(label,value);
}
static void visible(lv_obj_t *obj,bool show){
  if(show)lv_obj_clear_flag(obj,LV_OBJ_FLAG_HIDDEN);else lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
}
static lv_obj_t *labelAt(lv_obj_t *parent,const char *value,int x,int y,const lv_font_t *font,uint32_t rgb){
  auto *label=lv_label_create(parent);lv_label_set_text(label,value);
  lv_obj_set_style_text_font(label,font,0);lv_obj_set_style_text_color(label,color(rgb),0);
  lv_obj_set_pos(label,x,y);lv_obj_clear_flag(label,LV_OBJ_FLAG_CLICKABLE);return label;
}
static lv_obj_t *buttonAt(int x,int y,int width,const char *caption,lv_obj_t **label){
  auto *button=lv_btn_create(screen);lv_obj_remove_style_all(button);
  lv_obj_set_pos(button,x,y);lv_obj_set_size(button,width,46);
  lv_obj_set_style_radius(button,16,0);lv_obj_set_style_bg_color(button,color(0x8C1B24),0);
  lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);lv_obj_set_style_opa(button,LV_OPA_40,LV_STATE_DISABLED);
  *label=labelAt(button,caption,0,0,&orbit_font_zh_20,0xF3EBC9);lv_obj_center(*label);return button;
}
static void styleAccentButton(lv_obj_t *button,lv_obj_t *label){
  lv_obj_set_style_bg_color(button,color(0x8C1B24),0);lv_obj_set_style_text_color(label,color(0xF3EBC9),0);
}
static void startCharacterBounce(){
  if(focusMode||orientationCalibrating||settingsPanel||diceMode)return;
  bounceStarted=millis();bouncing=true;
}
static void pixelAnimationTick(lv_timer_t *){
  if(!bouncing)return;
  const uint32_t elapsed=uint32_t(millis()-bounceStarted);
  if(focusMode||orientationCalibrating||settingsPanel||elapsed>=240){
    bouncing=false;lv_obj_set_y(characterImage,10);return;
  }
  const int offset=elapsed<60?5:elapsed<120?10:elapsed<180?5:0;
  lv_obj_set_y(characterImage,10-offset);
}
static void onCharacter(lv_event_t *){
  if(focusMode||orientationCalibrating||settingsPanel||diceMode)return;
  clock_audio::play();
  classicCharacter=!classicCharacter;
  lv_img_set_src(characterImage,classicCharacter?&retro_character_classic:&retro_character);
  lv_obj_set_y(characterImage,10);startCharacterBounce();
}
static void onClockPointer(lv_event_t *event){
  if(orientationCalibrating||settingsPanel||focusMode||diceMode)return;
  auto *input=lv_indev_get_act();if(!input)return;
  lv_point_t point;lv_indev_get_point(input,&point);
  if(lv_event_get_code(event)==LV_EVENT_PRESSED){clockTouchOrigin=point;clockTouchMoved=false;}
  else if(abs(point.x-clockTouchOrigin.x)+abs(point.y-clockTouchOrigin.y)>12)clockTouchMoved=true;
}
static void onTimeDraw(lv_event_t *event){
  auto *context=lv_event_get_draw_ctx(event);const auto *clip=context->clip_area;
  lv_draw_rect_dsc_t style;lv_draw_rect_dsc_init(&style);
  style.radius=0;style.border_width=0;style.bg_color=color(0x201A1D);style.bg_opa=LV_OPA_COVER;
  for(int i=0;i<timeDotCount;++i){
    const auto &dot=timeDots[i];lv_area_t area={dot.x,dot.y,(lv_coord_t)(dot.x+3),(lv_coord_t)(dot.y+3)};
    if(area.x2<clip->x1||area.x1>clip->x2||area.y2<clip->y1||area.y1>clip->y2)continue;
    lv_draw_rect(context,&style,&area);
  }
}
static void onFormat(lv_event_t *){
  if(clockTouchMoved||orientationCalibrating||settingsPanel||focusMode||diceMode)return;
  clock_audio::play();
  hour24=!hour24;refreshUi(nullptr);
}

static void updateDirectionHint() {
  if (!directionHint) return;
  text(directionHint,automaticOrientation ? "长按时间校准方向" : "方向锁定，长按时间校准");
  lv_obj_align(directionHint,LV_ALIGN_TOP_MID,0,89);
  visible(directionHint,!focusMode && (!automaticOrientation || !pageOrientation.calibrated()));
}
static void setPageRotation(uint8_t quarter) {
  pageRotation=quarter&3;clockTouchMoved=false;
  lv_disp_set_rotation(nullptr,(lv_disp_rot_t)pageRotation);
  lv_obj_invalidate(screen);
}
static bool persistOrientation() {
  return !storeOrientationSettings || storeOrientationSettings();
}
static void closeCalibration() {
  orientationCalibrating=false;
  lv_obj_del_async(calibrationPanel);calibrationPanel=nullptr;
  updateDirectionHint();
  lv_obj_invalidate(screen);
}
static void onCalibrationCancel(lv_event_t *) {
  pageOrientation.cancelCalibration();
  pageOrientation.hold(rotationBeforeCalibration,millis());
  setPageRotation(rotationBeforeCalibration);closeCalibration();
}
static void onCalibrationLock(lv_event_t *) {
  automaticOrientation=false;
  pageOrientation.cancelCalibration();pageOrientation.hold(rotationBeforeCalibration,millis());
  setPageRotation(rotationBeforeCalibration);
  if (!persistOrientation()) {
    text(calibrationHint,"已锁定，但保存失败");calibrationStep=2;
    visible(calibrationCancelButton,false);visible(calibrationLockButton,false);
    text(calibrationActionLabel,"返回时钟");lv_obj_center(calibrationActionLabel);return;
  }
  closeCalibration();
}
static void onCalibrationCapture(lv_event_t *) {
  const uint32_t now=millis();
  if (calibrationStep==2) { closeCalibration();return; }
  if (!pageOrientation.captureReady(now)) {
    text(calibrationHint,"请保持不动一秒后再点击");return;
  }
  if (calibrationStep==0) {
    if (!pageOrientation.captureUpright(now)) return;
    calibrationStep=1;
    text(calibrationTitle,"向右转动板子90度");
    text(calibrationHint,"保持屏幕朝向自己\n保持竖立并停稳一秒");
    text(calibrationActionLabel,"完成校准");lv_obj_center(calibrationActionLabel);return;
  }
  if (!pageOrientation.captureClockwise(now)) {
    text(calibrationHint,"请向右转足90度\n保持竖立并停稳一秒");return;
  }
  automaticOrientation=true;setPageRotation(1);
  if (!persistOrientation()) {
    text(calibrationHint,"校准已生效，但保存失败");calibrationStep=2;
    visible(calibrationCancelButton,false);visible(calibrationLockButton,false);
    text(calibrationActionLabel,"返回时钟");lv_obj_center(calibrationActionLabel);return;
  }
  closeCalibration();
  if (!focusMode) startCharacterBounce();
}
static lv_obj_t *calibrationButton(int y,int width,const char *caption,lv_obj_t **captionLabel,
                                 lv_event_cb_t callback) {
  lv_obj_t *button=lv_btn_create(calibrationPanel);
  lv_obj_remove_style_all(button);lv_obj_set_size(button,width,48);
  lv_obj_align(button,LV_ALIGN_TOP_MID,0,y);
  lv_obj_set_style_radius(button,24,0);lv_obj_set_style_bg_color(button,color(0x8C1B24),0);
  lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);
  *captionLabel=labelAt(button,caption,0,0,&orbit_font_zh_20,0xF3EBC9);
  lv_obj_center(*captionLabel);lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,nullptr);
  return button;
}
static void onCalibrationOpen(lv_event_t *) {
  if (orientationCalibrating || focusMode || clockTouchMoved || diceMode) return;
  rotationBeforeCalibration=pageRotation;orientationCalibrating=true;calibrationStep=0;
  setPageRotation(0);pageOrientation.cancelCalibration();
  calibrationPanel=lv_obj_create(screen);lv_obj_remove_style_all(calibrationPanel);
  lv_obj_set_size(calibrationPanel,480,480);lv_obj_set_pos(calibrationPanel,0,0);
  lv_obj_clear_flag(calibrationPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(calibrationPanel,color(0xD7D8DF),0);
  lv_obj_set_style_bg_opa(calibrationPanel,LV_OPA_COVER,0);
  auto *title=labelAt(calibrationPanel,"校准屏幕方向",0,0,&orbit_font_zh_20,accentColors[themeIndex]);
  lv_obj_align(title,LV_ALIGN_TOP_MID,0,60);
  calibrationTitle=labelAt(calibrationPanel,"将屏幕竖起，文字朝上",0,0,&orbit_font_zh_20,0x201A1D);
  lv_obj_align(calibrationTitle,LV_ALIGN_TOP_MID,0,124);
  calibrationHint=labelAt(calibrationPanel,"保持不动一秒\n再点击下一步",0,0,&orbit_font_zh_16,0x595660);
  lv_obj_set_width(calibrationHint,400);lv_obj_set_style_text_align(calibrationHint,LV_TEXT_ALIGN_CENTER,0);
  lv_obj_align(calibrationHint,LV_ALIGN_TOP_MID,0,174);
  calibrationButton(256,232,"下一步",&calibrationActionLabel,onCalibrationCapture);
  lv_obj_t *caption;
  calibrationLockButton=calibrationButton(318,232,"锁定当前方向",&caption,onCalibrationLock);
  calibrationCancelButton=calibrationButton(380,160,"取消",&caption,onCalibrationCancel);
}

static void updateFocusControls() {
  char caption[40];
  snprintf(caption, sizeof(caption), "%u 分钟", (unsigned)focusMinutes);
  text(durationLabel, caption);
  lv_obj_align(durationLabel, LV_ALIGN_TOP_MID, 0, 296);
  if (focusMode) text(modeLabel, "返回时钟");
  else {
    snprintf(caption, sizeof(caption), "专注 %u 分", (unsigned)focusMinutes);
    text(modeLabel, caption);
  }
  lv_obj_center(modeLabel);
  if (focusRunning) {
    lv_obj_add_state(minusButton, LV_STATE_DISABLED);
    lv_obj_add_state(plusButton, LV_STATE_DISABLED);
  } else {
    if (focusMinutes <= 1) lv_obj_add_state(minusButton, LV_STATE_DISABLED);
    else lv_obj_clear_state(minusButton, LV_STATE_DISABLED);
    if (focusMinutes >= 180) lv_obj_add_state(plusButton, LV_STATE_DISABLED);
    else lv_obj_clear_state(plusButton, LV_STATE_DISABLED);
  }
  lv_obj_align(focusTitle, LV_ALIGN_TOP_MID, 0, 84);
  lv_obj_align(focusHint, LV_ALIGN_TOP_MID, 0, 264);
  lv_obj_center(startLabel);
}
static void onMode(lv_event_t *){
  focusMode=!focusMode;clockTouchMoved=false;
  visible(timeButton,!focusMode);visible(characterImage,!focusMode);visible(characterButton,!focusMode);visible(dateLabel,!focusMode);
  visible(focusPanel,focusMode);
  for(auto *obj:{focusTitle,focusLabel,orbit,startButton,resetButton,minusButton,plusButton,durationLabel,focusHint,modeButton})visible(obj,focusMode);
  updateFocusControls();refreshUi(nullptr);
}
static void updateRemaining() {
  if (!focusRunning) return;
  const uint64_t now = esp_timer_get_time();
  remainingUs = now < focusDeadlineUs ? focusDeadlineUs - now : 0;
  if (!remainingUs) {
    focusRunning = false;
    text(startLabel, "再来一次");
    text(focusTitle, "专注完成，休息一下");
    text(focusHint, "可调整时长后重新开始");
    updateFocusControls();
  }
}
static void onStart(lv_event_t *) {
  updateRemaining();
  if (focusRunning) {
    focusRunning = false;
    text(startLabel, "继续");
    text(focusHint, "修改时长会重新计时");
  } else {
    if (!remainingUs) remainingUs = focusDurationUs;
    focusDeadlineUs = esp_timer_get_time() + remainingUs;
    focusRunning = true;
    text(startLabel, "暂停");
    text(focusTitle, "专注模式");
    text(focusHint, "计时中，暂停后可调整");
  }
  updateFocusControls();
}
static void onReset(lv_event_t *) {
  remainingUs = focusDurationUs;
  focusRunning = false;
  text(startLabel, "开始");
  text(focusTitle, "专注模式");
  text(focusHint, "长按加减可快速调整");
  updateFocusControls();
}
static void setFocusMinutes(int minutes) {
  if (focusRunning) return;
  if (minutes < 1) minutes = 1;
  if (minutes > 180) minutes = 180;
  if (minutes == focusMinutes) return;
  focusMinutes = (uint16_t)minutes;
  focusDurationUs = (uint64_t)focusMinutes * 60 * 1000000;
  onReset(nullptr);
}
static void onAdjustDuration(lv_event_t *event) {
  const int direction = (int)(intptr_t)lv_event_get_user_data(event);
  const int step = lv_event_get_code(event) == LV_EVENT_LONG_PRESSED_REPEAT ? 5 : 1;
  setFocusMinutes((int)focusMinutes + direction * step);
}

static void closeSettings(){
  if(settingsPanel){lv_obj_del_async(settingsPanel);settingsPanel=nullptr;lv_obj_invalidate(screen);}
}
static void onSettingsClose(lv_event_t *){closeSettings();}
static void onSettingsFocus(lv_event_t *){closeSettings();onMode(nullptr);}
static void onSettingsCalibration(lv_event_t *){closeSettings();onCalibrationOpen(nullptr);}
static void onSettingsLock(lv_event_t *){
  automaticOrientation=false;pageOrientation.hold(pageRotation,millis());
  if(persistOrientation())closeSettings();
}
static lv_obj_t *menuButton(int y,const char *caption,lv_event_cb_t callback){
  auto *button=lv_btn_create(settingsPanel);lv_obj_remove_style_all(button);
  lv_obj_set_size(button,248,48);lv_obj_align(button,LV_ALIGN_TOP_MID,0,y);
  lv_obj_set_style_bg_color(button,color(0x8C1B24),0);lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);
  lv_obj_set_style_radius(button,16,0);
  auto *label=labelAt(button,caption,0,0,&orbit_font_zh_20,0xF3EBC9);lv_obj_center(label);
  lv_obj_add_event_cb(button,callback,LV_EVENT_CLICKED,nullptr);return button;
}
static void onSettingsOpen(lv_event_t *){
  if(focusMode||orientationCalibrating||settingsPanel||clockTouchMoved||diceMode)return;
  settingsPanel=lv_obj_create(screen);lv_obj_remove_style_all(settingsPanel);
  lv_obj_set_size(settingsPanel,480,480);lv_obj_set_pos(settingsPanel,0,0);
  lv_obj_clear_flag(settingsPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(settingsPanel,color(0xD7D8DF),0);lv_obj_set_style_bg_opa(settingsPanel,LV_OPA_COVER,0);
  auto *title=labelAt(settingsPanel,"像素时钟设置",0,0,&orbit_font_zh_20,0x201A1D);
  lv_obj_align(title,LV_ALIGN_TOP_MID,0,50);
  auto *state=labelAt(settingsPanel,lv_label_get_text(statusLabel),0,0,&orbit_font_zh_16,0x595660);
  lv_obj_align(state,LV_ALIGN_TOP_MID,0,94);
  const char *orientation=!automaticOrientation?"方向已锁定":pageOrientation.calibrated()?"自动转向":"方向待校准";
  state=labelAt(settingsPanel,orientation,0,0,&orbit_font_zh_16,0x595660);lv_obj_align(state,LV_ALIGN_TOP_MID,0,120);
  menuButton(164,"专注计时",onSettingsFocus);menuButton(230,"校准方向",onSettingsCalibration);
  menuButton(296,"锁定当前方向",onSettingsLock);menuButton(362,"返回时钟",onSettingsClose);
  state=labelAt(settingsPanel,"轻点时间可切换时制",0,0,&orbit_font_zh_16,0x595660);lv_obj_align(state,LV_ALIGN_TOP_MID,0,432);
}
#include "dice_ui.h"
#include "sand_ui.h"
#include "player_ui.h"

static void buildUi(){
  clockFont=lv_font_montserrat_48;clockFont.get_glyph_dsc=clockGlyphDescriptor;clockFont.get_glyph_bitmap=clockGlyphBitmap;
  clockFont.line_height*=2;clockFont.base_line*=2;clockFont.fallback=nullptr;
  screen=lv_scr_act();lv_obj_set_style_bg_color(screen,color(0x34333B),0);lv_obj_clear_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(screen,LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(screen,onCharacter,LV_EVENT_SHORT_CLICKED,nullptr);
  auto *background=lv_img_create(screen);lv_img_set_src(background,&retro_matrix);lv_obj_set_pos(background,0,0);
  lv_obj_clear_flag(background,LV_OBJ_FLAG_CLICKABLE);
  characterImage=lv_img_create(screen);lv_img_set_src(characterImage,&retro_character);lv_obj_set_pos(characterImage,80,10);
  lv_obj_clear_flag(characterImage,LV_OBJ_FLAG_CLICKABLE);
  characterButton=lv_btn_create(screen);lv_obj_remove_style_all(characterButton);
  lv_obj_set_pos(characterButton,80,10);lv_obj_set_size(characterButton,320,320);
  lv_obj_add_event_cb(characterButton,onCharacter,LV_EVENT_SHORT_CLICKED,nullptr);
  timeButton=lv_btn_create(screen);lv_obj_remove_style_all(timeButton);
  lv_obj_set_pos(timeButton,60,342);lv_obj_set_size(timeButton,360,102);lv_obj_add_flag(timeButton,LV_OBJ_FLAG_PRESS_LOCK);
  lv_obj_clear_flag(timeButton,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(timeButton,onTimeDraw,LV_EVENT_DRAW_MAIN,nullptr);
  lv_obj_add_event_cb(timeButton,onClockPointer,LV_EVENT_PRESSED,nullptr);
  lv_obj_add_event_cb(timeButton,onClockPointer,LV_EVENT_PRESSING,nullptr);
  lv_obj_add_event_cb(timeButton,onFormat,LV_EVENT_SHORT_CLICKED,nullptr);
  lv_obj_add_event_cb(timeButton,onSettingsOpen,LV_EVENT_LONG_PRESSED,nullptr);
  timeLabel=labelAt(timeButton,"--:--",0,0,&lv_font_montserrat_48,0x201A1D);visible(timeLabel,false);
  dateLabel=labelAt(screen,"等待校时",0,450,&orbit_font_zh_16,0x41404A);lv_obj_align(dateLabel,LV_ALIGN_TOP_MID,0,450);
  lv_obj_set_style_bg_color(dateLabel,color(0xA9ACBB),0);lv_obj_set_style_bg_opa(dateLabel,LV_OPA_COVER,0);
  lv_obj_set_style_pad_hor(dateLabel,4,0);lv_obj_set_style_pad_ver(dateLabel,2,0);
  secondsLabel=labelAt(screen,"",0,0,&orbit_font_zh_16,0x41404A);visible(secondsLabel,false);
  formatLabel=labelAt(screen,"24小时",0,0,&orbit_font_zh_16,0x41404A);visible(formatLabel,false);
  statusLabel=labelAt(screen,"待校时",0,0,&orbit_font_zh_16,0x41404A);visible(statusLabel,false);
  focusPanel=lv_obj_create(screen);lv_obj_remove_style_all(focusPanel);
  lv_obj_set_pos(focusPanel,0,0);lv_obj_set_size(focusPanel,480,480);
  lv_obj_set_style_bg_color(focusPanel,color(0xD7D8DF),0);lv_obj_set_style_bg_opa(focusPanel,LV_OPA_COVER,0);
  lv_obj_clear_flag(focusPanel,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);visible(focusPanel,false);
  focusTitle=labelAt(screen,"专注模式",0,84,&orbit_font_zh_20,0x8C1B24);
  focusLabel=labelAt(screen,"25:00",0,170,&clockFont,0x201A1D);
  orbit=lv_bar_create(screen);lv_obj_set_pos(orbit,112,132);lv_obj_set_size(orbit,256,8);lv_bar_set_range(orbit,0,359);
  lv_obj_set_style_bg_color(orbit,color(0xA9ACBB),LV_PART_MAIN);lv_obj_set_style_bg_color(orbit,color(0x8C1B24),LV_PART_INDICATOR);
  startButton=buttonAt(136,344,100,"开始",&startLabel);lv_obj_t *caption;
  resetButton=buttonAt(248,344,100,"重置",&caption);
  minusButton=buttonAt(86,284,64,"-1",&caption);plusButton=buttonAt(330,284,64,"+1",&caption);
  durationLabel=labelAt(screen,"25 分钟",0,296,&orbit_font_zh_20,0x8C1B24);
  focusHint=labelAt(screen,"长按加减可快速调整",0,264,&orbit_font_zh_16,0x595660);
  modeButton=buttonAt(136,408,208,"返回时钟",&modeLabel);
  for(auto *obj:{focusTitle,focusLabel,orbit,startButton,resetButton,minusButton,plusButton,durationLabel,focusHint,modeButton})visible(obj,false);
  lv_obj_add_event_cb(startButton,onStart,LV_EVENT_CLICKED,nullptr);lv_obj_add_event_cb(resetButton,onReset,LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(modeButton,onMode,LV_EVENT_CLICKED,nullptr);
  lv_obj_add_event_cb(minusButton,onAdjustDuration,LV_EVENT_SHORT_CLICKED,(void*)(intptr_t)-1);
  lv_obj_add_event_cb(minusButton,onAdjustDuration,LV_EVENT_LONG_PRESSED_REPEAT,(void*)(intptr_t)-1);
  lv_obj_add_event_cb(plusButton,onAdjustDuration,LV_EVENT_SHORT_CLICKED,(void*)(intptr_t)1);
  lv_obj_add_event_cb(plusButton,onAdjustDuration,LV_EVENT_LONG_PRESSED_REPEAT,(void*)(intptr_t)1);
  updateFocusControls();timeDotCount=retro_clock::makeTimeDots("--:--",timeDots);strcpy(dotTime,"--:--");
}

static void centerTimeLabel(lv_obj_t *label, lv_obj_t *parent, int centerY) {
  lv_obj_update_layout(label);
  lv_obj_set_pos(label, (lv_obj_get_width(parent) - lv_obj_get_width(label)) / 2,
                 centerY - lv_obj_get_height(label) / 2);
}
static void refreshUi(lv_timer_t *) {
  updateRemaining();
  const time_t epoch = time(nullptr);
  const bool valid = epoch > 1700000000;
  struct tm local = {};
  if (valid) localtime_r(&epoch, &local);
  char buffer[48];
  if (valid) {
    const int hour = hour24 ? local.tm_hour : (local.tm_hour % 12 ? local.tm_hour % 12 : 12);
    snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, local.tm_min);
    text(timeLabel, buffer);
    const char *week[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(buffer, sizeof(buffer), "%04d年%02d月%02d日 星期%s",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, week[local.tm_wday]);
    text(dateLabel, buffer);
    snprintf(buffer, sizeof(buffer), "%02d 秒", local.tm_sec);
    text(secondsLabel, buffer);
    text(formatLabel, hour24 ? "24小时" : (local.tm_hour >= 12 ? "下午" : "上午"));
  } else {
    text(timeLabel, "--:--");
    text(dateLabel, "等待校时");
    text(secondsLabel, "-- 秒");
    text(formatLabel, hour24 ? "24小时" : "12小时");
  }
  if (!touchOnline) text(statusLabel, "触摸异常");
  else if (!strlen(CLOCK_WIFI_SSID)) text(statusLabel, usbTimeSet ? "电脑校时" : "待校时");
  else if (WiFi.status() != WL_CONNECTED) text(statusLabel, valid ? "离线走时" : "未联网");
  else text(statusLabel, valid ? "已联网" : "校时中");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_RIGHT, -38, 36);
  lv_obj_align(dateLabel, LV_ALIGN_TOP_MID, 0, 450);
  const char *value=lv_label_get_text(timeLabel);
  if(strcmp(dotTime,value)!=0){
    strncpy(dotTime,value,5);dotTime[5]=0;timeDotCount=retro_clock::makeTimeDots(value,timeDots);
    lv_obj_invalidate(timeButton);
  }
  const uint32_t seconds = (remainingUs + 999999) / 1000000;
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
  text(focusLabel, buffer);
  centerTimeLabel(focusLabel, screen, 210);
  const int progress = focusMode ? (focusDurationUs - remainingUs) * 359 / focusDurationUs
                                : (valid ? local.tm_sec * 6 : 0);
  lv_bar_set_value(orbit,progress,LV_ANIM_OFF);
}

static void flushDisplay(lv_disp_drv_t *driver, const lv_area_t *area, lv_color_t *pixels) {
  uint32_t width = area->x2 - area->x1 + 1, height = area->y2 - area->y1 + 1;
  int x = area->x1, y = area->y1;
  if (pageRotation) {
    orbit_pixels::rotatePixelBuffer(pixels,rotationBuffer,width,height,pageRotation);
    const auto a = orbit_pixels::rotateToPanel(area->x1,area->y1,pageRotation);
    const auto b = orbit_pixels::rotateToPanel(area->x2,area->y2,pageRotation);
    x = a.x < b.x ? a.x : b.x; y = a.y < b.y ? a.y : b.y;
    if (pageRotation & 1) { const uint32_t oldWidth=width; width=height; height=oldWidth; }
    pixels = rotationBuffer;
  }
#if LV_COLOR_16_SWAP
  gfx->draw16bitBeRGBBitmap(x, y, (uint16_t *)pixels, width, height);
#else
  gfx->draw16bitRGBBitmap(x, y, (uint16_t *)pixels, width, height);
#endif
  lv_disp_flush_ready(driver);
}
static void roundArea(lv_disp_drv_t *, lv_area_t *area) {
  area->x1 &= ~1; area->y1 &= ~1;
  area->x2 |= 1; area->y2 |= 1;
}
static void monitorDisplay(lv_disp_drv_t *,uint32_t elapsed,uint32_t pixels) {
  lastRenderMs=elapsed;lastRenderPixels=pixels;
}
static void readTouch(lv_indev_drv_t *, lv_indev_data_t *data) {
  int16_t x[2] = {}, y[2] = {};
  // Poll each LVGL input cycle so releases are reported even without a new IRQ.
  const uint8_t count = touchOnline ? touch.getPoint(x, y, 1) : 0;
  touchDown = count != 0;
  data->state = count ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
  if (count) { data->point.x = x[0]; data->point.y = y[0]; }
}
static void tick(void *) { lv_tick_inc(2); }
static bool saveDiceSettings(uint8_t count,uint8_t faces) {
  Preferences preferences;if(!preferences.begin("retro-dice",false))return false;
  const uint16_t packed=uint16_t(count)<<8|faces;
  const bool saved=preferences.putUShort("config",packed)==sizeof(packed);
  preferences.end();return saved;
}
static bool savePlayerCount(uint8_t count) {
  if(count<player_game::MIN_PLAYERS||count>player_game::MAX_PLAYERS)return false;
  Preferences preferences;if(!preferences.begin("retro-player",false))return false;
  const bool ok=preferences.putUChar("players",count)==sizeof(uint8_t);preferences.end();return ok;
}
static void loadPlayerCount() {
  Preferences preferences;if(!preferences.begin("retro-player",true))return;
  const auto count=preferences.getUChar("players",4);preferences.end();playerGame.configure(count);
}
static bool saveSandSeconds(uint16_t seconds) {
  Preferences preferences;if(!preferences.begin("retro-sand",false))return false;
  const bool saved=preferences.putUShort("seconds",seconds)==sizeof(seconds);
  preferences.end();return saved;
}
static void loadSandSeconds() {
  Preferences preferences;if(!preferences.begin("retro-sand",true))return;
  sandTimer.configure(preferences.getUShort("seconds",60));preferences.end();
}
static void loadDiceSettings() {
  Preferences preferences;if(!preferences.begin("retro-dice",true))return;
  const uint16_t packed=preferences.getUShort("config",0x0106);
  diceGame.configure(packed>>8,packed&255);preferences.end();
}
static bool saveOrientationSettings() {
  Preferences preferences;
  if (!preferences.begin("retro-clock",false)) return false;
  bool saved=true;
  if (pageOrientation.calibrated()) {
    const auto &basis=pageOrientation.calibration();
    saved=preferences.putBytes("basis",&basis,sizeof(basis))==sizeof(basis);
  }
  saved=(preferences.putBool("auto",automaticOrientation)==1) && saved;
  saved=(preferences.putUChar("locked",pageRotation)==1) && saved;
  if (saved) saved=preferences.putUChar("version",3)==1;
  preferences.end();return saved;
}
static void loadOrientationSettings() {
  Preferences preferences;
  if (!preferences.begin("retro-clock",true)) return;
  if (preferences.getUChar("version",0)==3) {
    orbit_pixels::OrientationCalibration basis;
    if (preferences.getBytesLength("basis")==sizeof(basis)
        && preferences.getBytes("basis",&basis,sizeof(basis))==sizeof(basis))
      pageOrientation.loadCalibration(basis);
    automaticOrientation=preferences.getBool("auto",true);
    if (!automaticOrientation) pageRotation=preferences.getUChar("locked",0)&3;
  }
  preferences.end();
}
static void serviceNetwork() {
  if (!strlen(CLOCK_WIFI_SSID)) return;
  if (WiFi.status() == WL_CONNECTED) {
    if (!ntpStarted) {
      configTzTime(CLOCK_TIMEZONE, CLOCK_NTP_PRIMARY, CLOCK_NTP_SECONDARY);
      ntpStarted = true;
    }
  } else if ((uint32_t)(millis() - lastConnectAttempt) >= 30000) {
    lastConnectAttempt = millis();
    WiFi.reconnect();
  }
}
static void serviceMotion() {
  static uint32_t lastSample = 0;
  const uint32_t now = millis();
  if (!motionOnline || uint32_t(now - lastSample) < (diceMode?20U:50U)) return;
  lastSample = now;
  if (!motionSensor.getDataReady()) return;
  IMUdata acceleration, gyroscope;
  // Rates veto a moving pose; they no longer integrate into page orientation.
  if (motionGyroOnline && motionSensor.getGyroscope(gyroscope.x, gyroscope.y, gyroscope.z))
    pageOrientation.updateGyro(gyroscope.x,gyroscope.y,gyroscope.z,now);
  if (motionSensor.getAccelerometer(acceleration.x, acceleration.y, acceleration.z)) {
    pageOrientation.updateAccel(acceleration.x, acceleration.y, acceleration.z, now);
    feedDiceMotion(acceleration.x,acceleration.y,acceleration.z,now);
  } else ++motionReadErrors;
  if (diceMode || orientationCalibrating || settingsPanel || touchDown || !automaticOrientation || !pageOrientation.readyToApply(now)) return;
  if (pageRotation != pageOrientation.quarter()) {
    // LVGL rotates input coordinates. The flush callback rotates the matching
    // pixels in a small DMA buffer, without using LVGL's limited heap pool.
    setPageRotation(pageOrientation.quarter());
    if (!focusMode) startCharacterBounce();
  }
}
static void serviceConsole() {
  static char command[48];
  static size_t length = 0;
  static bool overflow = false;
  while (USBSerial.available()) {
    const char c = USBSerial.read();
    if (c == '\r') continue;
    if (c != '\n') {
      if (length < sizeof(command) - 1) command[length++] = c;
      else overflow = true;
      continue;
    }
    command[length] = '\0';
    if (overflow) {
      USBSerial.println("ORBIT ERROR: command too long");
    } else if (!strncmp(command, "TIME ", 5)) {
      char *end = nullptr;
      const unsigned long long epoch = strtoull(command + 5, &end, 10);
      if (end != command + 5 && *end == '\0' && epoch >= 1700000000ULL && epoch <= 4102444800ULL) {
        struct timeval now = {(time_t)epoch, 0};
        if (settimeofday(&now, nullptr) == 0) {
          usbTimeSet = true;
          refreshUi(nullptr);
          USBSerial.printf("ORBIT TIME OK: %llu\n", epoch);
        } else USBSerial.println("ORBIT ERROR: settimeofday failed");
      } else USBSerial.println("ORBIT ERROR: TIME requires UTC epoch seconds");
    } else if (!strcmp(command, "STATUS")) {
      USBSerial.printf("ORBIT STATUS: display=480x480 touch=%s imu=%s audio=%s audio_error=%d audio_plays=%lu dice=%u dice_count=%u dice_faces=%u dice_rolling=%u pixel_clock=1 style=retro_dot rotation=%u orientation=%s stable=%u particles=%d frame_ms=%lu pixels=%lu epoch=%lld free_heap=%u imu_samples=%lu imu_age_ms=%lu imu_read_errors=%lu accel=%.3f,%.3f,%.3f dice_armed=%u shake_g=%.3f finger=%u sand=%u sand_seconds=%u sand_left_ms=%lu sand_state=%u player=%u player_count=%u player_spinning=%u player_winner=%u\n",
                       touchOnline ? "OK" : "FAILED", motionOnline ? "OK" : "FAILED",
                       clock_audio::status(),clock_audio::lastError(),(unsigned long)clock_audio::playCount(),
                       diceMode?1:0,diceGame.count(),diceGame.faces(),diceGame.rolling()?1:0,pageRotation*90,
                       !automaticOrientation ? "LOCKED" : (pageOrientation.calibrated() ? "AUTO" : "NEEDS_CALIBRATION"),
                       pageOrientation.stable(millis()) ? 1 : 0,
                       0,(unsigned long)lastRenderMs,(unsigned long)lastRenderPixels,
                       (long long)time(nullptr), ESP.getFreeHeap(),(unsigned long)motionSampleCount,
                       (unsigned long)(motionSampleCount?millis()-motionSampleAt:0xffffffffU),
                       (unsigned long)motionReadErrors,motionX,motionY,motionZ,diceGame.armed()?1:0,
                       diceGame.shakeStrength(),touchDown?1:0,sandMode?1:0,sandTimer.seconds(),
                       (unsigned long)sandTimer.remainingMs(),unsigned(sandTimer.state()),playerMode?1:0,playerGame.players(),
                       playerGame.spinning()?1:0,playerGame.winner());
    } else if (length) {
      USBSerial.println("ORBIT COMMANDS: TIME <UTC epoch seconds>, STATUS");
    }
    length = 0;
    overflow = false;
  }
}
void setup() {
  USBSerial.begin(115200);
  pinMode(USER_BUTTON_PIN,INPUT_PULLUP);userButton.begin(digitalRead(USER_BUTTON_PIN)==LOW,millis());
  diceRandomSource=esp_random;storeDiceSettings=saveDiceSettings;loadDiceSettings();
  storeSandSeconds=saveSandSeconds;loadSandSeconds();
  storePlayerCount=savePlayerCount;loadPlayerCount();
  setenv("TZ", CLOCK_TIMEZONE, 1);
  tzset();
  touch.setPins(TP_RST, TP_INT);
  touchOnline = touch.begin(Wire, CST92XX_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (touchOnline) {
    touch.setMaxCoordinates(480, 480);
    touch.setSwapXY(true);
    touch.setMirrorXY(true, false);
  }
  // Reuse the address and setup verified by the official Widgets example.
  motionOnline = motionSensor.begin(Wire, QMI8658_L_SLAVE_ADDRESS, IIC_SDA, IIC_SCL);
  if (motionOnline) {
    motionOnline = motionSensor.configAccelerometer(SensorQMI8658::ACC_RANGE_8G,
                     SensorQMI8658::ACC_ODR_125Hz, SensorQMI8658::LPF_MODE_3) == 0
      && motionSensor.enableAccelerometer();
    motionGyroOnline = motionSensor.configGyroscope(SensorQMI8658::GYR_RANGE_512DPS,
                     SensorQMI8658::GYR_ODR_112_1Hz, SensorQMI8658::LPF_MODE_0) == 0
      && motionSensor.enableGyroscope();
  }
  if (!gfx->begin()) { USBSerial.println("Display init failed"); while (true) delay(1000); }
  gfx->setBrightness(180);
  bus->writeC8D8(0x36, 0xA0);
  lv_init();
  // One small DMA buffer: enough for 24 lines, no full-screen DMA allocation.
  const size_t pixelCount = LCD_WIDTH * 24;
  lv_color_t *pixels = (lv_color_t *)heap_caps_malloc(pixelCount * sizeof(lv_color_t), MALLOC_CAP_DMA);
  rotationBuffer = (lv_color_t *)heap_caps_malloc(pixelCount * sizeof(lv_color_t), MALLOC_CAP_DMA);
  if (!pixels || !rotationBuffer) { USBSerial.println("Display buffer allocation failed"); while (true) delay(1000); }
  lv_disp_draw_buf_init(&drawBuffer, pixels, nullptr, pixelCount);
  static lv_disp_drv_t display;
  lv_disp_drv_init(&display);
  display.hor_res = LCD_WIDTH; display.ver_res = LCD_HEIGHT;
  display.flush_cb = flushDisplay; display.rounder_cb = roundArea; display.draw_buf = &drawBuffer;
  display.sw_rotate = 0;
  display.monitor_cb = monitorDisplay;
  lv_disp_drv_register(&display);
  storeOrientationSettings=saveOrientationSettings;
  loadOrientationSettings();
  if (pageRotation) lv_disp_set_rotation(nullptr,(lv_disp_rot_t)pageRotation);
  static lv_indev_drv_t input;
  lv_indev_drv_init(&input);
  input.type = LV_INDEV_TYPE_POINTER; input.read_cb = readTouch;
  lv_indev_drv_register(&input);
  esp_timer_create_args_t args = {};
  args.callback = tick; args.name = "lvgl_tick";
  esp_timer_handle_t timer = nullptr;
  ESP_ERROR_CHECK(esp_timer_create(&args, &timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 2000));
  buildUi();
  refreshUi(nullptr);
  lv_timer_create(refreshUi, 100, nullptr);
  lv_timer_create(pixelAnimationTick, 60, nullptr);
  lv_timer_create(diceAnimationTick,40,nullptr);
  clock_audio::begin();
  USBSerial.printf("ORBIT READY: display=480x480 touch=%s imu=%s audio=%s audio_error=%d pixel_clock=1\n",
                   touchOnline ? "OK" : "FAILED", motionOnline ? "OK" : "FAILED",clock_audio::status(),clock_audio::lastError());
  USBSerial.println("ORBIT COMMANDS: TIME <UTC epoch seconds>, STATUS");
  if (strlen(CLOCK_WIFI_SSID)) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(CLOCK_WIFI_SSID, CLOCK_WIFI_PASSWORD);
    lastConnectAttempt = millis();
  }
}
void loop() {
  handleUserButton(digitalRead(USER_BUTTON_PIN)==LOW,millis());
  serviceConsole();
  serviceNetwork();
  serviceMotion();
  lv_timer_handler();
  delay(5);
}
