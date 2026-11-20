#pragma once
#include <algorithm>
#include <cmath>
namespace pes6::net_sound {
struct Vec {float x=0,y=0,z=0;};
struct Contact {
 Vec previous{},velocity{};bool ready=false,velocity_ready=false;float cooldown=0;
 void Reset(){*this={};}
 static bool Surface(Vec p){
  const float x=std::abs(p.x),z=std::abs(p.z);
  return x>=52.65f&&x<=56.f&&z<=4.15f&&p.y>=0&&p.y<=2.85f&&
    (x>=54.3f-.25f*p.y||z>=3.5f||p.y>=2.24f);
 }
 float Sample(Vec p,float dt){
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||dt<=0||dt>.08f){Reset();return 0;}
  if(!ready){previous=p;ready=true;return 0;}
  const auto old=previous;previous=p;
  if(std::hypot(p.x-old.x,p.y-old.y,p.z-old.z)>2.f){velocity_ready=false;cooldown=.12f;return 0;}
  cooldown=std::max(0.f,cooldown-dt);
  const Vec v{(p.x-old.x)/dt,(p.y-old.y)/dt,(p.z-old.z)/dt};
  const float speed=std::hypot(velocity.x,velocity.z),change=std::hypot(v.x-velocity.x,v.z-velocity.z);
  // Crossing the goal line alone is silent. Require a substantial horizontal
  // response alongside the back/side/roof of the net, excluding ordinary turf.
  const bool hit=velocity_ready&&cooldown==0&&(Surface(p)||Surface(old))&&
    speed>=.65f&&change>=.8f&&change>=speed*.22f;
  velocity=v;velocity_ready=true;
  if(!hit)return 0;cooldown=.28f;return speed;
 }
};
inline float Gain(float speed,unsigned sfx_volume){
 return std::clamp(float(sfx_volume)/15.f,0.f,1.f)*std::clamp(.40f+speed/40.f,.42f,1.f);
}
}

