#pragma once
#include <cmath>
#include <cstdint>
#include "dice_game_model.h"

namespace player_game {
constexpr uint8_t MIN_PLAYERS=2,MAX_PLAYERS=12;
constexpr uint32_t FAST_MS=3000,SLOW_MS=1200,SPIN_MS=FAST_MS+SLOW_MS;
constexpr float TAU=6.28318530718f;
enum class State { Idle,Spinning,Done };
inline float normalize(float angle){return angle-std::floor(angle/TAU)*TAU;}
class Game {
 public:
  bool configure(uint8_t players) {
    if(players<MIN_PLAYERS||players>MAX_PLAYERS)return false;
    players_=players;state_=State::Idle;winner_=0;pose_=0;return true;
  }
  template<class Rng>bool start(uint32_t now,Rng &random) {
    if(spinning())return false;
    winner_=dice_game::uniformFace(players_,random);
    const float target=(winner_-1)*TAU/players_;
    distance_=10*TAU+normalize(target-pose_);started_=now;state_=State::Spinning;return true;
  }
  void update(uint32_t now) {
    if(spinning()&&uint32_t(now-started_)>=SPIN_MS){pose_=(winner_-1)*TAU/players_;state_=State::Done;}
  }
  float angle(uint32_t now)const {
    if(!spinning())return pose_;
    const uint32_t elapsed=uint32_t(now-started_);
    if(elapsed>=SPIN_MS)return (winner_-1)*TAU/players_;
    const float fast=FAST_MS/1000.f,slow=SLOW_MS/1000.f;
    const float speed=distance_/(fast+slow*.5f);
    const float t=elapsed/1000.f;
    const float part=t<=fast?t:fast+(t-fast)-(t-fast)*(t-fast)/(2*slow);
    return normalize(pose_+speed*part);
  }
  void cancel(uint32_t now) {
    if(!spinning())return;
    pose_=angle(now);state_=State::Idle;winner_=0;
  }
  bool spinning()const{return state_==State::Spinning;}
  bool slowing(uint32_t now)const{return spinning()&&uint32_t(now-started_)>=FAST_MS;}
  State state()const{return state_;}
  uint8_t players()const{return players_;}
  uint8_t winner()const{return state_==State::Done?winner_:0;}
 private:
  State state_=State::Idle;uint8_t players_=4,winner_=0;uint32_t started_=0;
  float pose_=0,distance_=0;
};
} // namespace player_game
