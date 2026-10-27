#!/usr/bin/env python3
"""Render and exercise the combined LVGL 9 clock/game UI on the host. Run orbit_ai.py build first to fetch LVGL."""
from pathlib import Path
from concurrent.futures import ThreadPoolExecutor
import subprocess,re,sys
import orbit_ai
repo=Path(__file__).resolve().parents[1];base=repo.parent;app=repo/'examples/arduino/08_OrbitClock'
task_lock=orbit_ai.acquire_lock();orbit_ai.prepare()
board=base/'work/xiaozhi-upstream/main/boards/orbit-clock-ai';lv=base/'work/xiaozhi-upstream/managed_components/lvgl__lvgl';out=base/'work/lvgl9-check';out.mkdir(exist_ok=True)
conf=(lv/'lv_conf_template.h').read_text().replace('#if 0 /* Set this to "1" to enable content */','#if 1 /* Native validation */')
conf=conf.replace('(64 * 1024U)','(2 * 1024 * 1024U)')
for name in ('LV_FONT_MONTSERRAT_16','LV_FONT_MONTSERRAT_20','LV_FONT_MONTSERRAT_32','LV_FONT_MONTSERRAT_48','LV_FONT_FMT_TXT_LARGE'):
 conf=re.sub(r'(#define '+name+r'\s+)\d+',r'\g<1>1',conf)
conf=re.sub(r'(#define LV_USE_FONT_COMPRESSED\s+)\d+',r'\g<1>0',conf)
if not (out/'lv_conf.h').exists() or (out/'lv_conf.h').read_text()!=conf:
 for obj in out.glob('*.o'):obj.unlink()
 (out/'lv_conf.h').write_text(conf)
prefix='''
#include "orbit_lvgl_compat.h"
#include "orbit_flush.h"
#include "orbit_panel_orientation.h"
#include "orbit_network_state.h"
#include "orbit_fonts.h"
#include "orbit_ai_font.h"
#include "retro_assets.h"
#include "retro_clock_face.h"
#include "pixel_clock_model.h"
#include "clock_audio.h"
#include "clock_audio_model.h"
#include "dice_game_model.h"
#include "sand_timer_model.h"
#include "player_game_model.h"
#include "orbit_touch_filter.h"
#include "orbit_touch_calibration_ui.h"
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cassert>
#define CLOCK_WIFI_SSID ""
#define WL_CONNECTED 3
static bool touchOnline=true,touchDown=false,motionOnline=true;
static uint64_t simulatedUs=0;
static uint64_t esp_timer_get_time(){return simulatedUs;}
static int millis(){return simulatedUs/1000;}
static bool filterSimulatedInput=false,missingSimulatedReport=false;
static orbit_touch::ContactFilter simulatedContactFilter;
static struct {int status(){return 0;}} WiFi;
static clock_audio::SingleFlight simulatedAudio; static unsigned audioStarts=0,audioAttempts=0;
namespace clock_audio { bool play(){ ++audioAttempts; if(!simulatedAudio.request())return false; ++audioStarts;return true; } }
static time_t currentEpoch=0;
static time_t fakeTime(time_t *out){if(out)*out=currentEpoch;return currentEpoch;}
#define time fakeTime
namespace orbit_clock_ai {static unsigned aiOpenRequests=0;void OpenConversation(){++aiOpenRequests;}void SetRotation(uint8_t quarter){lv_display_set_rotation(lv_display_get_default(),(lv_display_rotation_t)(quarter&3));}}
'''
reader=(board/'orbit_touch_reader.h').read_text()
configure_input=reader[reader.index('inline void ConfigureInput('):reader.rindex('}  // namespace orbit_touch')]
prefix+='namespace orbit_touch {\n'+configure_input+'}\n'
generated=(board/'orbit_ui.cc').read_text();ui=generated[generated.index('static lv_obj_t *screen'):generated.index('static bool saveDiceSettings')]
test=(repo/'examples/esp-idf/06_OrbitClockAI/tests/native_ui.cpp').read_text().replace('lv_color_t frame[','uint16_t frame[').replace('static lv_color_t ','static uint16_t ').replace('lv_indev_drv_t','lv_indev_t')
test=test.replace('click(240,320)','click(240,290)').replace('click(240,254)','click(240,230)')
test=test.replace('lv_event_send(', 'lv_obj_send_event(')
test=test.replace('static void click(int x,int y){',
    'static void click(int x,int y){if(sandMode&&!playerMode&&!sandSettingsPanel){simulatedUs+=500000;lv_tick_inc(500);lv_timer_handler();}')
# The maintained harness predates the combined sand page's tap-anywhere layout.
test=test.replace('click(172,412)','click(240,220)')
test=test.replace('for(int i=0;i<20;++i)touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);assert(sandSettingsPanel);',
                  'for(int i=0;i<55;++i)touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);assert(sandSettingsPanel);')
# Feed native pointer input through the board's raw CST9217 normalization before
# LVGL applies its inverse display rotation, as on the physical board.
test=test.replace('*data=simulatedInput;', '*data=simulatedInput;const auto native=orbit_pixels::rotateToPanel(simulatedInput.point.x,simulatedInput.point.y,orbit_panel::UPRIGHT_QUARTER);const auto p=orbit_panel::TouchToDisplay(native.y,480-native.x);data->point={p.x,p.y};')
test=test.replace('data->point={p.x,p.y};', '''data->point={p.x,p.y};
 if(filterSimulatedInput){
  const auto report=missingSimulatedReport?orbit_touch::Report::Missing:
    data->state==LV_INDEV_STATE_PRESSED?orbit_touch::Report::Contact:orbit_touch::Report::Released;
  const auto sample=simulatedContactFilter.Update(report,p.x,p.y,millis());
  data->point={sample.x,sample.y};data->state=sample.down?LV_INDEV_STATE_PRESSED:LV_INDEV_STATE_RELEASED;
 }''')
test=test.replace('uint32_t rgb=lv_color_to32(c);','uint32_t rgb=((c>>11)*255/31)<<16|(((c>>5)&63)*255/63)<<8|((c&31)*255/31);')
test=test.replace('lv_color_hex(0x111C2B).full','lv_color_to_u16(lv_color_hex(0x111C2B))').replace('.full','')
start=test.index(' static uint16_t buffer[480*24]');end=test.index(' simulatedAudio.enable()',start)
test=test[:start]+''' static uint16_t buffer[480*24];
 auto *d=lv_display_create(480,480);lv_display_set_color_format(d,LV_COLOR_FORMAT_RGB565);
 lv_display_set_buffers(d,buffer,nullptr,sizeof(buffer),LV_DISPLAY_RENDER_MODE_PARTIAL);lv_display_set_flush_cb(d,flushDisplay);
 lv_display_add_event_cb(d,[](lv_event_t *event){auto *a=static_cast<lv_area_t*>(lv_event_get_param(event));a->x1&=~1;a->y1&=~1;a->x2|=1;a->y2|=1;},LV_EVENT_INVALIDATE_AREA,nullptr);
 auto *input=lv_indev_create();lv_indev_set_type(input,LV_INDEV_TYPE_POINTER);lv_indev_set_read_cb(input,readInput);
 lv_indev_set_mode(input,LV_INDEV_MODE_EVENT);orbit_touch::ConfigureInput(input);
 assert(lv_indev_get_mode(input)==LV_INDEV_MODE_TIMER&&!lv_timer_get_paused(lv_indev_get_read_timer(input)));
''' +test[end:]
flush='''
static void flushDisplay(lv_display_t *display,const lv_area_t *area,uint8_t *pixels){
 static uint16_t transferred[480*24];
 const auto a=OrbitPrepareStrip(*area,reinterpret_cast<uint16_t*>(pixels),transferred,lv_display_get_rotation(display));
 const uint16_t *p=transferred;
 // Record the native panel transfer, then view it with the physical buttons up.
 // The nonzero mounting correction is tested separately against explicit native
 // corner expectations, so UI/input tests retain a human-facing framebuffer.
 for(int y=a.y1;y<=a.y2;++y)for(int x=a.x1;x<=a.x2;++x){
  const auto viewed=orbit_panel::PanelToDisplay({x,y});
  frame[viewed.y*480+viewed.x]=__builtin_bswap16(*p++);
 }
 lv_display_flush_ready(display);
}
'''
runtime=(board/'orbit_ui_runtime.h').read_text()
start=runtime.index('void SetVoiceVisible(');end=runtime.index('void PollUi(',start)
bridge='static bool voiceWasVisible=false,key3WasDown=false,key3HoldUsed=false;\nnamespace orbit_clock_ai {\n'+runtime[start:end]+'}\n'
network_header=(board/'orbit_time_sync.h').read_text()
notice_text=network_header[network_header.index('inline const char* NetworkNoticeText('):network_header.rindex('}  // namespace orbit_clock_ai')]
bridge+='namespace orbit_clock_ai {\n'+notice_text+'}\n'
extra=r"""
 // Exercise every reported small control through the rotated input path, with
 // releases between changing positions and a mix of normal finger dwell times.
 auto tapFor=[](int x,int y,int held){
  if(sandMode&&!playerMode&&!sandSettingsPanel)advanceDice(500);
  auto p=orbit_pixels::rotateToPanel(x,y,pageRotation);touchAt(p.x,p.y,true);
  for(int i=0;i<held;++i)touchAt(p.x,p.y,true);
  touchAt(p.x,p.y,false);advanceDice(40);
 };
 for(int r=0;r<4;++r){
  setPageRotation(r);sandTimer.configure(60);keyClick();keyDouble();assert(sandMode);
  unsigned buttons=0;
  for(unsigned i=0;i<lv_obj_get_child_count(sandPanel);++i){
   auto *child=lv_obj_get_child(sandPanel,i);
   if(lv_obj_check_type(child,&lv_button_class)){
    ++buttons;assert(lv_obj_get_width(child)==272);
    assert(!strcmp(lv_label_get_text(lv_obj_get_child(child,0)),"重置"));
   }
  }
  assert(buttons==1); // The separate start button is gone.
  // Reported right-hand hot zones must only toggle the page, never reset time.
  sandTimer.toggle(millis());advanceDice(5000);
  const int rightSide[][2]={{360,160},{400,200},{412,240},{400,300},{360,320},
                            {100,412},{380,412},{240,384},{240,440}};
  for(const auto &point:rightSide){
   const auto before=sandTimer.remainingMs();const auto previous=sandTimer.state();
   assert(before<56000&&before>0);
   tapFor(point[0],point[1],0);
   assert(sandTimer.state()==(previous==sand_timer::State::Running?sand_timer::State::Paused:sand_timer::State::Running));
   assert(sandTimer.remainingMs()<=before&&before-sandTimer.remainingMs()<1000);
  }
  tapFor(240,412,0);assert(sandTimer.state()==sand_timer::State::Idle);
  // A reset child event may never also toggle the page, even if bubbling is enabled.
  lv_obj_add_flag(sandResetButton,LV_OBJ_FLAG_EVENT_BUBBLE);
  tapFor(240,412,0);assert(sandTimer.state()==sand_timer::State::Idle);
  lv_obj_remove_flag(sandResetButton,LV_OBJ_FLAG_EVENT_BUBBLE);
  tapFor(240,220,14);assert(sandTimer.state()==sand_timer::State::Running);
  tapFor(312,412,0);assert(sandTimer.state()==sand_timer::State::Idle);
  // A real finger can drift a few pixels during a stationary long press.
  auto p=orbit_pixels::rotateToPanel(240,210,pageRotation);touchAt(p.x,p.y,true);
  for(int i=0;i<55;++i){p=orbit_pixels::rotateToPanel(240+(i%2?14:0),210+(i%2?4:0),pageRotation);touchAt(p.x,p.y,true);}
  touchAt(p.x,p.y,false);advanceDice(40);
  assert(sandSettingsPanel&&"sand long press must tolerate a small finger drift");
  tapFor(168,410,0);assert(!sandSettingsPanel);
  // Holds under two seconds are clicks; a two-second hold consumes the release.
  tapFor(240,220,46);assert(!sandSettingsPanel&&sandTimer.state()==sand_timer::State::Running);
  advanceDice(1000);assert(sandTimer.remainingMs()<60000);
  tapFor(80,240,14);assert(sandTimer.state()==sand_timer::State::Paused);
  const auto paused=sandTimer.remainingMs();advanceDice(1000);assert(sandTimer.remainingMs()==paused);
  tapFor(240,346,0);assert(sandTimer.state()==sand_timer::State::Running);
  tapFor(312,412,14);assert(sandTimer.state()==sand_timer::State::Idle&&sandTimer.remainingSeconds()==60);
  // The visible hourglass, countdown, labels, blank area and reset accept a hold.
  const int holdPoints[][2]={{240,46},{240,180},{240,346},{70,240},{240,463},{172,412},{312,412}};
  for(const auto &point:holdPoints){
   tapFor(point[0],point[1],55);assert(sandSettingsPanel);
   tapFor(168,410,0);assert(!sandSettingsPanel&&sandTimer.state()==sand_timer::State::Idle);
  }
  // A drag is neither a settings hold nor a button click, even inside the button.
  p=orbit_pixels::rotateToPanel(240,220,pageRotation);touchAt(p.x,p.y,true);
  p=orbit_pixels::rotateToPanel(280,220,pageRotation);for(int i=0;i<55;++i)touchAt(p.x,p.y,true);
  touchAt(p.x,p.y,false);advanceDice(40);assert(!sandSettingsPanel&&sandTimer.state()==sand_timer::State::Idle);
  p=orbit_pixels::rotateToPanel(172,412,pageRotation);touchAt(p.x,p.y,true);
  p=orbit_pixels::rotateToPanel(400,380,pageRotation);touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);
  advanceDice(40);assert(!sandSettingsPanel&&sandTimer.state()==sand_timer::State::Idle);
  // Both ends of the reset button reset without bubbling a toggle to the page.
  for(int x:{110,240,370}){
   tapFor(240,220,0);assert(sandTimer.state()==sand_timer::State::Running);
   tapFor(x,412,0);assert(sandTimer.state()==sand_timer::State::Idle&&sandTimer.remainingSeconds()==60);
  }
  sandTimer.configure(1);tapFor(240,220,0);advanceDice(1000);assert(sandTimer.state()==sand_timer::State::Done);
  tapFor(240,220,14);assert(sandTimer.state()==sand_timer::State::Running);
  keyClick();assert(diceMode&&!sandMode);keyClick();assert(!diceMode);
 }
 puts("PASS: sand tap-anywhere toggle, two-second settings hold, single wide reset, drift/drag handling, restart and Key3 through four rotations");
 for(int r=0;r<4;++r){
  setPageRotation(r);keyClick();keyDouble();sandTimer.configure(60);sandGesture.Reset();
  auto rawTap=[](){auto p=orbit_pixels::rotateToPanel(240,220,pageRotation);touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);};
  rawTap();assert(sandTimer.state()==sand_timer::State::Running);
  rawTap();assert(sandTimer.state()==sand_timer::State::Running); // duplicate tap under 500ms
  advanceDice(500);rawTap();assert(sandTimer.state()==sand_timer::State::Paused);
  filterSimulatedInput=true;simulatedContactFilter.Reset();sandTimer.reset();sandGesture.Reset();
  auto p=orbit_pixels::rotateToPanel(240,220,pageRotation);
  touchAt(p.x,p.y,true);
  for(int i=0;i<55;++i){
   missingSimulatedReport=i%25>=3&&i%25<=23;
   touchAt(p.x,p.y,i%25!=1); // sparse reports plus a single false release
   if(i<48)assert(!sandSettingsPanel&&sandTimer.state()==sand_timer::State::Idle);
  }
  missingSimulatedReport=false;assert(sandSettingsPanel&&sandTimer.state()==sand_timer::State::Idle);
  for(int i=0;i<4;++i)touchAt(p.x,p.y,false);
  filterSimulatedInput=false;tapFor(168,410,0);assert(!sandSettingsPanel);
  // A noisy short tap is one transition after a stable release, not two clicks.
  filterSimulatedInput=true;simulatedContactFilter.Reset();sandGesture.Reset();
  touchAt(p.x,p.y,true);touchAt(p.x,p.y,false);touchAt(p.x,p.y,true);
  assert(sandTimer.state()==sand_timer::State::Idle);
  for(int i=0;i<4;++i)touchAt(p.x,p.y,false);
  assert(sandTimer.state()==sand_timer::State::Running);
  for(int i=0;i<4;++i)touchAt(p.x,p.y,false);
  assert(sandTimer.state()==sand_timer::State::Running);
  filterSimulatedInput=false;keyClick();keyClick();assert(!diceMode);
 }
 puts("PASS: actual LVGL sand input bridges invalid packets/release bounce, enforces 500ms new-press gap, and enters settings after a two-second noisy hold in all rotations");
 for(int r=0;r<4;++r){
  setPageRotation(r);diceGame.configure(1,6);sandTimer.configure(60);playerGame.configure(4);
  keyClick();gameLongPress();assert(diceSettingsPanel);
  auto p=orbit_pixels::rotateToPanel(348,176,pageRotation);touchAt(p.x,p.y,true);
  assert(lv_obj_has_state(diceCountPlus,LV_STATE_PRESSED));
  assert(lv_obj_get_style_opa(diceCountPlus,LV_PART_MAIN)==LV_OPA_70);
  touchAt(p.x,p.y,false);assert(draftDiceCount==2);
  tapFor(132,176,4);assert(draftDiceCount==1);
  tapFor(348,276,1);assert(draftDiceFaces==7);
  tapFor(132,276,4);assert(draftDiceFaces==6);
  tapFor(312,386,4);assert(!diceSettingsPanel&&diceGame.count()==1&&diceGame.faces()==6);
  gameLongPress();tapFor(348,176,1);tapFor(168,386,4);assert(!diceSettingsPanel&&diceGame.count()==1);
  keyDouble();assert(sandMode);
  tapFor(240,220,4);assert(sandTimer.state()==sand_timer::State::Running);
  tapFor(240,220,1);assert(sandTimer.state()==sand_timer::State::Paused);
  tapFor(312,412,0);assert(sandTimer.remainingSeconds()==60);
  sandLongPress();tapFor(312,238,0);assert(draftSandSeconds==61);
  tapFor(168,238,4);assert(draftSandSeconds==60);
  tapFor(312,298,1);assert(draftSandSeconds==70);
  tapFor(168,298,4);assert(draftSandSeconds==60);
  tapFor(144,350,1);assert(draftSandSeconds==30);
  tapFor(240,350,4);assert(draftSandSeconds==60);
  tapFor(336,350,0);assert(draftSandSeconds==180);
  tapFor(312,410,4);assert(!sandSettingsPanel&&sandTimer.seconds()==180);
  sandLongPress();tapFor(144,350,1);tapFor(168,410,4);assert(!sandSettingsPanel&&sandTimer.seconds()==180);
  keyDouble();assert(playerMode);playerLongPress();
  tapFor(132,190,1);assert(draftPlayers==3);tapFor(348,190,4);assert(draftPlayers==4);
  for(int i=0;i<4;++i){tapFor(132+i*72,310,i%2?4:0);assert(draftPlayers==2+i*2);}
  tapFor(312,410,4);assert(!playerSettingsPanel&&playerGame.players()==8);
  playerLongPress();tapFor(348,190,1);tapFor(168,410,4);assert(!playerSettingsPanel&&playerGame.players()==8);
  tapFor(240,424,1);assert(playerGame.spinning());keyTap();assert(sandMode&&!playerMode);
  keyClick();assert(diceMode&&!sandMode);keyClick();assert(!diceMode);
 }
 setPageRotation(0);
 puts("PASS: every dice/sand/player adjust, preset, start, save and return button through four rotations, released position changes and pressed feedback");
 setPageRotation(0);if(diceMode)toggleDiceMode();if(focusMode)onMode(nullptr);
 openMenu();const auto aiBefore=orbit_clock_ai::aiOpenRequests;click(240,350);advanceDice(40);
 assert(!settingsPanel&&orbit_clock_ai::aiOpenRequests==aiBefore+1);
 for(uint32_t c:{uint32_t(0x5C0F),uint32_t(0x667A)}) {lv_font_glyph_dsc_t g={};assert(lv_font_get_glyph_dsc(&orbit_font_ai_20,&g,c,0)&&!g.is_placeholder);}
 onMode(nullptr);setFocusMinutes(1);onStart(nullptr);
 orbit_clock_ai::SetVoiceVisible(true);assert(lv_obj_has_flag(screen,LV_OBJ_FLAG_HIDDEN));
 simulatedUs+=1500000;updateRemaining();orbit_clock_ai::SetVoiceVisible(false);
 assert(focusRunning&&remainingUs<60000000ULL&&!lv_obj_has_flag(screen,LV_OBJ_FLAG_HIDDEN));onReset(nullptr);onMode(nullptr);
 keyClick();settleDice();shakeDice();orbit_clock_ai::SetVoiceVisible(true);assert(!diceGame.rolling());
 orbit_clock_ai::SetVoiceVisible(false);assert(diceMode&&!sandMode);
 keyDouble();click(240,220);advanceDice(500);assert(sandTimer.state()==sand_timer::State::Running);
 orbit_clock_ai::SetVoiceVisible(true);const auto remaining=sandTimer.remainingMs();simulatedUs+=5000000;
 orbit_clock_ai::SetVoiceVisible(false);assert(sandMode&&sandTimer.remainingMs()==remaining&&sandTimer.state()==sand_timer::State::Paused);
 keyDouble();click(240,220);assert(playerGame.spinning());const auto players=playerGame.players();
 orbit_clock_ai::SetVoiceVisible(true);assert(!playerGame.spinning());orbit_clock_ai::SetVoiceVisible(false);
 assert(playerMode&&playerGame.players()==players);
 printf("PASS: AI menu glyphs and actual dispatch; voice overlay retains pages, pauses sand, cancels rolling/spinning and keeps focus countdown running\n");
 static orbit_touch::Sample calSample;
 static orbit_touch::Mapping measuredMapping;
 static bool saveCalibrationOK=true;static unsigned calibrationSaves=0;
 orbit_touch_calibration::Configure(+[](){return calSample;},+[](const orbit_touch::Mapping &m){
  ++calibrationSaves;if(!saveCalibrationOK)return false;measuredMapping=m;return true;
 });
 setPageRotation(3);calSample={true,300,300};orbit_touch_calibration::Open();
 orbit_touch_calibration::Poll(millis());calSample.down=false;orbit_touch_calibration::Poll(millis());
 assert(orbit_touch_calibration::calibrator.Count()==0);
 assert(orbit_touch_calibration::Active()&&lv_display_get_rotation(lv_display_get_default())==LV_DISPLAY_ROTATION_0);
 save("touch-calibration.ppm");
 auto measuredTap=[](int x,int y){
  for(int i=0;i<3;++i){calSample={true,x,y};simulatedUs+=40000;orbit_touch_calibration::Poll(millis());}
  calSample.down=false;simulatedUs+=40000;orbit_touch_calibration::Poll(millis());
 };
 for(auto target:orbit_touch::Calibrator::TARGETS)measuredTap(479-target.y,480-target.x);
 assert(calibrationSaves==1&&measuredMapping.Valid());
 auto right=measuredMapping.Apply(239,60);assert(right.x==420&&right.y==240);
 simulatedUs+=1200000;orbit_touch_calibration::Poll(millis());
 assert(!orbit_touch_calibration::Active()&&lv_display_get_rotation(lv_display_get_default())==LV_DISPLAY_ROTATION_270);
 orbit_touch_calibration::Open();measuredTap(100,100);measuredTap(100,100);measuredTap(100,100);
 assert(orbit_touch_calibration::Active()&&orbit_touch_calibration::calibrator.Count()==0&&calibrationSaves==1);
 saveCalibrationOK=false;
 for(auto target:orbit_touch::Calibrator::TARGETS)measuredTap(479-target.y,480-target.x);
 assert(calibrationSaves==2&&orbit_touch_calibration::Active()&&orbit_touch_calibration::calibrator.Count()==0);
 orbit_touch_calibration::Cancel();setPageRotation(0);advanceDice(40);
 puts("PASS: actual calibration overlay accepts raw samples despite swapped logical axes, restores rotation, and retains previous mapping on bad samples/save failure");
"""
last=test.rfind('\n}');test=test[:last]+extra+test[last:]
# Render the actual network notice setup/update code and verify it never steals
# input, fits the display and expires. No fake Wi-Fi success is needed for UI QA.
notice_setup=runtime[runtime.index('    networkNoticeLabel = lv_label_create'):runtime.index('    clock_audio::begin();')]
notice_update=runtime[runtime.index('    if (notice != NetworkNotice::None) {'):runtime.index('    const bool down =')]
network_check='''
 using namespace orbit_clock_ai;
 static lv_obj_t* networkNoticeLabel=nullptr;static uint32_t networkNoticeAt=0;
'''+notice_setup+'''
 for(auto notice:{NetworkNotice::Starting,NetworkNotice::Connecting,NetworkNotice::Config,
                  NetworkNotice::Syncing,NetworkNotice::Synced,NetworkNotice::SyncFailed,NetworkNotice::Offline,NetworkNotice::RetryFailed,NetworkNotice::ManualRequested}) {
'''+notice_update+'''
  lv_obj_update_layout(networkNoticeLabel);
  assert(!lv_obj_has_flag(networkNoticeLabel,LV_OBJ_FLAG_HIDDEN));
  assert(!lv_obj_has_flag(networkNoticeLabel,LV_OBJ_FLAG_CLICKABLE));
  assert(!strcmp(lv_label_get_text(networkNoticeLabel),NetworkNoticeText(notice)));
  assert(lv_obj_get_x(networkNoticeLabel)>=0&&lv_obj_get_x(networkNoticeLabel)+lv_obj_get_width(networkNoticeLabel)<=480);
  assert(lv_obj_get_y(networkNoticeLabel)+lv_obj_get_height(networkNoticeLabel)<240);
 }
 {auto notice=NetworkNotice::None;simulatedUs+=3500000;
'''+notice_update+'''
  assert(lv_obj_has_flag(networkNoticeLabel,LV_OBJ_FLAG_HIDDEN));}
 puts("PASS: Chinese network notices fit display, do not intercept touch and expire");
'''
last=test.rfind('\n}');test=test[:last]+network_check+test[last:]
split=test.index('static void save(');code=prefix+ui+bridge+test[:split]+flush+test[split:]
points=','.join(str(ord(c)) for c in sorted({c for c in generated.replace('小智 AI','')+''.join((board/'orbit'/n).read_text() for n in ('dice_ui.h','sand_ui.h','player_ui.h')) if 0x3000<=ord(c)<=0xffff}))
check='for(uint32_t point: {'+points+'}) { lv_font_glyph_dsc_t g={}; assert(lv_font_get_glyph_dsc(&orbit_font_zh_16,&g,point,0)&&!g.is_placeholder); assert(lv_font_get_glyph_dsc(&orbit_font_zh_20,&g,point,0)&&!g.is_placeholder); }'
network_points=','.join(str(ord(c)) for c in sorted(set(re.findall(r'[\u3000-\uffff]',network_header))))
network_points=','.join(str(ord(c)) for c in sorted(set(re.findall(r'[\u3000-\uffff]',network_header+(board/'orbit_touch_calibration_ui.h').read_text()))))
check+='for(uint32_t point: {'+network_points+'}) {lv_font_glyph_dsc_t g={}; assert(lv_font_get_glyph_dsc(&orbit_font_ai_20,&g,point,0)&&!g.is_placeholder);}'
code=code.replace('FONT_CHECK',check);(out/'main.cpp').write_text(code)
flags=['-O0','-DLV_CONF_INCLUDE_SIMPLE','-I'+str(out),'-I'+str(lv),'-I'+str(board),'-I'+str(board/'orbit')]
def compile_one(source):
 obj=out/(str(source.relative_to(lv)).replace('/','_')+'.o')
 if not obj.exists():
  result=subprocess.run(['clang',*flags,'-c',str(source),'-o',str(obj)],capture_output=True,text=True)
  if result.returncode:raise RuntimeError(result.stderr)
 return str(obj)
with ThreadPoolExecutor(max_workers=8) as pool:objects=list(pool.map(compile_one,(lv/'src').rglob('*.c')))
for name in ('orbit_fonts.c','retro_assets.c','orbit_ai_font.c'):
 obj=out/(name+'.o');subprocess.run(['clang',*flags,'-c',str(board/name),'-o',str(obj)],check=True);objects.append(str(obj))
subprocess.run(['clang++','-std=c++17',*flags,str(out/'main.cpp'),*objects,'-o',str(out/'check')],check=True)
subprocess.run([str(out/'check')],cwd=out,check=True,timeout=90)
