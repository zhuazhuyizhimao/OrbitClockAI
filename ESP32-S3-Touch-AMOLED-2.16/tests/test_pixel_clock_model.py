"""Run the board's animation and IMU-trigger math with a native C++ compiler."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which("clang++") or shutil.which("g++")


@unittest.skipUnless(COMPILER, "a native C++ compiler is needed for pixel math checks")
class PixelClockModelTests(unittest.TestCase):
    def test_reassembly_drag_restore_and_wraparound(self):
        code = r'''
#include "pixel_clock_model.h"
#include <cassert>
#include <cstdio>
#include <limits>
using namespace orbit_pixels;
int main() {
  Clock clock;
  char value[6];
  for(int minute=0; minute<1440; ++minute) {
    std::snprintf(value,sizeof(value),"%02d:%02d",minute/60,minute%60);
    clock.setTime(value,minute*1000);
    int count=clock.tileCount(minute*1000+FLIP_MS+40);
    assert(count>0 && count<=MAX_TILES);
    for(int i=0;i<count;++i) {
      Tile t=clock.tile(i,minute*1000+FLIP_MS+40);
      assert(std::isfinite(t.x)&&std::isfinite(t.y));
      assert(t.x>=84&&t.x<=396&&t.y>=194&&t.y<=278);
      assert(t.opacity==1&&t.size==10);
    }
  }
  clock.setTime("12:34",2000000);
  Point expected[MAX_TILES]; int count=makePoints("12:34",expected);
  for(int i=0;i<count;++i) {
    Tile t=clock.tile(i,2000000+FLIP_MS+40);
    assert(t.x==expected[i].x&&t.y==expected[i].y);
  }
  const uint32_t start=2010000, rest=start+FLIP_MS+40;
  clock.flip(start); assert(clock.active(start+100));
  assert(!clock.active(rest));
  Tile atRest=clock.tile(0,rest);
  clock.press(200,220,rest+100);clock.drag(320,280);
  assert(clock.dragged());
  Tile bent=clock.tile(0,rest+110);
  assert(std::fabs(bent.x-atRest.x)+std::fabs(bent.y-atRest.y)>1);
  clock.release(rest+200);assert(clock.active(rest+300));
  Tile restored=clock.tile(0,rest+1000);
  assert(!clock.active(rest+1000));
  assert(std::fabs(restored.x-atRest.x)<.001&&std::fabs(restored.y-atRest.y)<.001);
  clock.press(200,220,rest+1100);clock.drag(202,221);clock.release(rest+1110);
  assert(!clock.dragged()); // short taps remain available for hour-format changes
  for(int i=0;i<600;++i) {
    std::snprintf(value,sizeof(value),"%02d:%02d",i/60%24,i%60);
    clock.setTime(value,2030000+i*11,true);
    for(int j=0;j<clock.tileCount(2030000+i*11+5);++j) {
      Tile t=clock.tile(j,2030000+i*11+5);
      assert(std::isfinite(t.x)&&std::isfinite(t.y)&&t.opacity>=0&&t.opacity<=1);
    }
  }
  uint32_t wrap=std::numeric_limits<uint32_t>::max()-100;
  clock.flip(wrap);assert(clock.active(100));assert(!clock.active(2000));

}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cpp").write_text(code)
            subprocess.run([COMPILER, "-std=c++11", "-I" + str(ROOT / "examples/arduino/08_OrbitClock"),
                            str(path / "test.cpp"), "-o", str(path / "check")], check=True, capture_output=True)
            subprocess.run([str(path / "check")], check=True, timeout=10, capture_output=True)

    def _run_native(self, code):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "test.cpp").write_text('#include "pixel_clock_model.h"\n#include <cassert>\n'
                                           'using namespace orbit_pixels;\n' + code)
            subprocess.run([COMPILER, "-std=c++11", "-I" + str(ROOT / "examples/arduino/08_OrbitClock"),
                            str(path / "test.cpp"), "-o", str(path / "check")], check=True, capture_output=True)
            subprocess.run([str(path / "check")], check=True, timeout=10, capture_output=True)

    def test_extra_particles_spread_fade_and_stop(self):
        self._run_native(r'''
int main() {
  Clock clock; clock.setTime("12:34",0);
  assert(clock.particleCount(0)==0);
  clock.flip(1000);assert(clock.particleCount(1000)==EXTRA_PARTICLES);
  float left=480,right=0,top=480,bottom=0;int visible=0;
  for(int i=0;i<EXTRA_PARTICLES;++i) {
    Tile p=clock.particle(i,1550);
    assert(std::isfinite(p.x)&&std::isfinite(p.y)&&p.opacity>=0&&p.opacity<=1);
    assert(p.x>=18&&p.x<=462&&p.y>=70&&p.y<=395);
    if(p.opacity>.1f)++visible;
    left=std::fmin(left,p.x);right=std::fmax(right,p.x);
    top=std::fmin(top,p.y);bottom=std::fmax(bottom,p.y);
    assert(clock.particle(i,2190).opacity<.001f);
  }
  assert(visible>150&&right-left>350&&bottom-top>180);
  assert(clock.particleCount(1000+FLIP_MS)==0&&!clock.active(1000+FLIP_MS));
  clock.press(200,220,3000);clock.drag(330,280);
  assert(clock.particleCount(3050)==EXTRA_PARTICLES);
  clock.release(3100);assert(clock.particleCount(3200)==EXTRA_PARTICLES);
  assert(clock.particleCount(3100+SETTLE_MS)==0);
  clock.press(200,220,5000);clock.release(5010);
  assert(clock.particleCount(5010)==0);
}
''')

    def test_orientation_and_pixel_touch_rotation(self):
        self._run_native(r'''
int main() {
  for(int r=0;r<4;++r)for(int y=0;y<480;y+=7)for(int x=0;x<480;x+=11) {
    PixelPoint panel=rotateToPanel(x,y,r),logical=rotateToPanel(panel.x,panel.y,(4-r)%4);
    assert(logical.x==x&&logical.y==y);
  }
  for(int r=0;r<4;++r) {
    bool seen[17*24]={};
    int source[17*24],destination[17*24];
    for(int i=0;i<17*24;++i)source[i]=i;
    rotatePixelBuffer(source,destination,17,24,r);
    for(int y=0;y<24;++y)for(int x=0;x<17;++x) {
      int index=rotatedBufferIndex(x,y,17,24,r);
      assert(index>=0&&index<17*24&&!seen[index]);seen[index]=true;
      assert(destination[index]==y*17+x);
      PixelPoint panel=rotateToPanel(30+x,70+y,r);
      PixelPoint a=rotateToPanel(30,70,r),b=rotateToPanel(46,93,r);
      int minx=a.x<b.x?a.x:b.x,miny=a.y<b.y?a.y:b.y;
      int outputWidth=(r&1)?24:17;
      assert(index==(panel.y-miny)*outputWidth+panel.x-minx);
    }
  }
}
''')

    def test_calibration_handles_arbitrary_sensor_mounting_and_restart(self):
        self._run_native(r'''
static void feed(Orientation &o,Vector3 v,uint32_t &now,int count,float gyro=0) {
  for(int i=0;i<count;++i) {now+=50;o.updateGyro(gyro,0,0,now);o.updateAccel(v.x,v.y,v.z,now);}
}
int main() {
  const int orders[6][3]={{0,1,2},{0,2,1},{1,0,2},{1,2,0},{2,0,1},{2,1,0}};
  for(int order=0;order<6;++order)for(int mask=0;mask<8;++mask) {
    float up[3]={.2f,.9f,.4f},right[3]={-.6f,.3f,.7f};
    float av[3]={},bv[3]={};
    for(int axis=0;axis<3;++axis) {
      av[axis]=up[orders[order][axis]]*(mask&(1<<axis)?-1:1);
      bv[axis]=right[orders[order][axis]]*(mask&(1<<axis)?-1:1);
    }
    Vector3 a={av[0],av[1],av[2]},b={bv[0],bv[1],bv[2]};
    normalize(a);const float overlap=dot(a,b);
    b.x-=overlap*a.x;b.y-=overlap*a.y;b.z-=overlap*a.z;normalize(b);
    Orientation o;uint32_t now=0;
    feed(o,b,now,40);assert(!o.calibrated()&&o.quarter()==0);
    feed(o,a,now,25);assert(o.captureUpright(now));
    assert(!o.captureClockwise(now));
    feed(o,a,now,25);assert(!o.captureClockwise(now)); // same pose is not a quarter turn
    feed(o,b,now,25);assert(o.captureClockwise(now)&&o.quarter()==1);
    auto saved=o.calibration();
    const Vector3 directions[4]={a,b,{-a.x,-a.y,-a.z},{-b.x,-b.y,-b.z}};
    for(int r=2;r<=4;++r) {
      feed(o,directions[r%4],now,80);assert(o.quarter()==r%4);
      const auto panel=rotateToPanel(240,120,o.quarter());
      const auto viewer=rotateToPanel(panel.x,panel.y,(4-r)%4);
      assert(viewer.x==240&&viewer.y==120);
    }
    Orientation restored;assert(restored.loadCalibration(saved));
    feed(restored,b,now,25);assert(restored.quarter()==1);
  }
}
''')

    def test_orientation_requires_still_cardinal_pose_and_cooldown(self):
        self._run_native(r'''
static void feed(Orientation &o,float x,float y,float z,uint32_t &now,int count,float rate=0) {
  for(int i=0;i<count;++i){now+=50;o.updateGyro(0,0,rate,now);o.updateAccel(x,y,z,now);}
}
int main() {
  Orientation o;uint32_t now=0;
  assert(o.loadCalibration({{0,1,0},{-1,0,0}}));
  feed(o,0,1,0,now,25);assert(o.quarter()==0);
  feed(o,-1,0,0,now,12);assert(o.quarter()==0); // a short turn must not switch
  feed(o,0,1,0,now,25);assert(o.quarter()==0);
  feed(o,-.707f,.707f,0,now,80);assert(o.quarter()==0); // diagonal dead zone
  feed(o,-1,0,0,now,50,30);assert(o.quarter()==0&&!o.stable(now));
  feed(o,-1,0,0,now,25);assert(o.quarter()==1&&o.readyToApply(now));
  feed(o,0,1,0,now,25);assert(o.quarter()==1); // three-second hold after changing
  feed(o,0,1,0,now,40);assert(o.quarter()==0);
  feed(o,0,0,1,now,80,120);assert(o.quarter()==0&&!o.stable(now)&&!o.readyToApply(now)); // flat: do not integrate yaw
  feed(o,-1,0,0,now,12);assert(o.quarter()==0);
  now+=1000;o.updateGyro(0,0,0,now);o.updateAccel(-1,0,0,now);assert(o.quarter()==0);
  feed(o,-1,0,0,now,12);assert(o.quarter()==0); // a sampling gap resets the dwell
  feed(o,4,0,0,now,1);assert(!o.stable(now)); // impact
  feed(o,-1,0,0,now,12);assert(o.quarter()==0);
  feed(o,0,0,0,now,1);assert(!o.stable(now)); // free fall
  feed(o,-1,0,0,now,25);assert(o.quarter()==1);
  const auto old=o.calibration();
  feed(o,0,-1,0,now,25);assert(o.captureUpright(now));
  o.cancelCalibration();feed(o,1,0,0,now,25);assert(!o.captureClockwise(now));
  assert(dot(old.upright,o.calibration().upright)>.999f);
}
''')

    def test_orientation_rejects_invalid_calibration_and_stale_gyro(self):
        self._run_native(r'''
int main() {
  Orientation o;
  assert(!o.loadCalibration({{0,0,0},{0,1,0}}));
  assert(!o.loadCalibration({{0,1,0},{0,1,0}}));
  assert(!o.calibrated());
  assert(o.loadCalibration({{0,1,0},{-1,0,0}}));
  for(int i=0;i<80;++i)o.updateAccel(-1,0,0,i*50);
  assert(o.quarter()==0&&!o.stable(3950));
  uint32_t start=0xffffffffu-500;
  for(int i=0;i<30;++i) {
    const uint32_t now=start+i*50;o.updateGyro(0,0,0,now);o.updateAccel(-1,0,0,now);
  }
  assert(o.quarter()==1); // dwell survives millis rollover
}
''')

    def test_calibration_uses_still_pose_before_tap_and_expires_it(self):
        self._run_native(r'''
int main() {
  Orientation o;uint32_t now=0;
  for(int i=0;i<26;++i){now+=50;o.updateGyro(0,0,0,now);o.updateAccel(0,1,0,now);}
  assert(o.captureReady(now));
  now+=50;o.updateGyro(0,0,25,now);o.updateAccel(0,1,0,now);
  assert(!o.stable(now)&&o.captureUpright(now)); // a tap may nudge a steady board
  assert(!o.captureClockwise(now));
  for(int i=0;i<26;++i){now+=50;o.updateGyro(0,0,0,now);o.updateAccel(-1,0,0,now);}
  assert(o.captureReady(now));
  now+=400;assert(!o.captureClockwise(now)); // old pose is never accepted indefinitely
}
''')


if __name__ == "__main__":
    unittest.main()
