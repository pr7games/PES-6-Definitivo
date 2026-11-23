#pragma once
#include "cloth_grid.h"
namespace pes6::cloth {
// Independent, bounded fabric inertia. A stopped/paused pose never advances
// the spring; cuts and replay resets start exactly on the new attachment.
struct FabricLag {
 Vec position{},velocity{},delta{};bool ready=false;
 void Step(Vec target,float dt){
  if(!Finite(target)){position={};velocity={};delta={};ready=false;return;}
  if(!ready||!std::isfinite(dt)||dt<0||dt>.10f||Length(target-position)>.6f){position=target;velocity={};delta={};ready=true;return;}
  if(dt==0)return;
  const unsigned steps=std::max(1u,unsigned(std::ceil(dt*120.f)));const float h=dt/steps;
  for(unsigned i=0;i<steps;i++){velocity=velocity+((target-position)*82.f-velocity*8.6f)*h;position=position+velocity*h;}
  delta=Limit((position-target)*.52f,.038f);
 }
};
inline Frame MixHemFrame(Frame a,Frame b,float t,Vec pivot){
 const auto x=Unit(a.x*(1-t)+b.x*t,Unit(a.x,{1,0,0}));
 const auto raw=a.y*(1-t)+b.y*t,y=Unit(raw-x*Dot(raw,x),Unit(a.y)),z=Unit(Cross(x,y),Unit(a.z,{0,0,1}));
 Frame out{x*(Length(a.x)*(1-t)+Length(b.x)*t),y*(Length(a.y)*(1-t)+Length(b.y)*t),z*(Length(a.z)*(1-t)+Length(b.z)*t),{}};
 out.p=a.Point(pivot)*(1-t)+b.Point(pivot)*t-out.Linear(pivot);return out;
}
inline std::array<float,2> HemLift(Frame root,const std::array<Frame,2>& thighs){
 std::array<float,2> lift{};const auto up=Unit(root.y);
 for(unsigned leg=0;leg<2;leg++){const Vec p{leg?-.11f:.11f,.87f,.07f};float f=std::clamp((Dot(thighs[leg].Point(p)-root.Point(p),up)-.003f)/.10f,0.f,1.f);lift[leg]=f*f*(3-2*f);}return lift;
}
inline Frame LocalHemAttachment(Frame root,const std::array<Frame,2>& thighs,Vec pivot,const std::array<float,2>& lift,float activity=1.f){
 float side=std::clamp(std::abs(pivot.x)/.15f,0.f,1.f);side=side*side*(3-2*side);const unsigned leg=pivot.x>=0?0:1;
 const float front=std::clamp((pivot.z+.085f)/.14f,0.f,1.f);
 auto attached=MixHemFrame(root,thighs[leg],side*((.015f+.065f*activity)+(.20f+.52f*activity)*lift[leg])*front,pivot);
 attached.p=attached.p+Unit(root.y)*((.003f+.037f*activity)*lift[leg]*side+(.001f+.015f*activity)*(lift[0]+lift[1])*(1-side))*front;
 return attached;
}
inline Frame LocalHemAttachment(Frame root,const std::array<Frame,2>& thighs,Vec pivot){return LocalHemAttachment(root,thighs,pivot,HemLift(root,thighs));}
inline Frame StraightHemShape(Vec pivot){
 // Reduce the flare below the existing split; the seam itself stays fixed.
 // A shear, rather than scaling the whole garment, keeps the waist untouched.
 const float sx=.12f*std::clamp(pivot.x/.156f,-1.f,1.f);
 const float sz=.075f*std::clamp((pivot.z+.005f)/.132f,-1.f,1.f);
 Frame out;out.y={sx,1,sz};out.p={-sx*1.075f,0,-sz*1.075f};return out;
}
inline Frame MovingFoldShape(Vec pivot,Vec gait){
 // Alternating sectors compress/unfold with actual gait, never an idle timer.
 // Keep the upper split fixed and let final garment contact own clearance.
 const float sector=std::clamp(pivot.x/.156f,-1.f,1.f);
 Vec shear{gait.z*sector*.95f,0,gait.x*sector*.68f};shear=Limit(shear,.024f);
 Frame out;out.y=out.y+shear;out.p=shear*-1.075f;return out;
}
}

