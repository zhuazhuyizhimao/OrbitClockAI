"""Exercise the actual scheduled shutdown method, including offline/connection races."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT.parent/'work/xiaozhi-upstream/main/application.cc'
COMPILER=shutil.which('clang++') or shutil.which('g++')

@unittest.skipUnless(COMPILER and SOURCE.exists(),'prepare XiaoZhi and install a native compiler')
class AiChatTests(unittest.TestCase):
    def test_close_stops_capture_playback_and_channel_on_main_loop(self):
        source=SOURCE.read_text();start=source.index('void Application::CloseChat()');end=source.index('\n}\n',start)+3
        method=source[start:end]
        code=r'''
#include <cassert>
#include <functional>
#include <memory>
#include <vector>
enum DeviceState{kDeviceStateIdle,kDeviceStateWifiConfiguring,kDeviceStateActivating,kDeviceStateConnecting,kDeviceStateListening,kDeviceStateSpeaking,kDeviceStateNotifying};
constexpr int kAbortReasonNone=0;
struct Protocol {bool opened=true;int closes=0;bool IsAudioChannelOpened(){return opened;}void CloseAudioChannel(){opened=false;++closes;}};
struct Audio {bool processing=true;int resets=0;void EnableVoiceProcessing(bool value){processing=value;}void ResetDecoder(){++resets;}};
struct Application {
 DeviceState state=kDeviceStateIdle;int aborts=0,stops=0,transitions=0;
 std::unique_ptr<Protocol> protocol_=std::make_unique<Protocol>();Audio audio_service_;
 std::vector<std::function<void()>> work;
 DeviceState GetDeviceState(){return state;}
 void Schedule(std::function<void()> action){work.push_back(action);}
 void StopNotification(){++stops;state=kDeviceStateIdle;}
 void AbortSpeaking(int){++aborts;}
 void SetDeviceState(DeviceState value){state=value;++transitions;}
 void CloseChat();
 void run(){auto actions=std::move(work);work.clear();for(auto &action:actions)action();}
};
''' + method+r'''
int main(){
 for(auto state:{kDeviceStateConnecting,kDeviceStateListening,kDeviceStateSpeaking})for(int connection=0;connection<3;++connection){
  Application app;app.state=state;
  if(connection==0)app.protocol_.reset();else app.protocol_->opened=connection==2;
  app.CloseChat();assert(app.state==state&&app.work.size()==1&&app.audio_service_.resets==0);
  app.run();assert(app.state==kDeviceStateIdle&&!app.audio_service_.processing&&app.audio_service_.resets==1);
  assert(app.aborts==(state==kDeviceStateSpeaking?1:0));
  if(app.protocol_)assert(app.protocol_->closes==(connection==2?1:0));
 }
 for(auto state:{kDeviceStateIdle,kDeviceStateWifiConfiguring,kDeviceStateActivating}){
  Application app;app.state=state;app.CloseChat();app.run();
  assert(app.state==state&&app.audio_service_.resets==0&&app.protocol_->closes==0);
 }
 Application notification;notification.state=kDeviceStateNotifying;notification.CloseChat();notification.run();
 assert(notification.stops==1&&notification.state==kDeviceStateIdle);
 Application race;race.state=kDeviceStateConnecting;race.CloseChat();race.state=kDeviceStateListening;race.run();
 assert(race.state==kDeviceStateIdle&&race.protocol_->closes==1);
 Application disconnected;disconnected.state=kDeviceStateSpeaking;disconnected.CloseChat();disconnected.state=kDeviceStateIdle;disconnected.run();
 assert(disconnected.aborts==0&&disconnected.transitions==0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            temp=Path(directory);(temp/'check.cpp').write_text(code)
            result=subprocess.run([COMPILER,'-std=c++17',str(temp/'check.cpp'),'-o',str(temp/'check')],text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(temp/'check')],text=True,capture_output=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)

if __name__=='__main__':unittest.main()
