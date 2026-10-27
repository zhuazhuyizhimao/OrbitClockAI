"""Exercise playback exclusion and the PCM stream used by the actual audio worker."""
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which('clang++') or shutil.which('g++')


@unittest.skipUnless(COMPILER, 'a native C++ compiler is needed')
class ClockAudioTests(unittest.TestCase):
    def run_cpp(self, body):
        code = '''#include "clock_audio_model.h"
#include <cassert>
#include <vector>
#include <thread>
#include <cstring>
using namespace clock_audio;
''' + body
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path/'check.cpp').write_text(code)
            subprocess.run([COMPILER, '-std=c++17', '-pthread',
                            '-I'+str(ROOT/'examples/arduino/08_OrbitClock'),
                            str(path/'check.cpp'), '-o', str(path/'check')],
                           check=True, capture_output=True)
            subprocess.run([str(path/'check')], check=True, timeout=20, capture_output=True)

    def test_simultaneous_and_pending_taps_only_start_one_playback(self):
        self.run_cpp(r'''
int main() {
  SingleFlight gate;
  assert(!gate.request());gate.enable();
  std::atomic<int> accepted{0};std::atomic<bool> start{false};
  std::vector<std::thread> threads;
  for(int i=0;i<16;++i)threads.emplace_back([&]{
    while(!start.load())std::this_thread::yield();
    for(int n=0;n<500;++n)if(gate.request())++accepted;
  });
  start.store(true);for(auto &thread:threads)thread.join();
  assert(accepted==1&&gate.state()==State::Playing);
  gate.finish();assert(gate.state()==State::Idle); // Busy taps have not queued a replay.
  assert(gate.request());gate.finish();assert(gate.request());
  gate.fail();gate.finish();assert(!gate.request()&&gate.state()==State::Failed);
}
''')

    def test_entire_clip_stereo_order_and_silence_drain_keep_the_gate_closed(self):
        self.run_cpp(r'''
struct Sink {
  SingleFlight &gate;std::vector<uint8_t> output;
  bool writeSome(const void *data,size_t size,size_t &written) {
    assert(!gate.request()); // Also applies throughout the final DMA drain.
    written=std::min(size,size_t(124)); // Exercise partial writes, in stereo frames.
    const auto *bytes=static_cast<const uint8_t*>(data);
    output.insert(output.end(),bytes,bytes+written);return true;
  }
};
int main() {
  std::vector<int16_t> mono(35200);
  for(size_t i=0;i<mono.size();++i)mono[i]=int16_t(int(i%60000)-30000);
  SingleFlight gate;gate.enable();assert(gate.request());Sink sink{gate,{}};
  assert(streamEffect(mono.data(),mono.size(),sink));
  assert(gate.state()==State::Playing);
  assert(DRAIN_FRAMES>DMA_BUFFERS*DMA_FRAMES);
  assert(sink.output.size()==(mono.size()+DRAIN_FRAMES)*4);
  for(size_t i=0;i<mono.size()+DRAIN_FRAMES;++i) {
    int16_t left,right;std::memcpy(&left,&sink.output[i*4],2);
    std::memcpy(&right,&sink.output[i*4+2],2);
    assert(left==right&&left==(i<mono.size()?mono[i]:0));
  }
  gate.finish();assert(gate.request());
}
''')

    def test_failed_stalled_and_invalid_writes_stop_instead_of_looping(self):
        self.run_cpp(r'''
struct Sink {
  int mode,calls=0;
  bool writeSome(const void *,size_t size,size_t &written) {
    ++calls;
    if(calls==1){written=4;return true;}
    if(mode==0){written=0;return false;}
    written=mode==1?0:mode==2?size+4:3;return true;
  }
};
int main() {
  int16_t mono[3]={-32768,0,32767};
  for(int mode=0;mode<4;++mode) {
    SingleFlight gate;gate.enable();assert(gate.request());Sink sink{mode};
    assert(!streamEffect(mono,3,sink)&&sink.calls==2);
    gate.fail();assert(!gate.request());
  }
}
''')
