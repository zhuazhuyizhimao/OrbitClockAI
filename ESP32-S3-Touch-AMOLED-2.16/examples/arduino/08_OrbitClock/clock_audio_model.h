#pragma once
#include <atomic>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace clock_audio {
enum class State : uint32_t { Unavailable, Idle, Playing, Failed };

class SingleFlight {
 public:
  void enable() { state_.store(State::Idle); }
  bool request() {
    State expected=State::Idle;
    return state_.compare_exchange_strong(expected,State::Playing);
  }
  void finish() {
    State expected=State::Playing;
    state_.compare_exchange_strong(expected,State::Idle);
  }
  void fail() { state_.store(State::Failed); }
  State state() const { return state_.load(); }
 private:
  std::atomic<State> state_{State::Unavailable};
};

constexpr size_t DMA_FRAMES=256;
constexpr size_t DMA_BUFFERS=4;
constexpr size_t CHUNK_FRAMES=256;
// Enqueue more than a full ring of zeros before muting or accepting another tap.
// This ensures the final sound samples have left DMA, not merely been queued.
constexpr size_t DRAIN_FRAMES=2*DMA_FRAMES*DMA_BUFFERS;

template<class Sink>
bool writeAll(Sink &sink,const int16_t *frames,size_t frameCount) {
  const auto *data=reinterpret_cast<const uint8_t *>(frames);
  size_t remaining=frameCount*2*sizeof(int16_t);
  while(remaining) {
    size_t written=0;
    if(!sink.writeSome(data,remaining,written) || !written || written>remaining || written%4) return false;
    data+=written;remaining-=written;
  }
  return true;
}

template<class Sink>
bool streamEffect(const int16_t *mono,size_t count,Sink &sink) {
  alignas(4) int16_t stereo[CHUNK_FRAMES*2];
  for(size_t offset=0;offset<count;) {
    const size_t frames=std::min(CHUNK_FRAMES,count-offset);
    for(size_t i=0;i<frames;++i)stereo[2*i]=stereo[2*i+1]=mono[offset+i];
    if(!writeAll(sink,stereo,frames))return false;
    offset+=frames;
  }
  std::fill(stereo,stereo+CHUNK_FRAMES*2,0);
  for(size_t remaining=DRAIN_FRAMES;remaining;) {
    const size_t frames=std::min(CHUNK_FRAMES,remaining);
    if(!writeAll(sink,stereo,frames))return false;
    remaining-=frames;
  }
  return true;
}
}
