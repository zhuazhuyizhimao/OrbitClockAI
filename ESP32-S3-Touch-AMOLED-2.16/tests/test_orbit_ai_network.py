"""Execute actual board networking, NTP and saved-SSID selection with fake hardware."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
BOARD = ROOT/'examples/esp-idf/06_OrbitClockAI/board'
WIFI = BOARD.parent/'network/esp-wifi-connect'
COMPILER = shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER, 'native C++ compiler required')
class NetworkTests(unittest.TestCase):
    def compile_run(self, code, headers=None, sources=()):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            for name, content in (headers or {}).items():
                path = folder/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(content)
            (folder/'main.cpp').write_text(code)
            result = subprocess.run([COMPILER, '-std=c++17', '-pthread', '-I'+str(folder),
                '-I'+str(BOARD), str(folder/'main.cpp'), *map(str, sources), '-o', str(folder/'check')],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(folder/'check')], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_ntp_confirms_reply_retries_and_resyncs_without_blocking(self):
        header = r'''
#pragma once
#include <sys/time.h>
constexpr int ESP_OK=0;
struct esp_sntp_config_t { bool start=true,wait_for_sync=true; void(*sync_cb)(timeval*)=nullptr; const char* servers[3]; };
#define ESP_SNTP_SERVER_LIST(...) {__VA_ARGS__}
#define ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(n, list) {true,true,nullptr,list}
inline esp_sntp_config_t configured;
inline int init_count=0,start_count=0,init_error=0,start_error=0;
inline int esp_netif_sntp_init(const esp_sntp_config_t* c){configured=*c;++init_count;return init_error;}
inline int esp_netif_sntp_start(){++start_count;return start_error;}
'''
        self.compile_run(r'''
#include <cassert>
#include <cstring>
#include "orbit_time_sync.h"
using namespace orbit_clock_ai;
int main(){
 NetworkClock clock;
 assert(clock.Poll(false,0,true)==NetworkNotice::None&&init_count==0);
 assert(clock.Poll(true,50,false)==NetworkNotice::Syncing&&init_count==1&&start_count==1);
 assert(!configured.start&&!configured.wait_for_sync&&configured.sync_cb);
 assert(!strcmp(configured.servers[0],"ntp.aliyun.com")&&!strcmp(configured.servers[2],"pool.ntp.org"));
 for(unsigned i=100;i<30000;i+=20)assert(clock.Poll(true,i,false)==NetworkNotice::None);
 assert(clock.Poll(true,1000,true)==NetworkNotice::Syncing&&start_count==1);
 assert(clock.Poll(true,30050,false)==NetworkNotice::SyncFailed);
 assert(clock.Poll(true,60049,false)==NetworkNotice::None);
 assert(clock.Poll(true,60050,false)==NetworkNotice::Syncing&&start_count==2&&init_count==1);
 timeval invalid{0,0};configured.sync_cb(&invalid);
 assert(clock.Poll(true,60060,false)==NetworkNotice::None);
 timeval valid{1800000000,0};configured.sync_cb(&valid);
 assert(clock.Poll(true,60070,false)==NetworkNotice::Synced);
 assert(clock.Poll(true,60080,false)==NetworkNotice::None);
 assert(clock.Poll(true,60100,true)==NetworkNotice::Syncing&&start_count==3);
 configured.sync_cb(&valid);clock.Poll(false,60120,false);
 assert(clock.Poll(true,60140,false)==NetworkNotice::Syncing&&start_count==4);
 configured.sync_cb(&valid);assert(clock.Poll(true,60160,false)==NetworkNotice::Synced);
 assert(clock.Poll(true,200000,false)==NetworkNotice::None);
 NetworkClock failure;init_error=1;
 assert(failure.Poll(true,0,false)==NetworkNotice::SyncFailed);
 assert(failure.Poll(true,59999,false)==NetworkNotice::None);
 init_error=0;start_error=1;
 assert(failure.Poll(true,60000,false)==NetworkNotice::SyncFailed);
 start_error=0;
 assert(failure.Poll(true,120000,false)==NetworkNotice::Syncing);
 NetworkClock wrap;
 assert(wrap.Poll(true,0xfffffff0U,false)==NetworkNotice::Syncing);
 assert(wrap.Poll(true,uint32_t(0xfffffff0U+30000U),false)==NetworkNotice::SyncFailed);
}
''', {'esp_netif_sntp.h': header})

    def test_key2_uses_single_manager_and_preserves_ai_connected_events(self):
        fake = r'''
#pragma once
#include <cassert>
#include <functional>
#include <string>
#include <vector>
#include <utility>
enum class NetworkEvent {Scanning,Connecting,Connected,Disconnected,WifiConfigModeEnter,WifiConfigModeExit};
using NetworkEventCallback=std::function<void(NetworkEvent,const std::string&)>;
inline bool timer_active=false,task_fail=false;
inline int attempts=0,provision=0,deletes=0;
inline std::vector<std::function<void()>> tasks,scheduled;
constexpr int pdPASS=1;
inline int xTaskCreate(void(*fn)(void*),const char*,int,void* data,int,void*) {
 if(task_fail)return 0;tasks.push_back([=](){fn(data);});return pdPASS;
}
inline void vTaskDelete(void*){++deletes;}
inline int esp_timer_stop(void*){timer_active=false;return 0;}
inline bool esp_timer_is_active(void*){return timer_active;}
struct WifiBoard {
 void* connect_timer_=nullptr;
 virtual void SetNetworkEventCallback(NetworkEventCallback cb){callback=std::move(cb);}
 void TryWifiConnect(){++attempts;timer_active=true;}
 void EnterWifiConfigMode(){++provision;}
 NetworkEventCallback callback;
};
struct WifiManager {
 bool ready=false,online=false,config=false;int stops=0,config_stops=0;
 WifiBoard* board=nullptr;
 static WifiManager& GetInstance(){static WifiManager w;return w;}
 bool IsInitialized(){return ready;}bool IsConnected(){return online;}bool IsConfigMode(){return config;}
 void StopStation(){++stops;}
 void StopConfigAp(){++config_stops;config=false;if(board)board->TryWifiConnect();}
 std::string GetApSsid(){return "Xiaozhi-test";}std::string GetApWebUrl(){return "http://192.168.4.1";}
};
struct SsidItem {std::string ssid;};
struct SsidManager {
 std::vector<SsidItem> saved;
 static SsidManager& GetInstance(){static SsidManager s;return s;}
 const auto& GetSsidList(){return saved;}
};
struct Application {
 int alerts=0;
 static Application& GetInstance(){static Application app;return app;}
 void Schedule(std::function<void()> cb){scheduled.push_back(std::move(cb));}
 void Alert(const char*,const char*,const char*,const char*){++alerts;}
};
namespace Lang {namespace Strings {
 inline const char* CONNECT_TO_HOTSPOT="hotspot ";inline const char* ACCESS_VIA_BROWSER="url ";inline const char* WIFI_CONFIG_MODE="config";
} namespace Sounds {inline const char* OGG_WIFICONFIG="sound";}}
#define ESP_LOGE(...) ((void)0)
'''
        headers = {'fake.h': fake}
        for name in ('wifi_board.h','wifi_manager.h','ssid_manager.h','application.h','display.h',
                     'assets/lang_config.h','esp_log.h'):
            headers[name] = '#include "fake.h"\n'
        self.compile_run(r'''
#include "fake.h"
#include "orbit_network.h"
#include "orbit_network_state.h"
using namespace orbit_clock_ai;
void run(){auto work=std::move(tasks);tasks.clear();for(auto& fn:work)fn();}
int main(){
 OrbitWifiBoard board;auto& wifi=WifiManager::GetInstance();wifi.board=&board;
 int events=0;board.SetNetworkEventCallback([&](NetworkEvent event,const std::string&){
  ++events;if(event==NetworkEvent::Connected)assert(NetworkOnline());
  else if(event!=NetworkEvent::WifiConfigModeExit)assert(!NetworkOnline());
 });
 for(auto event:{NetworkEvent::Scanning,NetworkEvent::Connecting,NetworkEvent::Connected,
                NetworkEvent::Disconnected,NetworkEvent::WifiConfigModeEnter,NetworkEvent::WifiConfigModeExit})
   board.callback(event,"saved-wifi");
 assert(events==6&&TakeClockSyncRequest()&&!TakeClockSyncRequest());
 board.ConnectManually();run();assert(tasks.empty()&&TakeNetworkNotice()==NetworkNotice::Starting);
 wifi.ready=true;wifi.online=true;board.ConnectManually();run();
 assert(TakeClockSyncRequest()&&tasks.empty()&&wifi.stops==0);
 wifi.online=false;SsidManager::GetInstance().saved={{"home"},{"office"}};
 for(int i=0;i<20;++i)board.ConnectManually();assert(tasks.size()==1);run();
 assert(attempts==1&&timer_active&&wifi.stops==1&&deletes==3);
 board.ConnectManually();run();assert(attempts==1&&wifi.stops==1);
 timer_active=false;board.ConnectManually();run();assert(attempts==2&&wifi.stops==2);
 wifi.config=true;board.ConnectManually();run();
 assert(wifi.config_stops==1&&attempts==3&&!wifi.config);
 wifi.config=true;SsidManager::GetInstance().saved.clear();board.ConnectManually();run();
 assert(wifi.config&&wifi.config_stops==1&&scheduled.size()==1);
 assert(TakeNetworkSetupUiRequest()&&!TakeNetworkSetupUiRequest());
 scheduled.front()();scheduled.clear();assert(Application::GetInstance().alerts==1);
 wifi.config=false;timer_active=false;board.ConnectManually();run();
 assert(TakeNetworkSetupUiRequest()&&!TakeNetworkSetupUiRequest());
 assert(provision==0&&scheduled.size()==1);scheduled.front()();scheduled.clear();assert(provision==1);
 task_fail=true;board.ConnectManually();assert(tasks.empty());task_fail=false;
 assert(TakeNetworkNotice()==NetworkNotice::RetryFailed);
 board.ConnectManually();assert(tasks.size()==1);wifi.online=true;run();
 assert(TakeClockSyncRequest()&&wifi.stops==2);
}
''', headers, [BOARD/'orbit_network.cc'])

    def test_last_connected_ssid_precedes_stronger_fallbacks(self):
        source = (WIFI/'wifi_station.cc').read_text()
        start = source.index('void WifiStation::HandleScanResult()')
        end = source.index('void WifiStation::StartConnect()', start)
        scan = source[start:end]
        start = source.index('void WifiStation::IpEventHandler(')
        connected = source[start:]
        # Run the actual connection-success promotion as well as scan selection.
        promotion = connected[connected.index('    auto& ssid_manager ='):connected.index('    if (this_->on_connected_)')]
        self.compile_run(r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
struct SsidItem{std::string ssid,password;};
struct SsidManager {
 std::vector<SsidItem> saved={{"recent","new-pass"},{"fallback","old-pass"}};int writes=0;
 static SsidManager& GetInstance(){static SsidManager s;return s;}
 const auto& GetSsidList(){return saved;}
 void SetDefaultSsid(int i){auto item=saved[i];saved.erase(saved.begin()+i);saved.insert(saved.begin(),item);++writes;}
};
struct wifi_ap_record_t {uint8_t ssid[33]={};uint8_t bssid[6]={};int rssi=0,primary=1,authmode=0;};
struct WifiApRecord{std::string ssid,password;int channel,authmode;uint8_t bssid[6];};
std::vector<wifi_ap_record_t> scan_records;
void esp_wifi_scan_get_ap_num(uint16_t* n){*n=scan_records.size();}
void esp_wifi_scan_get_ap_records(uint16_t* n,wifi_ap_record_t* p){if(*n)memcpy(p,scan_records.data(),*n*sizeof(*p));}
void esp_timer_start_once(void*,int){}
#define ESP_LOGI(...) ((void)0)
struct WifiStation {
 std::vector<WifiApRecord> connect_queue_;std::string ssid_;
 bool last_scan_used_saved_channels_=false,use_saved_channels_scan_=true;
 int scan_current_interval_microseconds_=10000000;void* timer_handle_=nullptr;
 int starts=0,scans=0;std::string chosen;
 void StartConnect(){++starts;chosen=connect_queue_.front().ssid;}
 void StartScan(){++scans;}void UpdateScanInterval(){}
 void HandleScanResult();
 void Promote(){auto* this_=this;
''' + promotion + r'''
 }
};
''' + scan + r'''
wifi_ap_record_t ap(const char* name,int rssi,int marker){wifi_ap_record_t a;strcpy((char*)a.ssid,name);a.rssi=rssi;a.bssid[0]=marker;return a;}
int main(){
 scan_records={ap("fallback",-20,1),ap("recent",-70,2),ap("recent",-50,3),ap("unknown",-10,4)};
 WifiStation board;board.HandleScanResult();assert(board.starts==1&&board.chosen=="recent");
 assert(board.connect_queue_.size()==3&&board.connect_queue_[0].bssid[0]==3&&board.connect_queue_[1].bssid[0]==2);
 assert(board.connect_queue_[0].password=="new-pass"&&board.connect_queue_[2].ssid=="fallback");
 scan_records={ap("fallback",-20,1)};
 WifiStation partial;partial.last_scan_used_saved_channels_=true;partial.HandleScanResult();
 assert(partial.starts==0&&partial.scans==1&&!partial.use_saved_channels_scan_&&partial.connect_queue_.empty());
 partial.HandleScanResult();assert(partial.chosen=="fallback"&&partial.starts==1);
 partial.ssid_="fallback";partial.Promote();auto& saved=SsidManager::GetInstance();
 assert(saved.saved.front().ssid=="fallback"&&saved.saved.front().password=="old-pass"&&saved.writes==1);
 partial.Promote();assert(saved.writes==1);
 partial.ssid_="unknown";partial.Promote();assert(saved.writes==1&&saved.saved.size()==2);
 scan_records={ap("fallback",-80,1),ap("recent",-10,2)};
 WifiStation reboot;reboot.HandleScanResult();assert(reboot.chosen=="fallback");
 scan_records.clear();WifiStation empty;empty.HandleScanResult();assert(empty.starts==0);
}
''')


if __name__ == '__main__': unittest.main()
