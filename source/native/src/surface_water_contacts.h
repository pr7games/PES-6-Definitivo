#pragma once
#include "weather_particles.h"
#include "net_sound_contact.h"
namespace pes6::weather {
inline bool KeeperWaterMotion(unsigned clip){
  if((clip>=308&&clip<=343)||clip==356||(clip>=598&&clip<=600)||clip==633)return true;
  if((clip>=939&&clip<=977)||clip==979||clip==980||(clip>=984&&clip<=997))return true;
  switch(clip){
    case 78:case 345:case 346:case 347:case 348:case 349:case 350:case 351:case 352:case 353:case 354:
    case 376:case 438:case 439:case 440:case 500:case 501:case 601:case 602:case 603:case 604:
    case 608:case 609:case 610:case 611:case 614:case 615:case 616:case 617:case 618:case 619:
    case 620:case 621:case 622:case 623:case 624:case 625:case 626:case 627:case 635:case 636:return true;
    default:return false;
  }
}
inline bool WaterWorld(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&std::abs(p.x)<66&&std::abs(p.z)<46&&p.y>=-.2f&&p.y<10;}
inline void PostWaterSurface(Vec ball,Vec& point,Vec& normal){
  const float x=ball.x>=0?52.5f:-52.5f;
  const std::array<Vec,3> closest{{{x,std::clamp(ball.y,0.f,2.44f),-3.66f},
    {x,std::clamp(ball.y,0.f,2.44f),3.66f},{x,2.44f,std::clamp(ball.z,-3.66f,3.66f)}}};
  float best=1e30f;Vec center{};
  for(auto p:closest){const float d=(p.x-ball.x)*(p.x-ball.x)+(p.y-ball.y)*(p.y-ball.y)+(p.z-ball.z)*(p.z-ball.z);if(d<best){best=d;center=p;}}
  const float len=std::sqrt(best);
  normal=len>1e-5f?Vec{(ball.x-center.x)/len,(ball.y-center.y)/len,(ball.z-center.z)/len}:Vec{ball.x>=0?-1.f:1.f,0,0};
  point={center.x+normal.x*.065f,center.y+normal.y*.065f,center.z+normal.z*.065f};
}
struct WaterBallSample {bool keeper_impulse=false,goal_net=false,goal_entry=false;Vec impact{},incoming{};};
struct WaterBallContacts {
  Vec previous{},velocity{};bool ready=false,velocity_ready=false;
  int goal_side=0;float goal_window=0,keeper_cooldown=0;
  pes6::net_sound::Contact net;
  WaterBallSample Sample(Vec p,float dt){
    WaterBallSample result{};
    if(!WaterWorld(p)||!std::isfinite(dt)||dt<=0||dt>.1f){*this={};return result;}
    if(!ready){previous=p;ready=true;return result;}
    const Vec old=previous;previous=p;
    if(std::hypot(p.x-old.x,p.y-old.y,p.z-old.z)>2.f){*this={};previous=p;ready=true;return result;}
    const Vec v{(p.x-old.x)/dt,(p.y-old.y)/dt,(p.z-old.z)/dt};
    const float speed=std::hypot(velocity.x,velocity.y,velocity.z),now=std::hypot(v.x,v.y,v.z);
    const float change=std::hypot(v.x-velocity.x,v.y-velocity.y,v.z-velocity.z);
    const float dot=v.x*velocity.x+v.y*velocity.y+v.z*velocity.z;
    const bool ground_bounce=old.y<.3f&&p.y<.35f&&velocity.y<-.5f&&v.y>.2f&&
      std::hypot(v.x-velocity.x,v.z-velocity.z)<std::max(1.f,speed*.15f);
    keeper_cooldown=std::max(0.f,keeper_cooldown-dt);
    result.keeper_impulse=velocity_ready&&!ground_bounce&&keeper_cooldown==0&&speed>=1.f&&change>=std::max(.75f,speed*.08f)&&
      (now<speed*.97f||dot<speed*now*.995f);
    result.impact=p;result.incoming=velocity;
    goal_window=std::max(0.f,goal_window-dt);if(goal_window==0)goal_side=0;
    // Whole ball crossing within the mouth, not a wide shot hitting the net outside.
    constexpr float plane=52.61f;
    if(std::abs(old.x)<=plane&&std::abs(p.x)>plane){
      const float side=p.x>0?1.f:-1.f;
      const float fraction=(side*plane-old.x)/(p.x-old.x);
      const float y=old.y+(p.y-old.y)*fraction,z=old.z+(p.z-old.z)*fraction;
      if(std::abs(z)<3.55f&&y>=.05f&&y<2.33f){goal_side=int(side);goal_window=2.5f;result.goal_entry=true;}
    }
    const float hit=net.Sample({p.x,p.y,p.z},dt);
    if(goal_side&&goal_window>0&&p.x*goal_side>=plane&&hit>0){result.goal_net=true;goal_side=0;goal_window=0;}
    else if(goal_side&&p.x*goal_side<52.3f){goal_side=0;goal_window=0;}
    velocity=v;velocity_ready=true;return result;
  }
};
}
