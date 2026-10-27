#pragma once
#include <cmath>
#include <cstdint>
#include <cstring>

// Pure animation math shared by the board UI and the native rendering checks.
namespace orbit_pixels {
constexpr int MAX_TILES = 160;
constexpr int EXTRA_PARTICLES = 192;
constexpr uint32_t FLIP_MS = 1200;
constexpr uint32_t SETTLE_MS = 720;
struct Point { float x, y; };
struct Tile { float x, y, size, opacity; bool accent; };
inline float clamp(float x, float low, float high) { return x < low ? low : (x > high ? high : x); }
inline float ease(float t) { return t * t * (3 - 2 * t); }

static const uint8_t DIGITS[12][7] = {
  {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14},
  {14,17,1,2,4,8,31}, {30,1,1,14,1,1,30},
  {2,6,10,18,31,2,2}, {31,16,16,30,1,1,30},
  {14,16,16,30,17,17,14}, {31,1,2,4,8,8,8},
  {14,17,17,14,17,17,14}, {14,17,17,15,1,1,14},
  {0,0,4,0,4,0,0}, {0,0,0,31,0,0,0}
};

inline int makePoints(const char *value, Point *points) {
  const int offsets[5] = {0,72,144,180,252};
  int count = 0;
  for (int c = 0; c < 5 && value[c]; ++c) {
    const int glyph = value[c] >= '0' && value[c] <= '9' ? value[c] - '0' : (value[c] == ':' ? 10 : 11);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if (!(DIGITS[glyph][row] & (1 << (4 - column)))) continue;
        if (count >= MAX_TILES) return count;
        // Colon is one square wide, while digit glyphs are five squares wide.
        points[count++] = {84.0f + offsets[c] + (c == 2 ? 0 : column * 12.0f) + 5,
                           194.0f + row * 12.0f + 5};
      }
    }
  }
  return count;
}

class Clock {
 public:
  void setTime(const char *value, uint32_t now, bool force = false) {
    if (!force && initialized_ && std::strncmp(value, value_, 5) == 0) return;
    Point next[MAX_TILES];
    const int count = makePoints(value, next);
    if (!initialized_) {
      toCount_ = count;
      std::memcpy(to_, next, count * sizeof(Point));
      std::memcpy(from_, next, count * sizeof(Point));
      fromCount_ = count;
      initialized_ = true;
      flipping_ = false;
    } else {
      // Capture the current base pose so rapid repeated triggers do not jump.
      Point old[MAX_TILES];
      const int oldCount = tileCount(now);
      for (int i = 0; i < oldCount; ++i) old[i] = baseTile(i, now);
      fromCount_ = oldCount;
      std::memcpy(from_, old, oldCount * sizeof(Point));
      toCount_ = count;
      std::memcpy(to_, next, count * sizeof(Point));
      started_ = now;
      flipping_ = true;
    }
    std::strncpy(value_, value, 5);
    value_[5] = 0;
  }
  void flip(uint32_t now) {
    if (initialized_) {
      char value[6]; std::memcpy(value, value_, sizeof(value));
      setTime(value, now, true);
    }
  }
  void press(float x, float y, uint32_t now) {
    pressed_ = now;
    downX_ = x; downY_ = y;
    dx_ = 0; dy_ = 0;
    held_ = true; moved_ = false; settling_ = false;
  }
  void drag(float x, float y) {
    if (!held_) return;
    dx_ = clamp(x - downX_, -160, 160);
    dy_ = clamp(y - downY_, -140, 140);
    if (std::fabs(dx_) + std::fabs(dy_) > 10) moved_ = true;
  }
  void release(uint32_t now) {
    if (!held_) return;
    held_ = false;
    released_ = now;
    settling_ = moved_;
    if (!moved_) { dx_ = 0; dy_ = 0; }
  }
  void cancelDrag() { held_ = false; settling_ = false; dx_ = 0; dy_ = 0; }
  bool dragged() const { return moved_; }
  bool active(uint32_t now) const {
    return held_ || (flipping_ && uint32_t(now - started_) < FLIP_MS)
           || (settling_ && uint32_t(now - released_) < SETTLE_MS);
  }
  int tileCount(uint32_t now) const {
    return flipping_ && uint32_t(now - started_) < FLIP_MS
      ? (fromCount_ > toCount_ ? fromCount_ : toCount_) : toCount_;
  }
  int particleCount(uint32_t now) const {
    if (!initialized_) return 0;
    return ((flipping_ && uint32_t(now-started_) < FLIP_MS)
      || (moved_ && (held_ || (settling_ && uint32_t(now-released_) < SETTLE_MS))))
      ? EXTRA_PARTICLES : 0;
  }
  Tile particle(int i, uint32_t now) const {
    const bool burst = flipping_ && uint32_t(now-started_) < FLIP_MS;
    const float angle = i * 2.39996323f;
    Point point = toCount_ ? to_[i % toCount_] : Point{240,236};
    float opacity, size, distance, turn = 0;
    if (burst) {
      const float t = clamp((float(uint32_t(now-started_)) - (i*53 % 180)) / 1000, 0, 1);
      distance = (35 + i%13*12) * (1 - (1-t)*(1-t));
      opacity = std::sin(t*3.14159265f) * (1-t*.3f) * .85f;
      size = (4 + i%4*2) * (1-t*.6f);
      point.y += t*t*28;
    } else {
      const float fade = held_ ? 1 : std::exp(-float(uint32_t(now-released_))/180);
      const float strength = clamp((std::fabs(dx_)+std::fabs(dy_))/150, 0, 1)*fade;
      distance = (25 + i%11*9)*strength;
      turn = float(uint32_t(now-pressed_))/1800 + strength*.7f;
      opacity = .65f*strength;
      size = 3 + i%4*2;
    }
    point.x += std::cos(angle+turn)*distance;
    point.y += std::sin(angle+turn)*distance*.8f;
    point = warp(point, now);
    return {clamp(point.x,18,462), clamp(point.y,70,395), size,
            clamp(opacity,0,1), i%3 != 0};
  }
  Tile tile(int i, uint32_t now) const {
    Point point = baseTile(i, now);
    const bool moving = flipping_ && uint32_t(now - started_) < FLIP_MS;
    const float t = progress(i, now);
    float opacity = 1;
    if (moving && i >= toCount_) opacity = 1 - t;
    else if (moving && i >= fromCount_) opacity = t;
    float scale = 1;
    point = warp(point, now, &scale);
    return {clamp(point.x,12,468), clamp(point.y,78,388),
            (10 + (moving ? std::sin(t*3.14159265f)*5 : 0))*scale,
            opacity, (i % 7 == 0) || (moving && t > .15f && t < .8f)};
  }
  Point warp(Point point, uint32_t now, float *scale = nullptr) const {
    float amplitude = held_ ? 1.0f : 0.0f;
    if (!held_ && settling_ && uint32_t(now - released_) < SETTLE_MS) {
      const float elapsed = uint32_t(now - released_);
      amplitude = std::exp(-elapsed / 155.0f) * std::cos(elapsed / 100.0f);
    }
    const float dx = dx_ * amplitude, dy = dy_ * amplitude;
    if (std::fabs(dx)+std::fabs(dy) < .0001f) {
      if (scale) *scale=1;
      return point;
    }
    float x = point.x - 240, y = point.y - 236;
    const float radius = std::sqrt(x * x + y * y);
    const float twist = (dx + dy) / 620 * (1 - clamp(radius / 250, 0, 1));
    const float tx = x * std::cos(twist) - y * std::sin(twist);
    y = x * std::sin(twist) + y * std::cos(twist); x = tx;
    const float yaw = dx / 260, pitch = -dy / 280;
    const float z = -x * std::sin(yaw);
    x *= std::cos(yaw);
    const float zz = y * std::sin(pitch) + z * std::cos(pitch);
    y = y * std::cos(pitch) - z * std::sin(pitch);
    const float perspective = 480 / (480 - zz);
    if (scale) *scale = perspective;
    return {240 + x * perspective + dx * .07f, 236 + y * perspective + dy * .05f};
  }
 private:
  float progress(int i, uint32_t now) const {
    if (!flipping_ || uint32_t(now - started_) >= FLIP_MS) return 1;
    return clamp((float(uint32_t(now - started_)) - (i * 37 % 160)) / 1040, 0, 1);
  }
  Point baseTile(int i, uint32_t now) const {
    const bool moving = flipping_ && uint32_t(now - started_) < FLIP_MS;
    if (!moving) return i < toCount_ ? to_[i] : Point{240,236};
    const float t = progress(i, now), u = ease(t), wave = std::sin(t * 3.14159265f);
    Point a = fromCount_ ? from_[i % fromCount_] : Point{240,236};
    Point b = i < toCount_ ? to_[i] : Point{a.x + (i % 2 ? 95.0f : -95.0f), a.y};
    if (i >= fromCount_) { a.x += i % 2 ? 70 : -70; a.y += i % 3 * 14 - 14; }
    const float direction = i % 2 ? 1 : -1;
    return {a.x + (b.x - a.x) * u + direction * wave * (40 + i % 7 * 10),
            a.y + (b.y - a.y) * u + wave * (float(i % 9) - 4) * 18};
  }
  Point from_[MAX_TILES] = {}, to_[MAX_TILES] = {};
  int fromCount_ = 0, toCount_ = 0;
  char value_[6] = {};
  uint32_t started_ = 0, released_ = 0, pressed_ = 0;
  float downX_ = 0, downY_ = 0, dx_ = 0, dy_ = 0;
  bool initialized_ = false, flipping_ = false, held_ = false, settling_ = false, moved_ = false;
};

// Counterclockwise quarter turns, matching LVGL's display/input rotation enum.
struct PixelPoint { int x, y; };
inline PixelPoint rotateToPanel(int x, int y, uint8_t quarter, int side=480) {
  switch (quarter & 3) {
    case 1: return {y, side-1-x};
    case 2: return {side-1-x, side-1-y};
    case 3: return {side-1-y, x};
    default: return {x,y};
  }
}
inline int rotatedBufferIndex(int x, int y, int width, int height, uint8_t quarter) {
  switch (quarter & 3) {
    case 1: return (width-1-x)*height+y;
    case 2: return (height-1-y)*width+width-1-x;
    case 3: return x*height+height-1-y;
    default: return y*width+x;
  }
}
template<typename Pixel>
inline void rotatePixelBuffer(const Pixel *source, Pixel *destination,
                              int width, int height, uint8_t quarter) {
  // Dispatch once per strip instead of branching for every transmitted pixel.
  switch (quarter & 3) {
    case 1:
      for (int row=0;row<height;++row) {
        int index=(width-1)*height+row;
        for (int column=0;column<width;++column,index-=height)
          destination[index]=*source++;
      }
      break;
    case 2:
      for (int index=width*height-1;index>=0;--index) destination[index]=*source++;
      break;
    case 3:
      for (int row=0;row<height;++row) {
        int index=height-1-row;
        for (int column=0;column<width;++column,index+=height)
          destination[index]=*source++;
      }
      break;
    default: std::memcpy(destination,source,width*height*sizeof(Pixel)); break;
  }
}
inline float wrapAngle(float angle) {
  while (angle > 180) angle-=360;
  while (angle <= -180) angle+=360;
  return angle;
}

// Two physical poses define the sensor-to-screen basis. No mounting-axis or
// startup-pose assumption is needed, and gyroscope rates only veto motion.
struct Vector3 { float x,y,z; };
struct OrientationCalibration { Vector3 upright,clockwise; };
inline float dot(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline bool normalize(Vector3 &v) {
  const float length=std::sqrt(dot(v,v));
  if (!std::isfinite(length) || length < .01f) return false;
  v.x/=length;v.y/=length;v.z/=length;return true;
}
class Orientation {
 public:
  static constexpr uint32_t STABLE_MS=1000, COOLDOWN_MS=3000;
  void updateGyro(float x,float y,float z,uint32_t now) {
    gyroAt_=now;gyroReady_=true;
    moving_=!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z)||x*x+y*y+z*z>12*12;
    if (moving_) resetSteady();
  }
  void updateAccel(float x,float y,float z,uint32_t now) {
    Vector3 next={x,y,z};
    const float magnitude=std::sqrt(dot(next,next));
    const bool gap=sampleReady_ && uint32_t(now-sampleAt_)>200;
    sampleAt_=now;sampleReady_=true;
    if (!std::isfinite(magnitude)||magnitude<.88f||magnitude>1.12f
        ||!gyroReady_||uint32_t(now-gyroAt_)>150||moving_) {
      sampleValid_=false;resetSteady();return;
    }
    normalize(next);latest_=next;sampleValid_=true;
    if (gap || !steady_ || dot(next,anchor_)<.9962f) {
      anchor_=next;steadyAt_=now;steady_=true;candidateReady_=false;
    }
    if (!calibrated_) { rememberStable(now);return; }
    const float vertical=dot(next,basis_.upright),horizontal=dot(next,basis_.clockwise);
    // Hold the existing page while flat or far from a calibrated cardinal pose.
    if (vertical*vertical+horizontal*horizontal < .80f*.80f) {
      resetSteady();return;
    }
    rememberStable(now);
    const float angle=std::atan2(horizontal,vertical)*57.2957795f;
    const uint8_t nearest=(int(std::floor((angle+45)/90))+4)%4;
    if (std::fabs(wrapAngle(angle-nearest*90))>18) { candidateReady_=false;return; }
    if (!candidateReady_ || candidate_!=nearest) {
      candidate_=nearest;candidateAt_=now;candidateReady_=true;
    }
    if (nearest!=quarter_ && stable(now) && uint32_t(now-candidateAt_)>=STABLE_MS
        && (!changed_ || uint32_t(now-changedAt_)>=COOLDOWN_MS)) {
      quarter_=nearest;changedAt_=now;changed_=true;
    }
  }
  bool stable(uint32_t now) const {
    return sampleValid_ && steady_ && gyroReady_ && !moving_
      && uint32_t(now-sampleAt_)<=150 && uint32_t(now-gyroAt_)<=150
      && uint32_t(now-steadyAt_)>=STABLE_MS;
  }
  bool readyToApply(uint32_t now) const {
    return calibrated_ && stable(now) && candidateReady_ && candidate_==quarter_;
  }
  bool captureUpright(uint32_t now) {
    if (!captureReady(now)) return false;
    pendingUpright_=capture_;pendingReady_=true;captureValid_=false;resetSteady();return true;
  }
  bool captureClockwise(uint32_t now) {
    if (!pendingReady_||!captureReady(now)) return false;
    Vector3 right=capture_;
    const float overlap=dot(pendingUpright_,right);
    if (std::fabs(overlap)>.35f) return false;
    right.x-=overlap*pendingUpright_.x;right.y-=overlap*pendingUpright_.y;right.z-=overlap*pendingUpright_.z;
    if (!normalize(right)) return false;
    const OrientationCalibration next={pendingUpright_,right};
    if (!loadCalibration(next)) return false;
    pendingReady_=false;captureValid_=false;hold(1,now);return true;
  }
  bool loadCalibration(OrientationCalibration next) {
    if (!normalize(next.upright)||!normalize(next.clockwise)||std::fabs(dot(next.upright,next.clockwise))>.05f)
      return false;
    basis_=next;calibrated_=true;resetSteady();return true;
  }
  const OrientationCalibration &calibration() const { return basis_; }
  bool calibrated() const { return calibrated_; }
  uint8_t quarter() const { return quarter_; }
  void hold(uint8_t quarter,uint32_t now) {
    quarter_=quarter&3;changed_=true;changedAt_=now;resetSteady();
  }
  bool captureReady(uint32_t now) const { return captureValid_ && uint32_t(now-captureAt_)<=300; }
  void cancelCalibration() { pendingReady_=false;captureValid_=false;resetSteady(); }
 private:
  void resetSteady() { steady_=false;candidateReady_=false; }
  void rememberStable(uint32_t now) {
    // Use the pose just before a screen tap, so the tap itself need not be motionless.
    if (stable(now)) { capture_=latest_;captureAt_=now;captureValid_=true; }
  }
  OrientationCalibration basis_={};
  Vector3 latest_={},anchor_={},pendingUpright_={},capture_={};
  uint32_t sampleAt_=0,gyroAt_=0,steadyAt_=0,candidateAt_=0,changedAt_=0,captureAt_=0;
  uint8_t quarter_=0,candidate_=0;
  bool calibrated_=false,pendingReady_=false,gyroReady_=false,moving_=false;
  bool sampleReady_=false,sampleValid_=false,steady_=false,candidateReady_=false,changed_=false,captureValid_=false;
};
}  // namespace orbit_pixels
