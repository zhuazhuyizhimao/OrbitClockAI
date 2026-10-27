#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace dice_game {
constexpr uint32_t ROLL_MS=2000;

class Button {
 public:
  bool down()const { return raw_||stable_; }
  void begin(bool pressed,uint32_t now) {
    raw_=stable_=pressed;changed_=down_=now;armed_=false;
  }
  bool update(bool pressed,uint32_t now) {
    if(pressed!=raw_) { raw_=pressed;changed_=now; }
    if(raw_==stable_ || uint32_t(now-changed_)<35)return false;
    stable_=raw_;
    if(stable_) { down_=now;armed_=true;return false; }
    const bool click=armed_&&uint32_t(now-down_)<800;
    armed_=false;return click;
  }
 private:
  bool raw_=false,stable_=false,armed_=false;
  uint32_t changed_=0,down_=0;
};

enum class Click { None,Single,Double };
class DoubleClick {
 public:
  static constexpr uint32_t WINDOW_MS=300;
  void reset(){pending_=secondDown_=false;}
  Click update(bool released,bool down,uint32_t now) {
    if(!pending_) {
      if(released){pending_=true;started_=now;secondDown_=false;}
      return Click::None;
    }
    const uint32_t elapsed=now-started_;
    if(down&&elapsed<=WINDOW_MS)secondDown_=true;
    if(released) {
      const bool twice=secondDown_||elapsed<=WINDOW_MS;reset();
      return twice?Click::Double:Click::Single;
    }
    if(elapsed>=WINDOW_MS&&!down){reset();return Click::Single;}
    return Click::None;
  }
 private:
  bool pending_=false,secondDown_=false;uint32_t started_=0;
};

class Shake {
 public:
  void reset(bool waitForQuiet=true) { have_=quiet_=hit_=false;armed_=!waitForQuiet;strength_=0; }
  bool armed() const { return armed_; }
  float strength() const { return strength_; }
  bool update(float x,float y,float z,uint32_t now) {
    if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)) { reset();return false; }
    const float magnitude=std::sqrt(x*x+y*y+z*z);
    if(magnitude<.15f||magnitude>12.f) { reset();return false; }
    if(!have_||uint32_t(now-last_)>350) {
      if(have_)reset();
      have_=true;gx_=px_=x;gy_=py_=y;gz_=pz_=z;last_=now;return false;
    }
    const uint32_t dt=now-last_;
    if(!dt)return false;
    const float dx=x-px_,dy=y-py_,dz=z-pz_;
    const float delta=std::sqrt(dx*dx+dy*dy+dz*dz);
    px_=x;py_=y;pz_=z;last_=now;
    const float alpha=float(dt)/(300.f+dt);
    gx_+=alpha*(x-gx_);gy_+=alpha*(y-gy_);gz_+=alpha*(z-gz_);
    const float ax=x-gx_,ay=y-gy_,az=z-gz_;
    strength_=std::sqrt(ax*ax+ay*ay+az*az);
    if(delta<.12f&&magnitude>.75f&&magnitude<1.25f) {
      if(!quiet_) { quiet_=true;quietAt_=now;qx_=x;qy_=y;qz_=z; }
      const float qdx=x-qx_,qdy=y-qy_,qdz=z-qz_;
      if(qdx*qdx+qdy*qdy+qdz*qdz>.0144f)quiet_=false;
      else if(uint32_t(now-quietAt_)>=300) { armed_=true;gx_=x;gy_=y;gz_=z;strength_=0;hit_=false; }
    } else quiet_=false;
    if(hit_&&uint32_t(now-hitAt_)>700)hit_=false;
    if(!armed_||strength_<.28f)return false;
    if(!hit_) { hit_=true;hitAt_=now;hx_=ax;hy_=ay;hz_=az;hitStrength_=strength_;return false; }
    // Two opposed dynamic-acceleration peaks constitute an intentional shake.
    // The high-pass gravity estimate preserves a moderate 2-4 Hz hand shake,
    // even when adjacent 50 Hz samples differ by much less than 0.65 g.
    if(uint32_t(now-hitAt_)<80||ax*hx_+ay*hy_+az*hz_>-.25f*strength_*hitStrength_)return false;
    reset();return true;
  }
 private:
  bool have_=false,armed_=false,quiet_=false,hit_=false;
  float px_=0,py_=0,pz_=0,gx_=0,gy_=0,gz_=0,qx_=0,qy_=0,qz_=0,hx_=0,hy_=0,hz_=0,hitStrength_=0,strength_=0;
  uint32_t last_=0,quietAt_=0,hitAt_=0;
};

template<class Rng>
uint8_t uniformFace(uint8_t faces,Rng &random) {
  const uint32_t threshold=(uint32_t(0)-uint32_t(faces))%faces;
  uint32_t value;
  do { value=random(); } while(value<threshold);
  return uint8_t(value%faces+1);
}

class Game {
 public:
  bool configure(uint8_t count,uint8_t faces) {
    if(count<1||count>3||faces<2||faces>8)return false;
    count_=faces==2?1:count;faces_=faces;rolling_=result_=false;shake_.reset(false);return true;
  }
  void enter() { active_=true;settings_=rolling_=result_=false;shake_.reset(false); }
  void leave() { active_=settings_=rolling_=false;shake_.reset(); }
  void settings(bool open) {
    settings_=open;
    if(rolling_) { rolling_=false;result_=false; }
    shake_.reset(open||result_);
  }
  bool sample(float x,float y,float z,uint32_t now,bool touching=false) {
    if(!active_||settings_||rolling_)return false;
    if(touching) { shake_.reset(result_);return false; }
    if(!shake_.update(x,y,z,now))return false;
    rolling_=true;result_=false;started_=now;return true;
  }
  template<class Rng>
  bool tick(uint32_t now,Rng &random) {
    if(!rolling_||uint32_t(now-started_)<ROLL_MS)return false;
    for(uint8_t i=0;i<count_;++i)values_[i]=uniformFace(faces_,random);
    rolling_=false;result_=true;shake_.reset();return true;
  }
  uint8_t displayed(uint8_t i,uint32_t now) const {
    if(i>=count_)return 0;
    if(rolling_) {
      uint32_t h=uint32_t(now-started_)/80+0x9e3779b9U*(i+1);
      h^=h>>16;h*=0x7feb352dU;h^=h>>15;
      return uint8_t(h%faces_+1);
    }
    return result_?values_[i]:0;
  }
  uint8_t total() const {
    uint8_t total=0;if(result_)for(uint8_t i=0;i<count_;++i)total+=values_[i];return total;
  }
  uint8_t count() const { return count_; }
  uint8_t faces() const { return faces_; }
  bool rolling() const { return rolling_; }
  bool hasResult() const { return result_; }
  bool armed() const { return shake_.armed(); }
  float shakeStrength() const { return shake_.strength(); }
  uint32_t elapsed(uint32_t now) const { return now-started_; }
 private:
  uint8_t count_=1,faces_=6,values_[3]={};
  bool active_=false,settings_=false,rolling_=false,result_=false;
  uint32_t started_=0;
  Shake shake_;
};
}
