#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "native_player_preview.h"
#include "editor_preview_scope.h"
#include "player_celebration_clips.h"
#include "player_editor_runtime.h"
#include "preview_reload.h"
#include "editor_motion_recipe.h"
#include "setpiece_gameplay.h"
#include <windows.h>
#include <array>
#include <bit>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <sstream>
#include <vector>

extern "C" void pes6_editor_frame(uint8_t*);

namespace pes6::editor::preview {
namespace {
struct Preview {
  struct ExternalClip {uint32_t motion=0,raw=0,source_id=0;};
  uint16_t player=0;
  uint32_t actor=0,next_actor=0,controller=0,scratch=0,ui_root=0;
  bool ready=false,drawn=false,motions=false,metadata=false,dirty=false;
  unsigned phase=0,selected_motion=224,render_wait=0;
  bool motion_dirty=false,rebuild=false,pose_dirty=true;
  float animation_time=0,yaw=0;
  int framing=0;
  bool visible=true;
  bool playing=false,phase_end=false,mirror_kick=false;
  float phase_marker=0;
  MotionRecipe recipe{};
  unsigned recipe_index=0;
  bool recipe_done=false,recipe_return_pending=false,recipe_return_blend=false,external_bank=false;
  std::vector<ExternalClip> external_clips;
  std::string active_animation;
  unsigned pending_bank=0,pending_file=0,pending_base=0,pending_action=0,pending_initial=0;
  unsigned follow_action=0,follow_clip=0,tail_clip=0;
  float follow_start=0,tail_start=0;
  ULONGLONG last=0,changed=0,rotate_tick=0;
  std::array<uint8_t,48> appearance{};
  float actor_height=1.80f,next_actor_height=1.80f;
} state;
struct WarmCache {bool motions=false,metadata=false,kit_pending=false;int team=-1;uint32_t variant=0;unsigned kit_flags=0;ULONGLONG kit_queued_at=0;};
WarmCache warm;
int kit_team=-1;
unsigned kit_variant=0;
bool retiring=false;
uint16_t queued_player=0;
void Drain(PPCContext&,uint8_t*);
constexpr uint32_t model_global=0x83139610u,camera_global=0x83139614u;
// Derived directly from the native lis/addi instructions, including signed
// low halves. Do not add the low half to the next 64 KiB page.
constexpr uint32_t ui_queues_global=0x82d90000u-21324u;
constexpr uint32_t render_list_header=0x83150000u-19616u+12u;
static_assert(ui_queues_global==0x82d8acb4u&&render_list_header==0x8314b36cu);
bool Valid(uint8_t* base,uint32_t at,size_t length){
  if(at<0x10000||uint64_t(at)+length>=0xe0000000ull)return false;
  const uint8_t* p=base+at;
  struct Region{const uint8_t* start=nullptr,*end=nullptr;};
  thread_local std::array<Region,4> cache{};
  thread_local unsigned next_slot=0;
  for(const auto& r:cache){if(r.start&&p>=r.start&&p+length<=r.end)return true;}
  while(length){MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const auto* rs=static_cast<const uint8_t*>(info.BaseAddress);const auto* re=rs+info.RegionSize;
    cache[next_slot]={rs,re};next_slot=(next_slot+1)&3u;
    const size_t n=std::min(length,size_t(re-p));if(!n)return false;p+=n;length-=n;}return true;
}
uint32_t Call(PPCContext& ctx,uint8_t* base,PPCFunc* fn,uint32_t a=0,uint32_t b=0,uint32_t c=0,uint32_t d=0){
  PPCContext call=ctx;call.r3.u64=a;call.r4.u64=b;call.r5.u64=c;call.r6.u64=d;fn(call,base);return call.r3.u32;
}
uint32_t Alloc(PPCContext& ctx,uint8_t* base,unsigned size){auto p=Call(ctx,base,sub_826D61B0,size);if(p)std::memset(base+p,0,size);return p;}
void F(uint8_t* base,uint32_t at,float value){REX_STORE_U32(at,std::bit_cast<uint32_t>(value));}
void Log(const char* event){
  char buffer[160];snprintf(buffer,sizeof(buffer),"PES6 editor preview: %s (player %u, phase %u)\n",event,state.player,state.phase);OutputDebugStringA(buffer);
  static const std::filesystem::path log_path=[](){
    wchar_t file[MAX_PATH]{};GetModuleFileNameW(nullptr,file,MAX_PATH);
    auto root=std::filesystem::path(file).parent_path();
    for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
    return root/"data/editor/preview-3d.log";
  }();
  std::ofstream out(log_path,std::ios::app);if(out)out<<buffer;
}
uint32_t Appearance(uint8_t* base){const auto db=REX_LOAD_U32(0x83139440u);const auto at=db+152u*state.player+108;return state.player&&Valid(base,at,48)?at:0;}
// The actor's render group is registered into its private native list from
// construction to destruction. The world renderer never sees a half-built pose.
struct PrivateScene {
  uint8_t* base; std::array<uint8_t,12> saved{};
  explicit PrivateScene(uint8_t* memory):base(memory){
    std::memcpy(saved.data(),base+render_list_header,12);
    std::memcpy(base+render_list_header,base+state.scratch+16,12);
  }
  ~PrivateScene(){
    std::memcpy(base+state.scratch+16,base+render_list_header,12);
    std::memcpy(base+render_list_header,saved.data(),12);
  }
};
void PrepareMotions(PPCContext& ctx,uint8_t* base){
  if(!REX_LOAD_U32(0x83140000u-26612u)){warm.motions=false;warm.metadata=false;}
  else if(!REX_LOAD_U32(0x83140000u-26480u))warm.metadata=false;
  if(!warm.motions){if(!Call(ctx,base,sub_82271748))return;warm.motions=true;Log("motion cache allocated");}
  if(!warm.metadata){Call(ctx,base,sub_82271870);warm.metadata=true;Log("motion prefetch queued");}
}
void DisposeActorHandle(PPCContext& ctx,uint8_t* base,uint32_t& target){
  if(!target)return;
  // Release loaded renderables/skeleton buffers before the native destructor
  // unlinks the actor and render group from their global circular lists.
  Call(ctx,base,sub_822F2178,target);
  const auto skeleton=REX_LOAD_U32(target+68);
  if(skeleton&&Valid(base,skeleton,4)){
    const auto vt=REX_LOAD_U32(skeleton);
    if(Valid(base,vt,20))if(auto* fn=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(vt+16)))Call(ctx,base,fn,skeleton,1);
    REX_STORE_U32(target+68,0);
  }
  for(unsigned offset:{60u,64u}){const auto p=REX_LOAD_U32(target+offset);if(p)Call(ctx,base,sub_828115E8,p);REX_STORE_U32(target+offset,0);}
  {PrivateScene scene(base);Call(ctx,base,sub_822F2010,target,1);}
  target=0;
}
void DisposeActor(PPCContext& ctx,uint8_t* base){
  state.ready=false;state.drawn=false;state.render_wait=0;
  if(state.controller){Call(ctx,base,sub_8213E550,state.controller);Call(ctx,base,sub_8280E7B8,state.controller);state.controller=0;}
  DisposeActorHandle(ctx,base,state.actor);
}
bool ManagersReady(uint8_t* base){
  const auto manager=REX_LOAD_U32(model_global),camera=REX_LOAD_U32(camera_global);
  if(!Valid(base,manager,200)||!Valid(base,camera,1024))return false;
  if(REX_LOAD_U32(manager+112)!=2)return false;
  // State 2 means the native common resources have finished loading.
  // Proportion tables and common mesh resources are published asynchronously.
  return Valid(base,REX_LOAD_U32(manager+168),24)&&Valid(base,REX_LOAD_U32(manager+120),16);
}
void PrepareTeam(PPCContext& ctx,uint8_t* base){
  if(!ManagersReady(base))return;
  const unsigned team=kit_team>=0?unsigned(kit_team):REX_LOAD_U32(0x831398A0u)&0xffff;
  const unsigned flags=kit_team>=0&&kit_variant>=2&&kit_variant<4?0x3Fu:0u;
  if(team>=256||(warm.team==int(team)&&warm.kit_flags==flags))return;
  if(!warm.variant)warm.variant=Alloc(ctx,base,8);
  if(!warm.variant)return;
  // Same prefetch/release pairing as the game's native team loader82231660.
  if(warm.team>=0)for(unsigned keeper:{0u,1u})Call(ctx,base,sub_822F3A78,keeper,unsigned(warm.team),warm.variant);
  base[warm.variant]=uint8_t(flags);
  for(unsigned keeper:{0u,1u})Call(ctx,base,sub_822F3720,keeper,team,warm.variant);
  warm.team=int(team);warm.kit_flags=flags;warm.kit_queued_at=GetTickCount64();warm.kit_pending=true;Log("team kit prefetch queued");
}
void StartActor(PPCContext& ctx,uint8_t* base,uint32_t appearance,bool staging=false){
  Pes6EditorPreviewScope preview_scope;
  if(staging)DisposeActorHandle(ctx,base,state.next_actor);
  auto actor=Alloc(ctx,base,1424);if(!actor)return;
  {PrivateScene scene(base);Call(ctx,base,sub_822F1AD0,actor);}
  const auto kit=REX_LOAD_U32(actor+1348);
  if(!Valid(base,kit,144)){DisposeActorHandle(ctx,base,actor);return;}
  if(staging)state.next_actor=actor;else state.actor=actor;
  const unsigned team=kit_team>=0?unsigned(kit_team):REX_LOAD_U32(0x831398A0u)&0xffff;
  // Variant byte and empty external-kit name are owned until actor disposal.
  const auto db=REX_LOAD_U32(0x83139440u),record=db+152u*state.player;
  const bool auto_role=kit_team<0||kit_variant>=4;
  const unsigned role=auto_role?((base[record+73]&15u)==0?0u:1u):(kit_variant&1u);
  base[state.scratch]=(!auto_role&&kit_variant>=2)?uint8_t(role?0x07u:0x38u):0;
  Call(ctx,base,sub_822FB9E8,kit,role,team,state.scratch);
  REX_STORE_U32(actor+1344,role);
  // Native kit-name generator consumes a C string, not an external-kit path.
  REX_STORE_U32(actor+1352,record+50);
  unsigned number=0;
  for(unsigned slot=0;slot<32;slot++)if(Call(ctx,base,sub_821784C8,db,team,slot)==state.player){number=Call(ctx,base,sub_82178470,db,team,slot);break;}
  REX_STORE_U16(actor+48,base[kit]);base[actor+1375]=0;
  Call(ctx,base,sub_822F3218,actor,appearance);
  base[actor+1357]=uint8_t(number);
  const float h=pes6::editor::ReadBodyField(base+appearance,0)*.01f;
  if(staging)state.next_actor_height=h>0.f?h:1.80f;
  else{state.actor_height=h>0.f?h:1.80f;std::memcpy(state.appearance.data(),base+appearance,48);}
  Call(ctx,base,sub_822F2060,actor);REX_STORE_U32(actor+12,1);
  // The editor owns culling: the actor must not use the menu's 2D camera.
  base[actor+7]=1;
  const auto group=REX_LOAD_U32(actor+44);if(Valid(base,group,192))base[group+66]=0;
  if(!staging){state.phase=2;Log("native actor loading");}
}
void Motion(PPCContext& ctx,uint8_t* base,unsigned id,float start=0){
  if(!state.controller)return;
  state.pose_dirty=true;
  PPCContext call=ctx;call.r3.u64=state.controller;call.r4.u64=id;
  // PES6's set-piece caller 8248D390 / 8249E020 uses mode0. The later
  // engine's mode2 is not interchangeable with this sampler's cached-pose mode.
  call.r5.u64=0;call.r6.u64=state.playing&&id!=224&&state.mirror_kick?1:0;call.r7.u64=0;call.r8.u64=0;
  // r6 is the engine's mirrored-track flag, not a render-space reflection.
  // It propagates through 82146858 -> clip flags bit1 -> 82145748 bit0x20.
  const bool setPiece=state.active_animation=="free_kick"||state.active_animation=="penalty";
  call.f1.f64=start;call.f2.f64=state.playing&&id!=224&&setPiece?state.recipe.phases[state.recipe_index].blend:1.0/60.0;sub_8213F4D8(call,base);
  state.animation_time=start;state.phase_end=false;
}
std::filesystem::path RecipeRoot(){
  wchar_t file[MAX_PATH]{};GetModuleFileNameW(nullptr,file,MAX_PATH);
  auto root=std::filesystem::path(file).parent_path();
  for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root;
}
bool ExternalRecipe(PPCContext& ctx,uint8_t* base,const std::string& kind,unsigned style){
  const auto file=RecipeRoot()/"data/editor/motion-recipes"/(kind+"-"+std::to_string(style)+".p6motion");
  std::ifstream in(file,std::ios::binary|std::ios::ate);if(!in||in.tellg()<0||in.tellg()>1024*1024)return false;
  std::vector<uint8_t> bytes(size_t(in.tellg()),uint8_t{});in.seekg(0);if(!in.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(bytes.size())))return false;
  MotionRecipe recipe;if(!ReadMotionRecipe(bytes,recipe))return false;
  const auto manager=Call(ctx,base,sub_82149DE8);if(!Valid(base,manager,16))return false;
  const auto table=REX_LOAD_U32(manager+4);if(!Valid(base,table,12))return false;
  const auto entries=REX_LOAD_U32(table+4),end=REX_LOAD_U32(table+8);
  if(end<entries||end-entries<1733*4||!Valid(base,entries+1730*4,12))return false;
  if(!state.external_bank){if(!Call(ctx,base,sub_82149F98,manager,4))return false;state.external_bank=true;}
  std::array<uint32_t,3> instances{};
  for(unsigned i=0;i<recipe.count;i++){
    const auto& p=recipe.phases[i];
    const auto cached=std::find_if(state.external_clips.begin(),state.external_clips.end(),[&](const auto& c){return c.source_id==p.source_id&&c.motion&&REX_LOAD_U32(c.motion+4);});
    if(cached!=state.external_clips.end()){instances[i]=cached->motion;continue;}
    const auto raw=Alloc(ctx,base,p.bytes),motion=Alloc(ctx,base,12);
    state.external_clips.push_back({motion,raw,p.source_id});if(!raw||!motion)return false;
    std::memcpy(base+raw,bytes.data()+p.offset,p.bytes);REX_STORE_U32(motion,0x820aa24cu);
    if(Call(ctx,base,sub_8214A548,motion,raw)!=0||!REX_LOAD_U32(motion+4))return false;
    instances[i]=motion;
  }
  // Publish only after every phase has been validated and initialized. Buffers
  // remain owned until the preview controller (and its blend) is destroyed.
  for(unsigned i=0;i<3;i++)REX_STORE_U32(entries+(1730+i)*4,instances[i]);
  state.recipe=recipe;state.recipe_index=0;state.recipe_done=false;state.recipe_return_pending=false;state.recipe_return_blend=false;
  state.phase_marker=recipe.phases[0].marker;state.selected_motion=1730;state.pending_file=0;state.motion_dirty=true;
  std::ostringstream message;message<<kind<<" recipe "<<style<<" phases="<<recipe.count;for(unsigned i=0;i<recipe.count;i++)message<<" "<<recipe.phases[i].source_id;Log(message.str().c_str());return true;
}
void ReleaseExternalRecipes(PPCContext& ctx,uint8_t* base){
  if(state.external_clips.empty())return;
  const auto manager=Call(ctx,base,sub_82149DE8);
  if(Valid(base,manager,8)){const auto table=REX_LOAD_U32(manager+4);if(Valid(base,table,12)){
    const auto entries=REX_LOAD_U32(table+4),end=REX_LOAD_U32(table+8);
    if(end>=entries&&end-entries>=1733*4&&Valid(base,entries+1730*4,12))for(unsigned i=0;i<3;i++){
      const auto value=REX_LOAD_U32(entries+(1730+i)*4);for(const auto& c:state.external_clips)if(value==c.motion)REX_STORE_U32(entries+(1730+i)*4,0);
    }
  }}
  for(const auto& c:state.external_clips){if(c.motion){const auto curve=REX_LOAD_U32(c.motion+4);if(curve)Call(ctx,base,sub_82100000,curve);Call(ctx,base,sub_8280E7B8,c.motion);}if(c.raw)Call(ctx,base,sub_8280E7B8,c.raw);}
  state.external_clips.clear();state.external_bank=false;
}
// Procedural editor pose, independent of football motion resources.
struct V3{float x,y,z;V3 operator+(V3 b)const{return{x+b.x,y+b.y,z+b.z};}V3 operator-(V3 b)const{return{x-b.x,y-b.y,z-b.z};}V3 operator*(float f)const{return{x*f,y*f,z*f};}};
float Dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 Cross(V3 a,V3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
V3 Unit(V3 a){const float n=std::sqrt(Dot(a,a));return n>1e-6f?a*(1/n):V3{1,0,0};}
struct Q{float x,y,z,w;};
Q Mul(Q a,Q b){return{a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w,a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z};}
Q Inverse(Q q){const float d=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;return d>1e-6f?Q{-q.x/d,-q.y/d,-q.z/d,q.w/d}:Q{0,0,0,1};}
void EditorPose(PPCContext& ctx,uint8_t* base,uint32_t skeleton){
  const auto nodes=REX_LOAD_U32(skeleton+196);if(!Valid(base,nodes,50*4))return;
  const auto read=[&](uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));};
  const auto xyz=[&](uint32_t node){const auto at=Call(ctx,base,sub_821ED970,node);return V3{read(at),read(at+4),read(at+8)};};
  const auto quat=[&](uint32_t node){const auto at=Call(ctx,base,sub_821EDAE8,node);return Q{read(at),read(at+4),read(at+8),read(at+12)};};
  std::vector<uint32_t> bones;
  const auto update=[&](){for(auto node:bones)REX_STORE_U32(node+40,REX_LOAD_U32(node+40)|3);const auto vt=REX_LOAD_U32(skeleton);if(Valid(base,vt,16))if(auto* fn=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(vt+12)))Call(ctx,base,fn,skeleton);};
  for(unsigned i=0;i<50;i++){const auto node=REX_LOAD_U32(nodes+4*i);if(!Valid(base,node,168))continue;
    bones.push_back(node);std::memcpy(base+node+80,base+node+48,16);std::memcpy(base+node+112,base+node+64,16);
    REX_STORE_U32(node+40,REX_LOAD_U32(node+40)|3);
  }
  update();if(bones.empty())return;
  float floor=INFINITY,top=-INFINITY;for(auto node:bones){const auto p=xyz(node);if(std::isfinite(p.y)){floor=std::min(floor,p.y);top=std::max(top,p.y);}}
  const float height=top-floor;if(!std::isfinite(height)||height<.0001f||height>10000.f)return;
  const auto rotate=[&](uint32_t node,V3 from,V3 to){
    from=Unit(from);to=Unit(to);const auto axis=Cross(from,to);Q delta{axis.x,axis.y,axis.z,1+Dot(from,to)};
    float norm=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z+delta.w*delta.w);if(norm<1e-5f)return;
    delta={delta.x/norm,delta.y/norm,delta.z/norm,delta.w/norm};
    const auto parent=REX_LOAD_U32(node+20);Q parentQ{0,0,0,1};if(parent>=16&&Valid(base,parent-16,144))parentQ=quat(parent-16);
    const Q local=Mul(Inverse(parentQ),Mul(delta,quat(node)));
    F(base,node+80,local.x);F(base,node+84,local.y);F(base,node+88,local.z);F(base,node+92,local.w);
    REX_STORE_U32(node+40,REX_LOAD_U32(node+40)|1);update();
  };
  // A relaxed wide stance, keeping the sole baseline fixed. Original body
  // hierarchy: hips1/2, knees5/7, ankles9/10, toes12/13.
  if(bones.size()==50){
    const auto bone=[&](unsigned i){return REX_LOAD_U32(nodes+4*i);};
    const float pelvisX=xyz(bone(0)).x;
    for(unsigned side=0;side<2;side++){
      const auto hip=bone(side?2:1),ankle=bone(side?10:9),toe=bone(side?13:12);
      const auto hp=xyz(hip),ap=xyz(ankle);const Q footOrientation=quat(ankle);
      const float sign=side?-1.f:1.f;
      rotate(hip,ap-hp,V3{pelvisX+sign*height*.15f,ap.y,ap.z}-hp);
      // Counter-rotate the ankle so shoes remain flat instead of rolling out.
      const auto pp=REX_LOAD_U32(ankle+20);const Q pq=pp>=16?quat(pp-16):Q{0,0,0,1};
      const float yaw=sign*.30f;
      const Q outward{0,std::sin(yaw*.5f),0,std::cos(yaw*.5f)};
      const Q local=Mul(Inverse(pq),Mul(outward,footOrientation));
      F(base,ankle+80,local.x);F(base,ankle+84,local.y);F(base,ankle+88,local.z);F(base,ankle+92,local.w);update();
    }
    float sole=INFINITY;for(unsigned i:{12u,13u})sole=std::min(sole,xyz(bone(i)).y);
    const auto root=bone(0);F(base,root+116,read(root+116)+floor-sole);update();
  }
  // Twist/helper transforms can lie between the anatomical joints. Ignore
  // zero-length links instead of requiring three consecutive array entries.
  const auto parentJoint=[&](uint32_t node){
    const auto origin=xyz(node);
    for(unsigned depth=0;depth<50;depth++){
      const auto parent=REX_LOAD_U32(node+20);if(parent<16)return uint32_t(0);
      node=parent-16;if(std::find(bones.begin(),bones.end(),node)==bones.end())return uint32_t(0);
      const auto d=xyz(node)-origin;
      if(Dot(d,d)>height*height*.0001f)return node;
    }return uint32_t(0);
  };
  const float midX=xyz(bones.front()).x;
  for(float side:{-1.f,1.f}){
    uint32_t wrist=0,elbow=0,shoulder=0;float score=0;
    for(auto node:bones){const auto e=parentJoint(node);if(!e)continue;
      const auto sh=parentJoint(e);if(!sh)continue;
      const auto w=xyz(node),ep=xyz(e),sp=xyz(sh);
      const float l1=std::sqrt(Dot(ep-sp,ep-sp)),l2=std::sqrt(Dot(w-ep,w-ep));
      if(side*(w.x-midX)<height*.20f||w.y<floor+height*.45f||w.y>floor+height*.95f||l1<height*.06f||l2<height*.06f||l1>height*.30f||l2>height*.30f)continue;
      if(side*(w.x-midX)>score){score=side*(w.x-midX);wrist=node;elbow=e;shoulder=sh;}
    }
    // Canonical 50-bone body, checked against cv2753's original hierarchy:
    // shoulders18/19, elbows21/23, wrists24/25 (20/22 are twist helpers).
    if(bones.size()==50){
      const unsigned sh=side>0?18:19,el=side>0?21:23,wr=side>0?24:25;
      const auto s0=REX_LOAD_U32(nodes+sh*4),e0=REX_LOAD_U32(nodes+el*4),w0=REX_LOAD_U32(nodes+wr*4);
      if(Valid(base,s0,168)&&Valid(base,e0,168)&&Valid(base,w0,168)&&
         REX_LOAD_U32(e0+20)==s0+16&&REX_LOAD_U32(w0+20)==e0+16){shoulder=s0;elbow=e0;wrist=w0;}
    }
    if(!wrist)continue;
    const auto sp=xyz(shoulder),ep=xyz(elbow),wp=xyz(wrist);
    const float l1=std::sqrt(Dot(ep-sp,ep-sp)),l2=std::sqrt(Dot(wp-ep,wp-ep));
    const V3 target{midX+side*height*.14f,xyz(bones.front()).y+height*.085f,sp.z+height*.035f};
    const auto direction=Unit(target-sp);const float d=std::clamp(std::sqrt(Dot(target-sp,target-sp)),std::abs(l1-l2)+.001f,l1+l2-.001f);
    const float along=(l1*l1-l2*l2+d*d)/(2*d),bend=std::sqrt(std::max(0.f,l1*l1-along*along));
    const V3 hint{midX+side*height*.27f,xyz(bones.front()).y+height*.18f,sp.z-height*.025f};
    const auto perpendicular=Unit((hint-sp)-direction*Dot(hint-sp,direction));
    const auto desiredElbow=sp+direction*along+perpendicular*bend;
    rotate(shoulder,ep-sp,desiredElbow-sp);
    rotate(elbow,xyz(wrist)-xyz(elbow),target-xyz(elbow));
  }
}
// Close only the two hand skeletons while posing; animation tracks remain
// untouched during playback (celebrations keep their original finger motion).
void ClosedHand(PPCContext& ctx,uint8_t* base,uint32_t skeleton,float side){
  if(!Valid(base,skeleton,200))return;const auto vt=REX_LOAD_U32(skeleton);
  if(!Valid(base,vt,16))return;auto* countFn=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(vt+8));
  auto* updateFn=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(vt+12));if(!countFn||!updateFn)return;
  const unsigned count=Call(ctx,base,countFn,skeleton)&0xffff;const auto array=REX_LOAD_U32(skeleton+196);
  if(count<4||count>64||!Valid(base,array,count*4))return;
  std::vector<uint32_t> bones;for(unsigned i=0;i<count;i++){const auto n=REX_LOAD_U32(array+i*4);if(!Valid(base,n,168))return;bones.push_back(n);}
  const auto read=[&](uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));};
  const auto xyz=[&](uint32_t n){const auto p=Call(ctx,base,sub_821ED970,n);return V3{read(p),read(p+4),read(p+8)};};
  const auto quat=[&](uint32_t n){const auto p=Call(ctx,base,sub_821EDAE8,n);return Q{read(p),read(p+4),read(p+8),read(p+12)};};
  const auto update=[&](){for(auto n:bones)REX_STORE_U32(n+40,REX_LOAD_U32(n+40)|3);Call(ctx,base,updateFn,skeleton);};
  const auto childrenOf=[&](uint32_t n){std::vector<uint32_t> out;for(auto ch:bones)if(REX_LOAD_U32(ch+20)==n+16)out.push_back(ch);return out;};
  std::vector<uint32_t> fingers;
  for(auto n:bones){
    if(childrenOf(n).size()!=1)continue;auto ancestor=n;
    for(unsigned depth=0;depth<count;depth++){
      const auto p=REX_LOAD_U32(ancestor+20);if(p<16)break;ancestor=p-16;
      if(std::find(bones.begin(),bones.end(),ancestor)==bones.end())break;
      if(childrenOf(ancestor).size()>=3){fingers.push_back(n);break;}
    }
  }
  // Keep the sampled wrist/palm attachment. Reset only finger joints to
  // avoid accumulating curls on cached idle samples or detaching the hands.
  for(auto n:fingers)std::memcpy(base+n+80,base+n+48,16);update();
  for(auto n:fingers){
    const auto parent=REX_LOAD_U32(n+20);const auto children=childrenOf(n);
    const auto d=xyz(children[0])-xyz(n);if(Dot(d,d)<1e-8f)continue;
    const auto direction=Unit(d);V3 inward{-side,0,0};
    const auto bend=Unit(inward-direction*Dot(inward,direction));const auto axis=Unit(Cross(direction,bend));
    const float angle=1.18f;const Q delta{axis.x*std::sin(angle*.5f),axis.y*std::sin(angle*.5f),axis.z*std::sin(angle*.5f),std::cos(angle*.5f)};
    const Q local=Mul(Inverse(quat(parent-16)),Mul(delta,quat(n)));
    F(base,n+80,local.x);F(base,n+84,local.y);F(base,n+88,local.z);F(base,n+92,local.w);update();
  }
}
// Evaluate only the clip and skeletal transforms. 821400E0 also invokes
// ground/feet solvers, secondary bodies and match-only attachments; it is not
// the editor-preview entry point and crashed on its first update in the log.
bool SamplePose(PPCContext& ctx,uint8_t* base,float seconds){
  Pes6EditorPreviewScope preview_scope;
  if(state.recipe_return_pending){
    state.recipe_return_pending=false;state.recipe_return_blend=true;state.recipe_index=0;
    state.selected_motion=224;state.phase_marker=0;Motion(ctx,base,224,0);
    Log("recipe returning to default editor rest pose224");
  }
  // Finish sampling the outgoing clip, including its last frame, before
  // changing resource/clock on the following update.
  if(state.phase_end){
    state.phase_end=false;seconds=0;
    state.phase_marker=0;
    if(state.follow_clip){const auto next=state.follow_clip;const float start=state.follow_start;state.follow_clip=state.tail_clip;state.follow_start=state.tail_start;state.tail_clip=0;state.tail_start=0;state.selected_motion=next;Motion(ctx,base,next,start);}
    else {Rest();Motion(ctx,base,state.selected_motion);state.motion_dirty=false;}
  }
  const auto controller=state.controller;
  if(!Valid(base,controller,160)||!Valid(base,state.actor,1424))return false;
  const auto skeleton=REX_LOAD_U32(controller+4),sampler=REX_LOAD_U32(controller+20);
  const auto clip=REX_LOAD_U32(controller+16),auxiliary=REX_LOAD_U32(controller+128);
  if(!Valid(base,skeleton,304)||!Valid(base,sampler,80)||!Valid(base,clip,28))return false;
  const bool first=state.phase<4;
  // Advance the native clip clock, not a separate preview timeout. Update
  // mode2 samples to the endpoint; the clip itself uses PES6 playback mode0.
  // 82270148 uses these same two calls, without the match physics evaluator.
  if(state.playing&&!state.pending_file&&!state.recipe_done){
    float remaining=std::max(0.f,seconds);
    for(unsigned step=0;step<4;step++){
    float boundary=0;
    if(state.recipe.count){
      const auto motion=REX_LOAD_U32(clip+4);PPCContext duration=ctx;duration.r3.u64=motion;sub_82149A88(duration,base);
      boundary=state.phase_marker>0?state.phase_marker:float(duration.f1.f64);
    }
    const float delta=state.recipe.count?PhaseDelta(remaining,state.animation_time,boundary):remaining;
    PPCContext advance=ctx;advance.r3.u64=clip;advance.r4.u64=0;advance.r5.u64=2;
    advance.f1.f64=delta;sub_82146588(advance,base);
    const auto mixer=REX_LOAD_U32(controller+72),tracks=REX_LOAD_U32(controller+28);
    if(Valid(base,mixer,752)&&Valid(base,tracks,84)){
      PPCContext secondaryClock=ctx;secondaryClock.r3.u64=mixer;
      secondaryClock.r4.u64=tracks;secondaryClock.r5.u64=clip;
      secondaryClock.f1.f64=delta;sub_82144DB8(secondaryClock,base);
    }
    state.animation_time=std::bit_cast<float>(REX_LOAD_U32(clip+12));
    if(!state.recipe.count)break;
    remaining=std::max(0.f,remaining-delta);
    if(!PhaseMarkerReached(state.animation_time,boundary))break;
    if(state.recipe_index+1==state.recipe.count){state.recipe_done=true;state.recipe_return_pending=true;Log("recipe complete; returning next sample");break;}
    ++state.recipe_index;const auto& next=state.recipe.phases[state.recipe_index];
    state.phase_marker=next.marker;state.selected_motion=1730+state.recipe_index;Motion(ctx,base,state.selected_motion,next.start);
    {std::ostringstream message;message<<"recipe phase "<<state.recipe_index+1<<" id="<<next.source_id<<" start="<<next.start<<" marker="<<next.marker;Log(message.str().c_str());}
    if(remaining<=0)break;
    }
  }
  if(state.recipe_done&&state.recipe_return_blend){
    PPCContext advance=ctx;advance.r3.u64=clip;advance.r4.u64=0;advance.r5.u64=2;advance.f1.f64=std::max(0.f,seconds);sub_82146588(advance,base);
    // Blend back to the ordinary editor resource224, then enable the same
    // procedural rest pose as Rest(). Never return to the set-piece stance.
    F(base,clip+12,0);state.animation_time=0;
    if(REX_LOAD_U16(clip+20)==0){state.recipe_return_blend=false;state.playing=false;state.mirror_kick=false;state.active_animation.clear();}
  }
  if(first)Log("preview clip clock ready");
  Call(ctx,base,sub_821EDCB0,skeleton+16);
  Call(ctx,base,sub_82145BA0,sampler,skeleton,clip,auxiliary);
  if(first)Log("preview clip sampled");
  const auto transforms=REX_LOAD_U32(sampler+20);
  if(!Valid(base,transforms,48))return false;
  // Exact pose mapping used by the native animation evaluator at8214031C.
  Call(ctx,base,sub_82145940,skeleton,transforms,0x8209c5e8u,auxiliary);
  const auto table=REX_LOAD_U32(skeleton);
  if(!Valid(base,table,16))return false;
  auto* update=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(table+12));
  if(!update)return false;Call(ctx,base,update,skeleton);
  if(first)Log("preview skeleton transformed");
  // Native secondary face/hand tracks: body sampling alone leaves fingers open.
  const auto secondarySkeletons=REX_LOAD_U32(controller+24),secondarySamplers=REX_LOAD_U32(controller+32);
  const auto secondaryClips=REX_LOAD_U32(controller+28),secondaryAux=REX_LOAD_U32(controller+132);
  const auto secondaryMaps=REX_LOAD_U32(controller+136),mixer=REX_LOAD_U32(controller+72);
  if(Valid(base,secondarySkeletons,12)&&Valid(base,secondarySamplers,240)&&Valid(base,secondaryClips,84)&&
     Valid(base,secondaryAux,12)&&Valid(base,secondaryMaps,12)&&Valid(base,mixer,752)){
    for(unsigned i=0;i<3;i++){
      const auto sk=REX_LOAD_U32(secondarySkeletons+i*4),aux=REX_LOAD_U32(secondaryAux+i*4);
      if(!Valid(base,sk,304))continue;
      PPCContext secondary=ctx;
      secondary.r3.u64=mixer+4+i*248;secondary.r4.u64=secondarySamplers+i*80;
      secondary.r5.u64=sk;secondary.r6.u64=secondaryClips+i*28;secondary.r7.u64=clip;
      secondary.r8.u64=REX_LOAD_U32(secondaryMaps+i*4);secondary.r9.u64=aux;
      sub_82144228(secondary,base);
    }
  }
  if(!state.playing||state.pending_file){
    EditorPose(ctx,base,skeleton);
    if(Valid(base,secondarySkeletons,12))for(unsigned i=1;i<3;i++)
      ClosedHand(ctx,base,REX_LOAD_U32(secondarySkeletons+i*4),i==1?-1.f:1.f);
  }
  // 821A6500 ends in 821AB238, which skips BOTH the world matrix and
  // renderable skin uploads when group+66 is zero. Hiding before this update
  // left the native group's allocated world matrix uninitialized.
  const auto group=REX_LOAD_U32(state.actor+44);
  if(!Valid(base,group,192))return false;
  base[group+66]=1;
  Call(ctx,base,sub_821A6500,state.actor,0);
  base[group+66]=0;
  if(first)Log("preview skin matrices ready");
  // Upload the marked final pose before changing phase. The setup phase has
  // the source recipe's explicit marker; execution uses native completion.
  if(state.playing&&!state.pending_file&&!state.recipe.count&&Call(ctx,base,sub_821469A8,clip))state.phase_end=true;
  state.pose_dirty=false;
  return true;
}

}
void Enter(PPCContext& ctx,uint8_t* base,uint16_t player,int team,unsigned variant){
  // The roster has already queued native resources asynchronously. Do not
  // release that warmup merely because there is no preview actor yet.
  if(state.player||state.scratch||state.actor||warm.motions||warm.team>=0)Leave(ctx,base);
  kit_team=team;kit_variant=variant;
  if(retiring){queued_player=player;return;}
  state.player=player;state.phase=1;state.scratch=Alloc(ctx,base,64);state.last=GetTickCount64();
  pes6_editor_preview_active.store(1,std::memory_order_release);
  if(state.scratch){const uint32_t sentinel=state.scratch+32;REX_STORE_U32(state.scratch+20,sentinel);REX_STORE_U32(sentinel,sentinel);REX_STORE_U32(sentinel+4,sentinel);}
  PrepareMotions(ctx,base);PrepareTeam(ctx,base);
  if(warm.kit_pending&&state.actor&&GetTickCount64()-warm.kit_queued_at>=250){
    warm.kit_pending=false;state.dirty=true;state.rebuild=true;state.changed=GetTickCount64();
  }Log("enter");
}
void SwitchPlayer(uint16_t player){
  if(!player||state.player==player)return;
  state.player=player;
  state.dirty=true;
  state.rebuild=true;
  state.pose_dirty=true;
  state.changed=0;
}
void Leave(PPCContext& ctx,uint8_t* base){
  queued_player=0;retiring=true;state.ready=false;state.drawn=false;state.playing=false;
  if(state.actor){const auto group=REX_LOAD_U32(state.actor+44);if(Valid(base,group,192))base[group+66]=0;}
  Drain(ctx,base);
}
namespace {
void Drain(PPCContext& ctx,uint8_t* base){
  if(!retiring)return;
  // Keep every buffer alive until asynchronous actor requests have settled.
  if(state.actor&&state.phase==2&&!Call(ctx,base,sub_822F3090,state.actor))return;
  if(state.next_actor&&!Call(ctx,base,sub_822F3090,state.next_actor))return;
  DisposeActorHandle(ctx,base,state.next_actor);
  DisposeActor(ctx,base);
  ReleaseExternalRecipes(ctx,base);
  if(warm.team>=0&&warm.variant){
    if(!ManagersReady(base))return;
    for(unsigned keeper:{0u,1u})Call(ctx,base,sub_822F3A78,keeper,unsigned(warm.team),warm.variant);
    warm.team=-1;
  }
  // Native release restores its own reference count and returns zero while
  // metadata is still in flight. Retry on later menu frames, never lose ownership.
  if(warm.motions&&!Call(ctx,base,sub_822717B0))return;
  const auto variant=warm.variant;warm=WarmCache{};warm.variant=variant;
  if(state.scratch)Call(ctx,base,sub_8280E7B8,state.scratch);
  state=Preview{};retiring=false;
  pes6_editor_preview_active.store(0,std::memory_order_release);
  const auto player=queued_player;queued_player=0;
  if(player){const auto team=kit_team;const auto variant=kit_variant;Enter(ctx,base,player);kit_team=team;kit_variant=variant;}
}
}
bool Ready(){return state.ready&&state.drawn;}
const char* Status(){return state.phase==0?"":state.phase==5?"Vista 3D no disponible":Ready()?"":"Cargando modelo 3D...";}
void Changed(PPCContext& ctx,uint8_t* base,bool rebuild){
  // Applying on this guest update point avoids waiting for a new match.
  pes6_editor_frame(base);
  state.dirty=true;state.pose_dirty=true;state.rebuild|=rebuild;state.changed=GetTickCount64();
}
void Container(uint32_t root){state.ui_root=root;}
void Visible(bool value){state.visible=value;}
void Frame(int area){state.framing=area;}
void Rotate(int direction){
  const auto now=GetTickCount64();
  const auto elapsed=state.rotate_tick&&now>=state.rotate_tick&&now-state.rotate_tick<=80?std::max<ULONGLONG>(1,now-state.rotate_tick):16;
  state.rotate_tick=now;
  state.yaw=std::remainder(state.yaw+direction*3.3f*(float(elapsed)/1000.f),6.283185307f);
}
void Rest(){state.recipe={};state.recipe_index=0;state.recipe_done=false;state.recipe_return_pending=false;state.recipe_return_blend=false;state.phase_end=false;state.phase_marker=0;state.mirror_kick=false;state.pending_initial=0;state.follow_action=state.follow_clip=state.tail_clip=0;state.follow_start=state.tail_start=0;state.active_animation.clear();state.pending_file=0;state.playing=false;state.selected_motion=224;state.motion_dirty=true;}
void TeamKit(PPCContext& ctx,uint8_t* base,unsigned team,unsigned variant){
  const bool changed=kit_team!=int(team)||kit_variant!=variant;
  kit_team=int(team);kit_variant=variant;
  if(changed&&state.actor)Changed(ctx,base,true);
}
void Animation(PPCContext& ctx,uint8_t* base,const std::string& field){
  if(state.active_animation==field){Rest();return;}
  const auto a=Appearance(base);if(!a)return;
  state.pending_initial=0;state.follow_action=state.follow_clip=state.tail_clip=0;state.follow_start=state.tail_start=0;state.active_animation=field;
  state.recipe={};state.recipe_index=0;state.recipe_done=false;state.recipe_return_pending=false;state.recipe_return_blend=false;
  const auto stats=a-36;
  state.phase_marker=0;
  state.mirror_kick=KickMirror(base[stats],field=="free_kick"||field=="penalty"||field=="drop_kick");
  state.playing=true;state.motion_dirty=true;
  constexpr unsigned run[]{484,1053,491,1054};
  if(field=="drop_kick"){
    const unsigned style=(base[stats+1]>>4)&3;
    state.selected_motion=style<2?1048:692;state.follow_clip=style<2?1049:691;return;
  }
  if(field=="dribble"){state.selected_motion=run[(base[stats+1]>>6)&3];return;}
  if(field=="free_kick"||field=="penalty"){
    const unsigned which=field=="free_kick"?0:1;
    const int native_style=which==0?int((base[stats]>>3)&15):int(base[stats]&7);
    const int style=PreviewAnimationStyle(state.player,which,native_style);
    // This replaces the old hand-built kick/approach/common238 chains in full.
    Motion(ctx,base,224);state.motion_dirty=false;
    if(style<0||style>(which==0?10:16)||!ExternalRecipe(ctx,base,field,unsigned(style+1))){Log("recipe unavailable or invalid");Rest();}
    return;
  }
  const auto manager=Call(ctx,base,sub_82149DE8);if(!manager)return;
  unsigned file=0,bank=4,first=1730,action=6;
  if(field=="celebration1"||field=="celebration2"){
    const unsigned choice=base[stats+(field=="celebration1"?31:32)]>>1;
    const auto& selected=celebration_clips[choice>=1&&choice<=79?choice-1:0];
    // Some celebrations use the permanently loaded common bank (no file).
    if(selected.clip<1634){state.selected_motion=selected.clip;return;}
    file=selected.file;bank=5;first=1634;action=0;state.pending_initial=selected.clip;

  }else {Rest();return;}
  if(!file){Rest();return;}
  // Detach the old clip before releasing its resource bank.
  Motion(ctx,base,224);state.selected_motion=224;state.motion_dirty=false;
  if(!Call(ctx,base,sub_82149F98,manager,bank)){Rest();return;}
  Call(ctx,base,sub_82149E78,manager,bank,file,first);
  state.pending_bank=bank;state.pending_file=file;state.pending_base=first;state.pending_action=action;
}
void Tick(PPCContext& ctx,uint8_t* base){
  Drain(ctx,base);if(retiring)return;
  if(!state.player||!state.scratch||state.phase==5||!ManagersReady(base))return;
  const auto appearance=Appearance(base);if(!appearance)return;
  PrepareMotions(ctx,base);PrepareTeam(ctx,base);
  if(warm.kit_pending&&state.actor&&GetTickCount64()-warm.kit_queued_at>=250){
    warm.kit_pending=false;state.dirty=true;state.rebuild=true;state.changed=GetTickCount64();
  }
  if(state.pending_file&&Call(ctx,base,sub_822719A8,state.pending_bank)){
    unsigned clip=state.pending_initial?state.pending_initial:state.pending_base;state.pending_initial=0;
    if(state.pending_action){const auto action=Call(ctx,base,sub_825F16A8,state.pending_action);if(action>=1730&&action<1755)clip=action;}
    if(state.follow_action){
      const auto next=Call(ctx,base,sub_825F16A8,state.follow_action==0x100?0:state.follow_action);
      if(next>=1730&&next<1755&&next!=clip)state.follow_clip=next;
      state.follow_action=0;
    }
    state.selected_motion=clip;state.motion_dirty=true;state.pending_file=0;
  }
  if(!state.actor){
    if(!warm.metadata||!Call(ctx,base,sub_8226FE18))return;
    StartActor(ctx,base,appearance);return;
  }
  if(state.dirty&&(!state.rebuild||Call(ctx,base,sub_8226FE18))&&CanReload(state.phase==2||state.next_actor!=0,
      (state.phase!=2||Call(ctx,base,sub_822F3090,state.actor)!=0)&&(!state.next_actor||Call(ctx,base,sub_822F3090,state.next_actor)!=0),
      GetTickCount64()-state.changed)){
    state.dirty=false;
    if(state.rebuild){
      state.rebuild=false;
      if(state.ready&&state.actor)StartActor(ctx,base,appearance,true);
      else{DisposeActor(ctx,base);StartActor(ctx,base,appearance);}
      return;
    }
    // Geometry/material changes are reconstructed atomically using the same
    // native constructors, so boots, sleeves and accessories cannot stay stale.
    if(std::memcmp(state.appearance.data(),base+appearance,48)!=0){
      // Boots select a different mesh/material resource. Body dimensions and
      // accessory groups already exist in this actor and update in place.
      const bool boots=state.appearance[47]!=base[appearance+47];
      const bool resources=std::memcmp(state.appearance.data()+4,base+appearance+4,14)!=0;
      if(boots||resources||!state.ready){
        if(state.ready&&state.actor)StartActor(ctx,base,appearance,true);
        else{DisposeActor(ctx,base);StartActor(ctx,base,appearance);}
        return;
      }
      state.pose_dirty=true;
      Pes6EditorPreviewScope preview_scope;
      const auto number=base[state.actor+1357];
      Call(ctx,base,sub_822F3218,state.actor,appearance);
      base[state.actor+1357]=number;
      Call(ctx,base,sub_822F34E8,state.actor);
      Call(ctx,base,sub_822F2C10,state.actor);
      std::memcpy(state.appearance.data(),base+appearance,48);
      const float h=pes6::editor::ReadBodyField(base+appearance,0)*.01f;
      if(h>0.f)state.actor_height=h;
    }
  }
  if(state.next_actor){
    Pes6EditorPreviewScope preview_scope;
    if(Call(ctx,base,sub_822F3090,state.next_actor)&&warm.metadata&&Call(ctx,base,sub_8226FE18)){
      const auto next_controller=Alloc(ctx,base,160);
      if(next_controller){
        Call(ctx,base,sub_8213E458,next_controller);
        Call(ctx,base,sub_822F2230,state.next_actor);
        REX_STORE_U32(state.next_actor+12,2);base[state.next_actor+4]=1;base[state.next_actor+5]=0;
        Call(ctx,base,sub_8213E770,next_controller,state.next_actor);
        const auto saved_actor=state.next_actor;state.next_actor=0;
        const bool was_drawn=state.drawn;
        DisposeActor(ctx,base);
        state.actor=saved_actor;state.controller=next_controller;
        state.actor_height=state.next_actor_height;
        if(const auto cur_app=Appearance(base))std::memcpy(state.appearance.data(),base+cur_app,48);
        Motion(ctx,base,state.selected_motion);state.motion_dirty=false;
        state.phase=3;
        if(!SamplePose(ctx,base,0)){state.phase=5;return;}
        const auto group=REX_LOAD_U32(state.actor+44);if(group)base[group+66]=0;
        state.ready=true;state.drawn=was_drawn;state.phase=4;state.last=GetTickCount64();
        return;
      }
    }
  }
  if(!state.ready){
    Pes6EditorPreviewScope preview_scope;
    if(!Call(ctx,base,sub_822F3090,state.actor))return;
    if(!warm.metadata||!Call(ctx,base,sub_8226FE18))return;
    // Match 8226FB58: resources ready -> finalize -> attach animation -> pose.
    // Never run 821A6500 on the uninitialized matrix array from the loader.
    Log("creating animation controller");
    state.controller=Alloc(ctx,base,160);if(!state.controller)return;
    Call(ctx,base,sub_8213E458,state.controller);Log("animation controller constructed");
    Call(ctx,base,sub_822F2230,state.actor);Log("actor resources finalized");
    REX_STORE_U32(state.actor+12,2);base[state.actor+4]=1;base[state.actor+5]=0;
    Call(ctx,base,sub_8213E770,state.controller,state.actor);Log("animation attached");
    Motion(ctx,base,state.selected_motion);state.motion_dirty=false;Log("idle clip selected");
    state.phase=3;
    if(!SamplePose(ctx,base,0)){state.phase=5;Log("preview pose data unavailable");return;}
    const auto group=REX_LOAD_U32(state.actor+44);if(group)base[group+66]=0;
    state.ready=true;state.phase=4;state.last=GetTickCount64();Log("first animated pose ready");return;
  }
  if(state.controller){
    if(state.motion_dirty){Motion(ctx,base,state.selected_motion);state.motion_dirty=false;}
    const auto now=GetTickCount64();const float delta=float(state.recipe.count?now-state.last:std::min<ULONGLONG>(now-state.last,50))/1000.f;state.last=now;
    // At rest neither body nor secondary clocks advance. Reuse their exact
    // final matrices until an edit/clip change invalidates them. Camera yaw and
    // framing are applied by Draw and do not require resampling every bone.
    if((state.pose_dirty||state.playing||state.recipe_return_blend)&&!SamplePose(ctx,base,delta)){
      state.ready=false;state.phase=5;Log("preview pose data unavailable");
    }
  }
  // Suppress the regular world pass; Draw enables this group only for the
  // editor's isolated render list after the native UI has finished drawing.
  const auto group=REX_LOAD_U32(state.actor+44);if(group)base[group+66]=0;
}
} // namespace pes6::editor::preview

extern "C" void pes6_native_preview_draw(PPCContext& ctx,uint8_t* base,uint32_t queue){
  using namespace pes6::editor::preview;
  const auto queues=REX_LOAD_U32(ui_queues_global);
  if(queue!=queues+64)return;
  if(retiring){Drain(ctx,base);return;}
  if(!state.visible||!state.ready||!state.actor||!ManagersReady(base))return;
  Pes6EditorPreviewScope preview_scope;
  // Called after the last (index 2) native UI queue, including empty queues.
  if(state.render_wait==0){state.render_wait=1;Log("native UI draw callback reached");}
  if(state.render_wait<2){state.render_wait=2;Log("final native UI queue reached");}
  const auto cameraOwner=REX_LOAD_U32(camera_global),camera=cameraOwner+320;
  const auto lighting=REX_LOAD_U32(0x831395b8u),gfx=REX_LOAD_U32(0x82ea4870u);
  const auto shader=REX_LOAD_U32(0x82ee5e34u);
  if(!Valid(base,lighting,464)||!Valid(base,gfx,12416)||!Valid(base,shader,16)){if(state.render_wait!=3){state.render_wait=3;Log("waiting for native render services");}return;}
  const auto material=REX_LOAD_U32(shader+12);
  if(!Valid(base,material,4)||!Valid(base,REX_LOAD_U32(material),24)){if(state.render_wait!=4){state.render_wait=4;Log("waiting for native material controller");}return;}
  const auto group=REX_LOAD_U32(state.actor+44);if(!Valid(base,group,192))return;
  std::array<uint8_t,704> savedCamera;std::memcpy(savedCamera.data(),base+camera,savedCamera.size());
  const auto constants=Call(ctx,base,sub_821FC940);if(!Valid(base,constants,336))return;
  std::array<uint8_t,64> savedView;std::memcpy(savedView.data(),base+constants+240,64);
  // The model renderer reads the native view/projection block, independently
  // of the camera object and the lighting constants. Preserve the UI matrices.
  constexpr uint32_t matrices=0x83150000u-21680;
  std::array<uint8_t,256> savedMatrices;std::memcpy(savedMatrices.data(),base+matrices,256);
  const int vx=int(REX_LOAD_U32(gfx+12376)),vy=int(REX_LOAD_U32(gfx+12380));
  const int vw=int(REX_LOAD_U32(gfx+12384)),vh=int(REX_LOAD_U32(gfx+12388));
  if(vw<=0||vh<=0)return;
  // Same affine transform as native UI vertices in 8273B918/8273BB98.
  // Panel coordinates are in the edit_basic frame, not percentages of a screenshot.
  const auto ui=REX_LOAD_U32(0x82d90000u-20956u);
  if(!Valid(base,ui,36)||!Valid(base,state.ui_root,80))return;
  const auto read=[&](uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));};
  const float sx=read(ui+28),sy=read(ui+32);
  const float ox=read(ui+20)+read(state.ui_root+16)*sx;
  const float oy=read(ui+24)+read(state.ui_root+20)*sy;
  const auto px=[&](float x){return (ox+x*sx-vx)/vw;};
  const auto py=[&](float y){return (oy+y*sy-vy)/vh;};
  const int framing=state.playing?(state.framing==6?6:0):state.framing;
  const bool big=framing>=6&&framing<=10;
  const float panelLeft=px(framing==5?112:big?50:177),panelRight=px(framing==5?420:big?390:335);
  const float panelTop=py(framing==5?-203:big?-203:-157),panelBottom=py(framing==5?232:big?205:155),footLine=py(framing==10?175:framing==5?230:big?200:150);
  if(!(panelRight>panelLeft&&footLine>panelTop))return;
  // 2.10 metres from the fixed foot baseline to the top, independent of height.
  // Boots use a closer 0.95 m framing with the green baseline at y=175.
  const float metres=framing==4?.43f:framing==9?.42f:framing==10?.95f:(framing==2||framing==8)?1.40f:framing==7?1.15f:(framing==1||framing==5)?1.05f:2.10f;
  const float halfHeight=metres/(2*(footLine-panelTop));
  const float panelCenter=(panelLeft+panelRight)*.5f;
  const auto appearance=Appearance(base);
  const float playerHeight=state.actor_height>0.f?state.actor_height:(appearance?pes6::editor::ReadBodyField(base+appearance,0)*.01f:1.80f);
  const float headCenter=playerHeight-.16f;
  const float topAnchor=playerHeight+.05f+2*halfHeight*(panelTop-.5f);
  const float centerY=framing==5?topAnchor:framing==7
      ?std::max(0.f,playerHeight-1.03f)+2*halfHeight*(footLine-.5f)
      :framing==9
      ?std::max(0.f,playerHeight-.36f)+2*halfHeight*(footLine-.5f)
      :(framing==1||framing==4)
      ?(framing==4?headCenter:1.48f)+2*halfHeight*((panelTop+panelBottom)*.5f-.5f)
      :2*halfHeight*(footLine-.5f);
  const float spanX=2*halfHeight*float(vw)/float(vh);
  // Root motion must not carry running/penalty previews out of their panel.
  // Follow only the horizontal root; keep the ground/feet anchor fixed in Y.
  float rootX=0,rootZ=0;
  const auto body=REX_LOAD_U32(state.actor+68);
  if(Valid(base,body,200)){const auto bones=REX_LOAD_U32(body+196);
    if(Valid(base,bones,4)){const auto node=REX_LOAD_U32(bones);
      if(Valid(base,node,168)){const auto position=Call(ctx,base,sub_821ED970,node);
        if(Valid(base,position,12)){rootX=read(position);rootZ=read(position+8);}
      }
    }
  }
  F(base,camera+624,rootX+4*std::sin(state.yaw));F(base,camera+628,centerY);F(base,camera+632,rootZ+4*std::cos(state.yaw));
  F(base,camera+636,rootX);F(base,camera+640,centerY);F(base,camera+644,rootZ);
  F(base,camera+648,0);F(base,camera+652,1);F(base,camera+656,0);
  F(base,camera+660,0);F(base,camera+664,-spanX*(1-panelCenter));F(base,camera+668,spanX*panelCenter);
  F(base,camera+672,-halfHeight);F(base,camera+676,halfHeight);
  F(base,camera+688,1.5f);F(base,camera+692,7.5f);base[camera+696]=1;base[camera+697]=1;
  Call(ctx,base,sub_8216C8D8,camera);
  std::array<uint8_t,16> savedScissor;std::memcpy(savedScissor.data(),base+gfx+12400,16);
  const uint32_t savedScissorEnable=REX_LOAD_U32(gfx+11584);
  // 82116540 ignores the requested rectangle unless this native enable is set.
  REX_STORE_U32(gfx+11584,1);
  const auto rect=state.scratch+48;
  REX_STORE_U32(rect,vx+int(vw*panelLeft));REX_STORE_U32(rect+4,vy+int(vh*panelTop));
  REX_STORE_U32(rect+8,vx+int(vw*panelRight));REX_STORE_U32(rect+12,vy+int(vh*(state.framing==3?py(64):panelBottom)));
  Call(ctx,base,sub_82116540,gfx,rect);
  // 821AB368 reads the active directional light at 0x831395B4 (via 821727D0),
  // where +12..+20 is the normalized vector pointing towards the light source.
  const auto active_light=REX_LOAD_U32(0x831395b4u);
  std::array<uint8_t,24> savedLight{};
  const bool has_light=Valid(base,active_light,24);
  if(has_light){
    std::memcpy(savedLight.data(),base+active_light,savedLight.size());
    const float sx=std::sin(state.yaw),sy=0.35f,sz=std::cos(state.yaw);
    const float len=std::sqrt(sx*sx+sy*sy+sz*sz);
    REX_STORE_U32(active_light,1);
    F(base,active_light+12,sx/len);
    F(base,active_light+16,sy/len);
    F(base,active_light+20,sz/len);
  }
  {PrivateScene scene(base);base[group+66]=1;
    Call(ctx,base,sub_821AB368,1,0);
    Call(ctx,base,sub_821AB368,1,1);
    base[group+66]=0;
  }
  if(has_light)std::memcpy(base+active_light,savedLight.data(),savedLight.size());
  REX_STORE_U32(gfx+11584,savedScissorEnable);
  std::memcpy(base+rect,savedScissor.data(),16);Call(ctx,base,sub_82116540,gfx,rect);
  if(!state.drawn){state.drawn=true;Log("preview render submitted");}
  std::memcpy(base+matrices,savedMatrices.data(),256);base[matrices+256]=1;
  std::memcpy(base+camera,savedCamera.data(),savedCamera.size());
  std::memcpy(base+constants+240,savedView.data(),64);REX_STORE_U32(constants+304,REX_LOAD_U32(constants+304)+1);
}

extern "C" void pes6_native_preview_warm(PPCContext& ctx,uint8_t* base){
  pes6::editor::preview::Drain(ctx,base);
}

extern "C" void pes6_native_preview_warm_team(PPCContext& ctx,uint8_t* base){
  pes6::editor::preview::Drain(ctx,base);
}
