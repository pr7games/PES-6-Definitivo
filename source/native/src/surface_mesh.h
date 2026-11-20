#pragma once
#include "weather_particles.h"

namespace pes6::weather {
inline constexpr unsigned triangles_per_piece=8;
struct MeshVertex {Vec p;uint32_t color;};
struct ParticleMesh {std::array<MeshVertex,60> vertices;unsigned count=24;};
inline Vec Add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec Mul(Vec a,float s){return {a.x*s,a.y*s,a.z*s};}
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline Vec Unit(Vec v){const float n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return n>1e-5f?Mul(v,1.f/n):Vec{0,1,0};}
inline uint32_t Shade(uint32_t color,float factor){
  auto channel=[&](unsigned shift){return uint32_t(std::clamp(float((color>>shift)&255)*factor,0.f,255.f));};
  return (color&0xff000000u)|(channel(16)<<16)|(channel(8)<<8)|channel(0);
}
// A closed, consistently wound eight-face solid. All faces have area: this
// submits real triangles, never a bundle of line primitives or crossed edges.
inline ParticleMesh Solid(Vec center,Vec axis,float half_length,float width,float depth,
                          float phase,float irregularity,uint32_t color,float bottom_scale=1.f){
  axis=Unit(axis);
  const Vec side=Unit(Cross(axis,std::abs(axis.y)<.9f?Vec{0,1,0}:Vec{1,0,0}));
  const Vec normal=Cross(axis,side);
  const Vec right=Add(Mul(side,std::cos(phase)),Mul(normal,std::sin(phase)));
  const Vec up=Add(Mul(normal,std::cos(phase)),Mul(side,-std::sin(phase)));
  const Vec top=Add(center,Mul(axis,half_length));
  const Vec bottom=Add(center,Mul(axis,-half_length*bottom_scale*(1.f-irregularity*.2f)));
  const std::array<Vec,4> ring{
    Add(center,Add(Mul(right,width),Mul(axis,half_length*irregularity*.17f))),
    Add(center,Mul(up,depth*(1.f-irregularity*.25f))),
    Add(center,Add(Mul(right,-width*(1.f-irregularity*.3f)),Mul(axis,-half_length*irregularity*.13f))),
    Add(center,Mul(up,-depth))};
  ParticleMesh mesh{};
  for(unsigned i=0;i<4;i++){
    const auto a=ring[i],b=ring[(i+1)%4];
    const uint32_t light=Shade(color,.83f+float(i)*.075f);
    mesh.vertices[i*6+0]={top,light};mesh.vertices[i*6+1]={a,light};mesh.vertices[i*6+2]={b,light};
    mesh.vertices[i*6+3]={bottom,Shade(light,.88f)};mesh.vertices[i*6+4]={b,Shade(light,.88f)};mesh.vertices[i*6+5]={a,Shade(light,.88f)};
  }
  return mesh;
}
// Perturbed icosahedron: a rounded clod without the two pointed poles of
// an octahedron. Stable per-piece irregularities survive replay pauses.
inline constexpr float clod_golden_ratio=1.618033989f;
inline constexpr std::array<Vec,12> clod_directions{{{-1,clod_golden_ratio,0},{1,clod_golden_ratio,0},{-1,-clod_golden_ratio,0},{1,-clod_golden_ratio,0},
    {0,-1,clod_golden_ratio},{0,1,clod_golden_ratio},{0,-1,-clod_golden_ratio},{0,1,-clod_golden_ratio},{clod_golden_ratio,0,-1},{clod_golden_ratio,0,1},{-clod_golden_ratio,0,-1},{-clod_golden_ratio,0,1}}};
inline constexpr unsigned clod_faces[20][3]={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
    {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},{3,9,4},{3,4,2},
    {3,2,6},{3,6,8},{3,8,9},{4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
inline ParticleMesh SoilClod(Vec center,float radius,float seed,float rotation,uint32_t base_color=0xFF3B2719u,bool water=false){
  const float cosine=std::cos(rotation),sine=std::sin(rotation);
  std::array<Vec,12> points;
  for(unsigned i=0;i<12;i++){
    Vec v=Mul(Unit(clod_directions[i]),radius*(1.f+(water?0.f:.09f)*std::sin(seed*1.7f+float(i)*2.37f)));
    v.y*=.93f;v.z*=.97f;
    const float x=v.x*cosine-v.z*sine;
    v.z=v.x*sine+v.z*cosine;v.x=x;
    points[i]=Add(center,v);
  }
  ParticleMesh mesh{};mesh.count=60;
  for(unsigned i=0;i<20;i++){
    const uint32_t color=Shade(base_color,(water?.98f:.90f)+(water?.02f:.12f)*(.5f+.5f*std::sin(seed+float(i)*1.3f)));
    for(unsigned j=0;j<3;j++)mesh.vertices[i*3+j]={points[clod_faces[i][j]],color};
  }
  return mesh;
}
// Radius, irregularity and face colors are fixed for one soil piece. Keep
// only that local shape; position and rotation are rebuilt on every draw.
// Keys depend on geometry, not playback time, slot identity or camera, so
// seeks and recycled slots cannot reuse stale animation state.
struct SoilClodMeshCache {
  std::array<Vec,12> local{};
  std::array<uint32_t,20> colors{};
  float radius=0,seed=0;
  bool ready=false;
  ParticleMesh Get(Vec center,float next_radius,float next_seed,float rotation){
    if(!ready||radius!=next_radius||seed!=next_seed){
      for(unsigned i=0;i<local.size();i++){
        auto v=Mul(Unit(clod_directions[i]),next_radius*(1.f+.09f*std::sin(next_seed*1.7f+float(i)*2.37f)));
        v.y*=.93f;v.z*=.97f;local[i]=v;
      }
      for(unsigned i=0;i<colors.size();i++)colors[i]=Shade(0xFF3B2719u,.90f+.12f*(.5f+.5f*std::sin(next_seed+float(i)*1.3f)));
      radius=next_radius;seed=next_seed;ready=true;
    }
    const float cosine=std::cos(rotation),sine=std::sin(rotation);
    std::array<Vec,12> points;
    for(unsigned i=0;i<local.size();i++){
      auto v=local[i];const float x=v.x*cosine-v.z*sine;
      v.z=v.x*sine+v.z*cosine;v.x=x;points[i]=Add(center,v);
    }
    ParticleMesh mesh{};mesh.count=60;
    for(unsigned i=0;i<colors.size();i++)for(unsigned j=0;j<3;j++)mesh.vertices[i*3+j]={points[clod_faces[i][j]],colors[i]};
    return mesh;
  }
};
// Round water keeps radius/phase throughout its lifetime. Cache its local
// geometry; translation and fading remain per draw, including replay seeks.
struct RoundWaterMeshCache {
  ParticleMesh mesh{};
  float radius=0,phase=0;
  bool ready=false;
  const ParticleMesh& Get(float next_radius,float next_phase){
    if(!ready||radius!=next_radius||phase!=next_phase){
      mesh=SoilClod({},next_radius,next_phase,next_phase,0x00F1F6FAu,true);
      radius=next_radius;phase=next_phase;ready=true;
    }
    return mesh;
  }
};
}
