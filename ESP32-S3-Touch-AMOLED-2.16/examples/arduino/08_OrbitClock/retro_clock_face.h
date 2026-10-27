#pragma once
#include "pixel_clock_model.h"

namespace retro_clock {
constexpr int MAX_DOTS=640;
struct Dot { int16_t x,y; };
// A clock-font pixel consists of four LED elements on the 5px matrix grid.
inline int makeTimeDots(const char *value,Dot *output) {
  const int offsets[5]={0,12,24,28,40};
  int count=0;
  for(int c=0;c<5 && value[c];++c) {
    const int glyph=value[c]>='0'&&value[c]<='9'?value[c]-'0':value[c]==':'?10:11;
    for(int row=0;row<7;++row)for(int column=0;column<5;++column) {
      if (!(orbit_pixels::DIGITS[glyph][row]&(1<<(4-column))))continue;
      for(int y=0;y<2;++y)for(int x=0;x<2;++x) {
        if(count>=MAX_DOTS)return count;
        output[count++]={(int16_t)(116+offsets[c]*5+(c==2?0:column*10)+x*5),
                         (int16_t)(356+row*10+y*5)};
      }
    }
  }
  return count;
}
} // namespace retro_clock
