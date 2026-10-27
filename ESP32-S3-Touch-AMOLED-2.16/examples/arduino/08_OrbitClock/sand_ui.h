#pragma once
#include "sand_view.h"

static lv_obj_t *sandPanel=nullptr,*sandCanvas,*sandValue,*sandHint,*sandStartLabel;
static lv_obj_t *sandSettingsPanel=nullptr,*sandDraftValue,*sandDraftHint;
static lv_obj_t *sandAdjustButtons[4]={};
static uint16_t draftSandSeconds=60;
static bool sandTouchMoved=false,sandFrameReady=false;
static lv_point_t sandTouchOrigin;
static bool sandSettingsOpen(){return sandSettingsPanel!=nullptr;}

static lv_obj_t *sandButton(lv_obj_t *parent,int x,int y,int width,const char *caption,
                            lv_event_cb_t callback,void *data=nullptr) {
  auto *button=diceButton(parent,x,y,width,caption,callback,data);
  lv_obj_set_style_radius(button,8,0);lv_obj_set_style_bg_color(button,color(0x65472D),0);
  lv_obj_set_style_border_width(button,1,0);lv_obj_set_style_border_color(button,color(0x9E7845),0);
  lv_obj_set_style_text_color(lv_obj_get_child(button,0),color(0xF3D591),0);
  return button;
}
static void updateSandLabels() {
  if(!sandMode||!sandPanel)return;
  const unsigned seconds=sandTimer.remainingSeconds();
  char buffer[16];snprintf(buffer,sizeof(buffer),"%02u:%02u",seconds/60,seconds%60);text(sandValue,buffer);
  lv_obj_align(sandValue,LV_ALIGN_TOP_MID,0,326);
  const auto state=sandTimer.state();
  const char *hint=state==sand_timer::State::Running?"流沙落下，时间流动":state==sand_timer::State::Paused?"已暂停，轻点继续":
                   state==sand_timer::State::Done?"时间到":"轻点开始，让流沙落下";
  text(sandHint,hint);lv_obj_align(sandHint,LV_ALIGN_TOP_MID,0,72);
  const auto tint=color(state==sand_timer::State::Done?sand_view::GOLD:sand_view::MUTED);
  if(lv_obj_get_style_text_color(sandHint,LV_PART_MAIN).full!=tint.full)lv_obj_set_style_text_color(sandHint,tint,0);
  text(sandStartLabel,state==sand_timer::State::Running?"暂停":state==sand_timer::State::Paused?"继续":
                      state==sand_timer::State::Done?"再来一次":"开始");
  lv_obj_center(sandStartLabel);
}
static void rebuildSandFrame() {
  sand_view::build(diceFrame,sandTimer,millis(),!sandFrameReady);sandFrameReady=true;lv_obj_invalidate(sandCanvas);
}
static void onSandDraw(lv_event_t *event){sand_view::draw(diceFrame,lv_event_get_draw_ctx(event));}
static void sandAnimationTick() {
  if(playerMode){playerAnimationTick();return;}
  if(!sandMode||!sandPanel||sandSettingsPanel)return;
  const bool moving=sandTimer.state()==sand_timer::State::Running;
  sandTimer.update(millis());if(moving)rebuildSandFrame();updateSandLabels();
}
static void onSandStart(lv_event_t *){sandTimer.toggle(millis());rebuildSandFrame();updateSandLabels();}
static void onSandReset(lv_event_t *){sandTimer.reset();rebuildSandFrame();updateSandLabels();}
static void updateSandDraft() {
  char buffer[16];snprintf(buffer,sizeof(buffer),"%u",draftSandSeconds);text(sandDraftValue,buffer);
  lv_obj_align(sandDraftValue,LV_ALIGN_TOP_MID,0,130);
  text(sandDraftHint,"1 至 3600 秒");lv_obj_align(sandDraftHint,LV_ALIGN_TOP_MID,0,190);
  for(int i=0;i<4;++i) {
    const bool disabled=i%2?draftSandSeconds==sand_timer::MAX_SECONDS:draftSandSeconds==sand_timer::MIN_SECONDS;
    if(disabled)lv_obj_add_state(sandAdjustButtons[i],LV_STATE_DISABLED);
    else lv_obj_clear_state(sandAdjustButtons[i],LV_STATE_DISABLED);
  }
}
static void onSandAdjust(lv_event_t *event) {
  const int option=int(intptr_t(lv_event_get_user_data(event)));
  int delta=option<2?1:10;
  if(lv_event_get_code(event)==LV_EVENT_LONG_PRESSED_REPEAT)delta=option<2?10:60;
  if(option%2==0)delta=-delta;
  draftSandSeconds=uint16_t(std::max(1,std::min(3600,int(draftSandSeconds)+delta)));updateSandDraft();
}
static void onSandPreset(lv_event_t *event) {
  draftSandSeconds=uint16_t(intptr_t(lv_event_get_user_data(event)));updateSandDraft();
}
static void closeSandSettings() {
  if(sandSettingsPanel){lv_obj_del_async(sandSettingsPanel);sandSettingsPanel=nullptr;}
  sandTouchMoved=false;rebuildSandFrame();updateSandLabels();
}
static void onSandReturn(lv_event_t *){closeSandSettings();}
static void onSandSave(lv_event_t *) {
  if(storeSandSeconds&&!storeSandSeconds(draftSandSeconds)) {
    text(sandDraftHint,"保存失败，请重试");lv_obj_align(sandDraftHint,LV_ALIGN_TOP_MID,0,190);return;
  }
  sandTimer.configure(draftSandSeconds);closeSandSettings();
}
static void onSandPointer(lv_event_t *event) {
  auto *input=lv_indev_get_act();if(!input)return;lv_point_t p;lv_indev_get_point(input,&p);
  if(lv_event_get_code(event)==LV_EVENT_PRESSED){sandTouchOrigin=p;sandTouchMoved=false;}
  else if(abs(p.x-sandTouchOrigin.x)+abs(p.y-sandTouchOrigin.y)>14)sandTouchMoved=true;
}
static void onSandSettingsOpen(lv_event_t *) {
  if(!sandMode||playerMode||sandSettingsPanel||sandTouchMoved)return;
  sandTimer.pause(millis());rebuildSandFrame();updateSandLabels();draftSandSeconds=sandTimer.seconds();
  sandSettingsPanel=lv_obj_create(sandPanel);lv_obj_remove_style_all(sandSettingsPanel);
  lv_obj_set_pos(sandSettingsPanel,0,0);lv_obj_set_size(sandSettingsPanel,480,480);
  lv_obj_clear_flag(sandSettingsPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(sandSettingsPanel,color(sand_view::BACKGROUND),0);lv_obj_set_style_bg_opa(sandSettingsPanel,LV_OPA_COVER,0);
  auto *label=labelAt(sandSettingsPanel,"流沙设置",0,0,&orbit_font_zh_20,sand_view::GOLD);lv_obj_align(label,LV_ALIGN_TOP_MID,0,54);
  label=labelAt(sandSettingsPanel,"倒计时秒数",0,0,&orbit_font_zh_16,sand_view::MUTED);lv_obj_align(label,LV_ALIGN_TOP_MID,0,104);
  sandDraftValue=labelAt(sandSettingsPanel,"60",0,0,&lv_font_montserrat_48,sand_view::GOLD);
  sandDraftHint=labelAt(sandSettingsPanel,"",0,0,&orbit_font_zh_16,sand_view::MUTED);
  const char *captions[4]={"-1 秒","+1 秒","-10 秒","+10 秒"};
  for(int i=0;i<4;++i) {
    sandAdjustButtons[i]=sandButton(sandSettingsPanel,i%2?248:104,i<2?214:274,128,captions[i],onSandAdjust,(void*)intptr_t(i));
    lv_obj_add_event_cb(sandAdjustButtons[i],onSandAdjust,LV_EVENT_LONG_PRESSED_REPEAT,(void*)intptr_t(i));
  }
  const int seconds[3]={30,60,180};const char *presets[3]={"30 秒","60 秒","180 秒"};
  for(int i=0;i<3;++i)sandButton(sandSettingsPanel,104+i*96,326,80,presets[i],onSandPreset,(void*)intptr_t(seconds[i]));
  sandButton(sandSettingsPanel,104,386,128,"返回游戏",onSandReturn);
  sandButton(sandSettingsPanel,248,386,128,"保存并返回",onSandSave);updateSandDraft();
}
static void openSandMode() {
  if(!diceMode||diceSettingsPanel||sandMode)return;
  diceClicks.reset();diceGame.settings(true);sandMode=true;sandTouchMoved=false;sandFrameReady=false;
  sandPanel=lv_obj_create(dicePanel);lv_obj_remove_style_all(sandPanel);
  lv_obj_set_pos(sandPanel,0,0);lv_obj_set_size(sandPanel,480,480);lv_obj_clear_flag(sandPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(sandPanel,color(sand_view::BACKGROUND),0);lv_obj_set_style_bg_opa(sandPanel,LV_OPA_COVER,0);
  lv_obj_add_event_cb(sandPanel,onSandPointer,LV_EVENT_PRESSED,nullptr);
  lv_obj_add_event_cb(sandPanel,onSandPointer,LV_EVENT_PRESSING,nullptr);
  lv_obj_add_event_cb(sandPanel,onSandSettingsOpen,LV_EVENT_LONG_PRESSED,nullptr);
  auto *label=labelAt(sandPanel,"流沙倒计时",0,0,&orbit_font_zh_20,sand_view::GOLD);lv_obj_align(label,LV_ALIGN_TOP_MID,0,36);
  sandHint=labelAt(sandPanel,"",0,0,&orbit_font_zh_16,sand_view::MUTED);
  sandCanvas=lv_obj_create(sandPanel);lv_obj_remove_style_all(sandCanvas);
  lv_obj_set_pos(sandCanvas,110,92);lv_obj_set_size(sandCanvas,260,230);
  lv_obj_clear_flag(sandCanvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(sandCanvas,onSandDraw,LV_EVENT_DRAW_MAIN,nullptr);
  sandValue=labelAt(sandPanel,"01:00",0,0,&lv_font_montserrat_48,sand_view::GOLD);
  auto *start=sandButton(sandPanel,112,388,120,"开始",onSandStart);sandStartLabel=lv_obj_get_child(start,0);
  sandButton(sandPanel,248,388,120,"重置",onSandReset);
  label=labelAt(sandPanel,"单击返回  双击转盘",0,0,&orbit_font_zh_16,sand_view::MUTED);lv_obj_align(label,LV_ALIGN_TOP_MID,0,436);
  label=labelAt(sandPanel,"长按界面设置",0,0,&orbit_font_zh_16,sand_view::MUTED);lv_obj_align(label,LV_ALIGN_TOP_MID,0,457);
  rebuildSandFrame();updateSandLabels();
}
static void closeSandMode() {
  if(!sandMode)return;
  if(playerMode)closePlayerMode();
  diceClicks.reset();sandTimer.pause(millis());sandMode=false;sandSettingsPanel=nullptr;sandFrameReady=false;
  lv_obj_del(sandPanel);sandPanel=nullptr;diceGame.settings(false);rebuildDiceFrame();updateDiceLabels();
}
