#pragma once
#include <lvgl.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include "player_game_model.h"

namespace player_view {
constexpr int SIZE=57,CELL=4,LEFT=126,TOP=110,CENTER=28;
static uint8_t base[SIZE*SIZE],pixels[SIZE*SIZE],prepared=0;
static lv_point_t labelPositions[player_game::MAX_PLAYERS];
constexpr uint32_t COLORS[]={0x111C2B,0x924D42,0x4B657D,0x5E7043,0x80658F,0x936A36,
  0x3F7774,0x8D526C,0x647AAA,0x8A784B,0x587863,0x77558C,0x88634D,
  0xE9C577,0x3C4A59,0x202B3A,0x4B301D,0xFFF1C1,0xD9AD55};
inline void prepare(uint8_t players) {
  if(prepared==players)return;prepared=players;
  const float slice=player_game::TAU/players;
  for(int y=0;y<SIZE;++y)for(int x=0;x<SIZE;++x) {
    const int dx=x-CENTER,dy=y-CENTER;const float radius=std::sqrt(float(dx*dx+dy*dy));uint8_t ink=0;
    if(radius<=28) {
      if(radius>=27)ink=13;
      else if(radius>=25.5f)ink=14;
      else {
        const float sector=player_game::normalize(std::atan2(float(dx),float(-dy))+slice*.5f)/slice;
        const float edge=sector-std::floor(sector);
        ink=radius>4&&std::min(edge,1-edge)*slice*radius<.38f?15:uint8_t(1+int(sector)%players);
      }
    }
    base[y*SIZE+x]=ink;
  }
  for(int i=0;i<players;++i) {
    const float angle=i*slice;
    labelPositions[i]={(lv_coord_t)std::lround(240+85*std::sin(angle)),
                       (lv_coord_t)std::lround(224-85*std::cos(angle))};
  }
}
inline void dot(int x,int y,int radius,uint8_t ink) {
  for(int dy=-radius;dy<=radius;++dy)for(int dx=-radius;dx<=radius;++dx) {
    const int px=x+dx,py=y+dy;
    if(px>=0&&px<SIZE&&py>=0&&py<SIZE&&base[py*SIZE+px])pixels[py*SIZE+px]=ink;
  }
}
inline void line(int x0,int y0,int x1,int y1,int thickness,uint8_t ink) {
  const int dx=std::abs(x1-x0),sx=x0<x1?1:-1,dy=-std::abs(y1-y0),sy=y0<y1?1:-1;
  int error=dx+dy;
  for(;;){dot(x0,y0,thickness,ink);if(x0==x1&&y0==y1)break;
    const int twice=2*error;if(twice>=dy){error+=dy;x0+=sx;}if(twice<=dx){error+=dx;y0+=sy;}}
}
inline void build(const player_game::Game &game,uint32_t now) {
  prepare(game.players());std::memcpy(pixels,base,sizeof(pixels));
  const float angle=game.angle(now),dx=std::sin(angle),dy=-std::cos(angle);
  const int tipX=CENTER+int(std::lround(dx*17)),tipY=CENTER+int(std::lround(dy*17));
  const int tailX=CENTER-int(std::lround(dx*4)),tailY=CENTER-int(std::lround(dy*4));
  line(tailX+1,tailY+1,tipX+1,tipY+1,1,16);
  line(tailX,tailY,tipX,tipY,0,17);
  for(int i=0;i<4;++i) {
    const int bx=tipX-int(std::lround(dx*i)),by=tipY-int(std::lround(dy*i));
    const int width=(i+1)/2;
    for(int w=-width;w<=width;++w)dot(bx+int(std::lround(-dy*w)),by+int(std::lround(dx*w)),0,17);
  }
  for(int y=-3;y<=3;++y)for(int x=-3;x<=3;++x)if(x*x+y*y<=9)dot(CENTER+x,CENTER+y,0,18);
  dot(CENTER,CENTER,0,17);
}
inline void draw(lv_draw_ctx_t *context,uint8_t players,uint8_t winner) {
  lv_draw_rect_dsc_t style;lv_draw_rect_dsc_init(&style);style.bg_opa=LV_OPA_COVER;style.radius=0;
  lv_color_t palette[sizeof(COLORS)/sizeof(COLORS[0])];
  for(unsigned i=0;i<sizeof(COLORS)/sizeof(COLORS[0]);++i)palette[i]=lv_color_hex(COLORS[i]);
  const auto &clip=*context->clip_area;
  for(int y=0;y<SIZE;++y) {
    const int py=TOP+y*CELL;if(py+CELL-1<clip.y1||py>clip.y2)continue;
    for(int x=0;x<SIZE;) {
      const int start=x;const uint8_t ink=pixels[y*SIZE+x++];
      while(x<SIZE&&pixels[y*SIZE+x]==ink)++x;if(!ink)continue;
      const lv_area_t area={(lv_coord_t)(LEFT+start*CELL),(lv_coord_t)py,
        (lv_coord_t)(LEFT+x*CELL-1),(lv_coord_t)(py+CELL-1)};
      if(area.x2<clip.x1||area.x1>clip.x2)continue;
      style.bg_color=palette[ink];lv_draw_rect(context,&style,&area);
    }
  }
  lv_draw_label_dsc_t label;lv_draw_label_dsc_init(&label);label.font=&lv_font_montserrat_16;
  label.align=LV_TEXT_ALIGN_CENTER;
  for(int i=0;i<players;++i) {
    char number[4];snprintf(number,sizeof(number),"%d",i+1);const auto p=labelPositions[i];
    lv_area_t area={(lv_coord_t)(p.x-15),(lv_coord_t)(p.y-9),(lv_coord_t)(p.x+15),(lv_coord_t)(p.y+10)};
    label.color=lv_color_hex(0x172332);lv_area_t shadow=area;++shadow.x1;++shadow.x2;++shadow.y1;++shadow.y2;
    lv_draw_label(context,&label,&shadow,number,nullptr);
    label.color=lv_color_hex(winner==i+1?0xFFF0A3:0xF4E9CE);lv_draw_label(context,&label,&area,number,nullptr);
  }
}
} // namespace player_view
