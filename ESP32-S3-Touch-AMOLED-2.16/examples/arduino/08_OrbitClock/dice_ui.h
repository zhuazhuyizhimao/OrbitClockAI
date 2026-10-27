#pragma once
#include "dice_view.h"

static lv_obj_t *dicePanel=nullptr,*diceCanvas,*diceCaption,*diceHint,*diceTotal,*diceTotalTitle;
static lv_obj_t *diceSettingsPanel=nullptr,*diceCountLabel,*diceFacesLabel,*diceConfigHint;
static lv_obj_t *diceCountMinus,*diceCountPlus,*diceFacesMinus,*diceFacesPlus;
static uint8_t draftDiceCount=1,draftDiceFaces=6;
static bool diceTouchMoved=false;
static lv_point_t diceTouchOrigin;
static dice_view::Frame diceFrame;
static const char *diceShapeName(uint8_t faces) {
  switch(faces) {
    case 2:return "正反面";
    case 3:return "三棱骰（3 面计分）";case 4:return "四面体";case 5:return "五面体";
    case 6:return "六面体";case 7:return "七面体";default:return "八面体";
  }
}

static void updateDiceLabels() {
  if(!diceMode||!dicePanel)return;
  char buffer[64];snprintf(buffer,sizeof(buffer),"%u 颗 / %s",diceGame.count(),diceShapeName(diceGame.faces()));
  text(diceCaption,buffer);lv_obj_align(diceCaption,LV_ALIGN_TOP_MID,0,88);
  const char *hint=!motionOnline?"运动传感器未就绪":diceGame.rolling()?"骰子转动中...":
    !motionSampleCount||uint32_t(millis()-motionSampleAt)>750?"未收到运动数据":
    !diceGame.armed()?"请先停稳板子":diceGame.hasResult()?"再次晃动可重新掷骰":"晃动板子开始掷骰";
  text(diceHint,hint);lv_obj_align(diceHint,LV_ALIGN_TOP_MID,0,334);
  const bool coin=diceGame.faces()==2;
  text(diceTotalTitle,coin?"投掷结果":"总点数");
  lv_obj_align(diceTotalTitle,LV_ALIGN_TOP_MID,0,356);
  const auto *font=coin?&orbit_font_zh_32:&lv_font_montserrat_48;
  if(lv_obj_get_style_text_font(diceTotal,LV_PART_MAIN)!=font)lv_obj_set_style_text_font(diceTotal,font,0);
  if(diceGame.hasResult()&&coin)snprintf(buffer,sizeof(buffer),"%s",diceGame.total()==1?"正面":"反面");
  else if(diceGame.hasResult())snprintf(buffer,sizeof(buffer),"%u",diceGame.total());
  else snprintf(buffer,sizeof(buffer),"--");
  text(diceTotal,buffer);lv_obj_align(diceTotal,LV_ALIGN_TOP_MID,0,376);
}
static void rebuildDiceFrame() {
  dice_view::build(diceFrame,diceGame,millis());lv_obj_invalidate(diceCanvas);
}
static void onDiceDraw(lv_event_t *event) { dice_view::draw(diceFrame,lv_event_get_draw_ctx(event)); }
static void diceAnimationTick(lv_timer_t *) {
  if(!diceMode||!dicePanel)return;
  if(sandMode){sandAnimationTick();return;}
  const bool wasRolling=diceGame.rolling();
  if(diceRandomSource)diceGame.tick(millis(),diceRandomSource);
  if(wasRolling)rebuildDiceFrame();
  updateDiceLabels();
}
static void feedDiceMotion(float x,float y,float z,uint32_t now) {
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) { ++motionReadErrors;return; }
  motionX=x;motionY=y;motionZ=z;motionSampleAt=now;++motionSampleCount;
  if(diceGame.sample(x,y,z,now,touchDown)) { rebuildDiceFrame();updateDiceLabels(); }
}
static lv_obj_t *diceButton(lv_obj_t *parent,int x,int y,int width,const char *caption,
                            lv_event_cb_t callback,void *data=nullptr) {
  auto *button=lv_btn_create(parent);lv_obj_remove_style_all(button);
  lv_obj_set_pos(button,x,y);lv_obj_set_size(button,width,48);lv_obj_clear_flag(button,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(button,10,0);lv_obj_set_style_bg_color(button,color(0xF0CF72),0);
  lv_obj_set_style_bg_opa(button,LV_OPA_COVER,0);lv_obj_set_style_opa(button,LV_OPA_40,LV_STATE_DISABLED);
  auto *captionLabel=labelAt(button,caption,0,0,&orbit_font_zh_20,0x111C2B);lv_obj_center(captionLabel);
  lv_obj_add_event_cb(button,callback,LV_EVENT_SHORT_CLICKED,data);return button;
}
static void updateDiceDraft() {
  if(draftDiceFaces==2)draftDiceCount=1;
  char value[12];snprintf(value,sizeof(value),"%u",draftDiceCount);text(diceCountLabel,value);
  snprintf(value,sizeof(value),"%u",draftDiceFaces);text(diceFacesLabel,value);
  lv_obj_align(diceCountLabel,LV_ALIGN_TOP_MID,0,155);lv_obj_align(diceFacesLabel,LV_ALIGN_TOP_MID,0,255);
  char hint[64];
  if(draftDiceFaces==2)snprintf(hint,sizeof(hint),"正反两面 · 数量固定为 1");
  else snprintf(hint,sizeof(hint),"%s · 点数 1 至 %u",diceShapeName(draftDiceFaces),draftDiceFaces);
  text(diceConfigHint,hint);lv_obj_align(diceConfigHint,LV_ALIGN_TOP_MID,0,320);
  const bool disabled[4]={draftDiceCount==1||draftDiceFaces==2,draftDiceCount==3||draftDiceFaces==2,draftDiceFaces==2,draftDiceFaces==8};
  lv_obj_t *buttons[4]={diceCountMinus,diceCountPlus,diceFacesMinus,diceFacesPlus};
  for(int i=0;i<4;++i) {
    if(disabled[i])lv_obj_add_state(buttons[i],LV_STATE_DISABLED);
    else lv_obj_clear_state(buttons[i],LV_STATE_DISABLED);
  }
}
static void onDiceAdjust(lv_event_t *event) {
  const int option=int(intptr_t(lv_event_get_user_data(event)));
  if(option==1&&draftDiceFaces!=2&&draftDiceCount>1)--draftDiceCount;
  if(option==2&&draftDiceFaces!=2&&draftDiceCount<3)++draftDiceCount;
  if(option==3&&draftDiceFaces>2)--draftDiceFaces;
  if(option==4&&draftDiceFaces<8)++draftDiceFaces;
  updateDiceDraft();
}
static void closeDiceSettings() {
  if(diceSettingsPanel) { lv_obj_del_async(diceSettingsPanel);diceSettingsPanel=nullptr; }
  diceGame.settings(false);diceTouchMoved=false;rebuildDiceFrame();updateDiceLabels();
}
static void onDiceCancel(lv_event_t *) { closeDiceSettings(); }
static void onDiceSave(lv_event_t *) {
  if(storeDiceSettings&&!storeDiceSettings(draftDiceCount,draftDiceFaces)) {
    text(diceConfigHint,"保存失败，请重试");lv_obj_align(diceConfigHint,LV_ALIGN_TOP_MID,0,320);return;
  }
  diceGame.configure(draftDiceCount,draftDiceFaces);closeDiceSettings();
}
static void onDicePointer(lv_event_t *event) {
  auto *input=lv_indev_get_act();if(!input)return;
  lv_point_t point;lv_indev_get_point(input,&point);
  if(lv_event_get_code(event)==LV_EVENT_PRESSED) { diceTouchOrigin=point;diceTouchMoved=false; }
  else if(abs(point.x-diceTouchOrigin.x)+abs(point.y-diceTouchOrigin.y)>14)diceTouchMoved=true;
}
static void onDiceSettingsOpen(lv_event_t *) {
  if(!diceMode||diceSettingsPanel||diceTouchMoved)return;
  diceGame.settings(true);rebuildDiceFrame();
  draftDiceCount=diceGame.count();draftDiceFaces=diceGame.faces();
  diceSettingsPanel=lv_obj_create(dicePanel);lv_obj_remove_style_all(diceSettingsPanel);
  lv_obj_set_pos(diceSettingsPanel,0,0);lv_obj_set_size(diceSettingsPanel,480,480);
  lv_obj_clear_flag(diceSettingsPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(diceSettingsPanel,color(0x111C2B),0);
  lv_obj_set_style_bg_opa(diceSettingsPanel,LV_OPA_COVER,0);
  auto *label=labelAt(diceSettingsPanel,"骰子设置",0,0,&orbit_font_zh_20,0xF0CF72);
  lv_obj_align(label,LV_ALIGN_TOP_MID,0,56);
  label=labelAt(diceSettingsPanel,"骰子数量",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,120);
  diceCountLabel=labelAt(diceSettingsPanel,"1",0,0,&lv_font_montserrat_32,0xF2E6C7);
  diceCountMinus=diceButton(diceSettingsPanel,100,152,64,"-",onDiceAdjust,(void*)1);
  diceCountPlus=diceButton(diceSettingsPanel,316,152,64,"+",onDiceAdjust,(void*)2);
  label=labelAt(diceSettingsPanel,"骰子面数",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,220);
  diceFacesLabel=labelAt(diceSettingsPanel,"6",0,0,&lv_font_montserrat_32,0xF2E6C7);
  diceFacesMinus=diceButton(diceSettingsPanel,100,252,64,"-",onDiceAdjust,(void*)3);
  diceFacesPlus=diceButton(diceSettingsPanel,316,252,64,"+",onDiceAdjust,(void*)4);
  diceConfigHint=labelAt(diceSettingsPanel,"",0,0,&orbit_font_zh_16,0xA5BBC4);
  diceButton(diceSettingsPanel,104,362,128,"取消",onDiceCancel);
  diceButton(diceSettingsPanel,248,362,128,"保存",onDiceSave);
  label=labelAt(diceSettingsPanel,"按键返回时钟",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,432);
  updateDiceDraft();
}
static void toggleDiceMode() {
  diceClicks.reset();
  if(playerMode){closePlayerMode();return;}
  if(sandMode){closeSandMode();return;}
  if(diceMode) {
    diceMode=false;diceGame.leave();diceSettingsPanel=nullptr;
    lv_obj_del_async(dicePanel);dicePanel=nullptr;
    pageOrientation.hold(pageRotation,millis());refreshUi(nullptr);return;
  }
  if(settingsPanel)closeSettings();
  if(orientationCalibrating)onCalibrationCancel(nullptr);
  if(focusMode)onMode(nullptr);
  bouncing=false;lv_obj_set_y(characterImage,10);
  diceMode=true;diceTouchMoved=false;diceGame.enter();pageOrientation.hold(pageRotation,millis());
  dicePanel=lv_obj_create(screen);lv_obj_remove_style_all(dicePanel);
  lv_obj_set_pos(dicePanel,0,0);lv_obj_set_size(dicePanel,480,480);lv_obj_clear_flag(dicePanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(dicePanel,color(0x111C2B),0);lv_obj_set_style_bg_opa(dicePanel,LV_OPA_COVER,0);
  lv_obj_add_event_cb(dicePanel,onDicePointer,LV_EVENT_PRESSED,nullptr);
  lv_obj_add_event_cb(dicePanel,onDicePointer,LV_EVENT_PRESSING,nullptr);
  lv_obj_add_event_cb(dicePanel,onDiceSettingsOpen,LV_EVENT_LONG_PRESSED,nullptr);
  auto *label=labelAt(dicePanel,"骰子小游戏",0,0,&orbit_font_zh_20,0xF0CF72);lv_obj_align(label,LV_ALIGN_TOP_MID,0,50);
  diceCaption=labelAt(dicePanel,"",0,0,&orbit_font_zh_16,0xA5BBC4);
  diceCanvas=lv_obj_create(dicePanel);lv_obj_remove_style_all(diceCanvas);
  lv_obj_set_pos(diceCanvas,40,110);lv_obj_set_size(diceCanvas,400,224);
  lv_obj_clear_flag(diceCanvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(diceCanvas,onDiceDraw,LV_EVENT_DRAW_MAIN,nullptr);
  diceHint=labelAt(dicePanel,"",0,0,&orbit_font_zh_16,0x85D8BE);
  diceTotalTitle=labelAt(dicePanel,"总点数",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(diceTotalTitle,LV_ALIGN_TOP_MID,0,356);
  diceTotal=labelAt(dicePanel,"--",0,0,&lv_font_montserrat_48,0xF0CF72);
  label=labelAt(dicePanel,"单击返回  双击流沙",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,436);
  label=labelAt(dicePanel,"长按界面设置",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,457);
  rebuildDiceFrame();updateDiceLabels();
}
static void handleUserButton(bool pressed,uint32_t now) {
  const bool released=userButton.update(pressed,now);
  if(playerMode) {diceClicks.reset();if(released)closePlayerMode();return;}
  if(sandMode) {
    if(sandSettingsOpen()){diceClicks.reset();if(released)closeSandMode();return;}
    const auto click=diceClicks.update(released,userButton.down(),now);
    if(click==dice_game::Click::Double)openPlayerMode();
    else if(click==dice_game::Click::Single)closeSandMode();
    return;
  }
  if(diceMode&&!diceSettingsPanel) {
    const auto click=diceClicks.update(released,userButton.down(),now);
    if(click==dice_game::Click::Double)openSandMode();
    else if(click==dice_game::Click::Single)toggleDiceMode();
    return;
  }
  diceClicks.reset();if(released)toggleDiceMode();
}
