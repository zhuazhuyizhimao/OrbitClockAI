"""Validate the actual firmware's square-grid clock geometry for a full day."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER, 'a native C++ compiler is needed')
class RetroClockFaceTests(unittest.TestCase):
    def test_all_minutes_fit_the_grid_without_overlaps(self):
        code = r'''
#include "retro_clock_face.h"
#include <cassert>
#include <cstdio>
#include <set>
int main() {
  retro_clock::Dot dots[retro_clock::MAX_DOTS];char value[6];
  for(int minute=-1;minute<1440;++minute) {
    if(minute<0)std::snprintf(value,sizeof(value),"--:--");
    else std::snprintf(value,sizeof(value),"%02d:%02d",minute/60,minute%60);
    const int count=retro_clock::makeTimeDots(value,dots);
    assert(count>0&&count<=retro_clock::MAX_DOTS&&count%4==0);
    std::set<int> positions;
    for(int i=0;i<count;++i) {
      assert(dots[i].x>=116&&dots[i].x<=361&&dots[i].y>=356&&dots[i].y<=421);
      assert(dots[i].x%5==1&&dots[i].y%5==1);
      assert(positions.insert(dots[i].y*480+dots[i].x).second);
    }
  }
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory);(path/'check.cpp').write_text(code)
            subprocess.run([COMPILER,'-std=c++11','-I'+str(ROOT/'examples/arduino/08_OrbitClock'),
                            str(path/'check.cpp'),'-o',str(path/'check')],check=True,capture_output=True)
            subprocess.run([str(path/'check')],check=True,timeout=10,capture_output=True)
