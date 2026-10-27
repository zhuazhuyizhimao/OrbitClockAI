#pragma once
#include "dice_view.h"
#include "sand_timer_model.h"

namespace sand_view {
using dice_view::Frame;
constexpr uint32_t BACKGROUND=0x171610,GOLD=0xE6BF65,MUTED=0xA3987C;
constexpr unsigned BED_GRAINS=460,MAX_GRAINS=BED_GRAINS;
constexpr int GLASS_TOP=108,GLASS_BOTTOM=308,NECK=208;
struct Seat { lv_coord_t x,y; };
struct Grain { lv_coord_t x,y;uint8_t ink,size; };
struct Grains { Grain points[MAX_GRAINS];unsigned count=0;bool overflow=false; };
static Grains grains;
static Seat topSeats[BED_GRAINS],bottomSeats[BED_GRAINS];
static uint8_t widths[GLASS_BOTTOM-GLASS_TOP+1];
static bool seatsReady=false;
static uint32_t flowTime=0;
constexpr uint32_t PALETTE[]={0xF8D653,0x72E1D9,0xE583D5,0x9EEA7D,0xEEE5D4,
                              0xED7771,0xA49BEF,0x78BEEA,0xF6AD55,0x54CBB4};
inline uint32_t hash(uint32_t value) {
  value^=value>>16;value*=0x7FEB352Du;value^=value>>15;value*=0x846CA68Bu;return value^(value>>16);
}
inline int halfWidth(int y) {
  return y<GLASS_TOP||y>GLASS_BOTTOM?0:widths[y-GLASS_TOP];
}
inline void prepareSeats() {
  if(seatsReady)return;
  // A cubic glass profile, mirrored at the narrow neck. Cache integer widths
  // once; neither grain placement nor drawing needs per-particle trigonometry.
  float lastX=46,lastY=GLASS_TOP;
  for(int i=1;i<=32;++i) {
    const float t=i/32.f,s=1-t;
    const float x=s*s*s*46+3*s*s*t*85+3*s*t*t*9+t*t*t*4;
    const float y=s*s*s*GLASS_TOP+3*s*s*t*151+3*s*t*t*185+t*t*t*NECK;
    for(int row=int(std::ceil(lastY));row<=int(y);++row) {
      const int width=int(std::lround(lastX+(x-lastX)*(row-lastY)/(y-lastY)));
      widths[row-GLASS_TOP]=uint8_t(width);widths[GLASS_BOTTOM-row]=uint8_t(width);
    }
    lastX=x;lastY=y;
  }
  widths[NECK-GLASS_TOP]=4;
  unsigned top=0,bottom=0;
  for(int y=203;y>=GLASS_TOP+4&&top<BED_GRAINS;y-=3) {
    const int half=halfWidth(y)-5;
    for(int distance=0;distance<=half&&top<BED_GRAINS;distance+=3) {
      topSeats[top++]={(lv_coord_t)(240-distance),(lv_coord_t)y};
      if(distance&&top<BED_GRAINS)topSeats[top++]={(lv_coord_t)(240+distance),(lv_coord_t)y};
    }
  }
  // Rank seats by their height plus distance from the stream: the accumulated
  // bed forms a low, broad mound instead of a flat solid triangle.
  for(int level=0;level<360&&bottom<BED_GRAINS;level+=3)for(int x=180;x<=300&&bottom<BED_GRAINS;x+=3) {
    const int depth=level-abs(x-240);if(depth<0||depth%6)continue;
    const int y=303-depth/2;
    if(y<NECK+6||abs(x-240)>halfWidth(y)-5)continue;
    bottomSeats[bottom++]={(lv_coord_t)x,(lv_coord_t)y};
  }
  seatsReady=top==BED_GRAINS&&bottom==BED_GRAINS;
}
inline bool insideGlass(int x,int y,int size) {
  if(y<GLASS_TOP+3||y+size-1>GLASS_BOTTOM-3)return false;
  for(int row=y;row<y+size;++row)
    if(x<240-halfWidth(row)+3||x+size-1>240+halfWidth(row)-3)return false;
  return true;
}
inline void addGrain(int x,int y,uint32_t seed,int size=0) {
  if(grains.count>=MAX_GRAINS){grains.overflow=true;return;}
  const auto noise=hash(seed);if(!size)size=noise%5?2:3;
  if(!insideGlass(x,y,size))return;
  grains.points[grains.count++]={(lv_coord_t)x,(lv_coord_t)y,uint8_t(noise%10),uint8_t(size)};
}
inline void box(Frame &frame,int x,int y,int w,int h,uint32_t rgb) {
  const lv_point_t p[4]={{(lv_coord_t)x,(lv_coord_t)y},{(lv_coord_t)(x+w),(lv_coord_t)y},
    {(lv_coord_t)(x+w),(lv_coord_t)(y+h)},{(lv_coord_t)x,(lv_coord_t)(y+h)}};
  frame.add(p,rgb);
}
inline void band(Frame &frame,int y,int next,int inset,uint32_t rgb) {
  const int a=halfWidth(y)-inset,b=halfWidth(next)-inset;
  const lv_point_t p[4]={{(lv_coord_t)(240-a),(lv_coord_t)y},{(lv_coord_t)(240+a),(lv_coord_t)y},
    {(lv_coord_t)(240+b),(lv_coord_t)next},{(lv_coord_t)(240-b),(lv_coord_t)next}};
  frame.add(p,rgb);
}
inline void buildGlass(Frame &frame) {
  frame.count=0;frame.overflow=false;
  // Thin warm glass walls and restrained reflections, without side pillars.
  for(int y=GLASS_TOP;y<GLASS_BOTTOM;y+=4)band(frame,y,y+4,0,0x77796D);
  for(int y=GLASS_TOP;y<GLASS_BOTTOM;y+=4)band(frame,y,y+4,2,0x22251E);
  for(int y=120;y<296;y+=4) {
    if(y>184&&y<232)continue;
    const int x=240-halfWidth(y)+2,next=240-halfWidth(y+4)+2;
    const lv_point_t p[4]={{(lv_coord_t)x,(lv_coord_t)y},{(lv_coord_t)(x+1),(lv_coord_t)y},
      {(lv_coord_t)(next+1),(lv_coord_t)(y+4)},{(lv_coord_t)next,(lv_coord_t)(y+4)}};
    frame.add(p,0xBAC0A6);
  }
  // Shallow wood caps: warm top edge, grain and a dark bevel underneath.
  for(int y:{96,306}) {
    box(frame,171,y+2,138,12,0x56341E);box(frame,173,y,134,11,0x94623A);
    box(frame,175,y+1,130,2,0xB48050);box(frame,177,y+5,126,1,0x9F6C40);
    box(frame,175,y+10,130,2,0x724727);
  }
}
inline void build(Frame &frame,const sand_timer::Countdown &timer,uint32_t now,bool refreshGlass=true) {
  prepareSeats();if(refreshGlass)buildGlass(frame);
  grains.count=0;grains.overflow=!seatsReady;
  if(!seatsReady)return;
  const float remaining=std::max(0.f,std::min(1.f,timer.fraction()));
  const unsigned top=unsigned(std::lround(BED_GRAINS*remaining));
  const bool flowing=timer.state()==sand_timer::State::Running||timer.state()==sand_timer::State::Paused;
  if(timer.state()==sand_timer::State::Running)flowTime=now;
  else if(timer.state()==sand_timer::State::Idle)flowTime=0;
  const unsigned falling=flowing?std::min(18u,BED_GRAINS-top):0;
  const unsigned bottom=BED_GRAINS-top-falling;
  for(unsigned i=0;i<top;++i)addGrain(topSeats[i].x-1,topSeats[i].y-1,i+913);
  int landing=303;
  for(unsigned i=0;i<bottom;++i) {
    const auto seat=bottomSeats[i];landing=std::min(landing,int(seat.y)-2);
    addGrain(seat.x-1,seat.y-1,i+4207);
  }
  // Only this short stream moves. Settled grain positions and colors remain
  // deterministic as the timer changes or LVGL repaints individual DMA strips.
  const int travel=std::max(6,landing-NECK-3);
  for(unsigned i=0;i<falling;++i) {
    const int phase=int((flowTime/12+i*travel/std::max(1u,falling))%unsigned(travel));
    addGrain(239+int(hash(i+71)%2),NECK+1+phase,i+107,2);
  }
}
inline void draw(const Frame &frame,lv_draw_ctx_t *context) {
  dice_view::draw(frame,context);
  lv_draw_rect_dsc_t style;lv_draw_rect_dsc_init(&style);
  style.bg_opa=LV_OPA_COVER;style.border_width=0;style.radius=0;
  const auto &clip=*context->clip_area;
  lv_color_t colors[10];for(int i=0;i<10;++i)colors[i]=lv_color_hex(PALETTE[i]);
  for(unsigned i=0;i<grains.count;++i) {
    const auto &grain=grains.points[i];
    const lv_area_t area={grain.x,grain.y,(lv_coord_t)(grain.x+grain.size-1),(lv_coord_t)(grain.y+grain.size-1)};
    if(area.y2<clip.y1||area.y1>clip.y2||area.x2<clip.x1||area.x1>clip.x2)continue;
    style.bg_color=colors[grain.ink];lv_draw_rect(context,&style,&area);
    if(grain.size==3) {
      const lv_area_t glint={grain.x,grain.y,grain.x,grain.y};
      style.bg_color=lv_color_hex(0xF9F3D4);lv_draw_rect(context,&style,&glint);
    }
  }
}
} // namespace sand_view
