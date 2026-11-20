#pragma once
#include "net_sound_contact.h"
#include <array>
#include <vector>
#include <map>
#include <set>
namespace pes6::stadium_net {
using Vec=net_sound::Vec;
inline Vec Add(Vec a,Vec b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
inline Vec Mul(Vec a,float s){return {a.x*s,a.y*s,a.z*s};}
inline float Length(Vec a){return std::hypot(a.x,a.y,a.z);}
inline Vec Sub(Vec a,Vec b){return Add(a,Mul(b,-1));}
inline float Dot(Vec a,Vec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline Vec Cross(Vec a,Vec b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline bool Finite(Vec p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
inline Vec ClosestTriangle(Vec p,Vec a,Vec b,Vec c){
 const auto ab=Sub(b,a),ac=Sub(c,a),ap=Sub(p,a);const float d1=Dot(ab,ap),d2=Dot(ac,ap);
 if(d1<=0&&d2<=0)return a;
 const auto bp=Sub(p,b);const float d3=Dot(ab,bp),d4=Dot(ac,bp);if(d3>=0&&d4<=d3)return b;
 const float vc=d1*d4-d3*d2;if(vc<=0&&d1>=0&&d3<=0)return Add(a,Mul(ab,d1/(d1-d3)));
 const auto cp=Sub(p,c);const float d5=Dot(ab,cp),d6=Dot(ac,cp);if(d6>=0&&d5<=d6)return c;
 const float vb=d5*d2-d1*d6;if(vb<=0&&d2>=0&&d6<=0)return Add(a,Mul(ac,d2/(d2-d6)));
 const float va=d3*d6-d5*d4;if(va<=0&&(d4-d3)>=0&&(d5-d6)>=0)return Add(b,Mul(Sub(c,b),(d4-d3)/((d4-d3)+(d5-d6))));
 const float inv=1/(va+vb+vc);return Add(a,Add(Mul(ab,vb*inv),Mul(ac,vc*inv)));
}
// Visual dynamics also need vertical roof impacts. Keep the audio detector
// unchanged: its horizontal response deliberately excludes turf bounces.
struct Contact {
 Vec previous{},velocity{};bool ready=false,velocity_ready=false;float cooldown=0;
 void Reset(){*this={};}
 float Sample(Vec p,float dt){
  if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z)||dt<=0||dt>.08f){Reset();return 0;}
  if(!ready){previous=p;ready=true;return 0;}
  const auto old=previous;previous=p;
  if(Length(Add(p,Mul(old,-1)))>2.f){velocity_ready=false;cooldown=.12f;return 0;}
  cooldown=std::max(0.f,cooldown-dt);
  const Vec v=Mul(Add(p,Mul(old,-1)),1/dt);
  const float horizontal=std::hypot(velocity.x,velocity.z),change=std::hypot(v.x-velocity.x,v.z-velocity.z);
  const bool side=horizontal>=.65f&&change>=.8f&&change>=horizontal*.22f;
  // Both samples must be near the roof. A low turf rebound inside the
  // goal must never shake the net, even when its vertical speed is large.
  const float vertical=std::abs(velocity.y),verticalChange=std::abs(v.y-velocity.y);
  const bool roof=std::min(old.y,p.y)>=2.14f&&vertical>=.65f&&verticalChange>=.8f&&verticalChange>=vertical*.22f;
  const bool hit=velocity_ready&&cooldown==0&&(net_sound::Contact::Surface(p)||net_sound::Contact::Surface(old))&&(side||roof);
  const float speed=Length(velocity);velocity=v;velocity_ready=true;
  if(!hit)return 0;cooldown=.28f;return speed;
 }
};
struct Node {Vec rest{},offset{},velocity{};bool pinned=false;std::vector<unsigned> adjacent;};
struct Face {unsigned a,b,c;Vec normal;};
struct Net {
 static constexpr float MaxOffset=1.10f,BallRadius=.115f;
 std::vector<Node> nodes;std::vector<unsigned> vertexNodes;std::vector<Vec> forces;
 std::vector<Face> faces;Vec previousBall{},pressurePoint{},pressureNormal{},pressureBall{};bool ballReady=false,pressing=false;float cooldown=0;
 void Build(const std::vector<Vec>& vertices){
  nodes.clear();vertexNodes.clear();faces.clear();std::map<std::array<int,3>,unsigned> welded;
  for(auto p:vertices){const std::array<int,3> key{int(std::lround(p.x*10000)),int(std::lround(p.y*10000)),int(std::lround(p.z*10000))};auto [it,fresh]=welded.emplace(key,unsigned(nodes.size()));
   if(fresh){const float x=std::abs(p.x);const bool pin=p.y<.025f||x<52.61f||(x>55.35f&&std::abs(p.z)>4.20f&&p.y>2.75f);nodes.push_back({p,{},{},pin,{}});}vertexNodes.push_back(it->second);
  }
  std::set<std::pair<unsigned,unsigned>> edges;
  for(size_t i=0;i+2<vertexNodes.size();i+=3)for(unsigned j=0;j<3;j++){auto a=vertexNodes[i+j],b=vertexNodes[i+(j+1)%3];if(a!=b)edges.emplace(std::min(a,b),std::max(a,b));}
  for(auto [a,b]:edges){nodes[a].adjacent.push_back(b);nodes[b].adjacent.push_back(a);}
  std::set<std::array<unsigned,3>> faceKeys;
  for(size_t i=0;i+2<vertexNodes.size();i+=3){
   const auto a=vertexNodes[i],b=vertexNodes[i+1],c=vertexNodes[i+2];const auto center=Mul(Add(nodes[a].rest,Add(nodes[b].rest,nodes[c].rest)),1.f/3);
   std::array<unsigned,3> key{a,b,c};std::sort(key.begin(),key.end());if(!faceKeys.insert(key).second)continue;
   auto n=Cross(Sub(nodes[b].rest,nodes[a].rest),Sub(nodes[c].rest,nodes[a].rest));const float length=Length(n);
   // Exclude thin suspension cords behind/above the actual cloth.
   if(length<.0001f||center.y>2.6f)continue;n=Mul(n,1/length);
   const Vec inside{center.x<0?-53.2f:53.2f,1.2f,0};if(Dot(n,Sub(center,inside))<0)n=Mul(n,-1);
   faces.push_back({a,b,c,n});
  }
  forces.resize(nodes.size());
  Reset();
 }
 void Reset(){for(auto& n:nodes){n.offset={};n.velocity={};}ballReady=pressing=false;cooldown=0;}
 void ForgetBall(){ballReady=pressing=false;cooldown=0;}
 void Impulse(Vec point,Vec direction,float speed){
  if(nodes.empty()||speed<.65f||!std::isfinite(speed))return;
  unsigned closest=0;float distance=1e9f;for(unsigned i=0;i<nodes.size();i++){const float d=Length(Add(nodes[i].rest,Mul(point,-1)));if(d<distance&&!nodes[i].pinned){distance=d;closest=i;}}
  const auto center=nodes[closest].rest;float length=Length(direction);if(length<.01f)direction={point.x<0?-1.f:1.f,0,0};else direction=Mul(direction,1/length);
  for(auto& n:nodes)if(!n.pinned){const float d=Length(Sub(n.rest,center)),weight=std::exp(-d*d/.75f);n.velocity=Add(n.velocity,Mul(direction,std::min(4.f,speed*.16f)*weight));}
 }
 // Sweep the real ball against donor cloth, before the deeper PES6 collider
 // rebounds it. Keep a local pressure constraint while the ball stretches it.
 float SampleBall(Vec ball){
  const bool nearGoal=Finite(ball)&&!nodes.empty()&&ball.x*nodes.front().rest.x>0&&std::abs(ball.x)>52.2f&&std::abs(ball.x)<56.6f&&std::abs(ball.z)<5.1f&&ball.y>=-.15f&&ball.y<3.5f;
  if(!nearGoal){ballReady=pressing=false;return 0;}
  if(!ballReady){previousBall=ball;ballReady=true;return 0;}
  const auto delta=Sub(ball,previousBall),old=previousBall;previousBall=ball;
  if(Length(delta)>2.f){pressing=false;cooldown=.12f;return 0;}
  cooldown=std::max(0.f,cooldown-1.f/60);float hitSpeed=0,best=2;Vec hitPoint{},hitNormal{};
  if(cooldown==0&&!pressing)for(const auto& f:faces){
   const auto a=nodes[f.a].rest,b=nodes[f.b].rest,c=nodes[f.c].rest;
   const float approach=Dot(delta,f.normal);if(std::abs(approach)<.003f)continue;
   const auto normal=Mul(f.normal,approach>0?1.f:-1.f);
   const float before=Dot(Sub(old,a),normal),after=Dot(Sub(ball,a),normal);
   if(before> -BallRadius+.015f||after< -BallRadius||after<=before)continue;
   const float t=std::clamp((-BallRadius-before)/(after-before),0.f,1.f);if(t>=best)continue;
   const auto center=Add(old,Mul(delta,t)),onPlane=Sub(center,Mul(normal,Dot(Sub(center,a),normal)));
   const auto closest=ClosestTriangle(onPlane,a,b,c);
   if(Length(Sub(onPlane,closest))>BallRadius*.7f)continue;
   best=t;hitPoint=closest;hitNormal=normal;hitSpeed=Length(delta)*60;
  }
  if(hitSpeed>0){pressurePoint=hitPoint;pressureNormal=hitNormal;pressing=true;cooldown=.18f;Impulse(hitPoint,Mul(delta,60),hitSpeed);}
  pressureBall=ball;
  if(pressing){const auto diff=Sub(ball,pressurePoint);const float depth=Dot(diff,pressureNormal);const auto tangent=Sub(diff,Mul(pressureNormal,depth));if(depth< -BallRadius-.04f||depth>MaxOffset+.3f||Length(tangent)>1.25f||Dot(delta,pressureNormal)<-.025f){pressing=false;cooldown=.3f;}}
  return hitSpeed;
 }
 void ApplyPressure(){
  if(!pressing)return;
  const float depth=Dot(Sub(pressureBall,pressurePoint),pressureNormal);
  const auto center=Sub(pressureBall,Mul(pressureNormal,depth));
  for(auto& n:nodes)if(!n.pinned){
   const auto diff=Sub(n.rest,center);const float plane=Dot(diff,pressureNormal);if(std::abs(plane)>.3f)continue;
   const float d=Length(Sub(diff,Mul(pressureNormal,plane))),weight=std::exp(-d*d/.38f);
   const float target=std::clamp(depth+BallRadius,0.f,MaxOffset)*weight,current=Dot(n.offset,pressureNormal);
   if(target>current){n.offset=Add(n.offset,Mul(pressureNormal,(target-current)*.45f));const float normalSpeed=Dot(n.velocity,pressureNormal);n.velocity=Sub(n.velocity,Mul(pressureNormal,normalSpeed*.85f));const float length=Length(n.offset);if(length>MaxOffset)n.offset=Mul(n.offset,MaxOffset/length);}
  }
 }
 void Tick(){
  // Fixed 120Hz spring integration, two substeps per native 60Hz ball sample.
  for(unsigned step=0;step<2;step++){
   for(unsigned i=0;i<nodes.size();i++){auto& n=nodes[i];if(n.pinned)continue;Vec average{};for(auto j:n.adjacent)average=Add(average,nodes[j].offset);if(!n.adjacent.empty())average=Mul(average,1.f/n.adjacent.size());forces[i]=Add(Mul(Sub(average,n.offset),90.f),Add(Mul(n.offset,-24.f),Mul(n.velocity,-6.f)));}
   for(unsigned i=0;i<nodes.size();i++){auto& n=nodes[i];if(n.pinned){n.offset={};n.velocity={};continue;}n.velocity=Add(n.velocity,Mul(forces[i],1.f/120));n.offset=Add(n.offset,Mul(n.velocity,1.f/120));const float length=Length(n.offset);if(!std::isfinite(length)){n.offset={};n.velocity={};}else if(length>MaxOffset){n.offset=Mul(n.offset,MaxOffset/length);n.velocity=Mul(n.velocity,.5f);}}
   // Project after integration. Computing spring forces before an artificial
   // position jump and then adding the old impulse double-injected energy.
   ApplyPressure();
  }
 }
};
}
