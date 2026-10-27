#pragma once
#include <cmath>
#include <cstdint>

// Convex meshes shared by the renderer and topology tests. D3 uses the three
// rectangular scoring faces of a triangular prism; its two ends are unnumbered.
namespace dice_geometry {
struct Vec3 {
  float x,y,z;
  Vec3 operator+(Vec3 v)const { return {x+v.x,y+v.y,z+v.z}; }
  Vec3 operator-(Vec3 v)const { return {x-v.x,y-v.y,z-v.z}; }
  Vec3 operator*(float k)const { return {x*k,y*k,z*k}; }
  float dot(Vec3 v)const { return x*v.x+y*v.y+z*v.z; }
  Vec3 cross(Vec3 v)const { return {y*v.z-z*v.y,z*v.x-x*v.z,x*v.y-y*v.x}; }
  Vec3 unit()const { const float length=std::sqrt(dot(*this));return (*this)*(1.f/length); }
};
struct Face {
  uint8_t indices[5]={},count=0;bool scored=true;
  Vec3 center={},normal={},u={},v={};float labelSize=1;
};
struct Mesh {
  Vec3 vertices[10]={};Face faces[8]={};uint8_t vertexCount=0,faceCount=0,scoringFaces=0;
  void add(uint8_t a,uint8_t b,uint8_t c) {
    auto &f=faces[faceCount++];f.count=3;f.indices[0]=a;f.indices[1]=b;f.indices[2]=c;
  }
};
inline void prepare(Mesh &m,bool orient=true) {
  for(int i=0;i<m.faceCount;++i) {
    auto &f=m.faces[i];f.center={0,0,0};
    for(int j=0;j<f.count;++j)f.center=f.center+m.vertices[f.indices[j]]*(1.f/f.count);
    Vec3 a=m.vertices[f.indices[0]],b=m.vertices[f.indices[1]],c=m.vertices[f.indices[2]];
    f.normal=(b-a).cross(c-a).unit();
    if(f.normal.dot(f.center)<0) {
      for(int j=0;j<f.count/2;++j) { auto temp=f.indices[j];f.indices[j]=f.indices[f.count-1-j];f.indices[f.count-1-j]=temp; }
      f.normal=f.normal*(-1.f);
    }
    f.u=(m.vertices[f.indices[0]]-m.vertices[f.indices[1]]).unit();f.v=f.normal.cross(f.u);
    // Keep a 5x7 numeral inside even the smallest triangular face.
    float inradius=10;
    for(int j=0;j<f.count;++j) {
      const Vec3 first=m.vertices[f.indices[j]]-f.center;
      const Vec3 edge=m.vertices[f.indices[(j+1)%f.count]]-m.vertices[f.indices[j]];
      inradius=std::fmin(inradius,std::sqrt(first.cross(edge).dot(first.cross(edge)))/std::sqrt(edge.dot(edge)));
    }
    f.labelSize=std::fmin(1.15f,inradius/.76f);
  }
  if(orient) {
    const auto first=m.faces[0];
    for(int i=0;i<m.vertexCount;++i) {
      const auto p=m.vertices[i];m.vertices[i]={p.dot(first.u),p.dot(first.v),p.dot(first.normal)};
    }
    prepare(m,false);
  }
}
inline Mesh make(uint8_t sides) {
  Mesh m;
  if(sides==4) {
    m.vertexCount=4;m.vertices[0]={.95f,.95f,.95f};m.vertices[1]={.95f,-.95f,-.95f};
    m.vertices[2]={-.95f,.95f,-.95f};m.vertices[3]={-.95f,-.95f,.95f};
    m.add(0,1,2);m.add(0,3,1);m.add(0,2,3);m.add(1,3,2);
  } else if(sides==8) {
    m.vertexCount=6;const float radius=1.38f;
    m.vertices[0]={radius,0,0};m.vertices[1]={-radius,0,0};m.vertices[2]={0,radius,0};
    m.vertices[3]={0,-radius,0};m.vertices[4]={0,0,radius};m.vertices[5]={0,0,-radius};
    for(int x=0;x<2;++x)for(int y=2;y<4;++y)for(int z=4;z<6;++z)m.add(x,y,z);
  } else if(sides==6) {
    m.vertexCount=8;
    for(int i=0;i<8;++i)m.vertices[i]={(i&1)?1.f:-1.f,(i&2)?1.f:-1.f,(i&4)?1.f:-1.f};
    const uint8_t indices[6][4]={{4,5,7,6},{1,0,2,3},{5,1,3,7},{0,4,6,2},{0,1,5,4},{6,7,3,2}};
    m.faceCount=6;for(int i=0;i<6;++i) { m.faces[i].count=4;for(int j=0;j<4;++j)m.faces[i].indices[j]=indices[i][j]; }
  } else {
    const int n=sides==7?5:3;m.vertexCount=2*n;m.faceCount=n+2;
    const float radius=n==3?1.12f:1.02f,half=sides==3?1.34f:.80f;
    for(int i=0;i<n;++i) {
      const float angle=6.283185307f*i/n;
      m.vertices[i]={radius*std::cos(angle),radius*std::sin(angle),-half};
      m.vertices[i+n]={radius*std::cos(angle),radius*std::sin(angle),half};
      auto &f=m.faces[i];f.count=4;
      f.indices[0]=i;f.indices[1]=(i+1)%n;f.indices[2]=(i+1)%n+n;f.indices[3]=i+n;
    }
    for(int end=0;end<2;++end) {
      auto &f=m.faces[n+end];f.count=n;f.scored=sides!=3;
      for(int i=0;i<n;++i)f.indices[i]=i+end*n;
    }
  }
  // Keep the octahedron's two tips upright so its equatorial ridge is visible.
  prepare(m,sides!=6&&sides!=8);
  if(sides==3) {
    // Lay the longer prism across the screen. Its triangular end is visible
    // beside two scoring side faces, rather than reading as another box.
    for(int i=0;i<m.vertexCount;++i){const auto p=m.vertices[i];m.vertices[i]={p.y,-p.x,p.z};}
    prepare(m,false);
    for(int i=0;i<m.faceCount;++i)if(m.faces[i].scored) {
      auto &face=m.faces[i];face.v=(Vec3{0,1,0}-face.normal*face.normal.y).unit();
      face.u=face.v.cross(face.normal);
    }
  }
  if(sides==8)for(auto &face:m.faces) {
    face.v=(Vec3{0,1,0}-face.normal*face.normal.y).unit();
    face.u=face.v.cross(face.normal);
  }
  for(int i=0;i<m.faceCount;++i)if(m.faces[i].scored)++m.scoringFaces;
  return m;
}
inline const Mesh &get(uint8_t sides) {
  static const Mesh meshes[6]={make(3),make(4),make(5),make(6),make(7),make(8)};
  return meshes[sides>=3&&sides<=8?sides-3:3];
}
} // namespace dice_geometry
