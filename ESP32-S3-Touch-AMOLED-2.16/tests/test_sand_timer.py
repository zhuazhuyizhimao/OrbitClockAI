"""Check countdown timing and the physical-key single/double-click boundary."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
COMPILER=shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER, 'a native C++ compiler is needed')
class SandTimerTests(unittest.TestCase):
    def run_cpp(self, body):
        source='#include "sand_timer_model.h"\n#include "dice_game_model.h"\n#include <cassert>\n'+body
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);(path/'check.cpp').write_text(source)
            subprocess.run([COMPILER,'-std=c++17','-I'+str(ROOT/'examples/arduino/08_OrbitClock'),
                            str(path/'check.cpp'),'-o',str(path/'check')],check=True,capture_output=True)
            subprocess.run([str(path/'check')],check=True,timeout=20,capture_output=True)

    def test_countdown_pause_resume_reset_and_exact_completion(self):
        self.run_cpp(r'''
int main(){
 using namespace sand_timer;Countdown t;assert(t.seconds()==60&&t.remainingSeconds()==60);
 assert(t.configure(3));t.toggle(100);assert(t.state()==State::Running);
 t.update(101);assert(t.remainingSeconds()==3&&t.remainingMs()==2999);
 t.update(1100);assert(t.remainingSeconds()==2);
 t.pause(1600);assert(t.state()==State::Paused&&t.remainingMs()==1500);
 t.update(9000);assert(t.remainingMs()==1500);t.toggle(9000);t.update(10499);
 assert(t.remainingSeconds()==1&&t.remainingMs()==1);t.update(10500);
 assert(t.state()==State::Done&&t.remainingSeconds()==0&&t.fraction()==0);
 t.update(20000);assert(t.remainingMs()==0);t.toggle(21000);
 assert(t.state()==State::Running&&t.remainingMs()==3000);t.reset();
 assert(t.state()==State::Idle&&t.remainingMs()==3000);
}
''')

    def test_configuration_bounds_and_wraparound(self):
        self.run_cpp(r'''
int main(){
 using namespace sand_timer;Countdown t;
 assert(!t.configure(0)&&!t.configure(3601)&&t.seconds()==60);
 assert(t.configure(3600));t.toggle(0xfffffc00U);t.update(0x000003d0U);
 assert(t.remainingMs()==3598000&&t.remainingSeconds()==3598);
 t.pause(0x7b8U);const auto remaining=t.remainingMs();t.update(0x12345678U);
 assert(t.remainingMs()==remaining);assert(t.configure(1));assert(t.state()==State::Idle);
 t.toggle(0xfffffff0U);t.update(0x3d7U);assert(t.remainingMs()==1);
 t.update(0x3d8U);assert(t.state()==State::Done);
}
''')

    def test_double_click_defers_single_and_accepts_a_second_press_before_deadline(self):
        self.run_cpp(r'''
int main(){
 using namespace dice_game;DoubleClick clicks;
 assert(clicks.update(true,false,100)==Click::None);
 assert(clicks.update(false,false,399)==Click::None);
 assert(clicks.update(false,false,400)==Click::Single);
 assert(clicks.update(false,false,900)==Click::None);
 assert(clicks.update(true,false,1000)==Click::None);
 assert(clicks.update(false,true,1290)==Click::None);
 assert(clicks.update(false,true,1400)==Click::None);
 assert(clicks.update(true,false,1450)==Click::Double);
 assert(clicks.update(false,false,1700)==Click::None);
 // Navigation clears a pending first click; it must not pair across pages.
 clicks.update(true,false,2000);clicks.reset();assert(clicks.update(true,false,2100)==Click::None);
 assert(clicks.update(false,false,2400)==Click::Single);
 clicks.update(true,false,0xfffffff0U);
 assert(clicks.update(true,false,0x80U)==Click::Double);
}
''')

    def test_debounced_button_double_click_and_held_key(self):
        self.run_cpp(r'''
int main(){
 using namespace dice_game;Button button;DoubleClick clicks;button.begin(false,0);
 auto poll=[&](bool down,uint32_t now){bool release=button.update(down,now);return clicks.update(release,button.down(),now);};
 assert(poll(true,10)==Click::None);assert(poll(false,20)==Click::None); // bounce
 assert(poll(true,30)==Click::None);assert(poll(true,65)==Click::None);
 assert(poll(false,110)==Click::None);assert(poll(false,145)==Click::None);
 assert(poll(true,230)==Click::None);assert(poll(true,265)==Click::None);
 assert(poll(false,310)==Click::None);assert(poll(false,345)==Click::Double);
 assert(poll(false,800)==Click::None);
 assert(poll(true,900)==Click::None);assert(poll(true,935)==Click::None);
 assert(poll(false,1900)==Click::None);assert(poll(false,1935)==Click::None); // hold ignored
}
''')
