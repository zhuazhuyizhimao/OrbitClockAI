#pragma once
#include <lvgl.h>
#include <cmath>
#include <initializer_list>
#include "dice_game_model.h"
#include "dice_geometry.h"
#include "coin_glyphs.h"
#include "pixel_clock_model.h"

namespace dice_view {
constexpr unsigned MAX_POLYGONS=224;
struct Polygon { lv_point_t points[8];lv_area_t bounds;uint32_t rgb;uint8_t vertices,opacity; };
struct Frame {
  Polygon shapes[MAX_POLYGONS];unsigned count=0;bool overflow=false;
  void add(const lv_point_t *points,uint32_t rgb,uint8_t vertices=4,uint8_t opacity=LV_OPA_COVER) {
    if(count>=MAX_POLYGONS||vertices<3||vertices>16) { overflow=true;return; }
    // Integer projection can make a nearly edge-on ellipse slightly concave.
    // LVGL only accepts convex polygons; normalize the rounded silhouette.
    lv_point_t sorted[16],hull[32];unsigned n=0;
    for(unsigned i=0;i<vertices;++i) {
      const auto point=points[i];unsigned at=n;
      for(unsigned j=0;j<n;++j)if(sorted[j].x>point.x||(sorted[j].x==point.x&&sorted[j].y>=point.y)){at=j;break;}
      if(at<n&&sorted[at].x==point.x&&sorted[at].y==point.y)continue;
      for(unsigned j=n;j>at;--j)sorted[j]=sorted[j-1];sorted[at]=point;++n;
    }
    if(n<3)return;
    auto turn=[](lv_point_t a,lv_point_t b,lv_point_t c)->int32_t {
      return int32_t(b.x-a.x)*(c.y-a.y)-int32_t(b.y-a.y)*(c.x-a.x);
    };
    unsigned used=0;
    for(unsigned i=0;i<n;++i){while(used>=2&&turn(hull[used-2],hull[used-1],sorted[i])<=0)--used;hull[used++]=sorted[i];}
    const unsigned lower=used+1;
    for(int i=int(n)-2;i>=0;--i){while(used>=lower&&turn(hull[used-2],hull[used-1],sorted[i])<=0)--used;hull[used++]=sorted[i];}
    --used;if(used<3)return;
    if(used>8) {
      // Bound LVGL software-mask usage; a smooth coin need not use 16 masks.
      for(unsigned first=1;first+1<used;) {
        const unsigned last=std::min(first+6,used-1);lv_point_t part[8];part[0]=hull[0];
        for(unsigned i=first;i<=last;++i)part[i-first+1]=hull[i];
        add(part,rgb,uint8_t(last-first+2),opacity);first=last;
      }
      return;
    }
    vertices=uint8_t(used);points=hull;
    auto &p=shapes[count++];p.rgb=rgb;p.vertices=vertices;p.opacity=opacity;
    p.bounds={points[0].x,points[0].y,points[0].x,points[0].y};
    for(int i=0;i<vertices;++i) {
      p.points[i]=points[i];
      p.bounds.x1=std::min(p.bounds.x1,points[i].x);p.bounds.x2=std::max(p.bounds.x2,points[i].x);
      p.bounds.y1=std::min(p.bounds.y1,points[i].y);p.bounds.y2=std::max(p.bounds.y2,points[i].y);
    }
  }
};
using Vec3=dice_geometry::Vec3;
struct Rotation {
  Vec3 right,down,front;
  Vec3 apply(Vec3 p)const { return right*p.x+down*p.y+front*p.z; }
};
inline Rotation rotation(float x,float y,float z) {
  const float sx=std::sin(x),cx=std::cos(x),sy=std::sin(y),cy=std::cos(y),sz=std::sin(z),cz=std::cos(z);
  return {{cz*cy,sz*cy,-sy},{cz*sy*sx-sz*cx,sz*sy*sx+cz*cx,cy*sx},
          {cz*sy*cx+sz*sx,sz*sy*cx-cz*sx,cy*cx}};
}
struct Projection {
  float cx,cy,scale;
  lv_point_t point(Vec3 p)const {
    const float perspective=6.f/(6.f-p.z);
    return {(lv_coord_t)std::lround(cx+p.x*scale*perspective),
            (lv_coord_t)std::lround(cy+p.y*scale*perspective)};
  }
};
struct Plane {
  Vec3 normal,u,v,center;
  constexpr Plane(Vec3 n,Vec3 horizontal,Vec3 vertical):normal(n),u(horizontal),v(vertical),center(n) {}
  constexpr Plane(Vec3 n,Vec3 horizontal,Vec3 vertical,Vec3 position):normal(n),u(horizontal),v(vertical),center(position) {}
};
constexpr Plane PLANES[6]={
  {{0,0,1},{1,0,0},{0,1,0}},{{0,0,-1},{-1,0,0},{0,1,0}},
  {{1,0,0},{0,0,-1},{0,1,0}},{{-1,0,0},{0,0,1},{0,1,0}},
  {{0,-1,0},{1,0,0},{0,0,1}},{{0,1,0},{1,0,0},{0,0,-1}}
};
// Front/back, right/left and top/bottom remain opposite pairs on a D6.
constexpr uint8_t D6_VALUES[6][6]={
  {1,6,3,4,2,5},{2,5,3,4,6,1},{3,4,6,1,2,5},
  {4,3,1,6,2,5},{5,2,3,4,1,6},{6,1,4,3,2,5}
};
inline void tile(Frame &frame,const Projection &p,const Plane &face,
                 float x,float y,float w,float h,uint32_t rgb) {
  const float px[4]={x,x+w,x+w,x},py[4]={y,y,y+h,y+h};lv_point_t points[4];
  for(int i=0;i<4;++i)points[i]=p.point(face.center+face.u*px[i]+face.v*py[i]);
  frame.add(points,rgb);
}
inline uint32_t shade(uint32_t rgb,float light) {
  return (uint32_t(((rgb>>16)&255)*light)<<16)|(uint32_t(((rgb>>8)&255)*light)<<8)|
         uint32_t((rgb&255)*light);
}
inline float lighting(Vec3 normal) {
  const float diffuse=std::max(0.f,-.35f*normal.x-.65f*normal.y+.68f*normal.z);
  return .73f+.27f*diffuse;
}
inline uint32_t blend(uint32_t dark,uint32_t light,float amount) {
  const int a=int(std::max(0.f,std::min(1.f,amount))*256),b=256-a;
  return ((((dark>>16&255)*b+(light>>16&255)*a)>>8)<<16)|
         ((((dark>>8&255)*b+(light>>8&255)*a)>>8)<<8)|(((dark&255)*b+(light&255)*a)>>8);
}
inline uint32_t materialNoise(uint32_t seed) {
  seed^=seed>>16;seed*=0x7FEB352Du;seed^=seed>>15;seed*=0x846CA68Bu;return seed^(seed>>16);
}
inline float amberLight(Vec3 normal) {
  return std::max(0.f,-.43f*normal.x-.64f*normal.y+.63f*normal.z);
}
inline uint32_t amberBody(Vec3 normal) {
  return blend(0x9C4E08,0xF5B43E,.24f+.76f*amberLight(normal));
}
inline void resinDetails(Frame &frame,const Projection &p,const Vec3 *vertices,uint8_t count,
                          Vec3 center,Vec3 normal,uint32_t seed,int quality,bool rim=true) {
  // Face-local folded foil follows the solid while it rolls. Each fragment
  // fits inside its nearest edge; paired triangles suggest a crumpled gold leaf.
  const float light=amberLight(normal);
  const int broad=quality==1?count:1,flakes=quality==1?32:quality==2?9:3;
  for(int i=0;i<broad;++i) {
    const unsigned j=quality==1?i:materialNoise(seed)%count;
    const Vec3 a=center+(vertices[j]-center)*.985f;
    const Vec3 b=center+(vertices[(j+1)%count]-center)*.985f;
    const lv_point_t triangle[3]={p.point(a),p.point(b),p.point(center)};
    const float variation=.18f+(materialNoise(seed+j*17)%97)/170.f;
    frame.add(triangle,blend(0x80420D,0xEDB746,variation*(.65f+.35f*light)),3);
  }
  const Vec3 u=(vertices[1]-vertices[0]).unit(),v=normal.cross(u);
  float radius=10;
  for(int i=0;i<count;++i) {
    const Vec3 edge=(vertices[(i+1)%count]-vertices[i]).unit();
    radius=std::min(radius,std::fabs(edge.cross(center-vertices[i]).dot(normal)));
  }
  constexpr float directions[8][2]={{1,0},{.7071f,.7071f},{0,1},{-.7071f,.7071f},
                                   {-1,0},{-.7071f,-.7071f},{0,-1},{.7071f,-.7071f}};
  constexpr uint32_t foil[]={0xFFD35A,0xA95512,0xFFEBA5,0xE89917,0xC67714,0xF9C63D,0x71350B};
  for(int i=0;i<flakes;i+=2) {
    const uint32_t noise=materialNoise(seed+i*359+91);
    const unsigned j=(noise>>8)%count,k=(j+1)%count;
    const float a=.06f+(noise&255)/420.f,b=.08f+((noise>>16)&127)/650.f;
    const Vec3 anchor=center*(1-a-b)+vertices[j]*a+vertices[k]*b;
    float safe=radius;
    for(int edge=0;edge<count;++edge) {
      const Vec3 direction=(vertices[(edge+1)%count]-vertices[edge]).unit();
      safe=std::min(safe,std::fabs(direction.cross(anchor-vertices[edge]).dot(normal)));
    }
    const float size=std::min(safe*.86f,radius*(.12f+((noise>>24)&127)/480.f));
    const auto &angle=directions[(noise>>12)%8];
    const Vec3 x=(u*angle[0]+v*angle[1])*size,y=(v*angle[0]-u*angle[1])*size;
    const lv_point_t points[4]={p.point(anchor-x*.9f-y*.4f),p.point(anchor+x*.6f-y*.7f),
                                p.point(anchor+x+y*.1f),p.point(anchor-x*.6f+y*.8f)};
    const lv_point_t first[3]={points[0],points[1],points[2]},second[3]={points[0],points[2],points[3]};
    const uint32_t tint=foil[(noise>>5)%7];
    frame.add(first,shade(tint,.82f+.18f*light),3);
    if(i+1<flakes)frame.add(second,blend(tint,0xFFEAA2,.22f+.25f*light),3);
  }
  // A faint clear coating crosses the foil without flattening its colors.
  // Two translucent bands supply a small reflection gradient using LVGL's
  // existing draw buffer; no texture or alpha framebuffer is allocated.
  if(quality<3)for(int i=0;i<2;++i) {
    const int j=materialNoise(seed+137)%count,k=(j+1)%count;
    const float outer=.91f-i*.11f,inner=outer-.11f;
    const lv_point_t sheen[4]={p.point(center+(vertices[j]-center)*outer),p.point(center+(vertices[k]-center)*outer),
                               p.point(center+(vertices[k]-center)*inner),p.point(center+(vertices[j]-center)*inner)};
    frame.add(sheen,0xFFF3CE,4,uint8_t((i?16:32)+16*light));
  }
  if(!rim)return;
  const int edges=quality==1?count:quality==2?2:1;
  for(int edge=0;edge<edges;++edge) {
    const int j=edge*count/edges,k=(j+1)%count;
    const Vec3 a=center+(vertices[j]-center)*.985f,b=center+(vertices[k]-center)*.985f;
    const Vec3 c=center+(vertices[k]-center)*.95f,d=center+(vertices[j]-center)*.95f;
    const lv_point_t line[4]={p.point(a),p.point(b),p.point(c),p.point(d)};
    frame.add(line,blend(0xD79B42,0xFFF8E5,.48f+.48f*light));
  }
}
inline Vec3 axis(int index,float value) {
  return {index==0?value:0.f,index==1?value:0.f,index==2?value:0.f};
}
struct DieLayout { Projection view;float center,floor,width; };
inline DieLayout layout(const Rotation &r,uint8_t sides,int count,int index,float lift) {
  float minX=10,maxX=-10,minY=10,maxY=-10;
  auto include=[&](Vec3 vertex) {
    const auto v=r.apply(vertex);const float perspective=6.f/(6.f-v.z);
    const float x=v.x*perspective,y=v.y*perspective;
    minX=std::min(minX,x);maxX=std::max(maxX,x);
    minY=std::min(minY,y);maxY=std::max(maxY,y);
  };
  if(sides==2) {
    for(const auto &v:COIN_RING)for(int sign:{-1,1})include({v[0]*1.35f,v[1]*1.35f,sign*.12f});
  } else if(sides==6) {
    // The 24 chamfer vertices give a closer fit than a sharp cube's corners.
    for(int a=0;a<3;++a)for(int sign:{-1,1})for(int b:{-1,1})for(int c:{-1,1})
      include(axis(a,sign)+axis((a+1)%3,b*.91f)+axis((a+2)%3,c*.91f));
  } else {
    const auto &mesh=dice_geometry::get(sides);
    for(int i=0;i<mesh.vertexCount;++i)include(mesh.vertices[i]);
  }
  const float center=count==1?240:count==2?160+index*160:104+index*136;
  const float halfWidth=count==1?120:count==2?76:60;
  const float requested=count==1?70:count==2?50:40;
  const float scale=std::min(requested,std::min(halfWidth*2/(maxX-minX),(173-2*lift)/(maxY-minY)));
  const float width=(maxX-minX)*scale,height=(maxY-minY)*scale;
  // Body: y=113..286; badges: y=292..332. The total keeps its own area below.
  return {{center-(minX+maxX)*.5f*scale,199.5f-lift-(minY+maxY)*.5f*scale,scale},
          center,199.5f+height*.5f+6,width*.78f};
}
inline void surface(Frame &frame,const Rotation &r,const Projection &p,
                    const Vec3 *vertices,uint8_t count,Vec3 normal) {
  const auto n=r.apply(normal);Vec3 center={0,0,0};
  for(int i=0;i<count;++i)center=center+vertices[i]*(1.f/count);
  if(n.z*6.f<=n.dot(r.apply(center)))return;
  lv_point_t points[4];for(int i=0;i<count;++i)points[i]=p.point(r.apply(vertices[i]));
  frame.add(points,blend(amberBody(n),0xFFE8AD,.22f+.42f*amberLight(n)),count);
}
inline void bevels(Frame &frame,const Rotation &r,const Projection &p) {
  // Twelve chamfer strips and eight small corners join the inset face planes.
  // These are actual 3D surfaces, so edge thickness follows the tumble.
  constexpr float h=.91f;
  for(int a=0;a<3;++a)for(int b=a+1;b<3;++b)for(int sa:{-1,1})for(int sb:{-1,1}) {
    const int c=3-a-b;
    const Vec3 first=axis(a,sa)+axis(b,sb*h),second=axis(a,sa*h)+axis(b,sb);
    const Vec3 vertices[4]={first+axis(c,-h),second+axis(c,-h),second+axis(c,h),first+axis(c,h)};
    surface(frame,r,p,vertices,4,(axis(a,sa)+axis(b,sb))*.70710678f);
  }
  for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1}) {
    const Vec3 vertices[3]={{float(x),y*h,z*h},{x*h,float(y),z*h},{x*h,y*h,float(z)}};
    surface(frame,r,p,vertices,3,Vec3{float(x),float(y),float(z)}*.57735027f);
  }
}
inline void circle(Frame &frame,const Projection &p,const Plane &face,
                   float x,float y,float radius,uint32_t rgb) {
  constexpr float unit[8][2]={{1,0},{.70710678f,.70710678f},{0,1},{-.70710678f,.70710678f},
    {-1,0},{-.70710678f,-.70710678f},{0,-1},{.70710678f,-.70710678f}};
  lv_point_t points[8];
  for(int i=0;i<8;++i)points[i]=p.point(face.center+face.u*(x+unit[i][0]*radius)+face.v*(y+unit[i][1]*radius));
  frame.add(points,rgb,8);
}
inline void shadow(Frame &frame,float cx,float cy,float width,float lift) {
  // Three flat diamonds form a small ground shadow; no alpha or blur buffers.
  for(int i=0;i<3;++i) {
    const float rx=width*(.66f-i*.09f)*(1.f-lift*.006f),ry=width*(.105f-i*.025f);
    const lv_point_t points[4]={{(lv_coord_t)std::lround(cx-rx),(lv_coord_t)std::lround(cy)},
      {(lv_coord_t)std::lround(cx),(lv_coord_t)std::lround(cy-ry)},
      {(lv_coord_t)std::lround(cx+rx),(lv_coord_t)std::lround(cy)},
      {(lv_coord_t)std::lround(cx),(lv_coord_t)std::lround(cy+ry)}};
    frame.add(points,i==0?0x0D1925:i==1?0x09131E:0x060F19);
  }
}
inline void marking(Frame &frame,const Projection &p,const Plane &face,uint8_t value,
                    bool pips,uint32_t ink) {
  if(!value) { circle(frame,p,face,0,0,.12f,ink);return; }
  if(pips) {
    constexpr float gap=.43f;
    const uint32_t rim=shade(0xA6AEB7,lighting(face.normal));
    auto dot=[&](float x,float y) {
      circle(frame,p,face,x,y,.142f,rim);
      circle(frame,p,face,x,y-.008f,.112f,ink);
    };
    if(value>=2) { dot(-gap,-gap);dot(gap,gap); }
    if(value>=4) { dot(gap,-gap);dot(-gap,gap); }
    if(value%2)dot(0,0);
    if(value==6) { dot(-gap,0);dot(gap,0); }
  } else {
    // Merge both horizontal and vertical runs. Crisp engraved digits need only
    // 4-8 rectangles, leaving a bounded budget for the resin and foil facets.
    constexpr float unit=.165f;bool occupied[7][5];
    for(int y=0;y<7;++y)for(int x=0;x<5;++x)occupied[y][x]=orbit_pixels::DIGITS[value][y]&(1<<(4-x));
    for(int row=0;row<7;++row)for(int col=0;col<5;++col) {
      if(!occupied[row][col])continue;
      int w=1,h=1;while(col+w<5&&occupied[row][col+w])++w;
      for(;row+h<7;++h) {
        bool full=true;for(int x=col;x<col+w;++x)if(!occupied[row+h][x])full=false;
        if(!full)break;
      }
      for(int y=row;y<row+h;++y)for(int x=col;x<col+w;++x)occupied[y][x]=false;
      tile(frame,p,face,(col-2.5f)*unit,(row-3.5f)*unit,w*unit-.008f,h*unit-.008f,ink);
    }
  }
}
inline void engraved(Frame &frame,const Projection &p,const Plane &plane,uint8_t value,bool front) {
  if(front&&value) {
    const Plane inset={plane.normal,plane.u,plane.v,plane.center+plane.u*.026f+plane.v*.03f};
    marking(frame,p,inset,value,false,0x51300D);
  }
  marking(frame,p,plane,value,false,value?blend(0xE4C57F,0xFFF4CD,front?.95f:.6f):0xDAB361);
}
inline void polyhedron(Frame &frame,const Projection &p,const Rotation &r,uint8_t sides,uint8_t value,int quality) {
  const auto &mesh=dice_geometry::get(sides);
  for(int side=0;side<mesh.faceCount;++side) {
    const auto &f=mesh.faces[side];const Vec3 normal=r.apply(f.normal),center=r.apply(f.center);
    if(normal.z*6.f<=normal.dot(center))continue;
    lv_point_t polygon[5];Vec3 vertices[5];
    for(int j=0;j<f.count;++j){vertices[j]=r.apply(mesh.vertices[f.indices[j]]);polygon[j]=p.point(vertices[j]);}
    frame.add(polygon,amberBody(normal),f.count);
    if(sides==3&&!f.scored) {
      // Quiet triangular end caps make the three-sided cross section readable.
      lv_point_t inset[3];for(int j=0;j<3;++j)inset[j]=p.point(center+(vertices[j]-center)*.78f);
      frame.add(inset,blend(amberBody(normal),0xF7D37A,.38f),3);
      for(int j=0;j<3;++j) {
        const auto a=vertices[j],b=vertices[(j+1)%3];
        const lv_point_t edge[4]={p.point(a),p.point(b),p.point(center+(b-center)*.94f),p.point(center+(a-center)*.94f)};
        frame.add(edge,0xF9DFA0);
      }
    } else resinDetails(frame,p,vertices,f.count,center,normal,side*1193+sides*571,quality);
    if(!f.scored||(!value&&side!=0))continue;
    const Plane plane={normal,r.apply(f.u)*f.labelSize,r.apply(f.v)*f.labelSize,center};
    const uint8_t shown=value?uint8_t((value-1+side)%sides+1):0;
    engraved(frame,p,plane,shown,side==0);
  }
}
inline void resultBadge(Frame &frame,float cx,uint8_t value) {
  const lv_point_t points[4]={{(lv_coord_t)(cx-25),292},{(lv_coord_t)(cx+25),292},
                            {(lv_coord_t)(cx+25),332},{(lv_coord_t)(cx-25),332}};
  frame.add(points,0x192B3A);
  const Plane plane={{0,0,1},{1,0,0},{0,1,0},{0,0,0}};
  marking(frame,{cx,312,30},plane,value,false,0xF9DFA0);
}
inline void coinSurface(Frame &frame,const Projection &p,const Rotation &r,
                        const Vec3 *vertices,uint8_t count,Vec3 normal,uint32_t rgb) {
  const auto n=r.apply(normal);Vec3 center={0,0,0};
  for(int i=0;i<count;++i)center=center+vertices[i]*(1.f/count);
  if(n.z*6.f<=n.dot(r.apply(center)))return;
  lv_point_t points[16];for(int i=0;i<count;++i)points[i]=p.point(r.apply(vertices[i]));
  frame.add(points,shade(rgb,lighting(n)),count);
}
inline void coin(Frame &frame,const Projection &p,const Rotation &r) {
  for(int sign:{-1,1}) {
    Vec3 disc[16];
    for(int i=0;i<16;++i)disc[i]={COIN_RING[i][0]*1.22f,COIN_RING[i][1]*1.22f,sign*.12f};
    Vec3 underlay[8];for(int i=0;i<8;++i)underlay[i]=disc[i*2];
    coinSurface(frame,p,r,underlay,8,{0,0,float(sign)},0xDEA64C);
    coinSurface(frame,p,r,disc,16,{0,0,float(sign)},0xDEA64C);
    const Vec3 n=r.apply({0,0,float(sign)}),center=r.apply({0,0,sign*.121f});
    if(n.z*6.f>n.dot(center)) {
      Vec3 visibleDisc[16];for(int i=0;i<16;++i)visibleDisc[i]=r.apply(disc[i]);
      resinDetails(frame,p,visibleDisc,16,center,n,sign==1?389:799,2,false);
      const Plane face={n,r.apply({float(sign),0,0}),r.apply({0,1,0}),center};
      // A calm inset keeps the thin Chinese strokes readable over the foil.
      circle(frame,p,face,0,0,.88f,0xB77C2F);circle(frame,p,face,0,0,.84f,0xDDB464);
      const auto &rows=COIN_GLYPHS[sign==1?0:1];constexpr float unit=.075f;
      for(int row=0;row<16;++row)for(int col=0;col<16;) {
        if(!(rows[row]&(1<<(15-col)))){++col;continue;}
        const int first=col;while(col<16&&(rows[row]&(1<<(15-col))))++col;
        tile(frame,p,face,(first-8.f)*unit,(row-8.f)*unit,(col-first)*unit,unit,0x3C250D);
      }
    }
    for(int i=0;i<16;++i) {
      const int next=(i+1)%16;const auto &a=COIN_RING[i],&b=COIN_RING[next];
      const Vec3 bevel[4]={{a[0]*1.22f,a[1]*1.22f,sign*.12f},{b[0]*1.22f,b[1]*1.22f,sign*.12f},
                          {b[0]*1.35f,b[1]*1.35f,sign*.04f},{a[0]*1.35f,a[1]*1.35f,sign*.04f}};
      const Vec3 radial=Vec3{a[0]+b[0],a[1]+b[1],0}.unit();
      coinSurface(frame,p,r,bevel,4,radial*.52f+Vec3{0,0,sign*.854166f},0xF1CD76);
      if(sign==1) {
        const Vec3 edge[4]={{a[0]*1.35f,a[1]*1.35f,.04f},{b[0]*1.35f,b[1]*1.35f,.04f},
                            {b[0]*1.35f,b[1]*1.35f,-.04f},{a[0]*1.35f,a[1]*1.35f,-.04f}};
        coinSurface(frame,p,r,edge,4,radial,0xA8681D);
      }
    }
  }
}
inline void build(Frame &frame,const dice_game::Game &game,uint32_t now) {
  frame.count=0;frame.overflow=false;
  const int count=game.count();
  const float progress=game.rolling()?std::min(1.f,float(game.elapsed(now))/2000.f):1.f;
  const float remainder=(1.f-progress)*(1.f-progress)*(1.f-progress);
  for(int i=0;i<count;++i) {
    const float lift=game.rolling()?std::fabs(std::sin(progress*18.8495559f+i))*10.f*(1.f-progress):0.f;
    const float spin=remainder*18.8495559f;
    const bool isCoin=game.faces()==2;
    const float restX=game.faces()==3?-.72f:game.faces()==8||isCoin?-.16f:-.34f;
    const float restY=isCoin?(game.hasResult()&&game.total()==2?3.14159265f-.18f:-.18f):game.faces()==3?-.7f:game.faces()==8?-.18f:-.43f;
    const auto r=rotation(restX+spin*(i%2?-1.f:1.f),restY+spin*(.67f+i*.15f),
                          (i-(count-1)*.5f)*.04f+spin*.33f);
    const auto fitted=layout(r,game.faces(),count,i,lift);const auto &p=fitted.view;
    shadow(frame,fitted.center,fitted.floor,fitted.width,lift);
    const uint8_t value=game.displayed(i,now);
    if(isCoin){coin(frame,p,r);continue;}
    if(game.faces()!=6) {
      polyhedron(frame,p,r,game.faces(),value,count);
      if(game.hasResult())resultBadge(frame,fitted.center,value);
      continue;
    }
    for(int side=0;side<6;++side) {
      const auto &source=PLANES[side];
      const Plane face={r.apply(source.normal),r.apply(source.u),r.apply(source.v)};
      // Perspective back-face test: normal dot (camera - face center) > 0.
      if(face.normal.z*6.f<=1.f)continue;
      tile(frame,p,face,-.91f,-.91f,1.82f,1.82f,amberBody(face.normal));
      const Vec3 vertices[4]={face.center-face.u*.91f-face.v*.91f,face.center+face.u*.91f-face.v*.91f,
                             face.center+face.u*.91f+face.v*.91f,face.center-face.u*.91f+face.v*.91f};
      resinDetails(frame,p,vertices,4,face.center,face.normal,side*1193+631,count);
      const uint8_t shown=value?D6_VALUES[value-1][side]:0;
      const Plane lettering={face.normal,face.u*.82f,face.v*.82f,face.center};
      if(value||side==0)engraved(frame,p,lettering,shown,side==0);
    }
    bevels(frame,r,p);
    if(game.hasResult())resultBadge(frame,fitted.center,value);
  }
}
inline void draw(const Frame &frame,lv_draw_ctx_t *context) {
  lv_draw_rect_dsc_t style;lv_draw_rect_dsc_init(&style);style.bg_opa=LV_OPA_COVER;
  style.border_width=0;style.radius=0;
  const auto &clip=*context->clip_area;
  for(unsigned i=0;i<frame.count;++i) {
    const auto &shape=frame.shapes[i];const auto &b=shape.bounds;
    if(b.x2<clip.x1||b.x1>clip.x2||b.y2<clip.y1||b.y1>clip.y2)continue;
    style.bg_color=lv_color_hex(shape.rgb);style.bg_opa=shape.opacity;lv_draw_polygon(context,&style,shape.points,shape.vertices);
  }
}
} // namespace dice_view
