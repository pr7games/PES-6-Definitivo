#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace pes6::weather {
inline constexpr uint32_t drops=1500, rain_lines=drops, splash_base_limit=864,splash_limit=1245;
inline constexpr uint32_t torrential_drops=10000,max_rain_drops=std::max(drops,torrential_drops);
inline constexpr uint32_t net_base_drops=640,net_drop_limit=922,surface_triangle_capacity=19840;
inline constexpr uint32_t surface_batch_count=3,surface_batch_vertices=surface_triangle_capacity*3;
inline constexpr uint32_t rain_vertex_capacity=std::max(drops*2,torrential_drops*4);
// Separate draws start at index zero, even though their CPU scratch is shared.
inline constexpr uint32_t index_capacity=std::max(rain_vertex_capacity,surface_triangle_capacity*3);
inline constexpr uint32_t vertex_capacity=rain_vertex_capacity+surface_batch_count*surface_batch_vertices;
inline constexpr uint32_t primitive_capacity=(vertex_capacity+2)/3;
static_assert(index_capacity<65536,"Each weather draw must fit16-bit indices");
inline constexpr float rain_fall_step=2.f;
inline constexpr float torrential_fall_step=1.29024f,water_animation_rate=1.2f;
inline constexpr float net_animation_scale=.81f;
inline constexpr float torrential_water_scale=1.134f*.95f*1.15f;
inline constexpr float drizzle_rain_half_width=.0011f,drizzle_rain_length=.8f;
inline constexpr float torrential_rain_half_width=.0012705f,torrential_rain_length=.68f;
inline constexpr uint32_t working_bytes=max_rain_drops*24;
inline constexpr uint32_t scratch_bytes=vertex_capacity*16;
inline constexpr uint32_t index_bytes=index_capacity*2;
struct Vec { float x{},y{},z{}; };
struct Drop { Vec p{},v{}; float life{}; bool wide=false,round=true,trail=false,low_trail=false;float size=1,phase=0,travel_gain=1,travel=0,release_distance=0; };
inline bool Field(float x,float z){return std::isfinite(x)&&std::isfinite(z)&&std::abs(x)<=53.f&&std::abs(z)<=35.f;}
// Screen-size compensation for the added stroke. Native line rasterization
// and camera perspective still affect the final footprint.
inline float RainHalfWidth(unsigned output_height){
  return .02376f*1080.f/float(std::clamp(output_height,720u,4320u));
}
struct SlideContact {
  Vec previous{};
  bool ready=false,fall_armed=false;
  float cooldown=0,travel=0;
  bool Slide(Vec p,float dt){
    if(!Field(p.x,p.z)||!std::isfinite(p.y)){*this={};return false;}
    if(!ready){previous=p;ready=true;fall_armed=p.y>=.65f;return false;}
    const Vec old=previous;previous=p;
    const float distance=std::hypot(p.x-old.x,p.z-old.z);
    if(dt<=0||dt>.2f||distance>2.f||std::abs(p.y-old.y)>1.2f){
      travel=0;fall_armed=p.y>=.65f;return false;
    }
    cooldown=std::max(0.f,cooldown-dt);
    if(p.y>=.65f)fall_armed=true;
    // One burst when a previously upright body drops onto the turf. Normal
    // jump landings keep the pelvis above this threshold; lying still cannot
    // retrigger until the player rises. Use the same large slide burst.
    if(fall_armed&&p.y>=.08f&&p.y<=.48f&&(old.y-p.y)/dt>.7f){
      fall_armed=false;travel=0;cooldown=.10f;return true;
    }
    // Both samples must have a low pelvis and horizontal ground motion.
    // Running, footsteps, a standing tackle, jumps and a stationary fallen
    // player cannot emit. This is pose detection, not an animation-ID hook.
    const bool low=p.y>=.08f&&p.y<.55f&&old.y>=.08f&&old.y<.55f;
    if(!low||distance/dt<1.2f||std::abs(p.y-old.y)/dt>1.5f){travel=0;return false;}
    travel+=distance;
    if(travel<.12f||cooldown>0)return false;
    travel=0;cooldown=.10f;return true;
  }
};
struct Splashes {
  bool heavy=false;
  unsigned Limit()const{return heavy?1037u:splash_base_limit;}
  unsigned WaterCount(unsigned count)const{return heavy?(count*6+2)/5:count;}
  void Density(bool value){if(heavy!=value){heavy=value;light_budget=WaterCount(48);}}
  std::array<Drop,splash_limit> particles{};
  std::array<Drop,net_drop_limit> net_particles{};
  unsigned net_next=0;
  bool net_active=false;
  unsigned next=0,light_budget=48;
  float cooldown=0;
  uint32_t random=0x6136a91u;
  void Reset(){particles={};net_particles={};net_next=0;net_active=false;next=0;cooldown=0;light_budget=WaterCount(48);}
  float Random(){random^=random<<13;random^=random>>17;random^=random<<5;return float(random&65535)/65535.f;}
  void Tick(float dt=1.f/60){
    if(!std::isfinite(dt)||dt<=0||dt>0.1f)return;
    dt*=water_animation_rate;
    light_budget=WaterCount(48);
    cooldown=std::max(0.f,cooldown-dt);
    auto advance=[&](auto& pool,float step){bool alive=false;for(auto& d:pool)if(d.life>0){
      if(d.trail){
        d.p.x+=d.v.x*step;d.p.y+=d.v.y*step;d.p.z+=d.v.z*step;
        if(d.low_trail)d.p.y=std::clamp(d.p.y,.025f,.45f);
        alive=true;continue;
      }
      d.life-=step;d.v.y-=9.81f*step;
      d.p.x+=d.v.x*step;d.p.y+=d.v.y*step;d.p.z+=d.v.z*step;
      if(d.p.y<0.012f)d.life=0;
      alive|=d.life>0;
    }return alive;};advance(particles,dt*(heavy?.9f:1.f));if(net_active)net_active=advance(net_particles,dt*net_animation_scale);
  }
  bool Impact(float x,float z,float incoming){
    // Original ball units:256 per metre, vertical speed is16.16 per tick.
    // Positive incoming speed means descending in the game's Y-down space.
    if(cooldown>0||!Field(x,z)||!std::isfinite(incoming)||incoming<512.f)return false;
    const float strength=std::clamp(incoming/4096.f,0.35f,1.7f);
    Burst(x,z,strength);
    cooldown=5.f/60;return true;
  }
  void BodyImpact(Vec p){
    if(Field(p.x,p.z))Burst(p.x,p.z,1.f);
  }
  void Burst(float x,float z,float strength,unsigned requested=0,Vec direction={},unsigned count_override=0){
    const unsigned count=WaterCount(count_override?count_override:8*(((requested?requested:unsigned(26+strength*24))*13+5)/10));
    const float norm=std::hypot(direction.x,direction.z),dx=norm>.01f?direction.x/norm:0,dz=norm>.01f?direction.z/norm:0;
    for(unsigned i=0;i<count;++i){
      const float angle=Random()*6.283185307f;
      auto& d=particles[next++%Limit()];d.trail=false;d.round=i%5<3;
      const bool directional=norm>.01f;
      d.p={x+std::cos(angle)*.035f,directional?.055f:.025f,z+std::sin(angle)*.035f};
      if(directional){
        const bool low=requested==30;
        const float force=std::clamp(norm/25.f,.55f,1.15f);
        const float life=low?.22f+Random()*.12f:.36f+Random()*.14f;
        const float reach=low?.6f+Random()*.7f:(1.4f+Random()*1.6f)*force;
        const float forward=reach/life,lateral=(Random()-.5f)*(low?2.5f:5.5f);
        d.v={dx*forward-dz*lateral,low?1.3f+Random()*1.3f:2.7f+Random()*1.5f,dz*forward+dx*lateral};
        d.life=life;
      }else{
        // Ground impacts throw a fast, low radial sheet instead of a column.
        const float radial=(3.5f+Random()*4.f)*std::clamp(strength,.6f,1.3f);
        d.v={std::cos(angle)*radial,.7f+Random()*.9f,std::sin(angle)*radial};
        d.life=.2f+Random()*.16f;
      }
      d.size=.9f+.2f*(.5f+.5f*std::cos(angle));d.phase=angle;
      d.wide=true;
    }
  }
  void Radial(Vec center,float strength=1.f,unsigned count=48,Vec normal={0,1,0}){
    const unsigned first=next;Burst(center.x,center.z,strength,0,{},count);
    const float length=std::hypot(normal.x,normal.y,normal.z);
    const Vec n=length>1e-5f?Vec{normal.x/length,normal.y/length,normal.z/length}:Vec{0,1,0};
    const Vec ref=std::abs(n.y)<.9f?Vec{0,1,0}:Vec{0,0,1};
    Vec tangent{n.y*ref.z-n.z*ref.y,n.z*ref.x-n.x*ref.z,n.x*ref.y-n.y*ref.x};
    const float tlen=std::hypot(tangent.x,tangent.y,tangent.z);tangent={tangent.x/tlen,tangent.y/tlen,tangent.z/tlen};
    const Vec side{tangent.y*n.z-tangent.z*n.y,tangent.z*n.x-tangent.x*n.z,tangent.x*n.y-tangent.y*n.x};
    for(unsigned i=first;i<next;i++){
      auto& d=particles[i%Limit()];const Vec v=d.v;
      if(n.x==0&&n.y==1&&n.z==0){d.p.y+=center.y-.025f;continue;}
      const float x=d.p.x-center.x,z=d.p.z-center.z;
      d.p={center.x+tangent.x*x+side.x*z,center.y+tangent.y*x+side.y*z,center.z+tangent.z*x+side.z*z};
      d.v={tangent.x*v.x+n.x*v.y+side.x*v.z,tangent.y*v.x+n.y*v.y+side.y*v.z,tangent.z*v.x+n.z*v.y+side.z*v.z};
    }
  }
  void Follow(Vec delta,bool release){
    const float step=std::hypot(delta.x,delta.z);
    for(auto& d:particles)if(d.life>0&&d.trail){
      const float remaining=std::max(0.f,d.release_distance-d.travel);
      const float portion=step>.00001f?std::min(1.f,remaining/step):1.f;
      const float gain=d.travel_gain*portion;
      d.p.x+=delta.x*gain;d.p.y+=delta.y*gain;d.p.z+=delta.z*gain;
      d.travel+=step*portion;
      if(d.low_trail)d.p.y=std::clamp(d.p.y,.025f,.45f);
      if(release||d.travel>=d.release_distance){
        d.trail=false;d.life=.2f+Random()*.25f;
        d.v.x*=.35f;d.v.z*=.35f;d.v.y=std::min(d.v.y,0.f);
      }
    }
  }
  void BallTrail(Vec position,Vec direction,unsigned requested,bool low){
    Follow({},true);
    const auto first=next;Burst(position.x,position.z,1,requested,direction);
    const float norm=std::max(.01f,std::hypot(direction.x,direction.z));
    const float dx=direction.x/norm,dz=direction.z/norm;
    const float limit=low?3.f:5.f+std::clamp(norm/35.f,0.f,1.f);
    for(unsigned i=first;i<next;i++){
      auto& d=particles[i%Limit()];
      // A small slow fringe drops at the boot; most water explodes forward
      // with independent speeds and a widening lateral fan.
      const float lag=Random()*.04f,side=(Random()-.5f)*.06f;
      d.p={position.x-dx*lag-dz*side,low?.035f:std::max(.035f,position.y),position.z-dz*lag+dx*side};
      const bool near_boot=(i-first)%20<3; //15% slow fringe,85% forward spray
      d.travel_gain=near_boot?.06f+Random()*.19f:.4f+Random()*.6f;
      d.travel=0;
      d.release_distance=near_boot?.3f+Random()*.45f:limit*(.5f+.5f*d.travel_gain)*(.9f+Random()*.1f);
      const float lateral=(Random()-.5f)*(low?1.2f:4.6f);
      d.v={dx*.12f-dz*lateral,low?(i%8==0?.6f+Random()*.7f:0.f):(Random()-.5f)*1.2f,dz*.12f+dx*lateral};
      d.trail=true;d.low_trail=low;d.life=.35f;
    }
  }
  void Light(Vec p,bool slide){
    const unsigned count=WaterCount(slide?40:24);
    if(!Field(p.x,p.z)||light_budget<count)return;
    light_budget-=count;
    for(unsigned i=0;i<count;++i){
      const float angle=Random()*6.283185307f,speed=0.25f+Random()*0.45f;
      auto& d=particles[next++%Limit()];d.round=i%5<3;d.trail=false;d.low_trail=false;
      d.p={p.x,0.022f,p.z};
      d.v={std::cos(angle)*speed,0.35f+Random()*(slide?0.6f:0.4f),std::sin(angle)*speed};
      d.size=.9f+.2f*(.5f+.5f*std::cos(angle));d.phase=angle;
      d.life=0.12f+Random()*0.10f;d.wide=false;
    }
  }
  // Small droplets at the actual 3D contact, including outside pitch bounds.
  void Contact(Vec origin,unsigned count,bool falling=false){
    if(!std::isfinite(origin.x)||!std::isfinite(origin.y)||!std::isfinite(origin.z)||
       std::abs(origin.x)>66||std::abs(origin.z)>46||origin.y<0||origin.y>10)return;
    count=std::min(WaterCount(count),Limit());
    for(unsigned i=0;i<count;i++){
      const float angle=Random()*6.283185307f,radial=falling?.05f+Random()*.15f:1.2f+Random()*1.8f;
      auto& d=particles[next++%Limit()];d={};d.round=i%5<3;
      d.p={origin.x+(Random()-.5f)*(falling?.18f:.045f),origin.y+(Random()-.5f)*.035f,
           origin.z+(Random()-.5f)*(falling?.8f:.045f)};
      d.v={std::cos(angle)*radial,falling?-.15f-Random()*.35f:(Random()-.5f)*2.8f,std::sin(angle)*radial};
      d.life=falling?.75f+Random()*.2f:.18f+Random()*.16f;
      d.size=.9f+Random()*.2f;d.phase=angle;d.wide=!falling;
    }
  }
  void NetShower(Vec hit,unsigned count=net_base_drops){
    count=std::min(WaterCount(count),net_drop_limit);net_active=count>0||net_active;const float side=hit.x>=0?1.f:-1.f;
    for(unsigned i=0;i<count;i++){
      auto& d=net_particles[net_next++%net_drop_limit];d={};d.round=true;d.wide=true;
      const float depth=Random()*1.8f;
      const float z=(Random()-.5f)*7.1f;
      d.p={side*(52.5f+depth),2.42f-depth*.08f,z};
      d.v={(Random()-.5f)*.25f,-3.f-Random()*3.f,(Random()-.5f)*.35f};
      d.life=.55f+Random()*.15f;d.size=1.1f+Random()*.2f;d.phase=Random()*6.283185307f;
    }
  }
};
}
