#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace pes6::cloth {
struct Vec {float x=0,y=0,z=0; Vec operator+(Vec b)const{return{x+b.x,y+b.y,z+b.z};} Vec operator-(Vec b)const{return{x-b.x,y-b.y,z-b.z};} Vec operator*(float s)const{return{x*s,y*s,z*s};}};
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Length(Vec a){return std::sqrt(Dot(a,a));}
inline Vec Limit(Vec a,float max){const auto n=Dot(a,a);return n>max*max?a*(max/std::sqrt(n)):a;}
inline bool Finite(Vec a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
// Six existing lower-shirt controls. Output is a bounded offset from the
// unmodified animated pose, never written back into the animation skeleton.
struct Shirt {
  std::array<Vec,6> previous{},velocity{},offset{},anchor_velocity{},filtered_acceleration{};
  bool ready=false;
  void Reset(const std::array<Vec,6>& targets){previous=targets;velocity={};offset={};anchor_velocity={};filtered_acceleration={};ready=true;}
  void Step(const std::array<Vec,6>& target,const std::array<Vec,6>& outward,float dt){
    if(!ready||!std::isfinite(dt)||dt<0||dt>.10f){Reset(target);return;}
    if(dt==0)return;
    for(unsigned i=0;i<6;i++)if(!Finite(target[i])||Length(target[i]-previous[i])>.6f){Reset(target);return;}
    const unsigned steps=std::max(1u,unsigned(std::ceil(dt*60.f)));
    const float h=dt/steps;
    std::array<Vec,6> acceleration{};
    for(unsigned i=0;i<6;i++){
      const Vec v=(target[i]-previous[i])*(1/dt);
      const auto raw=Limit((v-anchor_velocity[i])*(1/dt),60.f);
      filtered_acceleration[i]=filtered_acceleration[i]+(raw-filtered_acceleration[i])*std::min(1.f,dt*24.f);
      acceleration[i]=filtered_acceleration[i];
      anchor_velocity[i]=v;previous[i]=target[i];
    }
    for(unsigned s=0;s<steps;s++){
      const auto old=offset;
      for(unsigned i=0;i<6;i++){
        // Spring, damping and neighbouring panels resist independent flapping.
        constexpr unsigned left[6]={3,0,1,4,5,2},right[6]={1,2,5,0,3,4};
        const Vec coupling=(old[left[i]]+old[right[i]]-old[i]*2)*35.f;
        velocity[i]=velocity[i]+(offset[i]*-95.f+velocity[i]*-13.8f-acceleration[i]*.80f+coupling+Vec{0,-.8f,0})*h;
        offset[i]=offset[i]+velocity[i]*h;
        // Local outward plane prevents this layer pushing cloth into the body.
        const float inward=Dot(offset[i],outward[i]);
        if(inward<0){offset[i]=offset[i]-outward[i]*inward;const float v=Dot(velocity[i],outward[i]);if(v<0)velocity[i]=velocity[i]-outward[i]*v;}
        const auto bounded=Limit(offset[i],.045f);
        if(Dot(offset[i],offset[i])>.045f*.045f)velocity[i]=velocity[i]*.5f;
        offset[i]=bounded;
      }
    }
  }
};
}
