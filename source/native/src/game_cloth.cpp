#include "../generated/default/pes6_pch.h"
#include "cloth_grid.h"
#include "cloth_hem_attachment.h"
#include "cloth_bind_reference.h"
#include "cloth_contact_reference.h"
#include "cloth_short_reference.h"
#include "cloth_control_bounds.h"
#include "cloth_profiles.h"


#include "cloth_native_lods.h"
#include "weather_memory.h"
#if __has_include(<rex/system/kernel_state.h>)
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#define PES6_CLOTH_GUEST_PAGE_TABLE 1
#endif
#include <windows.h>
#include <bit>
#include <cstdlib>
#include <mutex>
#include <filesystem>
#include <fstream>
extern "C" void sub_82A00AC8(PPCContext&,uint8_t*);
extern "C" bool pes6_cloth_enabled(){
 static const bool on=[](){const char* value=std::getenv("PES6_REALISTIC_KITS");return value&&value[0]=='1'&&!value[1];}();return on;
}
namespace {
using namespace pes6::cloth;
std::mutex mutex;
struct Track {
 uint32_t actor=0,skeleton=0,matrices=0,nodes=0;
 std::array<uint16_t,50> slots{};
 std::array<uint16_t,50> native_order{};
 std::array<uint32_t,50> position_addresses{};
 std::array<Frame,6> native_sample{};
 std::array<Vec,36> pending_contact{};
 std::array<uint8_t,50*64> pose_input{},pose_output{};double cached_time=-1;uint64_t cached_generation=0;unsigned cached_profile=0;bool pose_cached=false;
 Frame view_root{},view_inverse{},view_transform{};std::array<Frame,6> view_original_hem{};uint32_t view_bind=0;bool view_ready=false;

 Grid<3,12> shirt;std::array<Grid<2,8>,2> pants;std::array<FabricLag,2> leg_lag,arm_lag;std::array<Vec,2> sleeve_delta{};
 uint32_t mode_flags=0,mode_manager=0,mode_mesh=0,profile_model=0,profile_data=0;unsigned collision_profile=0;bool profile_known=false;unsigned mode_age=0;bool outside=false,mode_known=false;
 Vec soft_position{},soft_velocity{},soft_delta{};bool soft_ready=false,native_lod=false;
 float shirt_activity=0;bool shirt_activity_ready=false;
 Vec last_foot_separation{};std::array<Frame,6> idle_hem{};float leg_quiet=0,leg_motion=1;bool feet_ready=false,idle_hem_ready=false;
 std::array<float,2> hem_lift_filtered{};std::array<Vec,6> hem_motion{};bool hem_ready=false,hem_motion_ready=false;FabricLag torso_lag;
 uint64_t generation=0;double cloth_time=0;bool mapped=false,reported=false,node_fallback=false;ContactStats contact;ShortFitStats short_fit;unsigned rig_fallbacks=0;
 void Reset(){soft_ready=false;shirt_activity_ready=false;feet_ready=idle_hem_ready=false;leg_quiet=0;leg_motion=1;hem_ready=hem_motion_ready=false;torso_lag.ready=false;for(auto& lag:leg_lag)lag.ready=false;for(auto& lag:arm_lag)lag.ready=false;pose_cached=false;view_ready=false;shirt.ready=false;pending_contact={};for(auto& p:pants)p.ready=false;}
};
std::array<Track,64> tracks{};
// Called under mutex. Consecutive palette slots normally belong to the same
// actor, so avoid scanning all tracks again for each individual bone.
Track* PoseTrack(uint32_t matrices,uint32_t input,uint32_t instance){
 static thread_local Track* previous=nullptr;
 const auto matches=[&](const Track& t){return t.pose_cached&&!t.native_lod&&
   (t.matrices==matrices||(input&&t.matrices==input)||(instance&&t.mode_mesh==instance));};
 if(previous&&matches(*previous)&&(previous->matrices==matrices||(instance&&previous->mode_mesh==instance)))return previous;
 for(auto& t:tracks)if(matches(t)){previous=&t;return previous;}
 return nullptr;
}
double cloth_time=0;uint64_t generation=1;bool replay=false;float cursor=0;int replay_direction=0;
bool Valid(uint8_t* base,uint32_t at,size_t bytes,bool write=false){
 if(at<0x10000||uint64_t(at)+bytes>=0xe0000000ull)return false;
#ifdef PES6_CLOTH_GUEST_PAGE_TABLE
 if(!write){
  if(auto* kernel=rex::runtime::current_kernel_state()){
   if(auto* memory=kernel->memory()){
    rex::memory::HeapAllocationInfo info{};
    return pes6::weather::ReadableGuestRange(*memory,base,at,bytes,info);
   }
  }
 }
#endif
 auto* p=base+at;
 while(bytes){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(p,&m,sizeof(m))||m.State!=MEM_COMMIT||(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
  if(write&&!(m.Protect&(PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return false;
  const size_t n=std::min(bytes,size_t(static_cast<uint8_t*>(m.BaseAddress)+m.RegionSize-p));if(!n)return false;p+=n;bytes-=n;
 }return true;
}
float F(uint8_t* base,uint32_t a){return std::bit_cast<float>(REX_LOAD_U32(a));}
Vec P(uint8_t* base,uint32_t a){return{F(base,a),F(base,a+4),F(base,a+8)};}
void Store(uint8_t* base,uint32_t a,Vec p){REX_STORE_U32(a,std::bit_cast<uint32_t>(p.x));REX_STORE_U32(a+4,std::bit_cast<uint32_t>(p.y));REX_STORE_U32(a+8,std::bit_cast<uint32_t>(p.z));}
Frame Read(uint8_t* base,uint32_t at){return{P(base,at),P(base,at+16),P(base,at+32),P(base,at+48)};}
bool Good(Frame a){return Finite(a.p)&&Finite(a.x)&&Finite(a.y)&&Finite(a.z)&&std::abs(Dot(a.x,Cross(a.y,a.z)))>.01f;}
Frame Mix(Frame a,Frame b,float t){return{a.x*(1-t)+b.x*t,a.y*(1-t)+b.y*t,a.z*(1-t)+b.z*t,a.p*(1-t)+b.p*t};}
// A linear matrix blend contracts the garment when pelvis and torso turn.
// Blend the directions, then restore an orthogonal basis and physique scale.
Frame MixSkin(Frame a,Frame b,float t,Vec pivot){
 const auto mixed=Mix(a,b,t);
 const auto x=Unit(mixed.x,Unit(a.x,{1,0,0}));
 const auto y=Unit(mixed.y-x*Dot(mixed.y,x),Unit(a.y));
 const auto z=Unit(Cross(x,y),Unit(a.z,{0,0,1}));
 Frame out{x*(Length(a.x)*(1-t)+Length(b.x)*t),y*(Length(a.y)*(1-t)+Length(b.y)*t),z*(Length(a.z)*(1-t)+Length(b.z)*t),{}};
 out.p=mixed.Point(pivot)-out.Linear(pivot);return out;
}
void Write(uint8_t* base,uint32_t at,Frame a){Store(base,at,a.x);Store(base,at+16,a.y);Store(base,at+32,a.z);Store(base,at+48,a.p);}
Frame SavedClothFrame(const std::array<uint8_t,50*64>& bytes,unsigned b){
 const auto f=[&](unsigned k){const auto* p=bytes.data()+b*64+k;return std::bit_cast<float>((uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]);};const auto v=[&](unsigned k){return Vec{f(k),f(k+4),f(k+8)};};return {v(0),v(16),v(32),v(48)};
}
void ClothView(Track& t,uint8_t* base,uint32_t instance,uint32_t matrices){
 const auto object=REX_LOAD_U32(instance+248),data=REX_LOAD_U32(object+12),bind=data+REX_LOAD_U32(data+28);const auto inverse0=Read(base,bind+80),renderRoot=Read(base,matrices);
 if(!t.view_ready||t.view_bind!=bind||std::memcmp(&renderRoot,&t.view_root,sizeof(Frame))||std::memcmp(&inverse0,&t.view_inverse,sizeof(Frame))){const auto rootSkin=Combine(inverse0,SavedClothFrame(t.pose_output,0));t.view_transform=Combine(Inverse(rootSkin),renderRoot);t.view_root=renderRoot;t.view_inverse=inverse0;t.view_bind=bind;for(unsigned b=28;b<34;b++)t.view_original_hem[b-28]=Combine(Combine(Read(base,bind+b*176+80),SavedClothFrame(t.pose_input,b)),t.view_transform);t.view_ready=true;}
}
float SleeveBoneFactor(unsigned bone){return bone==24||bone==25?1.f:bone==21||bone==23?.85f:bone==20||bone==22?.35f:.20f;}
unsigned SleeveSide(unsigned bone){return bone==19||bone==22||bone==23||bone==25?1:0;}
template<size_t N>std::array<Vec,N> Transform(const std::array<Vec,N>& rest,Frame frame){std::array<Vec,N> out;for(unsigned i=0;i<N;i++)out[i]=frame.Point(rest[i]);return out;}
Frame Surface(const Grid<3,12>& grid,unsigned row,unsigned col,Frame chest,const std::array<Vec,36>* target=nullptr){
 const auto& points=target?*target:grid.position;
 const unsigned i=row*12+col,left=row*12+(col+11)%12,right=row*12+(col+1)%12,up=(row?row-1:row)*12+col,down=std::min(2u,row+1)*12+col;
 Frame out=chest;
 if(row){
  const auto rt=Unit(grid.rest[right]-grid.rest[left]),rv=Unit(grid.rest[down]-grid.rest[up]),rn=Unit(Cross(rt,rv));
  const auto t=Unit(points[right]-points[left],chest.Linear(rt));
  const auto raw=points[down]-points[up],v=Unit(raw-t*Dot(raw,t),chest.Linear(rv)),n=Unit(Cross(t,v),chest.Linear(rn));
  out.x=t*rt.x+v*rv.x+n*rn.x;out.y=t*rt.y+v*rv.y+n*rn.y;out.z=t*rt.z+v*rv.z+n*rn.z;
 }
 out.p=points[i]-out.Linear(grid.rest[i]);return out;
}
Frame TrouserFrame(const Grid<2,8>& grid,unsigned leg,Frame leg_frame){
 Vec centre{};for(unsigned c=0;c<8;c++)centre=centre+grid.position[8+c]*(1.f/8);
 const auto bounded=[](Vec v,Vec fallback){const float length=Length(v);return Unit(v,fallback)*std::clamp(length,.99f,1.26f);};
 Frame out;
 out.x=bounded((grid.position[8]-grid.position[12])*(1.f/(.210f*.88f)),leg_frame.x);
 out.z=bounded((grid.position[10]-grid.position[14])*(1.f/(.230f*.88f)),leg_frame.z);
 out.y=leg_frame.y;
 out.p=centre-out.Linear({leg?-.09047f:.09048f,.642f,.009f});return out;
}
Frame Anatomical(Vec right,Vec up,Vec position,Vec reference){
 Frame out;out.y=Unit(up);out.z=Unit(Cross(right,out.y),{0,0,1});out.x=Unit(Cross(out.y,out.z),{1,0,0});out.p=position-out.Linear(reference);return out;
}
Frame RelativeSurface(Frame surface,Frame reference,Vec pivot,Vec displacement){
 Frame relative=Combine(surface,Inverse(reference));
 const float cosine=std::clamp((relative.x.x+relative.y.y+relative.z.z-1)*.5f,-1.f,1.f),angle=std::acos(cosine);
 if(angle>.55f){const float t=.55f/angle;relative.x=Unit(Vec{1,0,0}*(1-t)+relative.x*t,{1,0,0});const auto y=Vec{0,1,0}*(1-t)+relative.y*t;relative.y=Unit(y-relative.x*Dot(y,relative.x));relative.z=Unit(Cross(relative.x,relative.y),{0,0,1});}
 relative.p=pivot+Limit(Inverse(reference).Linear(displacement),.075f)-relative.Linear(pivot);return relative;
}
void ModelClothProfile(Track& t,uint8_t* base,uint32_t mesh){
 if(!mesh)return;const auto model=REX_LOAD_U32(mesh+248);if(model!=t.profile_model){if(!Valid(base,model,16))return;t.profile_model=model;}
 const auto data=REX_LOAD_U32(model+12);if(data==t.profile_data)return;if(!Valid(base,data,148))return;
 t.native_lod=NativeClothLod(REX_LOAD_U32(data+144),REX_LOAD_U32(data+120),REX_LOAD_U32(data+40),REX_LOAD_U32(data+48));
 const int profile=FindClothProfile(REX_LOAD_U32(data+144),REX_LOAD_U32(data+120),REX_LOAD_U32(data+40),REX_LOAD_U32(data+48));t.profile_known=profile>=0;t.collision_profile=profile<0?0:unsigned(profile);t.profile_data=data;
}
void ShirtMode(Track& t,uint8_t* base){
 const uint32_t manager=REX_LOAD_U32(t.actor+44);
 if(manager!=t.mode_manager||++t.mode_age>=120){t.mode_flags=0;t.mode_manager=manager;t.mode_age=0;}
 if(t.mode_flags){
  if(base[t.mode_flags+34]||base[t.mode_flags+35]){t.mode_known=true;t.outside=base[t.mode_flags+35]!=0;ModelClothProfile(t,base,t.mode_mesh);return;}
  // A LOD swap can deactivate the old flag block before the manager changes.
  t.mode_flags=0;t.mode_mesh=0;
 }
 t.mode_known=false;t.outside=false;
 if(!Valid(base,manager,28))return;
 const auto actor_begin=REX_LOAD_U32(t.actor+32),actor_end=REX_LOAD_U32(t.actor+36);
 if(!actor_begin||actor_end<actor_begin||actor_end-actor_begin>256)return;
 const auto begin=REX_LOAD_U32(manager+20),end=begin+(actor_end-actor_begin);
 if(!begin||end<begin||end-begin>256||!Valid(base,begin,end-begin))return;
 for(uint32_t at=begin;at<end;at+=4){
  const auto handle=REX_LOAD_U32(at);if(!Valid(base,handle,12))continue;
  const auto mesh=REX_LOAD_U32(handle+8);if(!Valid(base,mesh,256))continue;
  const auto model=REX_LOAD_U32(mesh+248),flags=REX_LOAD_U32(mesh+252);if(!Valid(base,model,16))continue;
  const auto data=REX_LOAD_U32(model+12);if(!Valid(base,data,68)||REX_LOAD_U32(data+64)<=35||!Valid(base,flags,36))continue;
  if(!base[flags+34]&&!base[flags+35])continue;
  t.mode_flags=flags;t.mode_mesh=mesh;t.mode_known=true;t.outside=base[flags+35]!=0;ModelClothProfile(t,base,mesh);return;
 }
}
float VertexDelta(Frame a,Frame b,bool shorts,unsigned bone=50){
 float largest=0;
 const Vec low=shorts?Vec{-.28f,.58f,-.25f}:Vec{-.45f,.79f,-.35f},high=shorts?Vec{.28f,1.13f,.25f}:Vec{.45f,1.65f,.35f};
 for(unsigned corner=0;corner<8;corner++){
  const auto l=bone<50?control_bounds[bone].low:low,h=bone<50?control_bounds[bone].high:high;
  const Vec p{corner&1?h.x:l.x,corner&2?h.y:l.y,corner&4?h.z:l.z};largest=std::max(largest,Length(a.Point(p)-b.Point(p)));
 }return largest;
}
void Diagnostic(Track& track,uint8_t* base,Frame native_root,Vec pelvis,unsigned repairs,unsigned clamps,float peak){
#ifdef PES6_CLOTH_TEST
 return;
#else
 if(track.reported)return;track.reported=true;
 static unsigned remaining=32,unknown_remaining=4;if(!remaining||(!track.mode_known&&!unknown_remaining))return;
 if(!track.mode_known)--unknown_remaining;--remaining;
 static std::ofstream log=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::error_code error;std::filesystem::create_directories(root/"data/diagnostics",error);return std::ofstream(root/"data/diagnostics/cloth-v11-native.txt",std::ios::trunc);}();
 if(!log)return;
 log<<"post-native actor="<<std::hex<<track.actor<<std::dec<<" mode="<<(track.mode_known?(track.outside?"outside":"tucked"):"unknown-tethered")<<" collision-profile="<<track.collision_profile<<" profile-known="<<track.profile_known<<" rig-fallbacks="<<track.rig_fallbacks<<" short-scale="<<track.short_fit.scale[0]<<','<<track.short_fit.scale[1]<<" short-residual="<<track.short_fit.remaining<<" short-worst="<<track.short_fit.worst<<" contact-corrections="<<track.contact.corrected<<" contact-residual="<<track.contact.remaining<<" contact-worst="<<track.contact.worst<<" contact-max-offset="<<track.contact.maxCorrection<<" pelvis="<<pelvis.x<<','<<pelvis.y<<','<<pelvis.z<<" root-matrix="<<native_root.p.x<<','<<native_root.p.y<<','<<native_root.p.z<<" repairs="<<repairs<<" bounded="<<clamps<<" max-precontact-rendered-vertex-delta="<<peak<<'\n';
 log<<"mode-manager="<<std::hex<<track.mode_manager<<" mode-flags="<<track.mode_flags<<" actor-vector="<<REX_LOAD_U32(track.actor+32)<<','<<REX_LOAD_U32(track.actor+36)<<std::dec<<'\n';
 log<<"native-node-order=";for(auto slot:track.native_order)log<<slot<<',';log<<'\n';
 unsigned sample=0;for(unsigned bone:{0u,11u,26u,28u,36u,42u}){const auto f=Read(base,track.matrices+track.slots[bone]*64),before=track.native_sample[sample++];log<<"bone="<<bone<<" slot="<<track.slots[bone]<<" node=";const auto p=P(base,track.position_addresses[bone]);log<<p.x<<','<<p.y<<','<<p.z<<" native-before=";for(const auto v:{before.x,before.y,before.z,before.p})log<<v.x<<','<<v.y<<','<<v.z<<';';log<<" cloth-after=";for(const auto v:{f.x,f.y,f.z,f.p})log<<v.x<<','<<v.y<<','<<v.z<<';';log<<'\n';}
 const uint32_t reference=REX_LOAD_U32(track.actor+64);for(unsigned bone:{0u,11u,42u}){const auto at=reference+track.slots[bone]*64;if(Valid(base,at,64)){const auto f=Read(base,at);log<<"actor64-reference bone="<<bone<<' ';for(auto v:{f.x,f.y,f.z,f.p})log<<v.x<<','<<v.y<<','<<v.z<<';';log<<'\n';}}
 log.flush();
#endif
}
}
extern "C" void pes6_cloth_tick(uint8_t* base){if(!pes6_cloth_enabled())return;std::lock_guard lock(mutex);if(base[0x8313A93E])return;if(replay){generation++;replay=false;replay_direction=0;}cloth_time+=1./60;}
extern "C" void pes6_cloth_replay(uint8_t* base,uint32_t manager){
 if(!pes6_cloth_enabled()||!Valid(base,manager,40))return;const float next=F(base,manager+36);if(!std::isfinite(next))return;
 std::lock_guard lock(mutex);const float delta=next-cursor;
 const int direction=delta>0?1:delta<0?-1:0;
 // Rewinding is a sequence of animation samples, not forward physics time.
 // Discard the prior sample's inertia/contact feedback on each backward step.
 if(delta<0){generation++;replay=true;replay_direction=-1;}
 else if(!replay||delta>4||(direction&&replay_direction&&direction!=replay_direction)){generation++;replay=true;replay_direction=direction;}
 else{cloth_time+=delta/60.;if(direction)replay_direction=direction;}
 cursor=next;
}
extern "C" void pes6_cloth_forget(uint8_t*,uint32_t actor){if(!pes6_cloth_enabled())return;std::lock_guard lock(mutex);for(auto& t:tracks)if(t.actor==actor)t={};}
extern "C" void pes6_cloth_pose(uint8_t* base,uint32_t actor,uint32_t count){
 if(!pes6_cloth_enabled()||(count<26||count>50)||!actor)return;
 std::lock_guard lock(mutex);Track* t=nullptr;
 for(auto& slot:tracks)if(slot.actor==actor){t=&slot;break;}
 if(!t){if(!Valid(base,actor,72))return;for(auto& slot:tracks)if(!slot.actor||cloth_time-slot.cloth_time>2){t=&slot;*t={};t->actor=actor;break;}}
 if(!t||REX_LOAD_U32(actor)!=0x820C7A80u)return;
 const uint32_t matrices=REX_LOAD_U32(actor+60),skeleton=REX_LOAD_U32(actor+68);
 if((!t->mapped||t->matrices!=matrices||t->skeleton!=skeleton)&&(!Valid(base,skeleton,200)||!Valid(base,matrices,50*64,true)))return;
 if(!t->mapped||t->matrices!=matrices||t->skeleton!=skeleton||REX_LOAD_U32(skeleton+196)!=t->nodes){
  t->mapped=false;
  if(!Valid(base,skeleton,200)||!Valid(base,matrices,50*64,true))return;
  const uint32_t nodes=REX_LOAD_U32(skeleton+196);if(!Valid(base,nodes,50*4))return;
  std::array<bool,50> seen{};
  for(unsigned i=0;i<50;i++){
   const uint32_t node=REX_LOAD_U32(nodes+i*4);if(!Valid(base,node,166))return;
   const unsigned slot=REX_LOAD_U16(node+164);if(slot>=50)return;
   if(seen[slot])return;seen[slot]=true;
   // Native821EEC98 writes by node+164, not by traversal-array order.
   // Mesh palette IDs refer to those final matrix slots.
   t->native_order[i]=uint16_t(slot);t->slots[slot]=uint16_t(slot);t->position_addresses[slot]=node+144;
  }
  t->shirt.Setup(ShirtRest());for(unsigned leg=0;leg<2;leg++)t->pants[leg].Setup(PantsRest(leg));
  t->nodes=nodes;t->skeleton=skeleton;t->matrices=matrices;t->mapped=true;t->reported=false;t->mode_flags=0;t->mode_manager=0;t->Reset();
 }
 const bool old_outside=t->outside,old_known=t->mode_known;ShirtMode(*t,base);if(old_outside!=t->outside||old_known!=t->mode_known){t->Reset();t->reported=false;}
 if(t->profile_known&&!t->native_lod&&t->mode_flags)base[t->mode_flags+9]=t->outside?1:0; // Prepared upper packet only; native tucked/far resources remain unchanged.
 const bool tucked=!t->outside;
 // User-requested native distant meshes also retain their native poses.
 // Never feed sparse distant controls through the close-up cloth cage.
 if(t->native_lod||count<50){t->Reset();t->soft_ready=false;t->cloth_time=cloth_time;return;}
 // Repeated render passes/replay draws at the same simulation time consume
 // the same native pose. Reuse the already solved result instead of solving
 // contacts again (and avoid accumulating a correction on its own output).
 if(t->pose_cached&&t->cached_time==cloth_time&&t->cached_generation==generation&&t->cached_profile==t->collision_profile&&
    (!std::memcmp(base+matrices,t->pose_input.data(),50*64)||!std::memcmp(base+matrices,t->pose_output.data(),50*64))){std::memcpy(base+matrices,t->pose_output.data(),50*64);return;}
 // Native animation may leave unused cloth controls unchanged. Never use
 // our previous ripple as a new native baseline and accumulate deformation.
 if(t->outside&&t->pose_cached&&t->cached_profile==t->collision_profile){
  for(unsigned bone=36;bone<48;bone++)if(!std::memcmp(base+matrices+bone*64,t->pose_output.data()+bone*64,64))
   std::memcpy(base+matrices+bone*64,t->pose_input.data()+bone*64,64);
 }
 std::memcpy(t->pose_input.data(),base+matrices,50*64);
 const auto cache_pose=[&](){std::memcpy(t->pose_output.data(),base+matrices,50*64);t->view_ready=false;t->cached_time=cloth_time;t->cached_generation=generation;t->cached_profile=t->collision_profile;t->pose_cached=true;};
 t->shirt.damping=1.02f;t->shirt.pin_bottom=tucked;t->shirt.max_offset=tucked?.025f:.14f;t->shirt.tether=tucked?.24f:0.f;
 for(auto& pants:t->pants){pants.damping=1.9f;pants.max_offset=.04f;pants.tether=.025f;}
 if(t->generation!=generation){t->generation=generation;t->Reset();t->cloth_time=cloth_time;}
 const float dt=float(cloth_time-t->cloth_time);t->cloth_time=cloth_time;
 std::array<Vec,26> joints;for(unsigned b=0;b<26;b++)joints[b]=P(base,t->position_addresses[b]);
 const auto position=[&](unsigned bone){return joints[bone];};
 std::array<Frame,50> native_frames;for(unsigned b=0;b<50;b++)native_frames[b]=Read(base,matrices+b*64);
 const auto native_root=native_frames[t->slots[0]];
 if(!t->reported){unsigned i=0;for(unsigned b:{0u,11u,26u,28u,36u,42u})t->native_sample[i++]=Read(base,matrices+t->slots[b]*64);}
 if(!Good(native_root)){Diagnostic(*t,base,native_root,position(0),999,0,0);return;}
 const auto root_skin=Combine(cloth_inverse_bind[0],native_root);
 const auto inverse_root_skin=Inverse(root_skin);
 auto torso_skin=Combine(cloth_inverse_bind[8],native_frames[8]);if(!Good(torso_skin))torso_skin=root_skin;
 auto neck_skin=Combine(cloth_inverse_bind[11],native_frames[11]);if(!Good(neck_skin))neck_skin=torso_skin;
 std::array<Frame,2> thigh_skin;
 for(unsigned leg=0;leg<2;leg++){
  const unsigned bone=leg?2:1;thigh_skin[leg]=Combine(cloth_inverse_bind[bone],native_frames[bone]);if(!Good(thigh_skin[leg]))thigh_skin[leg]=root_skin;
 }
 // Preserve the native cut, skinning, scale and gait of the shorts. Add only
 // a small damped lag from the animated thighs, with no radial enlargement.
 const auto soft_target=thigh_skin[0].Point({.09048f,.70f,.01f})-thigh_skin[1].Point({-.09047f,.70f,.01f});
 if(!t->soft_ready||dt<0||dt>.1f){t->soft_position=soft_target;t->soft_velocity={};t->soft_delta={};t->soft_ready=true;}
 else if(dt>0){const unsigned steps=std::max(1u,unsigned(std::ceil(dt*120)));const float h=dt/steps;for(unsigned i=0;i<steps;i++){t->soft_velocity=t->soft_velocity+((soft_target-t->soft_position)*70.f-t->soft_velocity*8.f)*h;t->soft_position=t->soft_position+t->soft_velocity*h;}t->soft_delta=Limit((t->soft_position-soft_target)*.42f,.032f);}
 std::array<float,2> trouser_flex{};std::array<Vec,2> trouser_lag{};
 // Foot motion owns free cloth motion. Torso sway alone cannot keep the hem alive.
 const Vec foot_separation=inverse_root_skin.Linear(native_frames[9].p-native_frames[10].p);
 if(!t->feet_ready||dt<0||dt>.1f){t->last_foot_separation=foot_separation;t->leg_quiet=0;t->leg_motion=1;t->feet_ready=true;t->idle_hem_ready=false;}
 else if(dt>0){
  const float speed=Length(foot_separation-t->last_foot_separation)/dt;t->last_foot_separation=foot_separation;
  if(speed>.18f){t->leg_quiet=0;t->leg_motion=1;t->idle_hem_ready=false;}
  else{t->leg_quiet=std::min(.30f,t->leg_quiet+dt);t->leg_motion=std::clamp((.30f-t->leg_quiet)/.15f,0.f,1.f);}
 }
 const auto pelvis_up=Unit(root_skin.y);
 for(unsigned leg=0;leg<2;leg++){
  float f=std::clamp((1.f-Dot(Unit(thigh_skin[leg].y),pelvis_up)-.10f)/.65f,0.f,1.f);
  trouser_flex[leg]=f*f*(3-2*f);
  const Vec sample{leg?-.09047f:.09048f,.70f,.01f};
  t->leg_lag[leg].Step(thigh_skin[leg].Point(sample)-root_skin.Point(sample),dt);
  trouser_lag[leg]=t->leg_lag[leg].delta*(1.f-.75f*trouser_flex[leg]);
 }
 // Low-excursion walking should not receive sprint-sized fabric amplification.
 // Read native pose excursion; high knees/kicks retain full response even in place.
 const auto smooth_activity=[](float x){x=std::clamp(x,0.f,1.f);return x*x*(3.f-2.f*x);};
 const float thigh_excursion=std::max(1.f-Dot(Unit(thigh_skin[0].y),pelvis_up),1.f-Dot(Unit(thigh_skin[1].y),pelvis_up));
 const float torso_excursion=1.f-Dot(Unit(torso_skin.y),pelvis_up);
 const float activity_target=std::max(smooth_activity((thigh_excursion-.07f)/.25f),smooth_activity((torso_excursion-.06f)/.16f));
 if(!t->shirt_activity_ready||dt<0||dt>.1f){t->shirt_activity=activity_target;t->shirt_activity_ready=true;}
 else if(dt>0){const float response=activity_target>t->shirt_activity?.055f:.22f;t->shirt_activity+=(activity_target-t->shirt_activity)*(1.f-std::exp(-dt/response));}
 const float shirt_activity=t->outside?t->shirt_activity:1.f;
 t->shirt.damping=1.02f+2.0f*(1.f-shirt_activity);
 const auto raw_hem_lift=HemLift(root_skin,thigh_skin);
 if(!t->hem_ready||dt<0||dt>.1f){t->hem_lift_filtered=raw_hem_lift;t->hem_ready=true;}
 else if(dt>0){const float blend=1.f-std::exp(-dt/(.12f-.10f*shirt_activity));for(unsigned leg=0;leg<2;leg++)t->hem_lift_filtered[leg]+=(raw_hem_lift[leg]-t->hem_lift_filtered[leg])*blend;}
 const auto& hem_lift=t->hem_lift_filtered;
 t->torso_lag.Step(inverse_root_skin.Point(torso_skin.Point({0,1.32f,.04f})),dt);
 const Vec torso_fold=Limit(t->torso_lag.delta*.18f+inverse_root_skin.Linear(t->soft_delta)*.065f,.004f)*t->leg_motion;

 for(unsigned arm=0;arm<2;arm++){
  const unsigned elbow=arm?23:21;const Vec sample{arm?-.4312f:.4312f,1.4553f,-.01925f};
  const auto arm_skin=Combine(cloth_inverse_bind[elbow],native_frames[elbow]);
  t->arm_lag[arm].Step(arm_skin.Point(sample)-root_skin.Point(sample),dt);t->sleeve_delta[arm]=Limit(t->arm_lag[arm].delta*.19f,.006f);
  auto cuff=native_frames[48+arm];if(Good(cuff)){cuff.p=cuff.p+t->sleeve_delta[arm];Write(base,matrices+(48+arm)*64,cuff);}
 }
 const auto native_shorts=[&](){for(unsigned b:{26u,27u,34u,35u}){
  auto f=native_frames[b];const bool right=b==27||b==35;
  const auto& thigh=thigh_skin[right?1:0];
  const float flex=trouser_flex[right?1:0];
  const Vec attachment{right?-.09047f:.09048f,.84f,.01f};
  // Under a deep knee lift, follow that thigh instead of pulling the front
  // of the trouser leg back toward the pelvis. The waist blends more gently.
  auto following=Combine(cloth_inverse_bind[b],f);
  if(flex>1e-6f){following=MixHemFrame(following,thigh,flex*(b<28?.35f:.90f),attachment);f=Combine(cloth_bind[b],following);}
  const auto lag=trouser_lag[right?1:0];
  if(b<28)f.p=f.p+lag*.12f;
  else {
   // Swing the lower fabric about its upper attachment without changing scale.
   const auto skin=following;
   const auto pivot=skin.Point(attachment);
   const auto omega=Limit(Cross(pelvis_up,lag)*9.5f,.18f);const float angle=Length(omega);
   const float cosine=std::cos(angle),sine=std::sin(angle);const auto axis=angle>1e-7f?omega*(1/angle):Vec{};
   const auto rotate=[&](Vec v){if(angle<1e-7f)return v;return v*cosine+Cross(axis,v)*sine+axis*(Dot(axis,v)*(1-cosine));};
   f={rotate(f.x),rotate(f.y),rotate(f.z),pivot+rotate(f.p-pivot)+lag*1.30f};
  }
  if(Good(f))Write(base,matrices+b*64,f);
 }t->short_fit={};};
 if(tucked){
  native_shorts();
  // Tucked fabric stays anchored at the waist. Reuse the existing damped gait
  // response, with a small common shear instead of freeing its tucked hem.
  // All native shirt controls receive the same deformation, without a new mesh.
  auto sway=inverse_root_skin.Linear(t->soft_delta);sway.y=0;sway=Limit(sway*1.7f,.030f);
  Frame fabric;fabric.y=fabric.y+sway;fabric.p=sway*-1.f;
  for(unsigned b=28;b<48;b++)if(b<34||b>=36){const auto skin=Combine(cloth_inverse_bind[b],native_frames[b]);Write(base,matrices+b*64,Combine(cloth_bind[b],Combine(fabric,skin)));}
  cache_pose();return;
 }
 constexpr unsigned control_bones[3][6]={{42,43,44,45,46,47},{36,37,38,39,40,41},{28,29,30,31,32,33}},control_column[6]={1,3,5,11,9,7};
 const auto shirt_base=[&](unsigned row,Vec pivot){
  if(row==0)return neck_skin;
  if(row==1)return MixSkin(Combine(cloth_inverse_bind[3],native_frames[3]),torso_skin,.58f,pivot);
  if(tucked)return root_skin;
  return LocalHemAttachment(root_skin,thigh_skin,pivot,hem_lift,shirt_activity);
 };
 // Each distinct attachment is constructed once per pose. Rendering and
 // contact reuse the exact same frame as the simulation target.
 std::array<Frame,36> shirt_bases;for(unsigned row=0;row<3;row++)for(unsigned col=0;col<12;col++)shirt_bases[row*12+col]=shirt_base(row,t->shirt.rest[row*12+col]);
 // Install valid skin bases before testing simulation-node quality. Sparse LODs
 // must retain their visible garment even when their unused nodes are stale.
 for(unsigned row=2;row<3;row++)for(unsigned col=0;col<6;col++){const auto pivot=t->shirt.rest[row*12+control_column[col]];Write(base,matrices+control_bones[row][col]*64,Combine(cloth_bind[control_bones[row][col]],shirt_bases[row*12+control_column[col]]));}
 for(unsigned leg=0;leg<2;leg++)for(unsigned b:{leg?27u:26u,leg?35u:34u})Write(base,matrices+b*64,Combine(cloth_bind[b],thigh_skin[leg]));
 const bool node_fallback=!Finite(position(0))||!Finite(position(8))||!Finite(position(14))||Length(position(1)-position(2))<.08f||Length(position(1)-position(2))>.40f||Length(position(14)-position(8))<.12f;
 if(node_fallback){t->rig_fallbacks++;for(unsigned b=0;b<26;b++){auto skin=Combine(cloth_inverse_bind[b],Read(base,matrices+b*64));if(!Good(skin))skin=root_skin;joints[b]=skin.Point(cloth_bind[b].p);}}
 if(t->node_fallback!=node_fallback){t->node_fallback=node_fallback;t->Reset();}
 const Vec pelvis=position(0);std::array<Vec,2> hips{position(1),position(2)},knees{position(5),position(7)};
 const auto root=Anatomical(hips[0]-hips[1],position(3)-pelvis,pelvis,{0,.9548f,0});
 const auto chest=Anatomical(position(15)-position(16),position(14)-position(8),position(8),{0,1.22045f,-.01925f});
 if(!Good(root)||!Good(chest)){t->Reset();t->rig_fallbacks++;for(unsigned b=26;b<48;b++)if(Good(native_frames[b]))Write(base,matrices+b*64,native_frames[b]);return;}
 unsigned repairs=0,clamps=0;float peak=0;
 const auto set=[&](unsigned bone,Frame deformation,bool shorts){
  const uint32_t at=matrices+t->slots[bone]*64;auto original=Read(base,at);
  if(!Good(original)||Length(original.x)>3||Length(original.y)>3||Length(original.z)>3||Length(original.p-native_root.p)>4){original=Combine(cloth_bind[bone],Combine(cloth_inverse_bind[0],native_root));repairs++;}
  // Logs prove actor+64 is physique scale. Rendering still consumes bind^-1*M.
  // Conjugate model-space cloth through the real KTMDL bind basis.
  // Low-LOD native cloth controls may be unused/stale. Drive every newly
  // weighted shirt control from current core bones, never from stale controls.
  if(!shorts){const unsigned row=2;unsigned col=0;while(col<5&&control_bones[row][col]!=bone)col++;original=Combine(cloth_bind[bone],shirt_bases[row*12+control_column[col]]);}
  else {
   // Visible bare thighs are skinned by 1/2, not the extra muscle controls4/6.
   // Sparse LODs may
   // leave their cloth matrices stale but finite: validity alone is insufficient.
   // Follow the animated thigh, retaining its physique scale, at every LOD.
   original=Combine(cloth_bind[bone],thigh_skin[(bone==27||bone==35)?1:0]);
  }
  const auto skin=Combine(cloth_inverse_bind[bone],original),desired=Combine(deformation,skin);
  auto value=Combine(cloth_bind[bone],desired);if(!Good(value))return;
  const float delta=VertexDelta(skin,desired,shorts,bone),limit=shorts?.048f:(tucked?.020f:.065f);
  if(!std::isfinite(delta))return;
  if(delta>limit){value=Mix(original,value,limit/delta);clamps++;}
  peak=std::max(peak,VertexDelta(skin,Combine(cloth_inverse_bind[bone],value),shorts,bone));Write(base,at,value);
 };
 bool legs_valid=true;for(unsigned leg=0;leg<2;leg++){const float length=Length(knees[leg]-hips[leg]);if(!Finite(hips[leg])||!Finite(knees[leg])||length<.2f||length>.7f)legs_valid=false;}
 std::array<float,2> pants_radius{.10f,.10f};
 const Frame rest_thigh=Anatomical({1,0,0},{0,.4158f,.01539f},{0,0,0},{0,0,0});
 std::array<Frame,2> leg_frames;
 for(unsigned leg=0;leg<2;leg++){
  auto basis=Anatomical(root.x,hips[leg]-knees[leg],{0,0,0},{0,0,0});basis=Combine(Inverse(rest_thigh),basis);
  basis.p=hips[leg]-basis.Linear({leg?-.09047f:.09048f,.9548f,.015404f});leg_frames[leg]=basis;
 }
 legs_valid=legs_valid&&Good(leg_frames[0])&&Good(leg_frames[1]);
 const std::array<ThighProjector,2> thigh_projectors={ThighProjector(hips[0],knees[0],root.x),ThighProjector(hips[1],knees[1],root.x)};
 if(legs_valid)for(unsigned leg=0;leg<2;leg++){
  const auto frame=leg_frames[leg];const auto target=Transform(t->pants[leg].rest,frame);
  t->pants[leg].Step(target,dt,[&](Vec& p){thigh_projectors[leg].Project(p);thigh_projectors[1-leg].Project(p);});
  const auto hem=TrouserFrame(t->pants[leg],leg,frame);
  pants_radius[leg]=std::clamp(std::max(Length(hem.x)*.105f,Length(hem.z)*.115f)+.004f,.085f,.135f);
 }
 const auto update_short_skin=[&](){
  native_shorts();
 };
 update_short_skin();for(unsigned leg=0;leg<2;leg++)pants_radius[leg]=std::min(.19f,pants_radius[leg]*t->short_fit.scale[leg]);
 Frame hip=root;hip.x=Unit(root.x,{1,0,0});hip.y=Unit(root.y);hip.z=Unit(root.z,{0,0,1});hip.p=pelvis+hip.y*.050f;
 Frame torso=chest;torso.p=chest.Point({0,1.20f,-.005f});
 bool arms_valid=true;for(const auto [a,b]:{std::pair<unsigned,unsigned>{21,24},{23,25}}){const float n=Length(position(a)-position(b));if(!std::isfinite(n)||n<.12f||n>.45f)arms_valid=false;}
 const auto body_project=[&](Vec& p){
  ProjectEllipsoid(p,hip,{.171f,.14f,.115f});
  ProjectEllipsoid(p,torso,{.155f,.20f,.105f});
  if(legs_valid)for(unsigned leg=0;leg<2;leg++)ProjectCapsule(p,hips[leg],knees[leg],pants_radius[leg]);
  if(arms_valid){ProjectCapsule(p,position(21),position(24),.050f);ProjectCapsule(p,position(23),position(25),.050f);}
  ProjectEllipsoid(p,hip,{.171f,.14f,.115f});
 };
 const auto render_to_world=Combine(inverse_root_skin,root);
 std::array<Vec,36> targets;
 for(unsigned row=0;row<3;row++)for(unsigned col=0;col<12;col++){const unsigned i=row*12+col;targets[i]=render_to_world.Point(shirt_bases[i].Point(t->shirt.rest[i]));}
 if(dt>0&&t->leg_quiet<.15f&&t->shirt.ready)for(unsigned i=12;i<36;i++){if(!t->shirt.Pinned(i)){t->shirt.position[i]=t->shirt.position[i]+t->pending_contact[i];t->shirt.previous[i]=t->shirt.previous[i]+t->pending_contact[i];}t->pending_contact[i]={};}
 // Cuts have no elapsed motion: skip the synthetic gravity/contact step
 // that Grid::Step applies to a new seed. Settled cloth uses the retained
 // pose and precise visible-mesh contact instead of inflating the coarse cage.
 const bool settled=t->leg_quiet>=.15f;
 if(settled&&!t->idle_hem_ready){t->pending_contact={};t->hem_motion={};t->hem_motion_ready=false;t->torso_lag.ready=false;t->idle_hem_ready=true;}
 if(dt==0||settled)t->shirt.Reset(targets);
 else t->shirt.Step(targets,dt,body_project);
 if(legs_valid){
  // Resolve fabric/fabric only once per simulation update, not per draw pass.
  if(dt>0)for(unsigned iteration=0;iteration<2;iteration++){
   Couple(t->pants[0],t->pants[1]);Couple(t->pants[1],t->pants[0]);
   if(!settled)for(unsigned leg=0;leg<2;leg++)Couple(t->shirt,t->pants[leg]);
  }
  if(dt>0)for(unsigned leg=0;leg<2;leg++){
   const auto target=Transform(t->pants[leg].rest,leg_frames[leg]);
   t->pants[leg].LimitStrain(target,[&](Vec& p){thigh_projectors[leg].Project(p);thigh_projectors[1-leg].Project(p);},1);
  }
 }
 if(dt>0&&!settled)t->shirt.LimitStrain(targets,body_project,2);
 constexpr unsigned bones[3][6]={{42,43,44,45,46,47},{36,37,38,39,40,41},{28,29,30,31,32,33}},column[6]={1,3,5,11,9,7};
 for(unsigned row=2;row<3;row++)for(unsigned col=0;col<6;col++){
  const unsigned point=row*12+column[col];const auto reference=Surface(t->shirt,row,column[col],chest,&targets);
  auto model=tucked&&row==2?Frame{}:RelativeSurface(Surface(t->shirt,row,column[col],chest),reference,t->shirt.rest[point],t->shirt.position[point]-targets[point]);
  set(bones[row][col],model,false);
 }
 // Final contact pass uses the deformed visible mesh, including all LODs.
 t->contact={};if(t->outside){std::array<Frame,50> skin;for(unsigned b=0;b<50;b++)skin[b]=Combine(cloth_inverse_bind[b],Read(base,matrices+b*64));
  std::array<Frame,6> before_contact;for(unsigned i=0;i<6;i++)before_contact[i]=skin[28+i];
  t->contact=ResolveClothProfile(t->collision_profile,skin,3,!t->reported);
  std::array<Frame,6> contact_only;for(unsigned i=0;i<6;i++)contact_only[i]=skin[28+i];
  const float hem_blend=dt>0?1.f-std::exp(-dt/(.10f-.08f*shirt_activity)):0.f;
  // Keep each sector attached locally. Neighbour smoothing avoids abrupt
  // folds, without lifting the opposite side/centre as a single rigid band.
  {
   constexpr unsigned left[6]={3,0,1,4,5,2},right[6]={1,2,5,0,3,4};
   std::array<Frame,6> anchors,anchor_inverse,relative;std::array<Vec,6> motion;
   for(unsigned col=0;col<6;col++){const unsigned b=bones[2][col];const auto pivot=t->shirt.rest[24+column[col]];anchors[col]=shirt_bases[24+column[col]];anchor_inverse[col]=Inverse(anchors[col]);relative[col]=Combine(anchor_inverse[col],skin[b]);motion[col]=skin[b].Point(pivot)-anchors[col].Point(pivot);}
   for(unsigned col=0;col<6;col++){
    const auto pivot=t->shirt.rest[24+column[col]];Frame rotation;rotation.x=Unit(relative[col].x,{1,0,0});rotation.y=Unit(relative[col].y-rotation.x*Dot(relative[col].y,rotation.x),{0,1,0});rotation.z=Unit(Cross(rotation.x,rotation.y),{0,0,1});
    const bool rear=pivot.z<-.035f;const float rotationLimit=(rear?.070f:.17f)*(.30f+.70f*shirt_activity);
    const float angle=std::acos(std::clamp((rotation.x.x+rotation.y.y+rotation.z.z-1)*.5f,-1.f,1.f));if(angle>rotationLimit)rotation=MixSkin(Frame{},rotation,rotationLimit/angle,{});
    auto shift=Limit((motion[col]*.75f+(motion[left[col]]+motion[right[col]])*.125f)*(.16f+1.24f*shirt_activity),.010f+.054f*shirt_activity);
    if(!t->hem_motion_ready||dt<0||dt>.1f)t->hem_motion[col]=shift;
    else if(dt>0)t->hem_motion[col]=t->hem_motion[col]+(shift-t->hem_motion[col])*hem_blend;
    shift=t->hem_motion[col];
    {const auto up=Unit(anchors[col].y);const float rise=Dot(shift,up),cap=.003f+.061f*shirt_activity;if(rise>cap)shift=shift-up*(rise-cap);}
    if(rear){auto local=anchor_inverse[col].Linear(shift);local.z=std::clamp(local.z,-.018f,.018f);local.y=std::min(local.y,.020f);shift=anchors[col].Linear(local);}
    const auto centre=anchors[col].Point(pivot);rotation.p=centre+shift-rotation.Linear(centre);skin[bones[2][col]]=Combine(anchors[col],rotation);
   }
  }
  t->hem_motion_ready=true;
  // Straighten the lower silhouette without narrowing the seam/upper torso.
  // Contact runs afterwards, so the extra clearance still takes precedence.
  const auto foldGait=inverse_root_skin.Linear(t->soft_delta)*((.20f+.80f*shirt_activity)*t->leg_motion);
  for(unsigned col=0;col<6;col++){const auto pivot=t->shirt.rest[24+column[col]];skin[bones[2][col]]=Combine(Combine(MovingFoldShape(pivot,foldGait),StraightHemShape(pivot)),skin[bones[2][col]]);}
  // Contact is enforced on the FINAL shape too, locally per affected control.
  const unsigned coherent_steps=t->contact.maxCorrection>.025f?7u:3u;
  std::array<Frame,6> before_final_contact;for(unsigned i=0;i<6;i++)before_final_contact[i]=skin[28+i];
  const auto final_contact=ResolveClothProfileCoherent(t->collision_profile,skin,coherent_steps);
  t->contact.corrected+=final_contact.corrected;t->contact.maxCorrection=std::max(t->contact.maxCorrection,final_contact.maxCorrection);
  if(dt>0&&!settled){
   constexpr unsigned sorted[2][6]={{36,37,38,41,40,39},{28,29,30,33,32,31}};
   for(unsigned row=2;row<3;row++)for(unsigned col=0;col<12;col++){
    Vec delta{};if(col%2){const unsigned b=sorted[row-1][col/2];const auto rest=t->shirt.rest[row*12+col];delta=contact_only[b-28].Point(rest)-before_contact[b-28].Point(rest)+skin[b].Point(rest)-before_final_contact[b-28].Point(rest);}
    else {const unsigned left=sorted[row-1][((col+11)%12)/2],right=sorted[row-1][((col+1)%12)/2];const auto rest=t->shirt.rest[row*12+col];delta=(contact_only[left-28].Point(rest)-before_contact[left-28].Point(rest)+skin[left].Point(rest)-before_final_contact[left-28].Point(rest)+contact_only[right-28].Point(rest)-before_contact[right-28].Point(rest)+skin[right].Point(rest)-before_final_contact[right-28].Point(rest))*.5f;}
    t->pending_contact[row*12+col]=Limit(render_to_world.Linear(delta)*.35f,.012f);
   }
  }
  if(settled){t->pending_contact={};t->torso_lag.ready=false;}
  for(unsigned b=28;b<34;b++)Write(base,matrices+b*64,Combine(cloth_bind[b],skin[b]));
  // Restore the approved native front and protected back controls.
  // Protected rear/printing controls retain their native pose.
  static const auto torso_radial=[](){std::array<Vec,12> result{};for(unsigned i=0;i<12;i++){const auto pivot=cloth_bind[36+i].p;const float radius=std::hypot(pivot.x,pivot.z+.005f);result[i]=radius>.01f?Vec{pivot.x/radius,0,(pivot.z+.005f)/radius}:Vec{0,0,1};}return result;}();
  for(unsigned b=36;b<48;b++){
   const auto pivot=cloth_bind[b].p;const auto radial=torso_radial[b-36];
   const float sector=std::clamp(pivot.x/.18f,-1.f,1.f),front=radial.z;
   const float wave=std::clamp((torso_fold.x*sector+torso_fold.z*front)*(b<42?1.f:-.65f),-.003f,.003f);
   const Vec shear=radial*(wave*4.f);Frame ripple;ripple.y=ripple.y+shear;ripple.p=radial*wave-shear*pivot.y;
   const auto native_skin=Combine(cloth_inverse_bind[b],native_frames[b]);
   Write(base,matrices+b*64,Combine(cloth_bind[b],Combine(ripple,native_skin)));
  }

 }
 cache_pose();
 Diagnostic(*t,base,native_root,pelvis,repairs,clamps,peak);
}


// Read-only counterpart of the native palette override for attached decals.
// Export once per capture, not once per vertex; no guest scratch or pose writes.
extern "C" bool pes6_cloth_surface_palette(uint8_t* base,uint32_t instance,uint32_t matrices,float* output){
 if(!pes6_cloth_enabled())return false;
 const auto input=REX_LOAD_U32(instance+240);
 std::lock_guard lock(mutex);
 if(auto* matched=PoseTrack(matrices,input,instance);matched&&matched->outside){auto& t=*matched;
  const auto saved=[&](const auto& bytes,unsigned b){
   const auto f=[&](unsigned k){const auto* p=bytes.data()+b*64+k;return std::bit_cast<float>((uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]);};
   const auto v=[&](unsigned k){return Vec{f(k),f(k+4),f(k+8)};};return Frame{v(0),v(16),v(32),v(48)};
  };
  const auto object=REX_LOAD_U32(instance+248),data=REX_LOAD_U32(object+12),bind=data+REX_LOAD_U32(data+28);
  Frame view{};
  if(t.matrices!=matrices)view=Combine(Inverse(Combine(Read(base,bind+80),saved(t.pose_output,0))),Read(base,matrices));
  for(unsigned bone=28;bone<=33;++bone){
   auto frame=saved(t.pose_input,bone);
   if(t.matrices!=matrices)frame=Combine(Combine(Read(base,bind+bone*176+80),frame),view);
   auto* p=output+(bone-28)*12;
   for(const auto& v:{frame.x,frame.y,frame.z,frame.p}){*p++=v.x;*p++=v.y;*p++=v.z;}
  }return true;
 }return false;
}

// Keep the donor palette unchanged. Separate existing upper/lower face draws
// select the solved hem pose or the untouched native torso pose.
extern "C" uint32_t pes6_cloth_palette_matrix(uint8_t* base,uint32_t matrices,uint32_t bone,uint32_t scratch,uint32_t draw,uint32_t slot){
 const bool arm=bone>=18&&bone<=25;
 if(!pes6_cloth_enabled()||(!arm&&(bone<28||bone>33)))return matrices+bone*64;
 const uint32_t pair=REX_LOAD_U32(draw+32),packet=REX_LOAD_U32(pair);
 const unsigned group=REX_LOAD_U32(packet+8),paletteCount=REX_LOAD_U32(packet+24);
 const bool longSleeve=((group==24||group==25)&&paletteCount==6)||((group==36||group==37)&&paletteCount==13);
 if(arm&&!longSleeve)return matrices+bone*64;

 const bool hem=group==35&&REX_LOAD_U32(packet+24)==26;
 if(hem)return matrices+bone*64;
 const uint32_t instance=REX_LOAD_U32(draw+24),input=instance?REX_LOAD_U32(instance+240):0;
 std::lock_guard lock(mutex);
 if(auto* matched=PoseTrack(matrices,input,instance)){auto& t=*matched;
  if(arm){auto delta=t.sleeve_delta[SleeveSide(bone)]*SleeveBoneFactor(bone);if(t.matrices!=matrices){ClothView(t,base,instance,matrices);delta=t.view_transform.Linear(delta);}auto frame=Read(base,matrices+bone*64);frame.p=frame.p+delta;std::memcpy(base+scratch,base+matrices+bone*64,64);Write(base,scratch,frame);return scratch;}
  if(!t.outside)return matrices+bone*64; // Native tucked draw and printing share the solved pose.
  if(t.matrices==matrices){if(bone<28||bone>33)return matrices+bone*64;std::memcpy(base+scratch,t.pose_input.data()+bone*64,64);return scratch;}
  // 82200518 builds a DIFFERENT render array: view * pose * inverseBind.
  // Copying a raw actor frame into that array is not a valid replacement.
  const auto saved=[&](const auto& bytes,unsigned b){
   const auto f=[&](unsigned k){const auto* p=bytes.data()+b*64+k;return std::bit_cast<float>((uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]);};
   const auto v=[&](unsigned k){return Vec{f(k),f(k+4),f(k+8)};};return Frame{v(0),v(16),v(32),v(48)};
  };
  ClothView(t,base,instance,matrices);
  if(bone<28||bone>33)return matrices+bone*64;
  std::memcpy(base+scratch,base+matrices+bone*64,64);Write(base,scratch,t.view_original_hem[bone-28]);return scratch;
 }
 return matrices+bone*64;
}





extern "C" bool pes6_cloth_sleeve_offsets(uint8_t* base,uint32_t instance,uint32_t matrices,float* output){
 if(!pes6_cloth_enabled())return false;const auto input=REX_LOAD_U32(instance+240);std::lock_guard lock(mutex);auto* t=PoseTrack(matrices,input,instance);if(!t)return false;
 if(t->matrices!=matrices)ClothView(*t,base,instance,matrices);for(unsigned i=0;i<2;i++){const auto d=t->matrices==matrices?t->sleeve_delta[i]:t->view_transform.Linear(t->sleeve_delta[i]);output[i*3]=d.x;output[i*3+1]=d.y;output[i*3+2]=d.z;}return true;
}





