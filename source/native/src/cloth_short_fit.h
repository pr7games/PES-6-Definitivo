#pragma once
#include "cloth_contact.h"
namespace pes6::cloth {
struct ShortProbe {std::array<ContactVertex,3> v;};
struct ShortFitStats {std::array<float,2> scale{1,1};unsigned remaining=0;float worst=0;};
inline float ThighGap(Vec p,Vec hip,Vec knee,Vec right,float scale=1){
 const Vec axis=knee-hip;const float length2=Dot(axis,axis);if(length2<.04f)return 1;
 const float t=std::clamp(Dot(p-hip,axis)/length2,0.f,1.f);const auto up=Unit(hip-knee),forward=Unit(Cross(right,up),{0,0,1});right=Unit(Cross(up,forward),{1,0,0});
 const auto d=p-(hip+axis*t);const float rx=(.092f-.010f*t)*scale,rz=(.110f-.015f*t)*scale;
 const float x=Dot(d,right)/rx,z=Dot(d,forward)/rz;return (std::sqrt(x*x+z*z)-1)*std::min(rx,rz);
}
inline void ProjectThigh(Vec& p,Vec hip,Vec knee,Vec right){
 const auto axis=knee-hip;const float length2=Dot(axis,axis);if(length2<.04f)return;
 const float t=std::clamp(Dot(p-hip,axis)/length2,0.f,1.f);const auto up=Unit(hip-knee),forward=Unit(Cross(right,up),{0,0,1});right=Unit(Cross(up,forward),{1,0,0});
 const auto centre=hip+axis*t,d=p-centre;const float rx=.088f-.007f*t,rz=.104f-.012f*t,x=Dot(d,right)/rx,z=Dot(d,forward)/rz,n=std::sqrt(x*x+z*z);
 if(n<1){const float q=n>.001f?1/n:0;p=centre+right*(x*rx*q)+forward*(n>.001f?z*rz*q:rz);}
}
inline Frame ShortRadialFit(unsigned leg,float scale){Frame f;f.x={scale,0,0};f.z={0,0,scale};f.p={(leg?-.09047f:.09048f)*(1-scale),0,.015404f*(1-scale)};return f;}
inline Vec ShortPoint(const ShortProbe& p,const std::array<Frame,50>& skin){Vec out{};for(const auto& v:p.v)out=out+SkinContact(v,skin)*(1.f/3);return out;}
template<size_t N>ShortFitStats FitVisibleShorts(std::array<Frame,50>& skin,const std::array<ShortProbe,N>& probes){
 const auto initial=skin;ShortFitStats stats;std::array<Vec,2> hips,knees;std::array<float,2> scales;
 for(unsigned leg=0;leg<2;leg++){const unsigned h=leg?2:1,k=leg?7:5;hips[leg]=skin[h].Point({leg?-.09047f:.09048f,.9548f,.015404f});knees[leg]=skin[k].Point({leg?-.09047f:.09048f,.539f,.000014f});scales[leg]=std::max(Length(skin[h].x),Length(skin[h].z));}
 for(unsigned iteration=0;iteration<6;iteration++){
  std::array<float,2> deficit{};
  for(const auto& probe:probes){const Vec rest=(probe.v[0].p+probe.v[1].p+probe.v[2].p)*(1.f/3);const unsigned leg=rest.x<0;const float gap=ThighGap(ShortPoint(probe,skin),hips[leg],knees[leg],skin[leg?2:1].x,scales[leg]);deficit[leg]=std::max(deficit[leg],.004f-gap);}
  if(deficit[0]<.0002f&&deficit[1]<.0002f)break;
  for(unsigned leg=0;leg<2;leg++){stats.scale[leg]=std::min(1.45f,stats.scale[leg]+std::max(0.f,deficit[leg])/.065f);const auto fit=ShortRadialFit(leg,stats.scale[leg]);for(unsigned b:{leg?27u:26u,leg?35u:34u})skin[b]=Combine(fit,initial[b]);}
 }
 for(const auto& probe:probes){const Vec rest=(probe.v[0].p+probe.v[1].p+probe.v[2].p)*(1.f/3);const unsigned leg=rest.x<0;const float gap=ThighGap(ShortPoint(probe,skin),hips[leg],knees[leg],skin[leg?2:1].x,scales[leg]);if(gap<-.0005f){stats.remaining++;stats.worst=std::max(stats.worst,-gap);}}
 return stats;
}
}
