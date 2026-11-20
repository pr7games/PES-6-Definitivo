#pragma once
#include "cloth_grid.h"
namespace pes6::cloth {
inline Frame MixHemFrame(Frame a,Frame b,float t,Vec pivot){
 const auto x=Unit(a.x*(1-t)+b.x*t,Unit(a.x,{1,0,0}));
 const auto raw=a.y*(1-t)+b.y*t,y=Unit(raw-x*Dot(raw,x),Unit(a.y)),z=Unit(Cross(x,y),Unit(a.z,{0,0,1}));
 Frame out{x*(Length(a.x)*(1-t)+Length(b.x)*t),y*(Length(a.y)*(1-t)+Length(b.y)*t),z*(Length(a.z)*(1-t)+Length(b.z)*t),{}};
 out.p=a.Point(pivot)*(1-t)+b.Point(pivot)*t-out.Linear(pivot);return out;
}
inline Frame LocalHemAttachment(Frame root,const std::array<Frame,2>& thighs,Vec pivot){
 // Centre-front/back hang from the pelvis; only the side above the bending
 // thigh follows its lift. Keep the actual sector pivot, never a shared one.
 float side=std::clamp(std::abs(pivot.x)/.15f,0.f,1.f);side=side*side*(3-2*side);
 const unsigned leg=pivot.x>=0?0:1;
 const auto up=Unit(root.y),origin=root.Point(pivot);
 float lift=std::clamp(Dot(thighs[leg].Point(pivot)-origin,up)/.16f,0.f,1.f);lift=lift*lift*(3-2*lift);
 const float front=std::clamp((pivot.z+.055f)/.12f,0.f,1.f);
 const auto attached=MixHemFrame(root,thighs[leg],side*(.06f+.42f*lift)*front,pivot);
 return attached; // The rear hangs from the pelvis instead of kicking with the thigh.
}
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
 Vec shear{gait.z*sector*.65f,0,gait.x*sector*.45f};shear=Limit(shear,.018f);
 Frame out;out.y=out.y+shear;out.p=shear*-1.075f;return out;
}
}
