#pragma once
#include <cstdint>

namespace sand_timer {
constexpr uint16_t MIN_SECONDS=1,MAX_SECONDS=3600;
enum class State { Idle,Running,Paused,Done };
class Countdown {
 public:
  bool configure(uint16_t seconds) {
    if(seconds<MIN_SECONDS||seconds>MAX_SECONDS)return false;
    seconds_=seconds;reset();return true;
  }
  void reset(){remaining_=uint32_t(seconds_)*1000;state_=State::Idle;}
  void update(uint32_t now) {
    if(state_!=State::Running)return;
    const uint32_t elapsed=now-last_;last_=now;
    if(elapsed>=remaining_){remaining_=0;state_=State::Done;}
    else remaining_-=elapsed;
  }
  void toggle(uint32_t now) {
    update(now);
    if(state_==State::Running){state_=State::Paused;return;}
    if(state_==State::Done)reset();
    last_=now;state_=State::Running;
  }
  void pause(uint32_t now){update(now);if(state_==State::Running)state_=State::Paused;}
  State state()const{return state_;}
  uint16_t seconds()const{return seconds_;}
  uint16_t remainingSeconds()const{return uint16_t((remaining_+999)/1000);}
  uint32_t remainingMs()const{return remaining_;}
  float fraction()const{return float(remaining_)/(uint32_t(seconds_)*1000);}
 private:
  uint16_t seconds_=60;uint32_t remaining_=60000,last_=0;State state_=State::Idle;
};
} // namespace sand_timer
