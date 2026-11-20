#pragma once
#include "weather_particles.h"

namespace pes6::weather {
inline constexpr unsigned debris_limit=128;
struct TurfPiece {Vec p{},v{};float life=0;bool soil=false;float size=1,phase=0,spin=0,age=0;};
struct KickContact {
  Vec previous{},velocity{};bool ready=false,velocity_ready=false;float cooldown=0;
  void Reset(){*this={};}
  bool Sample(Vec p,float dt,Vec& origin,Vec& launch){
    if(!Field(p.x,p.z)||!std::isfinite(p.y)||dt<=0||dt>.1f){Reset();return false;}
    if(!ready){previous=p;ready=true;return false;}
    const Vec old=previous;previous=p;
    const Vec v{(p.x-old.x)/dt,(p.y-old.y)/dt,(p.z-old.z)/dt};
    if(std::hypot(p.x-old.x,p.z-old.z)>2.f||std::abs(p.y-old.y)>2.f){velocity_ready=false;cooldown=.15f;return false;}
    cooldown=std::max(0.f,cooldown-dt);
    const float speed=std::hypot(v.x,v.z),old_speed=std::hypot(velocity.x,velocity.z);
    const float dot=v.x*velocity.x+v.z*velocity.z;
    // Friction and bounces slow the horizontal ball: they are not foot strikes.
    const bool lift=v.y>1.f&&v.y>=velocity.y+.5f&&speed>=.15f;
    const bool impulse=speed>=old_speed+.35f||(speed>.6f&&old_speed>.6f&&dot<speed*old_speed*.9f)||lift;
    // The previous sample approximates contact height; the outgoing sample
    // may already be rising after a ground strike. Ball radius is about11cm.
    const bool kick=impulse&&velocity_ready&&cooldown==0&&old.y>=0&&old.y<=.24f&&
      std::hypot(v.x,v.y,v.z)>=.6f&&std::hypot(v.x-velocity.x,v.y-velocity.y,v.z-velocity.z)>=.4f;
    velocity=v;velocity_ready=true;
    if(kick){origin={old.x,.025f,old.z};launch=v;}return kick;
  }
};
struct TurfDebris {
  std::array<TurfPiece,debris_limit> particles{};unsigned next=0,budget=100;
  uint32_t random=0x6a55b39u;
  float Random(){random^=random<<13;random^=random>>17;random^=random<<5;return float(random&65535)/65535.f;}
  void Reset(){particles={};next=0;budget=100;}
  void Tick(float dt){
    if(!std::isfinite(dt)||dt<=0||dt>.1f)return;budget=100;
    for(auto& p:particles)if(p.life>0){p.life-=dt;p.age+=dt;p.v.y-=9.81f*dt;
      p.p.x+=p.v.x*dt;p.p.y+=p.v.y*dt;p.p.z+=p.v.z*dt;if(p.p.y<.01f)p.life=0;}
  }
  void Burst(Vec origin,Vec direction={},bool slide=false,unsigned requested=50){
    if(!Field(origin.x,origin.z))return;
    // Fifty pieces per burst:30grass +20soil, in every climate.
    // Length references the water BEFORE its latest5% increase.
    const unsigned count=budget>=requested?requested:0;budget-=count;
    const float speed=std::hypot(direction.x,direction.z),dx=speed>0?direction.x/speed:0,dz=speed>0?direction.z/speed:0;
    for(unsigned i=0;i<count;i++){
      const float angle=Random()*6.283185307f,spread=.7f+Random()*1.2f;
      const float vx=std::cos(angle)*spread,vz=std::sin(angle)*spread;
      auto& p=particles[next++%debris_limit];p.soil=i%5<2;
      const float forward=speed>0?.22f+Random()*.28f:0.f;
      const float lateral=(Random()-.5f)*.18f;
      p.p={origin.x+dx*forward-dz*lateral+std::cos(angle)*.04f,
           .045f+Random()*.065f,origin.z+dz*forward+dx*lateral+std::sin(angle)*.04f};
      const float force=std::clamp(speed/25.f,.55f,1.15f);
      p.life=slide?.3f+Random()*.12f:.4f+Random()*.14f;
      const float reach=slide?.65f+Random()*.7f:(1.4f+Random()*1.6f)*force;
      const float push=speed>0?reach/p.life:0.f;
      const float sideways=(Random()-.5f)*(slide?2.8f:5.f);
      p.v={vx*.3f+dx*push-dz*sideways,slide?1.5f+Random()*1.2f:2.7f+Random()*1.5f,vz*.3f+dz*push+dx*sideways};
      p.size=p.soil?.55f+Random()*1.15f:.9f+Random()*.2f;
      p.phase=Random()*6.283185307f;p.spin=(Random()-.5f)*(p.soil?12.f:5.f);p.age=0;
    }
  }
};
}
