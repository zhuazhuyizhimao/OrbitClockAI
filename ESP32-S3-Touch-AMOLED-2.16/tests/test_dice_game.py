"""Native checks for physical-key filtering, shake detection and dice outcomes."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER, 'a native C++ compiler is needed')
class DiceGameTests(unittest.TestCase):
    def run_cpp(self, body):
        code = '''#include "dice_game_model.h"
#include <cassert>
#include <limits>
#include <initializer_list>
using namespace dice_game;
static void still(Game &g,uint32_t &now) {
  for(int i=0;i<14;++i){now+=50;assert(!g.sample(0,0,1,now));}
}
static void shake(Game &g,uint32_t &now) {
  now+=100;assert(!g.sample(.55f,0,1,now));
  now+=100;assert(g.sample(-.55f,0,1,now));
}
''' + body
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path/'check.cpp').write_text(code)
            subprocess.run([COMPILER, '-std=c++17',
                            '-I'+str(ROOT/'examples/arduino/08_OrbitClock'),
                            str(path/'check.cpp'), '-o', str(path/'check')],
                           check=True, capture_output=True)
            subprocess.run([str(path/'check')], check=True, timeout=20, capture_output=True)

    def test_key_bounce_boot_hold_long_press_and_wrap(self):
        self.run_cpp(r'''
int main() {
  Button b;b.begin(false,0);
  assert(!b.update(true,10));assert(!b.update(false,20));assert(!b.update(true,25));
  assert(!b.update(true,60));assert(!b.update(true,70));
  assert(!b.update(false,100));assert(!b.update(true,110));assert(!b.update(false,120));
  assert(b.update(false,155));assert(!b.update(false,300));
  assert(!b.update(true,500));assert(!b.update(true,535));
  assert(!b.update(false,1500));assert(!b.update(false,1535)); // Long hold is not a click.
  b.begin(true,2000);assert(!b.update(false,2200));assert(!b.update(false,2235));
  b.begin(false,0xffffffe0U);assert(!b.update(true,0xfffffff0U));
  assert(!b.update(true,0x20));assert(!b.update(false,0x40));assert(b.update(false,0x70));
}
''')

    def test_entry_shake_and_slow_tilt_stale_and_freefall_filtering(self):
        self.run_cpp(r'''
int main() {
  Game g;uint32_t now=0;g.enter();
  assert(g.armed());still(g,now);assert(g.armed());
  for(int i=0;i<50;++i){now+=50;float angle=i*.025f;assert(!g.sample(std::sin(angle),0,std::cos(angle),now));}
  now+=500;assert(!g.sample(2,0,1,now)); // Stale samples reset the detector.
  assert(!g.armed());still(g,now);assert(g.armed());
  now+=50;assert(!g.sample(0,0,0,now));assert(!g.armed());
  still(g,now);shake(g,now);assert(g.rolling());
  for(int i=0;i<50;++i){now+=50;assert(!g.sample(i%2?1:-1,0,1,now));}
  assert(g.rolling()); // Motion does not finish or restart the animation.
}
''')

    def test_all_configurations_finish_at_two_seconds_and_hold_results(self):
        self.run_cpp(r'''
int main() {
  for(int count=1;count<=3;++count)for(int faces=2;faces<=8;++faces) {
    Game g;assert(g.configure(count,faces));const int expected=faces==2?1:count;assert(g.count()==expected);g.enter();uint32_t now=0xfffffc00U;
    still(g,now);shake(g,now);const uint32_t start=now;
    int calls=0;auto random=[&](){++calls;return uint32_t(123456789U+calls*3571U);};
    for(int frame=0;frame<50;++frame) {
      assert(!g.tick(start+frame*40,random));
      for(int i=0;i<expected;++i)assert(g.displayed(i,start+frame*40)>=1&&g.displayed(i,start+frame*40)<=faces);
    }
    assert(!g.tick(start+1999,random)&&calls==0);
    assert(g.tick(start+2000,random)&&g.hasResult()&&!g.rolling()&&calls==expected);
    int sum=0;for(int i=0;i<expected;++i){auto v=g.displayed(i,start+2000);assert(v>=1&&v<=faces);sum+=v;}
    assert(g.total()==sum&&sum>=expected&&sum<=expected*faces);
    assert(!g.tick(start+4000,random)&&g.total()==sum&&calls==expected);
    now=start+2100;for(int i=0;i<10;++i){now+=100;assert(!g.sample(i%2?1:-1,0,1,now));}
    still(g,now);shake(g,now);assert(g.rolling());
  }
}
''')

    def test_settings_touch_and_exit_cancel_without_random_results(self):
        self.run_cpp(r'''
int main() {
  Game g;uint32_t now=0;g.enter();still(g,now);
  now+=100;assert(!g.sample(1,0,1,now,true));assert(g.armed());
  still(g,now);shake(g,now);g.settings(true);
  int calls=0;auto random=[&](){++calls;return 32U;};
  assert(!g.tick(now+3000,random)&&calls==0&&!g.hasResult());
  for(int i=0;i<20;++i){now+=100;assert(!g.sample(i%2?1:-1,0,1,now));}
  assert(!g.configure(0,6)&&!g.configure(4,6)&&!g.configure(1,1)&&!g.configure(1,9));
  assert(g.count()==1&&g.faces()==6);assert(g.configure(3,8));g.settings(false);
  still(g,now);shake(g,now);g.leave();assert(!g.rolling());
  assert(!g.tick(now+3000,random)&&calls==0);
  g.enter();assert(g.count()==3&&g.faces()==8&&!g.hasResult());
}
''')

    def test_rejection_sampling_retains_equal_probability_for_each_face(self):
        self.run_cpp(r'''
int main() {
  for(uint8_t faces=2;faces<=8;++faces) {
    const uint32_t threshold=(uint32_t(0)-faces)%faces;
    int buckets[9]={};
    for(uint32_t i=threshold;i<threshold+faces*100U;++i) {
      auto random=[&](){return i;};++buckets[uniformFace(faces,random)];
    }
    for(int i=1;i<=faces;++i)assert(buckets[i]==100);
    if(threshold) {
      int calls=0;auto random=[&](){return ++calls<=3?0U:threshold;};
      assert(uniformFace(faces,random)==threshold%faces+1&&calls==4);
    }
  }
}
''')

    def test_moderate_hand_shake_at_realistic_sampling_rates(self):
        self.run_cpp(r'''
int main() {
  // A 0.45 g, 2-4 Hz hand shake has small adjacent deltas at 50 Hz. It must
  // still trigger from entry, without a compulsory quiet interval.
  for(int period: {20,40,70,110})for(int frequency=2;frequency<=4;++frequency) {
    Game g;g.enter();uint32_t now=100;assert(!g.sample(0,0,1,now));bool started=false;
    for(int elapsed=period;elapsed<1600;elapsed+=period) {
      now=100+elapsed;
      const float x=.45f*std::sin(6.283185307f*frequency*elapsed/1000.f);
      if(g.sample(x,0,1,now)){started=true;break;}
    }
    assert(started&&g.rolling());
    auto random=[](){return 12345678U;};assert(g.tick(now+2000,random));now+=2000;
    // A continuing gentle oscillation must not re-arm the completed roll.
    for(int elapsed=period;elapsed<2400;elapsed+=period) {
      const float x=.45f*std::sin(6.283185307f*frequency*elapsed/1000.f);
      assert(!g.sample(x,0,1,now+elapsed));
    }
    now+=2400;still(g,now);assert(g.armed());shake(g,now);assert(g.rolling());
  }
  Game slow;slow.enter();for(int i=0;i<200;++i) {
    const float angle=i*.006f;assert(!slow.sample(std::sin(angle),0,std::cos(angle),i*20));
  }
  Game tap;tap.enter();assert(!tap.sample(0,0,1,100));assert(!tap.sample(.6f,0,1,120));
  for(int i=0;i<100;++i)assert(!tap.sample(0,0,1,140+i*20));
}
''')

    def test_actual_motion_service_feeds_acceleration_without_a_gyro(self):
        ino = (ROOT/'examples/arduino/08_OrbitClock/08_OrbitClock.ino').read_text()
        service = ino[ino.index('static void serviceMotion()'):ino.index('static void serviceConsole()')]
        self.run_cpp(r'''
static uint32_t stamp=0;
static uint32_t millis(){return stamp;}
static bool diceMode=true,motionOnline=true,motionGyroOnline=false,orientationCalibrating=false;
static bool settingsPanel=false,touchDown=false,automaticOrientation=false,focusMode=false;
static uint8_t pageRotation=0;static unsigned motionReadErrors=0,accepted=0;
struct IMUdata { float x,y,z; };
struct Sensor {
 unsigned reads=0;bool fail=false;
 bool getDataReady(){return true;}
 bool getGyroscope(float &,float &,float &){assert(false);return false;}
 bool getAccelerometer(float &x,float &y,float &z) {
   ++reads;if(fail)return false;x=.45f*std::sin(6.283185307f*2*stamp/1000.f);y=0;z=1;return true;
 }
} motionSensor;
struct Orientation {
 void updateGyro(float,float,float,uint32_t){}void updateAccel(float,float,float,uint32_t){}
 bool readyToApply(uint32_t){return false;}uint8_t quarter(){return 0;}
} pageOrientation;
static Game game;
static void feedDiceMotion(float x,float y,float z,uint32_t now){if(game.sample(x,y,z,now))++accepted;}
static void setPageRotation(uint8_t){}static void startCharacterBounce(){}
''' + service + r'''
int main() {
 game.enter();for(stamp=0;stamp<1500;stamp+=5)serviceMotion();
 assert(game.rolling()&&accepted==1&&motionSensor.reads>=50&&motionSensor.reads<=75);
 const unsigned count=motionSensor.reads;motionOnline=false;stamp+=100;serviceMotion();assert(motionSensor.reads==count);
 motionOnline=true;motionSensor.fail=true;stamp+=100;serviceMotion();assert(motionReadErrors==1);
}
''')
