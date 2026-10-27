"""Touch packet gaps, sand gesture boundaries and measured coordinate calibration."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

BOARD=Path(__file__).resolve().parents[1]/'examples/esp-idf/06_OrbitClockAI/board'
COMPILER=shutil.which('clang++') or shutil.which('g++')

@unittest.skipUnless(COMPILER,'a native C++ compiler is needed')
class TouchStateTests(unittest.TestCase):
    def run_cpp(self,body):
        prefix='#include <cassert>\n#include "orbit_touch_filter.h"\n#include "orbit_sand_gesture.h"\n#include "orbit_touch_mapping.h"\n'
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'test.cpp').write_text(prefix+body)
            result=subprocess.run([COMPILER,'-std=c++17','-I'+str(BOARD),str(p/'test.cpp'),'-o',str(p/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(p/'test')],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_missing_reports_and_release_bounce_do_not_split_a_hold(self):
        self.run_cpp(r'''
int main(){using namespace orbit_touch;ContactFilter f;
 assert(f.Update(Report::Contact,240,220,0).down);
 for(unsigned t=20;t<=2200;t+=20){
  auto report=t%100==20?Report::Missing:t%100==40?Report::Released:Report::Contact;
  const auto p=f.Update(report,240,220,t);assert(p.down&&p.x==240&&p.y==220);
 }
 assert(f.Update(Report::Released,0,0,2220).down);
 assert(f.Update(Report::Missing,0,0,2299).down);
 const auto released=f.Update(Report::Missing,0,0,2300);
 assert(!released.down&&released.x==240&&released.y==220);
 f.Update(Report::Contact,100,200,2400);
 assert(f.Update(Report::Missing,0,0,3599).down);
 assert(!f.Update(Report::Missing,0,0,3600).down);
 f.Reset();
 for(unsigned t=0;t<=3000;t+=20)assert(f.Update(t%1000?Report::Missing:Report::Contact,1,2,t).down);
 f.Reset();f.Update(Report::Contact,1,2,0xfffffff0U);
 assert(f.Update(Report::Missing,0,0,0x49fU).down);
 assert(!f.Update(Report::Missing,0,0,0x4a0U).down);
}
''')

    def test_toggle_requires_a_new_press_after_500ms_and_hold_has_priority(self):
        self.run_cpp(r'''
int main(){using namespace orbit_sand;Gesture g;
 g.Begin(Target::Page,240,220,0);assert(g.End(240,220,100,true)==Action::Toggle);
 g.Begin(Target::Page,240,220,599);assert(g.End(240,220,700,true)==Action::None);
 g.Begin(Target::Page,240,220,701);assert(g.End(240,220,750,true)==Action::Toggle);
 g.Begin(Target::Page,240,220,800);assert(g.Move(240,220,2799)==Action::None);
 assert(g.Move(240,220,2800)==Action::Settings);
 assert(g.Move(240,220,3000)==Action::None);
 assert(g.End(240,220,3100,true)==Action::None);
 g.Reset();g.Begin(Target::Page,240,220,100);
 assert(g.End(240,220,2100,true)==Action::Settings);
 g.Reset();g.Begin(Target::Page,240,220,0xffffff00U);
 assert(g.End(240,220,0xffffff80U,true)==Action::Toggle);
 g.Begin(Target::Page,240,220,0x173U);assert(g.End(240,220,0x174U,true)==Action::None);
 g.Begin(Target::Page,240,220,0x174U);assert(g.End(240,220,0x180U,true)==Action::Toggle);
}
''')

    def test_drag_cancellation_reset_and_exact_hold_boundary(self):
        self.run_cpp(r'''
int main(){using namespace orbit_sand;Gesture g;
 g.Begin(Target::Reset,240,412,0);assert(g.End(240,412,50,true)==Action::Reset);
 g.Begin(Target::Reset,240,412,100);assert(g.End(240,440,200,false)==Action::None);
 g.Begin(Target::Page,240,220,300);assert(g.Move(280,220,400)==Action::None);
 assert(g.Move(240,220,2300)==Action::None);assert(g.End(240,220,2400,true)==Action::None);
 g.Begin(Target::Page,240,220,2500);g.Cancel();assert(g.End(240,220,2600,true)==Action::None);
 g.Begin(Target::Page,240,220,2700);assert(g.Move(254,224,4699)==Action::None);
 assert(g.Move(254,224,4700)==Action::Settings);
}
''')

    def test_three_measured_targets_correct_all_eight_axis_orientations(self):
        self.run_cpp(r'''
int main(){using namespace orbit_touch;
 for(int mode=0;mode<8;++mode){
  auto raw=[&](int x,int y){if(mode&1)std::swap(x,y);if(mode&2)x=479-x;if(mode&4)y=479-y;return orbit_panel::Point{x,y};};
  Calibrator c;Mapping m;
  for(const auto &target:Calibrator::TARGETS){auto p=raw(target.x,target.y);assert(c.Add(p.x,p.y));}
  assert(c.Solve(m)&&m.Valid());
  for(int y=0;y<480;y+=17)for(int x=0;x<480;x+=17){auto p=raw(x,y);auto actual=m.Apply(p.x,p.y);assert(actual.x==x&&actual.y==y);}
 }
}
''')

    def test_invalid_calibration_never_replaces_a_valid_mapping(self):
        self.run_cpp(r'''
int main(){using namespace orbit_touch;Mapping m;assert(!m.Valid());
 Calibrator good;for(auto p:Calibrator::TARGETS)good.Add(p.x,p.y);assert(good.Solve(m));
 const auto old=m;Calibrator bad;assert(!bad.Add(-1,0));
 bad.Add(100,100);bad.Add(100,100);bad.Add(100,100);assert(!bad.Solve(m));
 assert(m.a==old.a&&m.e==old.e&&m.magic==old.magic);
 bad.Reset();bad.Add(10,10);bad.Add(20,20);bad.Add(30,30);assert(!bad.Solve(m));
 m.a=NAN;assert(!m.Valid());m=old;m.magic=0;assert(!m.Valid());
}
''')

    def test_mapping_storage_requires_valid_blob_and_successful_commit(self):
        fake=r"""
#pragma once
#include <cassert>
#include <cstring>
#include <cstddef>
using nvs_handle_t=int;
constexpr int ESP_OK=0,NVS_READONLY=0,NVS_READWRITE=1;
inline unsigned char blob[128];inline size_t blob_size=0;inline bool fail_commit=false;
inline int nvs_open(const char* name,int,nvs_handle_t* h){assert(!strcmp(name,"orbit-touch"));*h=1;return ESP_OK;}
inline int nvs_get_blob(int,const char* key,void* out,size_t* size){
 assert(!strcmp(key,"matrix"));if(!blob_size||*size<blob_size)return -1;
 memcpy(out,blob,blob_size);*size=blob_size;return ESP_OK;
}
inline int nvs_set_blob(int,const char* key,const void* data,size_t size){assert(!strcmp(key,"matrix"));memcpy(blob,data,size);blob_size=size;return ESP_OK;}
inline int nvs_commit(int){return fail_commit?-1:ESP_OK;}inline void nvs_close(int){}
"""
        code=r"""
#include "orbit_touch_store.h"
namespace orbit_touch {Mapping mapping;}
int main(){using namespace orbit_touch;LoadCalibration();assert(!mapping.Valid());
 Calibrator c;for(auto p:Calibrator::TARGETS)c.Add(p.x,p.y);Mapping candidate;assert(c.Solve(candidate));
 assert(SaveCalibration(candidate)&&mapping.Valid());mapping={};LoadCalibration();assert(mapping.Valid());
 const auto previous=mapping;candidate.c=12;fail_commit=true;
 assert(!SaveCalibration(candidate)&&mapping.c==previous.c);
 mapping={};blob_size=1;LoadCalibration();assert(!mapping.Valid());
 blob_size=sizeof(candidate);memset(blob,0,sizeof(candidate));LoadCalibration();assert(!mapping.Valid());
}
"""
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory);(p/'nvs.h').write_text(fake);(p/'test.cpp').write_text(code)
            result=subprocess.run([COMPILER,'-std=c++17','-I'+str(p),'-I'+str(BOARD),str(p/'test.cpp'),'-o',str(p/'test')],capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)
            result=subprocess.run([str(p/'test')],capture_output=True,text=True,timeout=10)
            self.assertEqual(result.returncode,0,result.stderr)
