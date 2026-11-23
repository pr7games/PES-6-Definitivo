#pragma once
#include "cloth_grid.h"
#include "cloth_control_bounds.h"
#include <vector>
#include <cstring>
namespace pes6::cloth {
struct ContactInfluence {unsigned bone;float weight;};
struct ContactVertex {Vec p;std::array<ContactInfluence,4> influence;};
struct ContactNode {unsigned start,count,left,right;};
struct ContactBounds {Vec low{1e9f,1e9f,1e9f},high{-1e9f,-1e9f,-1e9f};void Add(Vec p){low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};}float Distance(Vec p)const{const Vec d{std::max({low.x-p.x,0.f,p.x-high.x}),std::max({low.y-p.y,0.f,p.y-high.y}),std::max({low.z-p.z,0.f,p.z-high.z})};return Dot(d,d);}};
inline Vec SkinContact(const ContactVertex& v,const std::array<Frame,50>& skin){Vec p{};for(auto i:v.influence)if(i.weight>0)p=p+skin[i.bone].Point(v.p)*i.weight;return p;}
inline bool ShirtControl(unsigned b){return b>=28&&b<=33;}
struct ContactStats {unsigned corrected=0,remaining=0,worstProbe=0;float worst=0,maxCorrection=0,penetrationEnergy=0;Vec worstPoint{},worstNormal{};};
// Collision is evaluated AFTER skinning/clamping on actual mesh vertices and
// triangle barycentres, rather than only on the much smaller simulation cage.
// BVH topology is immutable; bounds follow the currently skinned shorts.
template<size_t PV,size_t PT,size_t SV,size_t PR,size_t BN>
ContactStats ResolveGarmentContact(std::array<Frame,50>& skin,const std::array<ContactVertex,PV>& pants,const std::array<std::array<unsigned,3>,PT>& triangles,const std::array<ContactVertex,SV>& shirt,const std::array<std::array<unsigned,3>,PR>& probes,const std::array<ContactNode,BN>& nodes,const std::array<unsigned,PT>& order,unsigned iterations=3,bool audit=false,std::vector<std::pair<unsigned,float>>* violations=nullptr,bool coherent=false){
 // Share the posed collider between the two constraint phases and repeated
 // draws. Only bones actually referenced by the shorts invalidate this cache.
 // Probe membership is immutable. Keep its original order and arithmetic,
 // but avoid rechecking pinned-only probes on every contact iteration.
 struct ActiveProbes{const ContactVertex* source=nullptr;const void* topology=nullptr;std::vector<unsigned> indices;std::array<std::array<float,6>,PR> weights{},spread{};std::array<Vec,PR> rest{};std::array<float,PR> denominator{};};
 static thread_local ActiveProbes active;
 if(active.source!=shirt.data()||active.topology!=probes.data()){
  active.source=shirt.data();active.topology=probes.data();active.indices.clear();active.indices.reserve(PR);
  active.weights={};active.rest={};active.denominator={};
  for(unsigned k=0;k<PR;k++){for(auto v:probes[k]){active.rest[k]=active.rest[k]+shirt[v].p*(1.f/3);for(auto inf:shirt[v].influence)if(ShirtControl(inf.bone)&&inf.weight>0)active.weights[k][inf.bone-28]+=inf.weight/3.f;}constexpr unsigned left[6]={3,0,1,4,5,2},right[6]={1,2,5,0,3,4};for(unsigned i=0;i<6;i++){active.spread[k][i]=active.weights[k][i]*.80f+(active.weights[k][left[i]]+active.weights[k][right[i]])*.10f;active.denominator[k]+=active.weights[k][i]*active.spread[k][i];}}
  for(unsigned k=0;k<PR;k++){bool movable=false;for(auto v:probes[k])for(auto i:shirt[v].influence)if(ShirtControl(i.bone)&&i.weight>.001f){movable=true;break;}if(movable)active.indices.push_back(k);}
 }
 struct TriangleGeometry{Vec a,b,c,normal,e1,e2,rayH;float rayInv=0;bool valid=false;};
 struct GeometryCache {const ContactVertex* source=nullptr;std::array<bool,50> used{};std::array<Frame,50> frames{};std::array<Vec,PV> positions{};std::array<TriangleGeometry,PT> surfaces{};std::array<ContactBounds,BN> boxes{};bool ready=false;};
 static thread_local GeometryCache geometry;
 bool same=geometry.ready&&geometry.source==pants.data();
 if(same)for(unsigned b=0;b<50;b++)if(geometry.used[b]&&std::memcmp(&geometry.frames[b],&skin[b],sizeof(Frame))){same=false;break;}
 if(!same){
  if(geometry.source!=pants.data()){geometry.used={};for(const auto& v:pants)for(auto i:v.influence)if(i.weight>0)geometry.used[i.bone]=true;geometry.source=pants.data();}
  geometry.frames=skin;geometry.boxes={};for(unsigned i=0;i<PV;i++)geometry.positions[i]=SkinContact(pants[i],skin);
  for(unsigned i=0;i<PT;i++){const auto t=triangles[i];auto& g=geometry.surfaces[i];g.a=geometry.positions[t[0]];g.b=geometry.positions[t[1]];g.c=geometry.positions[t[2]];g.e1=g.b-g.a;g.e2=g.c-g.a;const auto cross=Cross(g.e1,g.e2);const float area=Length(cross);g.valid=area>=1e-8f;g.normal=g.valid?cross*(1.f/area):Vec{};g.rayH=Cross(Vec{0,0,1},g.e2);const float det=Dot(g.e1,g.rayH);g.rayInv=std::abs(det)<1e-8f?0.f:1.f/det;}
 }
 const auto& posed_pants=geometry.positions;auto& bounds=geometry.boxes;
 if(!same){
 for(unsigned j=BN;j-->0;){const auto n=nodes[j];if(n.count){for(unsigned k=n.start;k<n.start+n.count;k++)for(auto v:triangles[order[k]])bounds[j].Add(posed_pants[v]);}else{bounds[j].Add(bounds[n.left].low);bounds[j].Add(bounds[n.left].high);bounds[j].Add(bounds[n.right].low);bounds[j].Add(bounds[n.right].high);}}
 geometry.ready=true;}
 const auto inside=[&](Vec p){
  int winding=0;unsigned stack[32],size=1;stack[0]=0;
  while(size){const auto j=stack[--size];const auto box=bounds[j];if(p.x<box.low.x||p.x>box.high.x||p.y<box.low.y||p.y>box.high.y||p.z>box.high.z)continue;const auto n=nodes[j];if(!n.count){stack[size++]=n.left;stack[size++]=n.right;continue;}
   for(unsigned k=n.start;k<n.start+n.count;k++){const auto& g=geometry.surfaces[order[k]];const float inv=g.rayInv;if(inv==0)continue;const auto d=p-g.a;const float u=Dot(d,g.rayH)*inv;if(u<0||u>1)continue;const auto q=Cross(d,g.e1);const float v=q.z*inv;if(v<0||u+v>1)continue;const float z=Dot(g.e2,q)*inv;if(z>1e-6f)winding+=inv>0?1:-1;}
  }return winding!=0;
 };
 const auto closest=[&](Vec p,Vec& normal,float& signed_distance){
  const float rootDistance=bounds[0].Distance(p);if(rootDistance>.006f*.006f)return false;
  float best=.08f*.08f;bool found=false;unsigned stack[32],size=1;float distances[32];stack[0]=0;distances[0]=rootDistance;
  // Bounds stay fixed throughout this query. Carry the child distance onto
  // the stack instead of calculating it again when the child is popped.
  while(size){--size;const auto j=stack[size];if(distances[size]>best)continue;const auto n=nodes[j];if(!n.count){const auto dl=bounds[n.left].Distance(p),dr=bounds[n.right].Distance(p);const auto push=[&](unsigned child,float distance){if(distance<=best){stack[size]=child;distances[size++]=distance;}};if(dl<dr){push(n.right,dr);push(n.left,dl);}else{push(n.left,dl);push(n.right,dr);}continue;}
   for(unsigned k=n.start;k<n.start+n.count;k++){const auto& g=geometry.surfaces[order[k]];if(!g.valid)continue;const auto q=Closest(p,g.a,g.b,g.c).point,delta=p-q;const float squared=Dot(delta,delta);if(squared>=best)continue;best=squared;normal=g.normal;signed_distance=Dot(delta,normal);found=true;}
  }return found&&(signed_distance>=0||inside(p));
 };
 ContactStats stats;const auto initial=skin;std::array<Vec,50> angular_offset{};std::array<Vec,50> local_offset{};
 
 // Sequential PBD on the rendered garment. Keep the pinned top controls fixed;
 // independent averaged pushes used to cancel around a raised thigh.
 for(unsigned iteration=0;iteration<iterations;iteration++){
  unsigned contacts=0;
  for(const auto probeIndex:active.indices){
   const auto probe=probes[probeIndex];
   Vec p{};for(auto v:probe)p=p+SkinContact(shirt[v],skin)*(1.f/3);Vec normal;float distance;
   if(!closest(p,normal,distance)||distance>=.006f)continue;
   // A raised thigh can make the closest face point IN toward the pelvis.
   // The outer jersey must slide over that leg, not be pushed behind it.
   const auto rest=active.rest[probeIndex];const auto outward=Unit(skin[0].Linear({rest.x,0,rest.z+.005f}),{0,0,1});
   if(distance<0&&rest.z>-.015f)normal=Unit(normal*.35f+Unit(skin[0].y)*.85f+outward*.20f);
   else if(distance<0&&Dot(normal,outward)<.2f)normal=Unit(outward*.65f+Unit(skin[0].y)*.75f);

   if(coherent){
    // Final translation-only contact follows the actual controls touched by
    // this probe. A raised left thigh cannot lift the whole opposite band.
    const auto& w=active.spread[probeIndex];const float den=active.denominator[probeIndex];
    if(den>.016f){const float lambda=std::min(.009f,.007f-distance)/den;
     for(unsigned b=28;b<34;b++)if(w[b-28]>0){const auto next=Limit(local_offset[b]+normal*(lambda*w[b-28]),.055f),shift=next-local_offset[b];local_offset[b]=next;skin[b].p=skin[b].p+shift;stats.maxCorrection=std::max(stats.maxCorrection,Length(next));}
     contacts++;
    }
    continue;
   }
   std::array<float,50> weights{};std::array<Vec,50> moments{},rot_gradient{},pivots{};float denominator=0;
   for(auto v:probe)for(auto i:shirt[v].influence)if(ShirtControl(i.bone)){weights[i.bone]+=i.weight/3.f;moments[i.bone]=moments[i.bone]+shirt[v].p*(i.weight/3.f);}
   constexpr float inverse_inertia=32.f;
   for(unsigned b=28;b<34;b++)if(weights[b]>0){const auto bounds=control_bounds[b];pivots[b]={(bounds.low.x+bounds.high.x)*.5f,bounds.high.y,(bounds.low.z+bounds.high.z)*.5f};const auto lever=skin[b].Linear(moments[b]*(1/weights[b])-pivots[b]);rot_gradient[b]=Cross(lever,normal);denominator+=weights[b]*weights[b]*(1+inverse_inertia*Dot(rot_gradient[b],rot_gradient[b]));}
   if(denominator<.05f)continue;const float lambda=std::min(.012f,.008f-distance)/denominator;
   for(unsigned b=28;b<34;b++)if(weights[b]>0){
    const auto next_angle=Limit(angular_offset[b]+rot_gradient[b]*(lambda*weights[b]*inverse_inertia),.45f),omega=next_angle-angular_offset[b];angular_offset[b]=next_angle;
    const float angle=Length(omega);const bool rotating=angle>=1e-8f;const auto axis=rotating?omega*(1/angle):Vec{};const float cosine=rotating?std::cos(angle):1.f,sine=rotating?std::sin(angle):0.f;
    const auto rotate=[&](Vec v,Vec){if(!rotating)return v;return v*cosine+Cross(axis,v)*sine+axis*(Dot(axis,v)*(1-cosine));};
    const auto pivot=skin[b].Point(pivots[b]);Frame desired{rotate(skin[b].x,omega),rotate(skin[b].y,omega),rotate(skin[b].z,omega),pivot+rotate(skin[b].p-pivot,omega)+normal*(lambda*weights[b])};
    const auto bounds=control_bounds[b];float maximum=0;for(unsigned c=0;c<8;c++){const Vec point{c&1?bounds.high.x:bounds.low.x,c&2?bounds.high.y:bounds.low.y,c&4?bounds.high.z:bounds.low.z};maximum=std::max(maximum,Length(desired.Point(point)-initial[b].Point(point)));}
    const float limit=b>=42?.120f:.075f;if(maximum>limit){const float t=limit/maximum;desired={initial[b].x*(1-t)+desired.x*t,initial[b].y*(1-t)+desired.y*t,initial[b].z*(1-t)+desired.z*t,initial[b].p*(1-t)+desired.p*t};}
    skin[b]=desired;stats.maxCorrection=std::max(stats.maxCorrection,std::min(maximum,limit));
   }

   contacts++;
  }
  stats.corrected+=contacts;if(!contacts)break;
 }
 if(!audit)return stats;
 std::array<Vec,SV> final_positions;for(unsigned i=0;i<SV;i++)final_positions[i]=SkinContact(shirt[i],skin);
 unsigned probe_id=0;for(const auto probe:probes){const unsigned id=probe_id++;const auto p=(final_positions[probe[0]]+final_positions[probe[1]]+final_positions[probe[2]])*(1.f/3);Vec n;float d;if(closest(p,n,d)&&d<-.001f){if(violations)violations->emplace_back(id,-d);stats.remaining++;stats.penetrationEnergy+=d*d;if(-d>stats.worst){stats.worst=-d;stats.worstPoint=p;stats.worstNormal=n;stats.worstProbe=id;}}}
 return stats;
}
}



