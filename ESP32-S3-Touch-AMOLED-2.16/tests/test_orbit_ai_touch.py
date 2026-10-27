"""Exercise the pinned vendor decoder with an ACK-gated touch-report device."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
BOARD=ROOT/'examples/esp-idf/06_OrbitClockAI/board'
DRIVER=ROOT.parent/'work/xiaozhi-upstream/managed_components/waveshare__esp_lcd_touch_cst9217/esp_lcd_touch_cst9217.c'
COMPILER=shutil.which('clang++') or shutil.which('g++')

def function(source,signature):
    start=source.index(signature);opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]


@unittest.skipUnless(COMPILER and DRIVER.exists(),'prepare pinned touch component and native compiler')
class TouchReaderTests(unittest.TestCase):
    def test_ack_advances_reports_release_errors_and_timer_polling(self):
        source=DRIVER.read_text()
        # Skip the forward declarations and use the unchanged vendor definitions.
        start=source.index('static esp_err_t esp_lcd_touch_cst9217_read_data(',source.index(' * Private functions\n'))
        driver_read=function(source[start:],'static esp_err_t esp_lcd_touch_cst9217_read_data(')
        driver_get=function(source[start:],'static bool esp_lcd_touch_cst9217_get_xy(')
        fake=r'''
#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <deque>
#include <array>
#include <cassert>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_RESPONSE=2,GPIO_NUM_NC=-1;
constexpr int CST9217_DATA_LENGTH=10,CST9217_MAX_TOUCH_POINTS=1,CST9217_ACK_VALUE=0xAB;
constexpr int ESP_LCD_TOUCH_CST9217_DATA_REG=0xD000;
struct esp_lcd_touch_t;
using esp_lcd_touch_handle_t=esp_lcd_touch_t*;
struct esp_lcd_touch_t {
 void* io=nullptr;
 struct{int lock=0;uint8_t points=0;struct{uint16_t x=0,y=0;}coords[1];}data;
 struct{int rst_gpio_num=GPIO_NUM_NC;}config;
 esp_err_t(*read_data)(esp_lcd_touch_handle_t)=nullptr;
};
inline std::deque<std::array<uint8_t,10>> reports;
inline bool read_fail=false,ack_fail=false;
inline unsigned ack_count=0;
inline int esp_lcd_panel_io_tx_param(void*,int command,const void* data,size_t size){
 assert(command==0xD0&&size==2);const auto* bytes=static_cast<const uint8_t*>(data);
 assert(bytes[0]==0&&bytes[1]==0xAB);++ack_count;
 if(ack_fail)return ESP_FAIL;
 if(!reports.empty())reports.pop_front();return ESP_OK;
}
inline int cst9217_read_reg(esp_lcd_touch_handle_t,uint16_t reg,uint8_t* data,size_t size){
 assert(reg==0xD000&&size==10);if(read_fail)return ESP_FAIL;
 if(reports.empty()){memset(data,0,size);data[6]=0xAB;}else memcpy(data,reports.front().data(),size);
 return ESP_OK;
}
#define portENTER_CRITICAL(...) ((void)0)
#define portEXIT_CRITICAL(...) ((void)0)
// The native fake has no reset GPIO; return the same error directly so the
// vendor C function's goto cannot cross a C++ variable initialization.
#define ESP_GOTO_ON_ERROR(expr,label,...) do{if((expr)!=ESP_OK)return ESP_FAIL;}while(0)
#define ESP_LOGV(...) ((void)0)
inline void gpio_set_level(int,int){}inline void vTaskDelay(int){}
#define pdMS_TO_TICKS(n) (n)
'''
        lvgl=r'''
#pragma once
constexpr int LV_INDEV_MODE_EVENT=1,LV_INDEV_MODE_TIMER=0;
struct lv_timer_t{int period=30;};struct lv_indev_t{int mode=LV_INDEV_MODE_EVENT;lv_timer_t timer;};
inline void lv_indev_set_mode(lv_indev_t* p,int mode){p->mode=mode;}
inline lv_timer_t* lv_indev_get_read_timer(lv_indev_t* p){return &p->timer;}
inline void lv_timer_set_period(lv_timer_t* p,int period){p->period=period;}
'''
        code=r'''
#include "orbit_touch_reader.h"
#include "orbit_panel_orientation.h"
'''+driver_read+'\n'+driver_get+r'''
std::array<uint8_t,10> report(int x,int y,int status=6,int count=1){
 std::array<uint8_t,10> p{};p[0]=status;p[1]=x>>4;p[2]=y>>4;
 p[3]=((x&15)<<4)|(y&15);p[5]=count;p[6]=0xAB;return p;
}
int main(){
 esp_lcd_touch_t touch;touch.read_data=esp_lcd_touch_cst9217_read_data;
 uint16_t x=0,y=0,strength=0;uint8_t count=0;
 reports={report(100,120)};
 // Reproduce the old path: the first report is replayed without ACK.
 for(int i=0;i<3;++i){assert(touch.read_data(&touch)==ESP_OK);
  assert(esp_lcd_touch_cst9217_get_xy(&touch,&x,&y,&strength,&count,1));assert(x==100&&y==120);}
 assert(reports.size()==1&&ack_count==0);
 orbit_touch::Install(&touch);orbit_touch::Install(&touch);
 test_time_ms=0;assert(touch.read_data(&touch)==ESP_OK&&touch.data.points==1);
 reports={report(0,0,0)};test_time_ms=20;touch.read_data(&touch);assert(touch.data.points==1);
 test_time_ms=100;touch.read_data(&touch);assert(touch.data.points==0);
 reports={report(350,380)};test_time_ms=200;touch.read_data(&touch);assert(touch.data.points==1);
 reports={report(0,0,0,0)};test_time_ms=220;touch.read_data(&touch);assert(touch.data.points==1);
 test_time_ms=300;touch.read_data(&touch);assert(touch.data.points==0);
 assert(orbit_touch::read_count==6&&orbit_touch::contact_reports==2&&ack_count==6);
 assert(orbit_touch::raw_x==350&&orbit_touch::raw_y==380);
 // Invalid/failed reads bridge a short gap, then release on a bounded timeout.
 for(int error=0;error<3;++error){
  reports={report(42,84)};test_time_ms+=20;touch.read_data(&touch);assert(touch.data.points==1);
  reports={report(42,84)};read_fail=error==0;ack_fail=error==1;
  if(error==2)reports.front()[6]=0;
  test_time_ms+=20;assert(touch.read_data(&touch)==ESP_OK&&touch.data.points==1);
  assert(orbit_touch::last_error!=ESP_OK);
  reports={report(42,84)};if(error==2)reports.front()[6]=0;
  test_time_ms+=1200;assert(touch.read_data(&touch)==ESP_OK&&touch.data.points==0);
  read_fail=ack_fail=false;reports={report(42,84)};test_time_ms+=20;
  assert(touch.read_data(&touch)==ESP_OK&&touch.data.points==1&&orbit_touch::last_error==ESP_OK);
 }
 assert(orbit_touch::read_errors==6);
 lv_indev_t input;orbit_touch::ConfigureInput(&input);
 assert(input.mode==LV_INDEV_MODE_TIMER&&input.timer.period==20);
 // Verify the final buttons-up mapping on several distinct report positions.
 for(int view_x:{30,132,240,348,450})for(int view_y:{40,176,276,386,412}){
  reports={report(479-view_x,480-view_y)};assert(touch.read_data(&touch)==ESP_OK);
  assert(esp_lcd_touch_cst9217_get_xy(&touch,&x,&y,&strength,&count,1));
  const auto point=orbit_panel::TouchToDisplay(x,y);assert(point.x==view_x&&point.y==view_y);
 }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory)
            for name,text in [('esp_lcd_touch.h',fake),('lvgl.h',lvgl),('esp_timer.h','#pragma once\n#include <cstdint>\ninline uint64_t test_time_ms=0;inline int64_t esp_timer_get_time(){return test_time_ms*1000;}\n'),('check.cpp',code)]:
                (folder/name).write_text(text)
            result=subprocess.run([COMPILER,'-std=c++17','-I'+str(folder),'-I'+str(BOARD),str(folder/'check.cpp'),'-o',str(folder/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(folder/'check')],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_key2_real_binding_handles_release_even_without_single_click_event(self):
        board=(BOARD/'orbit_board.cc').read_text()
        method=function(board,'    void InitializeButtons()')
        code=r'''
#include <cassert>
#include <functional>
#include "orbit_network_state.h"
#define ESP_LOGI(...) ((void)0)
namespace orbit_clock_ai {int notices=0;void PostNetworkNotice(NetworkNotice n){assert(n==NetworkNotice::ManualRequested);++notices;}}
struct Button {std::function<void()> down,up;void OnPressDown(std::function<void()> cb){down=cb;}void OnPressUp(std::function<void()> cb){up=cb;}};
struct Board {Button boot_button_;int connects=0;void ConnectManually(){++connects;}
'''+method+r'''
};
int main(){Board board;board.InitializeButtons();
 for(int i=1;i<=3;++i){board.boot_button_.down();assert(board.connects==i-1);board.boot_button_.up();assert(board.connects==i);}
 assert(orbit_clock_ai::notices==3);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);(folder/'check.cpp').write_text(code)
            result=subprocess.run([COMPILER,'-std=c++17','-I'+str(BOARD),str(folder/'check.cpp'),'-o',str(folder/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(folder/'check')],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)


if __name__=='__main__':unittest.main()
