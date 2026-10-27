"""Run the real AI effect producer + official Ogg parser with deterministic audio/RTOS sinks."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
BOARD=ROOT/'examples/esp-idf/06_OrbitClockAI/board'
DEMUX=ROOT.parent/'work/xiaozhi-upstream/main/audio/demuxer'
COMPILER=shutil.which('clang++') or shutil.which('g++')

@unittest.skipUnless(COMPILER and (DEMUX/'ogg_demuxer.cc').exists(),'prepare the XiaoZhi source and install a native compiler')
class AiAudioTests(unittest.TestCase):
    def test_single_flight_wake_restore_offline_playback_timeout_and_ai_priority(self):
        headers={
          'esp_err.h':'#pragma once\n#define ESP_OK 0\n#define ESP_ERR_NO_MEM 1\n#define ESP_ERR_TIMEOUT 2\n',
          'esp_log.h':'#pragma once\n#define ESP_LOGE(...)\n#define ESP_LOGW(...)\n#define ESP_LOGI(...)\n#define ESP_LOGD(...)\n',
          'esp_timer.h':'#pragma once\n#include <cstdint>\nextern int64_t clockUs; inline int64_t esp_timer_get_time(){return clockUs;}\n',
          'freertos/FreeRTOS.h':'''#pragma once
#include <cstdint>
#define pdTRUE 1
#define pdPASS 1
#define portMAX_DELAY 0xffffffff
#define pdMS_TO_TICKS(x) (x)
using TaskHandle_t=void*;
''',
          'freertos/task.h':'''#pragma once
#include "FreeRTOS.h"
#include "esp_timer.h"
struct StopWorker{};
extern bool firstNotification;extern unsigned notifications;
inline unsigned ulTaskNotifyTake(int,unsigned){if(!firstNotification)throw StopWorker{};firstNotification=false;return 1;}
inline void vTaskDelay(unsigned ms){clockUs+=int64_t(ms)*1000;}
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t *handle,int){*handle=(void*)1;return pdPASS;}
inline void xTaskNotifyGive(TaskHandle_t){++notifications;}
''',
          'application.h':'''#pragma once
#include <memory>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "esp_timer.h"
enum DeviceState{kDeviceStateIdle,kDeviceStateWifiConfiguring,kDeviceStateActivating,kDeviceStateConnecting,kDeviceStateListening,kDeviceStateSpeaking};
struct AudioStreamPacket{int sample_rate=0,frame_duration=0;std::vector<uint8_t> payload;};
struct FakeAudio{
 bool wake=true,reject=false,interrupt=false;unsigned accepted=0,attempts=0,disabled=0,enabled=0;int64_t drainAt=0;
 bool IsPlaybackIdle()const{return clockUs>=drainAt;}
 bool IsWakeWordRunning()const{return wake;}
 void EnableWakeWordDetection(bool value){wake=value;if(value)++enabled;else ++disabled;}
 bool PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket> packet,bool wait);
};
struct Application{
 DeviceState state=kDeviceStateIdle;FakeAudio audio;
 static Application &GetInstance(){static Application app;return app;}
 DeviceState GetDeviceState()const{return state;}
 FakeAudio &GetAudioService(){return audio;}
};
inline bool FakeAudio::PushPacketToDecodeQueue(std::unique_ptr<AudioStreamPacket> packet,bool wait){
 ++attempts;if(reject)return false;
 if(packet->payload.empty()||packet->sample_rate!=16000||packet->frame_duration!=20)throw 1;
 ++accepted;drainAt=std::max(drainAt,clockUs)+packet->frame_duration*1000;
 if(interrupt&&accepted==5)Application::GetInstance().state=kDeviceStateListening;
 return true;
}
'''}
        code=r'''
#include <cassert>
#include <cstring>
#include "application.h"
#include "freertos/task.h"
int64_t clockUs=0;bool firstNotification=true;unsigned notifications=0;
#include "clock_audio_ai.cc"
static void reset(DeviceState state=kDeviceStateIdle){
 auto &app=Application::GetInstance();app.audio=FakeAudio{};app.state=state;clockUs=0;
 clock_audio::flight.enable();firstNotification=true;notifications=0;
}
static void runWorker(){try{clock_audio::effectTask(nullptr);}catch(StopWorker&){} }
int main(){
 auto &app=Application::GetInstance();assert(clock_audio::begin());
 reset();assert(clock_audio::play());for(int i=0;i<100;++i)assert(!clock_audio::play());
 assert(notifications==1);runWorker();assert(!strcmp(clock_audio::status(),"READY"));
 assert(app.audio.accepted>=110&&app.audio.accepted<=112&&app.audio.disabled==1&&app.audio.enabled==1&&app.audio.wake);
 assert(clockUs>=2200000&&clockUs<2400000);assert(clock_audio::lastError()==0);
 reset(kDeviceStateWifiConfiguring);app.audio.wake=false;assert(clock_audio::play());runWorker();
 assert(app.audio.accepted>=110&&!app.audio.wake&&app.audio.enabled==0);
 for(auto state:{kDeviceStateConnecting,kDeviceStateListening,kDeviceStateSpeaking}){reset(state);assert(!clock_audio::play()&&notifications==0);}
 reset();app.audio.drainAt=1000000;assert(clock_audio::play());runWorker();assert(app.audio.accepted==0&&app.audio.disabled==0);
 reset();app.audio.reject=true;assert(clock_audio::play());runWorker();
 assert(app.audio.accepted==0&&app.audio.attempts<=51&&clockUs<=510000&&clock_audio::lastError()==ESP_ERR_TIMEOUT&&app.audio.wake);
 reset();app.audio.interrupt=true;assert(clock_audio::play());runWorker();
 assert(app.audio.accepted==5&&app.state==kDeviceStateListening&&app.audio.enabled==0&&!app.audio.wake);
 assert(!clock_audio::play());
}
'''
        with tempfile.TemporaryDirectory() as directory:
            temp=Path(directory)
            for name,text in headers.items():
                target=temp/name;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text)
            (temp/'check.cpp').write_text(code)
            result=subprocess.run([COMPILER,'-std=c++17','-pthread','-I'+str(temp),'-I'+str(BOARD),
                '-I'+str(ROOT/'examples/arduino/08_OrbitClock'),'-I'+str(DEMUX),str(temp/'check.cpp'),str(DEMUX/'ogg_demuxer.cc'),'-o',str(temp/'check')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(temp/'check')],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)

if __name__=='__main__':unittest.main()
