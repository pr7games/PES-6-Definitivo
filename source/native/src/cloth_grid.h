#pragma once
#include "cloth_spring.h"
namespace pes6::cloth {
struct Frame {
 Vec x{1,0,0},y{0,1,0},z{0,0,1},p{};
 Vec Linear(Vec v)const{return x*v.x+y*v.y+z*v.z;}
 Vec Point(Vec v)const{return Linear(v)+p;}
};
inline Frame Combine(Frame a,Frame b){return{b.Linear(a.x),b.Linear(a.y),b.Linear(a.z),b.Point(a.p)};}
inline Frame Inverse(Frame a){
 const float d=Dot(a.x,Cross(a.y,a.z));if(!std::isfinite(d)||std::abs(d)<1e-6f)return{};
 const auto c0=Cross(a.y,a.z)*(1/d),c1=Cross(a.z,a.x)*(1/d),c2=Cross(a.x,a.y)*(1/d);
 Frame b{{c0.x,c1.x,c2.x},{c0.y,c1.y,c2.y},{c0.z,c1.z,c2.z},{}};b.p=b.Linear(a.p)*-1.f;return b;
}
inline Vec Unit(Vec a,Vec fallback={0,1,0}){const float n=Length(a);return Finite(a)&&n>1e-6f?a*(1/n):fallback;}
// Position-based cloth: pinned upper row, free fabric below, structural/shear
// constraints, softer bending constraints and collision projection per node.
template<unsigned Rows,unsigned Cols>struct Grid {
 static constexpr unsigned count=Rows*Cols;
 struct Edge{unsigned a,b;float length,stiffness;};
 std::array<Vec,count> rest{},position{},previous{},last_target{};
 std::array<Edge,count*6> edges{};unsigned edge_count=0;bool ready=false,pin_bottom=false;
 float damping=2.4f,max_offset=.16f,tether=0;
 bool Pinned(unsigned i)const{return i<Cols||(pin_bottom&&i>=count-Cols);}
 void Setup(const std::array<Vec,count>& points){
  rest=points;edge_count=0;
  const auto edge=[&](unsigned a,unsigned b,float stiffness){edges[edge_count++]={a,b,Length(rest[b]-rest[a]),stiffness};};
  for(unsigned r=0;r<Rows;r++)for(unsigned c=0;c<Cols;c++){
   const unsigned a=r*Cols+c,next=r*Cols+(c+1)%Cols;
   edge(a,next,.88f);edge(a,r*Cols+(c+2)%Cols,.075f);
   if(r+1<Rows){edge(a,a+Cols,.96f);edge(a,next+Cols,.28f);edge(next,a+Cols,.28f);}
   if(r+2<Rows)edge(a,a+2*Cols,.055f);
  }
 }
 void Reset(const std::array<Vec,count>& target){position=previous=last_target=target;ready=true;}
 template<class Project>void LimitStrain(const std::array<Vec,count>& target,Project project,unsigned passes=2,bool pin_to_target=true){
  for(unsigned pass=0;pass<passes;pass++){
   if(pin_to_target)for(unsigned i=0;i<count;i++)if(Pinned(i))position[i]=target[i];
   for(unsigned k=0;k<edge_count;k++){
    const auto& e=edges[k];if(e.stiffness<.8f)continue;
    const float wa=Pinned(e.a)?0.f:1.f,wb=Pinned(e.b)?0.f:1.f;if(wa+wb==0)continue;
    const auto delta=position[e.b]-position[e.a];const float n=Length(delta);if(n<1e-6f)continue;
    const float desired=std::clamp(n,e.length*.75f,e.length*1.06f);
    const auto correction=delta*((n-desired)/(n*(wa+wb)));position[e.a]=position[e.a]+correction*wa;position[e.b]=position[e.b]-correction*wb;
   }
   for(unsigned i=Cols;i<count;i++)if(!Pinned(i))project(position[i]);
  }
 }
 template<class Project>void Step(const std::array<Vec,count>& target,float dt,Project project){
  bool reset=!ready||!std::isfinite(dt)||dt<0||dt>.10f;
  for(unsigned c=0;c<Cols&&!reset;c++)if(Length(target[c]-last_target[c])>.6f)reset=true;
  if(reset){Reset(target);dt=0;}
  if(dt==0){if(!reset)return;}
  const unsigned steps=std::max(1u,unsigned(std::ceil(dt*60.f-1e-4f)));
  const float h=dt>0?dt/steps:1.f/60;
  const float attenuation=std::exp(-damping*h);
  for(unsigned step=0;step<steps;step++){
   const float blend=float(step+1)/steps;
   for(unsigned i=Cols;i<count;i++)if(!Pinned(i)){
    const auto p=position[i];
    const auto velocity=Limit((p-previous[i])*attenuation,6.f*h);
    position[i]=p+velocity+Vec{0,-9.81f*h*h,0};previous[i]=p;
   }
   for(unsigned iteration=0;iteration<3;iteration++){
    for(unsigned c=0;c<count;c++)if(Pinned(c))position[c]=last_target[c]+(target[c]-last_target[c])*blend;
    for(unsigned k=0;k<edge_count;k++){
     const auto& e=edges[k];const float wa=Pinned(e.a)?0.f:1.f,wb=Pinned(e.b)?0.f:1.f;if(wa+wb==0)continue;
     const auto delta=position[e.b]-position[e.a];const float n=Length(delta);if(n<1e-6f)continue;
     const auto correction=delta*((n-e.length)*e.stiffness/(n*(wa+wb)));
     position[e.a]=position[e.a]+correction*wa;position[e.b]=position[e.b]-correction*wb;
    }
    for(unsigned i=Cols;i<count;i++)if(!Pinned(i)){
     position[i]=target[i]+Limit((position[i]-target[i])*(1-tether),max_offset);
     project(position[i]);
    }
   }
   LimitStrain(target,project,1,false);
   for(unsigned c=0;c<count;c++)if(Pinned(c))previous[c]=position[c];
  }
  last_target=target;
 }
};
inline bool ProjectEllipsoid(Vec& p,Frame body,Vec radii){
 const auto q=p-body.p;
 const Vec local{Dot(q,body.x)/radii.x,Dot(q,body.y)/radii.y,Dot(q,body.z)/radii.z};
 const float squared=Dot(local,local);if(squared>=1)return false;const float n=std::sqrt(squared);
 const auto surface=n>.001f?local*(1/n):Vec{0,0,1};
 p=body.p+body.x*surface.x*radii.x+body.y*surface.y*radii.y+body.z*surface.z*radii.z;return true;
}
inline bool ProjectCapsule(Vec& p,Vec a,Vec b,float radius){
 const auto ab=b-a;const float n=Dot(ab,ab);if(n<.01f||n>1.f)return false;
 const auto centre=a+ab*std::clamp(Dot(p-a,ab)/n,0.f,1.f),delta=p-centre;const float squared=Dot(delta,delta);
 if(squared>=radius*radius)return false;p=centre+(squared>1e-10f?delta*(radius/std::sqrt(squared)):Vec{0,0,radius});return true;
}
inline std::array<Vec,36> ShirtRest(){
 std::array<Vec,36> points{};constexpr float height[3]={1.40f,1.16f,.805f};
 for(unsigned r=0;r<3;r++)for(unsigned c=0;c<12;c++){
  const float a=float(c)*6.28318530718f/12,cs=std::cos(a),sn=std::sin(a);
  points[r*12+c]={.180f*cs,height[r],(sn>=0?.137f:.120f)*sn-.005f};
 }return points;
}
inline std::array<Vec,16> PantsRest(unsigned leg){
 std::array<Vec,16> points{};
 for(unsigned r=0;r<2;r++)for(unsigned c=0;c<8;c++){
  const float a=float(c)*6.28318530718f/8,scale=r?.88f:1.f;
  points[r*8+c]={(leg?-.09047f:.09048f)+.105f*std::cos(a)*scale,r?.642f:.865f,.009f+.115f*std::sin(a)*scale};
 }return points;
}
struct TrianglePoint {Vec point;float a,b,c;};
inline TrianglePoint Closest(Vec p,Vec a,Vec b,Vec c){
 const auto ab=b-a,ac=c-a,ap=p-a;const float d1=Dot(ab,ap),d2=Dot(ac,ap);
 if(d1<=0&&d2<=0)return{a,1,0,0};
 const auto bp=p-b;const float d3=Dot(ab,bp),d4=Dot(ac,bp);if(d3>=0&&d4<=d3)return{b,0,1,0};
 const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0){const float v=d1/(d1-d3);return{a+ab*v,1-v,v,0};}
 const auto cp=p-c;const float d5=Dot(ab,cp),d6=Dot(ac,cp);if(d6>=0&&d5<=d6)return{c,0,0,1};
 const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0){const float w=d2/(d2-d6);return{a+ac*w,1-w,0,w};}
 const float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0){const float w=(d4-d3)/((d4-d3)+(d5-d6));return{b+(c-b)*w,0,1-w,w};}
 const float denominator=va+vb+vc;if(std::abs(denominator)<1e-10f)return{a,1,0,0};
 const float v=vb/denominator,w=vc/denominator;return{a+ab*v+ac*w,1-v-w,v,w};
}
// Thin vertex/face contact: corrections are shared with free shorts nodes.
// This complements continuous body capsules; it is not triangle/triangle CCD.
template<unsigned Rows,unsigned Cols>inline unsigned Couple(Grid<Rows,Cols>& shirt,Grid<2,8>& pants){
 Vec low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f},centre{};
 for(auto p:pants.position){low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};centre=centre+p*(1.f/16);}
 unsigned contacts=0;
 for(unsigned i=Cols;i<Rows*Cols;i++){
  if(shirt.Pinned(i))continue;
  auto& p=shirt.position[i];
  if(p.x<low.x-.015f||p.x>high.x+.015f||p.y<low.y-.015f||p.y>high.y+.015f||p.z<low.z-.015f||p.z>high.z+.015f)continue;
  for(unsigned col=0;col<8;col++)for(unsigned half=0;half<2;half++){
   const unsigned next=(col+1)%8,a=half?next:col,b=half?next+8:next,c=col+8;
   const auto pa=pants.position[a],pb=pants.position[b],pc=pants.position[c];const auto q=Closest(p,pa,pb,pc);
   const auto delta=p-q.point;if(Dot(delta,delta)>.015f*.015f)continue;
   auto normal=Unit(Cross(pb-pa,pc-pa));if(Dot(normal,(pa+pb+pc)*(1.f/3)-centre)<0)normal=normal*-1.f;
   const float distance=Dot(delta,normal);if(distance>=.004f||distance<-.015f)continue;
   const float wa=pants.Pinned(a)?0:q.a,wb=pants.Pinned(b)?0:q.b,wc=pants.Pinned(c)?0:q.c,denominator=1+wa*wa+wb*wb+wc*wc;
   const auto correction=normal*((.004f-distance)/denominator);
   p=p+correction;pants.position[a]=pants.position[a]-correction*wa;pants.position[b]=pants.position[b]-correction*wb;pants.position[c]=pants.position[c]-correction*wc;contacts++;
  }
 }return contacts;
}
}
