#pragma once
#include "player_view.h"

static lv_obj_t *playerPanel=nullptr,*playerCanvas,*playerCaption,*playerResult,*playerHint,*playerStartButton,*playerStartLabel;
static lv_obj_t *playerSettingsPanel=nullptr,*playerDraftValue,*playerDraftHint,*playerMinus,*playerPlus;
static uint8_t draftPlayers=4;
static bool playerTouchMoved=false;
static lv_point_t playerTouchOrigin;
static void rebuildPlayerFrame(){player_view::build(playerGame,millis());lv_obj_invalidate(playerCanvas);}
static void onPlayerDraw(lv_event_t *event){player_view::draw(lv_event_get_draw_ctx(event),playerGame.players(),playerGame.winner());}
static void updatePlayerLabels() {
  if(!playerMode||!playerPanel)return;
  char buffer[64];snprintf(buffer,sizeof(buffer),"%u 位玩家 / 公平转盘",playerGame.players());text(playerCaption,buffer);
  lv_obj_align(playerCaption,LV_ALIGN_TOP_MID,0,80);
  if(playerGame.winner())snprintf(buffer,sizeof(buffer),"玩家 %u 先开始",playerGame.winner());
  else snprintf(buffer,sizeof(buffer),"%s",playerGame.spinning()?"谁会先开始？":"轻点转盘开始");
  text(playerResult,buffer);lv_obj_align(playerResult,LV_ALIGN_TOP_MID,0,350);
  text(playerHint,playerGame.slowing(millis())?"正在减速...":playerGame.spinning()?"指针转动中...":"长按界面设置人数");
  lv_obj_align(playerHint,LV_ALIGN_TOP_MID,0,375);
  if(playerGame.spinning())lv_obj_add_state(playerStartButton,LV_STATE_DISABLED);
  else lv_obj_clear_state(playerStartButton,LV_STATE_DISABLED);
  text(playerStartLabel,playerGame.spinning()?"转动中":playerGame.winner()?"再转一次":"开始");lv_obj_center(playerStartLabel);
}
static void playerAnimationTick() {
  if(!playerMode||!playerPanel||playerSettingsPanel)return;
  const bool moving=playerGame.spinning();playerGame.update(millis());
  if(moving)rebuildPlayerFrame();updatePlayerLabels();
}
static void onPlayerStart(lv_event_t *) {
  if(playerSettingsPanel||playerTouchMoved||!diceRandomSource)return;
  if(playerGame.start(millis(),diceRandomSource)){rebuildPlayerFrame();updatePlayerLabels();}
}
static void onPlayerPointer(lv_event_t *event) {
  auto *input=lv_indev_get_act();if(!input)return;lv_point_t p;lv_indev_get_point(input,&p);
  if(lv_event_get_code(event)==LV_EVENT_PRESSED){playerTouchOrigin=p;playerTouchMoved=false;}
  else if(abs(p.x-playerTouchOrigin.x)+abs(p.y-playerTouchOrigin.y)>14)playerTouchMoved=true;
}
static void updatePlayerDraft() {
  char value[8];snprintf(value,sizeof(value),"%u",draftPlayers);text(playerDraftValue,value);
  lv_obj_align(playerDraftValue,LV_ALIGN_TOP_MID,0,158);
  text(playerDraftHint,"2 至 12 人");lv_obj_align(playerDraftHint,LV_ALIGN_TOP_MID,0,235);
  if(draftPlayers==player_game::MIN_PLAYERS)lv_obj_add_state(playerMinus,LV_STATE_DISABLED);else lv_obj_clear_state(playerMinus,LV_STATE_DISABLED);
  if(draftPlayers==player_game::MAX_PLAYERS)lv_obj_add_state(playerPlus,LV_STATE_DISABLED);else lv_obj_clear_state(playerPlus,LV_STATE_DISABLED);
}
static void onPlayerAdjust(lv_event_t *event) {
  const int change=int(intptr_t(lv_event_get_user_data(event)));
  draftPlayers=uint8_t(std::max(int(player_game::MIN_PLAYERS),std::min(int(player_game::MAX_PLAYERS),int(draftPlayers)+change)));
  updatePlayerDraft();
}
static void onPlayerPreset(lv_event_t *event){draftPlayers=uint8_t(intptr_t(lv_event_get_user_data(event)));updatePlayerDraft();}
static void closePlayerSettings() {
  if(playerSettingsPanel){lv_obj_del_async(playerSettingsPanel);playerSettingsPanel=nullptr;}
  playerTouchMoved=false;rebuildPlayerFrame();updatePlayerLabels();
}
static void onPlayerReturn(lv_event_t *){closePlayerSettings();}
static void onPlayerSave(lv_event_t *) {
  if(storePlayerCount&&!storePlayerCount(draftPlayers)) {
    text(playerDraftHint,"保存失败，请重试");lv_obj_align(playerDraftHint,LV_ALIGN_TOP_MID,0,235);return;
  }
  playerGame.configure(draftPlayers);closePlayerSettings();
}
static void onPlayerSettingsOpen(lv_event_t *) {
  if(!playerMode||playerSettingsPanel||playerTouchMoved)return;
  playerGame.cancel(millis());rebuildPlayerFrame();updatePlayerLabels();draftPlayers=playerGame.players();
  playerSettingsPanel=lv_obj_create(playerPanel);lv_obj_remove_style_all(playerSettingsPanel);
  lv_obj_set_pos(playerSettingsPanel,0,0);lv_obj_set_size(playerSettingsPanel,480,480);lv_obj_clear_flag(playerSettingsPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(playerSettingsPanel,color(0x111C2B),0);lv_obj_set_style_bg_opa(playerSettingsPanel,LV_OPA_COVER,0);
  auto *label=labelAt(playerSettingsPanel,"玩家设置",0,0,&orbit_font_zh_20,0xF0CF72);lv_obj_align(label,LV_ALIGN_TOP_MID,0,60);
  label=labelAt(playerSettingsPanel,"玩家人数",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,120);
  playerDraftValue=labelAt(playerSettingsPanel,"4",0,0,&lv_font_montserrat_48,0xF0CF72);
  playerMinus=diceButton(playerSettingsPanel,100,166,64,"-",onPlayerAdjust,(void*)-1);
  playerPlus=diceButton(playerSettingsPanel,316,166,64,"+",onPlayerAdjust,(void*)1);
  lv_obj_add_event_cb(playerMinus,onPlayerAdjust,LV_EVENT_LONG_PRESSED_REPEAT,(void*)-1);
  lv_obj_add_event_cb(playerPlus,onPlayerAdjust,LV_EVENT_LONG_PRESSED_REPEAT,(void*)1);
  playerDraftHint=labelAt(playerSettingsPanel,"",0,0,&orbit_font_zh_16,0xA5BBC4);
  for(int i=0;i<4;++i) {
    const char *captions[]={"2 人","4 人","6 人","8 人"};
    diceButton(playerSettingsPanel,104+i*72,286,56,captions[i],onPlayerPreset,(void*)intptr_t(2+i*2));
  }
  diceButton(playerSettingsPanel,104,386,128,"返回游戏",onPlayerReturn);
  diceButton(playerSettingsPanel,248,386,128,"保存并返回",onPlayerSave);updatePlayerDraft();
}
static void openPlayerMode() {
  if(!sandMode||sandSettingsPanel||playerMode)return;
  diceClicks.reset();sandTimer.pause(millis());rebuildSandFrame();updateSandLabels();
  playerMode=true;playerTouchMoved=false;
  playerPanel=lv_obj_create(sandPanel);lv_obj_remove_style_all(playerPanel);
  lv_obj_set_pos(playerPanel,0,0);lv_obj_set_size(playerPanel,480,480);lv_obj_clear_flag(playerPanel,LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(playerPanel,color(0x111C2B),0);lv_obj_set_style_bg_opa(playerPanel,LV_OPA_COVER,0);
  lv_obj_add_event_cb(playerPanel,onPlayerPointer,LV_EVENT_PRESSED,nullptr);
  lv_obj_add_event_cb(playerPanel,onPlayerPointer,LV_EVENT_PRESSING,nullptr);
  lv_obj_add_event_cb(playerPanel,onPlayerSettingsOpen,LV_EVENT_LONG_PRESSED,nullptr);
  lv_obj_add_event_cb(playerPanel,onPlayerStart,LV_EVENT_SHORT_CLICKED,nullptr);
  auto *label=labelAt(playerPanel,"开始玩家",0,0,&orbit_font_zh_20,0xF0CF72);lv_obj_align(label,LV_ALIGN_TOP_MID,0,44);
  playerCaption=labelAt(playerPanel,"",0,0,&orbit_font_zh_16,0xA5BBC4);
  playerCanvas=lv_obj_create(playerPanel);lv_obj_remove_style_all(playerCanvas);
  lv_obj_set_pos(playerCanvas,120,104);lv_obj_set_size(playerCanvas,240,244);
  lv_obj_clear_flag(playerCanvas,LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);lv_obj_add_event_cb(playerCanvas,onPlayerDraw,LV_EVENT_DRAW_MAIN,nullptr);
  playerResult=labelAt(playerPanel,"",0,0,&orbit_font_zh_20,0xF0CF72);
  playerHint=labelAt(playerPanel,"",0,0,&orbit_font_zh_16,0xA5BBC4);
  playerStartButton=diceButton(playerPanel,160,400,160,"开始",onPlayerStart);playerStartLabel=lv_obj_get_child(playerStartButton,0);
  lv_obj_add_event_cb(playerStartButton,onPlayerPointer,LV_EVENT_PRESSED,nullptr);
  lv_obj_add_event_cb(playerStartButton,onPlayerPointer,LV_EVENT_PRESSING,nullptr);
  lv_obj_add_event_cb(playerStartButton,onPlayerSettingsOpen,LV_EVENT_LONG_PRESSED,nullptr);
  label=labelAt(playerPanel,"按键返回沙漏",0,0,&orbit_font_zh_16,0xA5BBC4);lv_obj_align(label,LV_ALIGN_TOP_MID,0,452);
  rebuildPlayerFrame();updatePlayerLabels();
}
static void closePlayerMode() {
  if(!playerMode)return;
  diceClicks.reset();playerGame.cancel(millis());playerMode=false;playerSettingsPanel=nullptr;
  lv_obj_del(playerPanel);playerPanel=nullptr;
  rebuildSandFrame();updateSandLabels();
}
