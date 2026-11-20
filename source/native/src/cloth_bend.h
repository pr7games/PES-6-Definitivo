#pragma once
#include "cloth_spring.h"
namespace pes6::cloth {
struct Bend {
 Vec axis{};float sine=0,cosine=1;
 Vec Apply(Vec v)const{return v*cosine+Cross(axis,v)*sine+axis*(Dot(axis,v)*(1-cosine));}
 static Bend FromEndpoint(Vec down,Vec displacement){
  const auto desired=down*.27f+displacement;
  const float length=Length(desired);if(length<.05f)return{};
  const auto direction=desired*(1/length),cross=Cross(down,direction);
  const float magnitude=Length(cross);if(magnitude<1e-6f)return{};
  const float angle=std::min(.35f,std::atan2(magnitude,std::clamp(Dot(down,direction),-1.f,1.f)));
  return{cross*(1/magnitude),std::sin(angle),std::cos(angle)};
 }
};
// Each leg has one lightweight secondary spring. Mid-thigh gets35% of the
// hem's response. Waist, hip/knee joints and the skin rig remain untouched.
struct Shorts {
 std::array<Vec,2> previous{},anchor_velocity{},acceleration{},velocity{},offset{};
 bool ready=false;
 void Reset(const std::array<Vec,2>& target){previous=target;anchor_velocity={};acceleration={};velocity={};offset={};ready=true;}
 void Step(const std::array<Vec,2>& target,float dt){
  if(!ready||!std::isfinite(dt)||dt<0||dt>.1f){Reset(target);return;}if(dt==0)return;
  for(unsigned i=0;i<2;i++)if(!Finite(target[i])||Length(target[i]-previous[i])>.6f){Reset(target);return;}
  const unsigned steps=std::max(1u,unsigned(std::ceil(dt*60)));const float h=dt/steps;
  for(unsigned i=0;i<2;i++){
   const auto v=(target[i]-previous[i])*(1/dt),raw=Limit((v-anchor_velocity[i])*(1/dt),80.f);
   acceleration[i]=acceleration[i]+(raw-acceleration[i])*std::min(1.f,dt*22);
   previous[i]=target[i];anchor_velocity[i]=v;
   for(unsigned s=0;s<steps;s++){
    velocity[i]=velocity[i]+(offset[i]*-170.f+velocity[i]*-21.f-acceleration[i]*.16f)*h;
    offset[i]=Limit(offset[i]+velocity[i]*h,.012f);
   }
  }
 }
};
}
