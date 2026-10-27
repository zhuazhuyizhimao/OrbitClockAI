#!/usr/bin/env python3
"""Generate an LVGL 9 port from the maintained Arduino clock UI and assets."""
from pathlib import Path
import re
import shutil

REPO=Path(__file__).resolve().parents[1]
APP=REPO/'examples/arduino/08_OrbitClock'
BOARD=REPO.parent/'work/xiaozhi-upstream/main/boards/orbit-clock-ai'
PORT=BOARD/'orbit'


def convert_header(text):
    text=text.replace('lv_obj_set_style_opa(button,LV_OPA_40,LV_STATE_DISABLED);',
        'lv_obj_set_style_opa(button,LV_OPA_40,LV_STATE_DISABLED);'
        'lv_obj_set_style_opa(button,LV_OPA_70,LV_STATE_PRESSED);lv_obj_set_ext_click_area(button,6);')
    text=text.replace('const auto &clip=*context->clip_area','const auto &clip=context->_clip_area')
    text=text.replace('lv_obj_get_style_text_color(sandHint,LV_PART_MAIN).full!=tint.full',
                      '!lv_color_eq(lv_obj_get_style_text_color(sandHint,LV_PART_MAIN),tint)')
    replacements={
      'for(unsigned j=n;j>at;--j)sorted[j]=sorted[j-1];sorted[at]=point;++n;':
        'for(unsigned j=n;j>at;--j){sorted[j]=sorted[j-1];}sorted[at]=point;++n;',
      'if(prepared==players)return;prepared=players;':'if(prepared==players){return;}prepared=players;',
      'while(x<SIZE&&pixels[y*SIZE+x]==ink)++x;if(!ink)continue;':
        'while(x<SIZE&&pixels[y*SIZE+x]==ink){++x;}if(!ink)continue;',
      'if(moving)rebuildPlayerFrame();updatePlayerLabels();':'if(moving){rebuildPlayerFrame();}updatePlayerLabels();',
      'struct Grain { lv_coord_t x,y;':'struct Grain { int16_t x,y;',
      'grains.points[grains.count++]={(lv_coord_t)x,(lv_coord_t)y,':
        'grains.points[grains.count++]={(int16_t)x,(int16_t)y,',
    }
    for old,new in replacements.items():text=text.replace(old,new)
    return text.replace("context->clip_area","(&context->_clip_area)")


def convert_sand_input(text):
    """One stable press/release owns one action; holds never become clicks."""
    def replace_once(old,new):
        nonlocal text
        if text.count(old)!=1:raise ValueError('Sand adaptation no longer matches: '+old[:80])
        text=text.replace(old,new,1)

    replace_once('static lv_point_t sandTouchOrigin;', '\n'.join([
        '#include "orbit_sand_gesture.h"',
        'static orbit_sand::Gesture sandGesture;',
        'static lv_obj_t *sandResetButton=nullptr;',
        'static void bindSandMainInput(lv_obj_t *object);',
        'static void onSandSettingsOpen(lv_event_t *);']))
    replace_once(',*sandStartLabel;', ';')
    start=text.index('  text(sandStartLabel,');end=text.index('\n}',start)
    text=text[:start]+text[end:]
    replace_once('"流沙落下，时间流动"','"轻点暂停，长按 2 秒设置"')
    replace_once('"轻点开始，让流沙落下"','"轻点开始，长按 2 秒设置"')
    replace_once('  auto *start=sandButton(sandPanel,112,388,120,"开始",onSandStart);sandStartLabel=lv_obj_get_child(start,0);\n  sandButton(sandPanel,248,388,120,"重置",onSandReset);',
                 '  createSandReset();')
    replace_once('"长按界面设置"','"长按 2 秒设置"')
    replace_once('"单击返回  双击转盘"','"Key3 返回 / 双击转盘"')
    replace_once('static void onSandStart(lv_event_t *){sandTimer.toggle(millis());rebuildSandFrame();updateSandLabels();}', '')
    replace_once('static void onSandReset(lv_event_t *){sandTimer.reset();rebuildSandFrame();updateSandLabels();}', '\n'.join([
        'static void createSandReset() {',
        '  sandResetButton=lv_button_create(sandPanel);lv_obj_remove_style_all(sandResetButton);',
        '  lv_obj_set_pos(sandResetButton,104,388);lv_obj_set_size(sandResetButton,272,48);',
        '  lv_obj_clear_flag(sandResetButton,LV_OBJ_FLAG_SCROLLABLE|LV_OBJ_FLAG_EVENT_BUBBLE|LV_OBJ_FLAG_PRESS_LOCK);',
        '  lv_obj_set_ext_click_area(sandResetButton,0);',
        '  lv_obj_set_style_radius(sandResetButton,8,0);',
        '  lv_obj_set_style_bg_color(sandResetButton,color(0x65472D),0);',
        '  lv_obj_set_style_bg_opa(sandResetButton,LV_OPA_COVER,0);',
        '  lv_obj_set_style_border_width(sandResetButton,1,0);',
        '  lv_obj_set_style_border_color(sandResetButton,color(0x9E7845),0);',
        '  lv_obj_set_style_opa(sandResetButton,LV_OPA_70,LV_STATE_PRESSED);',
        '  auto *caption=labelAt(sandResetButton,"重置",0,0,&orbit_font_zh_20,0xF3D591);lv_obj_center(caption);',
        '  bindSandMainInput(sandResetButton);',
        '}']))
    start=text.index('static void onSandPointer(lv_event_t *event) {')
    end=text.index('static void onSandSettingsOpen(lv_event_t *) {',start)
    text=text[:start]+SAND_INPUT+text[end:]
    replace_once('  lv_obj_add_event_cb(sandPanel,onSandPointer,LV_EVENT_PRESSED,nullptr);\n  lv_obj_add_event_cb(sandPanel,onSandPointer,LV_EVENT_PRESSING,nullptr);\n  lv_obj_add_event_cb(sandPanel,onSandSettingsOpen,LV_EVENT_LONG_PRESSED,nullptr);',
                 '  sandGesture.Reset();bindSandMainInput(sandPanel);')
    replace_once('lv_obj_del(sandPanel);sandPanel=nullptr;',
                 'sandGesture.Cancel();lv_obj_del(sandPanel);sandPanel=nullptr;sandResetButton=nullptr;')
    if 'onSandStart(' in text or 'onSandReset(' in text:raise ValueError('Legacy sand callback remains')
    return text


SAND_INPUT = r"""static void applySandAction(orbit_sand::Action action) {
  if(!sandMode||playerMode||sandSettingsPanel)return;
  if(action==orbit_sand::Action::Settings) {
    onSandSettingsOpen(nullptr);
    if(auto *input=lv_indev_get_act())lv_indev_wait_release(input);
    return;
  }
  if(action==orbit_sand::Action::Toggle)sandTimer.toggle(millis());
  else if(action==orbit_sand::Action::Reset)sandTimer.reset();
  else return;
  rebuildSandFrame();updateSandLabels();
}
static void onSandPointer(lv_event_t *event) {
  if(!sandMode||playerMode||sandSettingsPanel)return;
  if(lv_event_get_target(event)!=lv_event_get_current_target(event))return;
  const auto code=lv_event_get_code(event);
  if(code==LV_EVENT_PRESS_LOST){sandGesture.Cancel();return;}
  auto *input=lv_indev_get_act();if(!input)return;
  lv_point_t p;lv_indev_get_point(input,&p);
  const bool reset=lv_event_get_target(event)==sandResetButton;
  if(code==LV_EVENT_PRESSED) {
    sandTouchMoved=false;
    sandGesture.Begin(reset?orbit_sand::Target::Reset:orbit_sand::Target::Page,p.x,p.y,millis());
  } else if(code==LV_EVENT_PRESSING)applySandAction(sandGesture.Move(p.x,p.y,millis()));
  else if(code==LV_EVENT_RELEASED) {
    lv_area_t bounds;lv_obj_get_coords(sandResetButton,&bounds);
    const bool inside=p.x>=bounds.x1&&p.x<=bounds.x2&&p.y>=bounds.y1&&p.y<=bounds.y2;
    applySandAction(sandGesture.End(p.x,p.y,millis(),reset==inside));
  }
}
static void bindSandMainInput(lv_obj_t *object) {
  for(auto code:{LV_EVENT_PRESSED,LV_EVENT_PRESSING,LV_EVENT_RELEASED,LV_EVENT_PRESS_LOST})
    lv_obj_add_event_cb(object,onSandPointer,code,nullptr);
}
"""


def clock_font():
    source=(REPO/'examples/arduino/libraries/lvgl/src/font/lv_font_montserrat_48.c').read_text()
    raw=source[source.index('glyph_bitmap[]'):source.index('static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[]')]
    raw=re.sub(r'/\*.*?\*/','',raw,flags=re.S)
    bitmap=bytes(int(x,16) for x in re.findall(r'0x([0-9a-fA-F]+)',raw))
    table=source[source.index('static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[]'):source.index('/*---------------------\n *  CHARACTER MAPPING')]
    descriptors=[dict((name,int(value)) for name,value in re.findall(r'\.(\w+)\s*=\s*(-?\d+)',row)) for row in re.findall(r'\{([^{}]+)\}',table)]
    chars=sorted(set('0123456789:-'));data=bytearray();entries=['{0,0,0,0,0,0}']
    for char in chars:
        d=descriptors[ord(char)-32+1];start=len(data);w=d['box_w'];h=d['box_h'];values=[]
        for y in range(h*2):
            for x in range(w*2):
                offset=(y//2)*w+x//2;byte=bitmap[d['bitmap_index']+offset//2]
                values.append((byte>>(0 if offset&1 else 4))&15)
        for i in range(0,len(values),2):data.append(values[i]<<4|(values[i+1] if i+1<len(values) else 0))
        entries.append('{'+','.join(str(v) for v in [start,d['adv_w']*2,w*2,h*2,d['ofs_x']*2,d['ofs_y']*2])+'}')
    content='#include "orbit_fonts.h"\n'
    content+='static const uint8_t clock_bits[]={'+','.join(hex(b) for b in data)+'};\n'
    content+='static const lv_font_fmt_txt_glyph_dsc_t clock_glyphs[]={'+','.join(entries)+'};\n'
    content+='static const uint16_t clock_codes[]={'+','.join(str(ord(c)-ord(chars[0])) for c in chars)+'};\n'
    content+='static const lv_font_fmt_txt_cmap_t clock_map[]={{.range_start='+str(ord(chars[0]))+',.range_length='+str(ord(chars[-1])-ord(chars[0])+1)+',.glyph_id_start=1,.unicode_list=clock_codes,.list_length='+str(len(chars))+',.type=LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}};\n'
    content+='static const lv_font_fmt_txt_dsc_t clock_description={.glyph_bitmap=clock_bits,.glyph_dsc=clock_glyphs,.cmaps=clock_map,.cmap_num=1,.bpp=4};\n'
    content+='const lv_font_t orbit_font_clock_96={.get_glyph_dsc=lv_font_get_glyph_dsc_fmt_txt,.get_glyph_bitmap=lv_font_get_bitmap_fmt_txt,.line_height=104,.base_line=18,.subpx=LV_FONT_SUBPX_NONE,.static_bitmap=1,.dsc=&clock_description};\n'
    return content


def main():
    PORT.mkdir(parents=True,exist_ok=True)
    headers=['clock_audio_model.h','clock_audio.h','orbit_fonts.h','pixel_clock_model.h','retro_clock_face.h','retro_assets.h','coin_glyphs.h',
             'dice_game_model.h','dice_geometry.h','dice_view.h','dice_ui.h','sand_timer_model.h','sand_view.h','sand_ui.h',
             'player_game_model.h','player_view.h','player_ui.h']
    for name in headers:
        text=convert_header((APP/name).read_text())
        if name=='sand_ui.h':text=convert_sand_input(text)
        (PORT/name).write_text(text)
    with (PORT/'orbit_fonts.h').open('a') as file:file.write('\n#ifdef __cplusplus\nextern "C" {\n#endif\nextern const lv_font_t orbit_font_clock_96;\n#ifdef __cplusplus\n}\n#endif\n')
    fonts=(APP/'orbit_fonts.c').read_text()
    fonts=re.sub(r'static lv_font_fmt_txt_glyph_cache_t cache_\d+;\n','',fonts)
    fonts=re.sub(r', \.cache = &cache_\d+','',fonts)
    fonts=fonts.replace('.fallback =', '.static_bitmap = 1, .fallback =')
    (BOARD/'orbit_fonts.c').write_text(fonts+clock_font())
    assets=(APP/'retro_assets.c').read_text()
    def image_header(match):
        w,h=map(int,match.groups())
        return '.header = {.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_I4,.w='+str(w)+',.h='+str(h)+',.stride='+str((w+1)//2)+'}'
    assets=re.sub(r'\.header = \{\.cf=LV_IMG_CF_INDEXED_4BIT,\.always_zero=0,\.reserved=0,\.w=(\d+),\.h=(\d+)\}',image_header,assets)
    (BOARD/'retro_assets.c').write_text(assets)
    # Preserve the same UI handlers/model headers; adapt only platform and LVGL APIs.
    ino=(APP/'08_OrbitClock.ino').read_text()
    ui=ino[ino.index('static lv_obj_t *screen'):ino.index('static void flushDisplay')]
    start=ui.index('// Enlarge only');end=ui.index('static lv_color_t color',start)
    ui=ui[:start]+'static lv_font_t clockFont;\n'+ui[end:]
    ui=re.sub(r'clockFont=lv_font_montserrat_48;clockFont.get_glyph_dsc=clockGlyphDescriptor;clockFont.get_glyph_bitmap=clockGlyphBitmap;\n  clockFont.line_height\*=2;clockFont.base_line\*=2;clockFont.fallback=nullptr;',
              'clockFont=orbit_font_clock_96;',ui)
    ui=ui.replace('screen=lv_scr_act();','screen=lv_obj_create(lv_screen_active());lv_obj_remove_style_all(screen);lv_obj_set_size(screen,480,480);')
    ui=ui.replace('lv_disp_set_rotation(nullptr,(lv_disp_rot_t)pageRotation);','orbit_clock_ai::SetRotation(pageRotation);')
    ui=ui.replace('static void onSettingsOpen(lv_event_t *){','static void onSettingsAi(lv_event_t *){closeSettings();orbit_clock_ai::OpenConversation();}\nstatic void onSettingsOpen(lv_event_t *){')
    ui=ui.replace('menuButton(164,"专注计时",onSettingsFocus);menuButton(230,"校准方向",onSettingsCalibration);',
                  'menuButton(146,"专注计时",onSettingsFocus);menuButton(206,"校准方向",onSettingsCalibration);')
    ui=ui.replace('menuButton(296,"锁定当前方向",onSettingsLock);menuButton(362,"返回时钟",onSettingsClose);',
                  'menuButton(266,"锁定当前方向",onSettingsLock);auto *ai=menuButton(326,"小智 AI",onSettingsAi);lv_obj_set_style_text_font(lv_obj_get_child(ai,0),&orbit_font_ai_20,0);menuButton(386,"返回时钟",onSettingsClose);')
    ui=ui.replace('LV_ALIGN_TOP_MID,0,432);','LV_ALIGN_TOP_MID,0,448);')
    ui=convert_header(ui)
    saves=ino[ino.index('static bool saveDiceSettings'):ino.index('static void serviceNetwork')]
    motion=ino[ino.index('static void serviceMotion'):ino.index('void setup()')].replace('ESP.getFreeHeap()','esp_get_free_heap_size()')
    status_end='playerGame.spinning()?1:0,playerGame.winner());'
    if motion.count(status_end)!=1:raise ValueError('Clock STATUS command no longer matches')
    motion=motion.replace(status_end,status_end+'''
      USBSerial.printf("ORBIT TOUCH: ui=sand_stable_v4 reads=%u contacts=%u errors=%u last_error=%d raw=%u,%u\\n",
                       orbit_touch::read_count,orbit_touch::contact_reports,orbit_touch::read_errors,
                       int(orbit_touch::last_error),orbit_touch::raw_x,orbit_touch::raw_y);''')
    prefix='''// Generated from 08_OrbitClock; edit its maintained UI or the port generator.
#include "orbit_platform.h"
#include "orbit_touch_reader.h"
#include "orbit_ai_font.h"
#include "orbit_lvgl_compat.h"
#include "orbit_fonts.h"
#include "retro_assets.h"
#include "retro_clock_face.h"
#include "pixel_clock_model.h"
#include "clock_audio.h"
#include "dice_game_model.h"
#include "sand_timer_model.h"
#include "player_game_model.h"
#include <initializer_list>
#define CLOCK_TIMEZONE "CST-8"
#define CLOCK_WIFI_SSID "xiaozhi-shared"
static bool touchOnline=true,touchDown=false,motionOnline=false,motionGyroOnline=false;
static constexpr uint8_t USER_BUTTON_PIN=18;
static SensorQMI8658 motionSensor;
'''
    (BOARD/'orbit_ui.cc').write_text(prefix+ui+saves+motion+'\n#include "orbit_ui_runtime.h"\n')
    sensor=REPO/'examples/arduino/libraries/SensorLib/src'
    shutil.copytree(sensor,BOARD/'sensorlib',dirs_exist_ok=True)
    # The bundled library predates newer optional fields in i2c_device_config_t.
    # Zero the whole struct in the generated port; retain the vendor source.
    i2c=BOARD/'sensorlib/platform/espidf/SensorCommEspIDF_I2C.hpp'
    i2c.write_text(i2c.read_text().replace('i2c_device_config_t devConf;','i2c_device_config_t devConf={};'))
    print('Generated shared clock/game UI, LVGL 9 fonts/images and SensorLib port.')


if __name__=='__main__':main()
