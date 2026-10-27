"""Compare the physical panel/touch boundary with the working Arduino drivers."""
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
BOARD=ROOT/'examples/esp-idf/06_OrbitClockAI/board'
LIB=ROOT/'examples/arduino/libraries'
COMPILER=shutil.which('clang++') or shutil.which('g++')

def function(source,signature):
    start=source.index(signature);opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

@unittest.skipUnless(COMPILER,'native C++ compiler required')
class PanelOrientationTests(unittest.TestCase):
    def run_cpp(self,code,headers=None):
        with tempfile.TemporaryDirectory() as directory:
            temp=Path(directory);(temp/'check.cpp').write_text(code)
            for name,text in (headers or {}).items():(temp/name).write_text(text)
            result=subprocess.run([COMPILER,'-std=c++17','-I'+str(temp),'-I'+str(BOARD),'-I'+str(ROOT/'examples/arduino/08_OrbitClock'),str(temp/'check.cpp'),'-o',str(temp/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(temp/'check')],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_actual_boot_madctl_matches_working_arduino_rotation_zero(self):
        gfx=LIB/'GFX_Library_for_Arduino/src/display'
        header=(gfx/'Arduino_CO5300.h').read_text()
        defines='\n'.join(line for line in header.splitlines() if re.match(r'#define CO5300_(?:W_MADCTL|MADCTL_)',line))
        rotation=function((gfx/'Arduino_CO5300.cpp').read_text(),'void Arduino_CO5300::setRotation(')
        board=(BOARD/'orbit_board.cc').read_text();start=board.index('static const co5300_lcd_init_cmd_t vendor_specific_init[]')
        commands=board[start:board.index('\n};',start)+3]
        self.run_cpp(r'''
#include "orbit_panel_orientation.h"
#include <cassert>
#include <cstddef>
#include <vector>
struct Bus{std::vector<std::pair<int,int>> writes;void beginWrite(){}void endWrite(){}void writeC8D8(int cmd,int value){writes.push_back({cmd,value});}};
struct Arduino_TFT{uint8_t _rotation=0;void setRotation(uint8_t r){_rotation=r;}};
struct Arduino_CO5300:Arduino_TFT{Bus *_bus;void setRotation(uint8_t);};
struct co5300_lcd_init_cmd_t{int cmd;const void *data;size_t size;unsigned delay;};
'''+defines+'\n'+rotation+'\n'+commands+r'''
int main(){
 Bus bus;Arduino_CO5300 reference;reference._bus=&bus;reference.setRotation(0);
 assert(bus.writes.size()==1&&bus.writes[0].first==0x36);
 int last=-1,writes=0;
 for(auto &command:vendor_specific_init)if(command.cmd==0x36){last=static_cast<const uint8_t*>(command.data)[0];++writes;}
 assert(writes==1&&last==bus.writes[0].second);
}
''')

    def test_raw_touch_matches_actual_sensorlib_and_stays_in_panel_bounds(self):
        implementation=function((LIB/'SensorLib/src/TouchDrvInterface.cpp').read_text(),'void TouchDrvInterface::updateXY(')
        setup=(ROOT/'examples/arduino/08_OrbitClock/08_OrbitClock.ino').read_text()
        settings='\n'.join(re.findall(r'\s*touch\.set(?:MaxCoordinates|SwapXY|MirrorXY)\([^;]+;',setup))
        self.run_cpp(r'''
#include "orbit_panel_orientation.h"
#include <cassert>
struct TouchDrvInterface{
 bool _swapXY=false,_mirrorX=false,_mirrorY=false;unsigned _xMax=0,_yMax=0;
 void setSwapXY(bool value){_swapXY=value;}
 void setMirrorXY(bool x,bool y){_mirrorX=x;_mirrorY=y;}
 void setMaxCoordinates(unsigned x,unsigned y){_xMax=x;_yMax=y;}
 void updateXY(uint8_t,int16_t*,int16_t*);
};
'''+implementation+r'''
int main(){TouchDrvInterface touch;
'''+settings+r'''
 for(int raw_x=0;raw_x<=480;++raw_x)for(int raw_y=0;raw_y<=480;++raw_y){
  int16_t x=raw_x,y=raw_y;touch.updateXY(1,&x,&y);
  const auto actual=orbit_panel::TouchToPanel(raw_x,raw_y);
  assert(actual.x==std::clamp(int(x),0,479)&&actual.y==std::clamp(int(y),0,479));
  assert(actual.x>=0&&actual.x<480&&actual.y>=0&&actual.y<480);
 }
 const auto outside=orbit_panel::TouchToPanel(4095,4095);assert(outside.x==0&&outside.y==479);
}
''')

    def test_software_rotation_has_no_later_hardware_mirror_override(self):
        for name in ('orbit_board.cc','orbit_display.cc'):
            self.assertNotRegex((BOARD/name).read_text(),r'esp_lcd_panel_(?:mirror|swap_xy)\s*\(')

    def test_buttons_up_applies_left_quarter_to_each_saved_or_automatic_rotation(self):
        self.run_cpp(r'''
#include <cassert>
#include "orbit_flush.h"
int main(){
 assert(orbit_panel::UPRIGHT_QUARTER==1);
 const lv_area_t source_area={10,20,11,22};
 const uint16_t source[]={1,2,3,4,5,6};uint16_t destination[6]={};
 const lv_area_t expected_areas[]={{20,468,22,469},{468,457,469,459},{457,10,459,11},{10,20,11,22}};
 const uint16_t expected_pixels[][6]={{2,4,6,1,3,5},{6,5,4,3,2,1},{5,3,1,6,4,2},{1,2,3,4,5,6}};
 for(int page=0;page<4;++page){
  const auto actual=OrbitPrepareStrip(source_area,source,destination,page);
  const auto expected=expected_areas[page];
  assert(actual.x1==expected.x1&&actual.y1==expected.y1&&actual.x2==expected.x2&&actual.y2==expected.y2);
  for(int i=0;i<6;++i)assert(__builtin_bswap16(destination[i])==expected_pixels[page][i]);
 }
}
''', {'lvgl.h':r'''
#pragma once
struct lv_area_t{int x1,y1,x2,y2;};
inline int lv_area_get_width(const lv_area_t* a){return a->x2-a->x1+1;}
inline int lv_area_get_height(const lv_area_t* a){return a->y2-a->y1+1;}
'''})

    def test_buttons_up_raw_touch_corners_and_all_pixels_align(self):
        self.run_cpp(r'''
#include <cassert>
#include "orbit_panel_orientation.h"
int main(){
 const auto top_left=orbit_panel::TouchToDisplay(479,480);
 const auto top_right=orbit_panel::TouchToDisplay(0,480);
 const auto bottom_left=orbit_panel::TouchToDisplay(479,1);
 const auto bottom_right=orbit_panel::TouchToDisplay(0,1);
 assert(top_left.x==0&&top_left.y==0);
 assert(top_right.x==479&&top_right.y==0);
 assert(bottom_left.x==0&&bottom_left.y==479);
 assert(bottom_right.x==479&&bottom_right.y==479);
 for(int y=0;y<480;++y)for(int x=0;x<480;++x){
  // The native panel point for a left quarter turn is (y,479-x).
  // CST raw coordinates are (native_y,480-native_x).
  const auto actual=orbit_panel::TouchToDisplay(479-x,480-y);
  assert(actual.x==x&&actual.y==y);
 }
}
''')

if __name__=='__main__':unittest.main()
