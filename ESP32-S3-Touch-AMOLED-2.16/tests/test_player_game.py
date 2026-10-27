"""Check the starting-player wheel's timing, fair selection and cancellation."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
COMPILER=shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER,'a native C++ compiler is needed')
class PlayerGameTests(unittest.TestCase):
    def run_cpp(self,body):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)
            (path/'check.cpp').write_text('#include "player_game_model.h"\n#include <cassert>\n'+body)
            subprocess.run([COMPILER,'-std=c++17','-I'+str(ROOT/'examples/arduino/08_OrbitClock'),
                            str(path/'check.cpp'),'-o',str(path/'check')],check=True,capture_output=True)
            subprocess.run([str(path/'check')],check=True,timeout=20,capture_output=True)

    def test_timing_deceleration_exact_sector_and_busy_clicks(self):
        self.run_cpp(r'''
int main(){
 using namespace player_game;Game g;assert(g.players()==4&&g.winner()==0);
 unsigned calls=0;auto rng=[&](){++calls;return 101U;};
 assert(g.start(100,rng)&&g.spinning()&&g.winner()==0&&calls==1);
 assert(!g.start(200,rng)&&calls==1);
 const float fast=normalize(g.angle(500)-g.angle(490));
 assert(std::fabs(fast-normalize(g.angle(2500)-g.angle(2490)))<.0001f);
 assert(!g.slowing(3099)&&g.slowing(3100));
 const float slow=normalize(g.angle(4200)-g.angle(4190));assert(slow<fast*.15f);
 g.update(4299);assert(g.spinning()&&g.winner()==0);g.update(4300);
 assert(!g.spinning()&&g.state()==State::Done&&g.winner()==2);
 assert(std::fabs(g.angle(4300)-TAU/4)<.0001f);
 assert(g.start(5000,rng)&&g.winner()==0&&calls==2);
}
''')

    def test_player_limits_all_results_and_rejection_sampling(self):
        self.run_cpp(r'''
int main(){
 using namespace player_game;Game g;assert(!g.configure(1)&&!g.configure(13)&&g.players()==4);
 for(int players=2;players<=12;++players)for(int result=1;result<=players;++result){
   assert(g.configure(players));auto rng=[&](){return uint32_t(players*100+result-1);};
   assert(g.start(50,rng));g.update(50+SPIN_MS);assert(g.winner()==result);
   const float angle=g.angle(9999);
   assert(std::fabs(angle-(result-1)*TAU/players)<.0001f);
 }
 g.configure(12);unsigned calls=0;auto rng=[&](){return ++calls==1?0U:20U;};
 assert(g.start(0,rng)&&calls==2);g.update(SPIN_MS);assert(g.winner()==9);
}
''')

    def test_cancel_preserves_pose_and_wraparound(self):
        self.run_cpp(r'''
int main(){
 using namespace player_game;Game g;auto rng=[](){return 100U;};g.start(0xfffffff0U,rng);
 g.update(0x00000054U);assert(g.spinning());const auto pose=g.angle(0x54U);
 g.cancel(0x54U);assert(!g.spinning()&&g.winner()==0&&g.angle(0x9999U)==pose);
 g.start(0xfffffff0U,rng);g.update(uint32_t(0xfffffff0U+SPIN_MS));
 assert(g.state()==State::Done&&g.winner()==1);
 g.cancel(9000);assert(g.state()==State::Done&&g.winner()==1);
 assert(g.configure(6)&&g.winner()==0&&g.angle(0)==0);
}
''')
