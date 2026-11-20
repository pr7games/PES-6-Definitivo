#include "../generated/default/pes6_pch.h"
#include "weather_particles.h"
#include "surface_particles.h"
#include "surface_mesh.h"
#include "surface_motion_clips.h"
#include "surface_water_contacts.h"
#include "weather_vertex.h"
#include "weather_memory.h"
#include "graphics_effect_options.h"
#include "editor_preview_scope.h"
#if __has_include(<rex/system/thread_state.h>)
#include <rex/system/thread_state.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#define PES6_WEATHER_GUEST_PAGE_TABLE 1
#endif
#include "../generated/default/pes6_funcs.h"
#include <windows.h>
#include <bit>
#include <mutex>
#include <atomic>
#include <cstdlib>
#include <cstdio>
#include <unordered_map>
#include <vector>
#include <memory>
#include <utility>
#include <string_view>
#include <filesystem>
#include <fstream>
#include <chrono>
extern "C" bool pes6_torrential_selected();
extern "C" uint32_t pes6_weather_drop_count();
extern "C" bool pes6_cloth_surface_palette(uint8_t*,uint32_t,uint32_t,float*);

extern "C" bool pes6_weather_enabled(){
  return !pes6_editor_preview_depth&&
    !pes6_editor_preview_active.load(std::memory_order_relaxed)&&
    pes6::graphics::EffectOptions().Surfaces();
}
extern "C" bool pes6_rain_enabled(){return pes6::graphics::EffectOptions().rain;}

namespace {
using namespace pes6::weather;
std::mutex weather_mutex;
Splashes splashes;
TurfDebris debris;
KickContact kicks;
uint32_t active_rain=0;
uint32_t prepared_rain_vertices=0,prepared_rain_primitives=0;
bool prepared_rain_enhanced=false;
std::array<uint32_t,surface_batch_count> prepared_surface_vertices{};
uint32_t surface_renderer=0,surface_owner=0;
uint32_t second_surface_renderer=0,second_surface_owner=0;
uint32_t third_surface_renderer=0,third_surface_owner=0;
uint32_t player_surface_renderer=0,player_surface_owner=0;
uint32_t player_surface_staging=0;
bool replay_mode=false;
bool wet_frame=false;
float effect_time=0,frame_dt=1.f/60,last_cursor=0,roll_travel=0,roll_cooldown=0,ball_descent=0;
Vec last_ball{};bool ball_ready=false;
struct SlideTrack {uint32_t skeleton=0,nodes=0,pelvis=0;uint32_t actor=0;SlideContact contact{};float time=0;};
std::array<SlideTrack,64> slides;
std::array<std::atomic<uint32_t>,64> actors{};
std::array<std::atomic<uint32_t>,64> motion_controllers{};
std::array<std::atomic<uint32_t>,64> motion_skeletons{};
std::atomic<uint32_t> motion_clock{0};
std::array<std::atomic<uint32_t>,64> recent_kick_clips{},recent_kick_ticks{},recent_keeper_clips{},recent_keeper_ticks{};
std::array<std::atomic<uint32_t>,64> observed_motion_clips{};
std::atomic<uint64_t> motion_observation_clock{0};
std::array<std::atomic<uint64_t>,64> motion_last_observed{};
struct PendingKeeperContact {bool active=false;Vec position{},incoming{};uint32_t tick=0;};
PendingKeeperContact pending_keeper_contact;
struct PendingGroundContact {bool active=false;Vec origin{},launch{};uint32_t tick=0;};
PendingGroundContact pending_ground_contact;
unsigned slide_cursor=0;
enum class EventKind {Kick,Slide,Bounce,Roll,TrailFollow,WaterContact,FrameDrip,RadialContact,NetShower};
struct SurfaceEvent {EventKind kind{};Vec origin{},direction{};float strength=0;unsigned count=0;bool low=false;uint32_t seed=0;Vec water_origin{};bool has_water_origin=false;};
struct AppearanceFrame;
struct RecordedEffects {std::array<SurfaceEvent,32> events{};unsigned count=0;bool valid=false;std::shared_ptr<AppearanceFrame> appearance;
  std::shared_ptr<RecordedEffects> shared;};
// Highlight copies can precede a frame's last contact event. Publish late
// writes to that frame's shared payload; recycling the ring detaches it.
void PublishEffects(RecordedEffects& frame){if(frame.shared){auto payload=frame.shared;*payload=frame;payload->shared.reset();}}
std::array<RecordedEffects,600> recorded_effects{};
// Native highlights keep their own 300-frame buffers and compact/move them.
// Only non-empty effect frames need a sidecar; copies never touch guest data.
std::unordered_map<uint32_t,RecordedEffects> archived_effect_frames;
uint32_t recording_bank=0,event_serial=0;unsigned recording_slot=600;
bool recording_ready=false;
struct BallWaterTrail {bool active=false;float distance=0,age=0,limit=0;};
BallWaterTrail water_trail;
WaterBallContacts water_contacts;
struct PendingWaterContact {Vec position;unsigned kind;};
std::array<PendingWaterContact,8> pending_water_contacts;
unsigned pending_water_count=0;
std::array<float,2> collision_cooldowns{};
float net_shower_cooldown=0;
int replay_cached_frame=-1;
uint32_t replay_cached_manager=0;
uint32_t replay_cached_frame_pointer=0;
unsigned replay_cached_capacity=0;
Splashes replay_cached_splashes;
TurfDebris replay_cached_debris;
pes6::net_sound::Contact replay_cached_net_contact;
float replay_cached_net_cooldown=0;
void ApplyEvent(const SurfaceEvent& e){
  splashes.Density(pes6_torrential_selected());
  debris.random=e.seed;splashes.random=e.seed^0x819fe81u;
  switch(e.kind){
    case EventKind::Kick:
      if(pes6::graphics::EffectOptions().particles)debris.Burst(e.origin,e.direction,e.low,e.count);
      if(wet_frame){const auto water=e.has_water_origin?e.water_origin:e.origin;splashes.BallTrail(water,e.direction,e.count,e.low);}
      break;
    case EventKind::Slide:
      if(pes6::graphics::EffectOptions().particles)debris.Burst(e.origin,e.direction,true);
      if(wet_frame)splashes.BodyImpact(e.origin);break;
    case EventKind::Bounce:if(wet_frame)splashes.Impact(e.origin.x,e.origin.z,e.strength);break;
    case EventKind::Roll:if(wet_frame)splashes.Light(e.origin,false);break;
    case EventKind::TrailFollow:splashes.Follow(e.direction,e.low);break;
    case EventKind::WaterContact:if(wet_frame)splashes.Contact(e.origin,e.count);break;
    case EventKind::FrameDrip:if(wet_frame)splashes.Contact(e.origin,e.count,true);break;
    case EventKind::RadialContact:if(wet_frame)splashes.Radial(e.origin,e.strength,e.count,e.direction);break;
    case EventKind::NetShower:if(wet_frame)splashes.NetShower(e.origin,e.count);break;
  }
}
void BeginRecordingFrame(uint8_t* base);
void EmitEvent(uint8_t* base,SurfaceEvent e);
bool RestoreRecordedEffects(uint8_t* base,uint32_t manager,float cursor);
void ProcessSlides(uint8_t* base);
void ProcessWaterContacts(uint8_t* base,Vec p,float dt);
unsigned TurfKickAllowed(uint8_t* base,Vec origin,Vec launch,bool* air=nullptr);
bool Valid(uint8_t* base,uint32_t at,size_t bytes){
  if(at<0x10000||uint64_t(at)+bytes>=0xe0000000ull)return false;
#ifdef PES6_WEATHER_GUEST_PAGE_TABLE
  if(auto* kernel=rex::runtime::current_kernel_state()){
    if(auto* memory=kernel->memory()){
      rex::memory::HeapAllocationInfo info{};
      return ReadableGuestRange(*memory,base,at,bytes,info);
    }
  }
#endif
  // Standalone fixtures / host threads not attached to the guest runtime.
  auto* p=base+at;
  while(bytes){MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||
       (info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const auto n=std::min(bytes,size_t(static_cast<uint8_t*>(info.BaseAddress)+info.RegionSize-p));
    if(!n)return false;p+=n;bytes-=n;
  }return true;
}
uint32_t Pointer(uint8_t* base,uint32_t at){
  if(!Valid(base,at,4))return 0;
  const auto value=REX_LOAD_U32(at);
  // These fields contain word-aligned object/table pointers, never byte data.
  return value&3u?0:value;
}
float Float(uint8_t* base,uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));}
void Float(uint8_t* base,uint32_t at,float v){REX_STORE_U32(at,std::bit_cast<uint32_t>(v));}
uint32_t Stage(uint8_t* base){return Pointer(base,0x83139360u);}
bool Wet(uint8_t* base){const auto s=Stage(base);return Valid(base,s,144)&&base[s+131]==1;}
void Vertex(uint8_t* base,uint32_t at,Vec p){Float(base,at,p.x);Float(base,at+4,p.y);Float(base,at+8,p.z);}
Vec Position(uint8_t* base,uint32_t at,float scale=1,float yscale=1){return {Float(base,at)*scale,Float(base,at+4)*scale*yscale,Float(base,at+8)*scale};}
#include "player_surface_effects.h"
void SelectRecordingBank(uint32_t bank){
  if(bank==recording_bank)return;
  if(recording_bank)for(unsigned i=0;i<recorded_effects.size();++i){
    const auto address=recording_bank+48+i*3664;
    if(recorded_effects[i].valid&&(recorded_effects[i].count||recorded_effects[i].appearance))archived_effect_frames[address]=recorded_effects[i];
    else archived_effect_frames.erase(address);
  }
  for(auto& frame:recorded_effects)frame={};
  recording_bank=bank;recording_slot=600;recording_ready=false;replay_cached_frame=-1;
}
void BeginRecordingFrame(uint8_t* base){
  recording_ready=false;
  const auto bank=Pointer(base,0x8313A964u);
  if(!bank||!Valid(base,bank+0x219630u,2))return;
  const unsigned slot=REX_LOAD_U16(bank+0x219630u);
  if(slot>=recorded_effects.size())return;
  SelectRecordingBank(bank);
  if(slot!=recording_slot){recorded_effects[slot]={};recorded_effects[slot].valid=true;recording_slot=slot;}
  recording_ready=true;
}
void EmitEvent(uint8_t* base,SurfaceEvent e){
  e.seed=0x6a55b39u^(++event_serial*0x9e3779b9u);
  if(!replay_mode){
    BeginRecordingFrame(base);
    if(recording_ready){auto& frame=recorded_effects[recording_slot];if(frame.count<frame.events.size())frame.events[frame.count++]=e;PublishEffects(frame);}
  }
  ApplyEvent(e);
}
RecordedEffects* NativeEffectsSlot(uint32_t address){
  constexpr uint32_t stride=3664,first=48;
  if(recording_bank&&address>=uint64_t(recording_bank)+first){
    const uint32_t offset=address-recording_bank-first;
    if(offset%stride==0&&offset/stride<recorded_effects.size())return &recorded_effects[offset/stride];
  }
  const auto found=archived_effect_frames.find(address);
  return found==archived_effect_frames.end()?nullptr:&found->second;
}
const RecordedEffects* EffectsAtNativeFrame(uint32_t address){
  const auto* slot=NativeEffectsSlot(address);return slot&&slot->shared?slot->shared.get():slot;
}
const RecordedEffects* EffectsAtReplayFrame(uint8_t* base,uint32_t manager,unsigned frame){
  const auto entry=manager+80+frame*12;
  if(Valid(base,entry,4)){
    const auto address=Pointer(base,entry);
    if(address)return EffectsAtNativeFrame(address);
  }
  // Standalone/legacy managers without a native frame pointer table.
  return frame<recorded_effects.size()?&recorded_effects[frame]:nullptr;
}
bool RestoreRecordedEffects(uint8_t* base,uint32_t manager,float cursor){
  if((!recording_bank&&archived_effect_frames.empty())||!std::isfinite(cursor))return false;
  const unsigned capacity=REX_LOAD_U16(manager+6);
  if(!capacity||capacity>recorded_effects.size())return false;
  int frame=int(std::floor(cursor));
  const unsigned native_frame=REX_LOAD_U16(manager+4);
  if(native_frame<capacity&&Valid(base,manager+80+native_frame*12,4)&&Pointer(base,manager+80+native_frame*12))frame=int(native_frame);
  if(frame<0||frame>=int(capacity))return false;
  if(const auto* saved=EffectsAtReplayFrame(base,manager,unsigned(frame)))replay_appearance=saved->appearance;
  else replay_appearance.reset();
  if(replay_cached_manager!=manager||replay_cached_capacity!=capacity){replay_cached_frame=-1;replay_cached_manager=manager;replay_cached_capacity=capacity;}
  if(replay_cached_frame>=0&&Valid(base,manager+80+unsigned(replay_cached_frame)*12,4)&&
     Pointer(base,manager+80+unsigned(replay_cached_frame)*12)!=replay_cached_frame_pointer)replay_cached_frame=-1;
  // Ordinary playback advances only new frames; full reconstruction is for
  // seeks. Fractional frames always start from the cached integer frame.
  const int advance=replay_cached_frame<0?600:(frame-replay_cached_frame+int(capacity))%int(capacity);
  pes6::net_sound::Contact net_contact;
  float net_cooldown=0;
  if(advance<=4){splashes=replay_cached_splashes;debris=replay_cached_debris;net_contact=replay_cached_net_contact;net_cooldown=replay_cached_net_cooldown;}
  else {splashes.Reset();debris.Reset();}
  const int steps=advance<=4?advance:210;
  for(int offset=steps-1;offset>=0;--offset){
    splashes.Tick();debris.Tick(1.f/60);
    const unsigned slot=unsigned((frame+int(capacity)-offset%int(capacity))%int(capacity));
    const auto* saved=EffectsAtReplayFrame(base,manager,slot);
    net_cooldown=std::max(0.f,net_cooldown-1.f/60);
    if(saved&&saved->valid)for(unsigned i=0;i<saved->count;i++){
      if(saved->events[i].kind==EventKind::NetShower&&net_cooldown>0)continue;
      ApplyEvent(saved->events[i]);
      if(saved->events[i].kind==EventKind::NetShower)net_cooldown=.65f;
    }
    // Net audio contacts can arrive after the highlight frame was copied.
    // Reconstruct missing roof water from the ball in the saved native frame,
    // using the same net-contact detector as live play. Integer-frame state
    // is cached too, so pause, slow motion and seeks remain deterministic.
    if(wet_frame){
      const auto entry=manager+80+slot*12;
      const auto address=Valid(base,entry,4)?Pointer(base,entry):0;
      if(address&&Valid(base,address+3600,12)){
        const auto p=Position(base,address+3600,1.f/256,-1);
        const float hit=net_contact.Sample({p.x,p.y,p.z},1.f/60);
        if(hit>0&&net_cooldown<=0){
          SurfaceEvent e{EventKind::NetShower,p,{},0,net_base_drops};
          e.seed=0x6a55b39u^(address*0x9e3779b9u);
          ApplyEvent(e);net_cooldown=.65f;
        }
      }else net_contact.Reset();
    }
  }
  replay_cached_frame=frame;replay_cached_splashes=splashes;replay_cached_debris=debris;
  replay_cached_net_contact=net_contact;replay_cached_net_cooldown=net_cooldown;
  replay_cached_frame_pointer=Valid(base,manager+80+unsigned(frame)*12,4)?Pointer(base,manager+80+unsigned(frame)*12):0;
  const float fraction=(cursor-std::floor(cursor))/60.f;
  if(fraction>0){splashes.Tick(fraction);debris.Tick(fraction);}
  return true;
}
void ResetMotion(){ResetReplayDirtBindings();water_trail={};water_contacts={};pending_ground_contact={};pending_keeper_contact={};collision_cooldowns={};net_shower_cooldown=0;replay_cached_frame=-1;replay_cached_manager=0;replay_cached_frame_pointer=0;replay_cached_capacity=0;slides={};ball_ready=false;ball_descent=0;roll_travel=0;roll_cooldown=0;splashes.Reset();debris.Reset();kicks.Reset();}
void SampleBall(uint8_t* base,Vec p,float dt,bool replay){
  if(!Field(p.x,p.z)||!std::isfinite(p.y)){ball_ready=false;return;}
  if(dt<=0)return;
  if(!ball_ready||dt>0.1f){last_ball=p;ball_ready=true;ball_descent=0;return;}
  const float distance=std::hypot(p.x-last_ball.x,p.z-last_ball.z);
  if(distance>2.f||std::abs(p.y-last_ball.y)>2.f){last_ball=p;ball_descent=0;roll_travel=0;return;}
  const float vy=(p.y-last_ball.y)/dt;
  if(water_trail.active){
    water_trail.distance+=distance;water_trail.age+=dt;
    const bool release=water_trail.distance>=water_trail.limit||water_trail.age>3.f;
    EmitEvent(base,{EventKind::TrailFollow,p,{p.x-last_ball.x,p.y-last_ball.y,p.z-last_ball.z},0,0,release});
    if(release)water_trail.active=false;
  }
  // Replay positions are native interpolation results, not the live ball state.
  // A descending-to-rising transition near the turf reconstructs its bounce.
  Vec origin{},launch{};bool emitted_kick=false;
  const bool contact=kicks.Sample(p,dt,origin,launch);
  const auto tick=motion_clock.load(std::memory_order_relaxed);
  bool air=false;unsigned count=0;
  if(contact){
    pending_ground_contact={};count=TurfKickAllowed(base,origin,launch,&air);
    if(!count&&!air&&!replay)pending_ground_contact={true,origin,launch,tick};
  }else if(pending_ground_contact.active&&!replay){
    if(tick-pending_ground_contact.tick>4)pending_ground_contact={};
    else {origin=pending_ground_contact.origin;launch=pending_ground_contact.launch;count=TurfKickAllowed(base,origin,launch,&air);if(air)pending_ground_contact={};}
  }
  {
    if(count){
      pending_ground_contact={};
      emitted_kick=true;kicks.cooldown=.22f;
      SurfaceEvent event{EventKind::Kick,origin,launch,0,count,count==30&&launch.y<1.5f};
      const float speed=std::max(.01f,std::hypot(launch.x,launch.z));
      event.water_origin={origin.x,.035f,origin.z};
      event.has_water_origin=true;EmitEvent(base,event);
      if(wet_frame)water_trail={true,0,0,event.low?3.f:5.f+std::clamp(speed/35.f,0.f,1.f)};
    }
  }
  if(replay&&wet_frame){
    if(vy<-.30f)ball_descent=std::max(ball_descent,-vy);
    if(ball_descent>.30f&&((p.y<.13f)||(last_ball.y<.40f&&p.y<.60f&&vy>.15f))){
      splashes.Impact(p.x,p.z,ball_descent*65536.f/60.f);ball_descent=0;
    }
  }
  roll_cooldown=std::max(0.f,roll_cooldown-dt);
  if(!emitted_kick&&!water_trail.active&&wet_frame&&p.y>=0.07f&&p.y<0.16f&&last_ball.y<0.18f&&distance/dt>0.6f){
    roll_travel+=distance;
    if(roll_travel>=0.18f&&roll_cooldown==0){
      EmitEvent(base,{EventKind::Roll,{p.x,0,p.z}});roll_travel=0;roll_cooldown=0.045f;
    }
  }else roll_travel=0;
  last_ball=p;
}
}

// Called before a native 3664-byte frame copy (including highlight compaction).
extern "C" void pes6_weather_frame_copy(uint8_t* base,uint32_t destination,uint32_t source){
  if(!pes6_weather_enabled()||destination==source||destination<0x10000||source<0x10000)return;
  std::lock_guard lock(weather_mutex);
  auto* found=NativeEffectsSlot(source);
  RecordedEffects frame{};
  if(found){
    if(!found->shared)found->shared=std::make_shared<RecordedEffects>(*found);
    frame=*found->shared;frame.shared=found->shared;
  }
  const auto current=Pointer(base,0x8313A964u);
  if(current&&destination>=uint64_t(current)+48){
    const uint32_t offset=destination-current-48;
    if(offset%3664==0&&offset/3664<recorded_effects.size()){
      SelectRecordingBank(current);
      recorded_effects[offset/3664]=frame;replay_cached_frame=-1;return;
    }
  }
  if(frame.valid)archived_effect_frames[destination]=frame;
  else archived_effect_frames.erase(destination);
  replay_cached_frame=-1;
}

extern "C" void pes6_weather_initialized(uint8_t* base,uint32_t rain){
  if(!pes6_weather_enabled())return;
  // Controllers outlive rain GPU resources. Clearing them here could silence
  // strikes/saves after a weather renderer rebuild until another animation hook.
  std::lock_guard lock(weather_mutex);ResetMotion();active_rain=0;replay_mode=false;effect_time=0;
  player_surface_models.clear();player_surface_instances={};
  ++player_surface_capture_epoch;
  if(!Valid(base,rain,112))return;
  const auto working=Pointer(base,rain+100),scratch=Pointer(base,rain+104);
  const auto indices=Pointer(base,rain+108),types=Pointer(base,rain+92);
  if(!Valid(base,working,working_bytes)||!Valid(base,scratch,scratch_bytes)||
     !Valid(base,indices,index_bytes)||!Valid(base,types,max_rain_drops*4)){
    REXLOG_WARN("PES6 weather: rain buffers unavailable");return;
  }
  for(uint32_t i=0;i<index_capacity;++i)REX_STORE_U16(indices+i*2,uint16_t(i));
  // GPU resources are rebuilt for cinematics. The recorded match/highlight
  // events must outlive those resources (and keep their deterministic seeds).
  if(pes6_rain_enabled())REX_STORE_U32(rain+84,std::bit_cast<uint32_t>(-(pes6_torrential_selected()?torrential_fall_step:rain_fall_step)));
  pending_water_count=0;
  active_rain=rain;
  REXLOG_INFO("PES6 weather v7: {} drops, resolution-compensated strokes, ball, slides and falls, no footsteps",drops);
}

// The native match-state destructor, unlike rain/actor recreation, also runs
// when the player abandons a match. Old sidecars must not repopulate new kits.
extern "C" void pes6_player_match_end(){
  std::lock_guard lock(weather_mutex);
  for(unsigned i=0;i<player_dirt.size();i++)if(player_dirt[i].patch_count)SurfaceTrace("match_end",i,player_dirt[i],{});
  player_dirt={};player_dirt_history.clear();player_dirt_identities.clear();player_input_identities.clear();
  player_surface_instances={};replay_appearance.reset();ResetReplayDirtBindings();++player_surface_capture_epoch;
  baked_dirt_cache={};dirt_topology_cache={};
  recorded_effects={};archived_effect_frames.clear();recording_bank=0;recording_slot=600;recording_ready=false;
  appearance_game=0;appearance_minute=0;appearance_period=0;appearance_finished=false;appearance_new_match_ticks=0;
  for(auto& actor:actors)actor.store(0,std::memory_order_relaxed);
  ResetMotion();replay_mode=false;
}

extern "C" void pes6_player_match_transition(){
  std::lock_guard lock(weather_mutex);
  RememberLivePlayerDirt();
  // Extra time and summaries can recreate the native match-state object.
  // Preserve session data. The pre-match frontend explicitly starts clean.
  player_dirt={};player_dirt_history.clear();player_input_identities.clear();
  player_surface_instances={};ResetReplayDirtBindings();++player_surface_capture_epoch;
  for(auto& actor:actors)actor.store(0,std::memory_order_relaxed);
  ResetMotion();replay_mode=false;
}

extern "C" void pes6_weather_tick(uint8_t* base){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);
  wet_frame=pes6_rain_enabled()&&Wet(base);
  splashes.Density(pes6_torrential_selected());
  if(!Valid(base,0x8313A93Eu,1)||base[0x8313A93Eu])return;
  if(replay_mode){ResetMotion();replay_mode=false;}
  replay_appearance.reset();PlayerAppearanceClock(base);
  BeginRecordingFrame(base);
  motion_clock.fetch_add(1,std::memory_order_relaxed);
  // Registration can precede population of actor+1352. Rebind identities
  // independently of rain/slide processing, including after halftime rebuilds.
  if((motion_clock.load(std::memory_order_relaxed)&7u)==0)
    for(unsigned i=0;i<actors.size();i++){
      const auto actor=actors[i].load(std::memory_order_relaxed);
      if(!actor||!Valid(base,actor,1356)||REX_LOAD_U32(actor)!=0x820C7A80u)continue;
      const auto identity=PlayerRecordIdentity(base,actor),input=Pointer(base,actor+60);
      auto& dirt=PlayerDirtForActor(i,actor,identity);
      if(input)dirt.input=input;
      if(identity&&input)player_input_identities.insert_or_assign(input,identity);
    }
  frame_dt=1.f/60;effect_time+=frame_dt;
  splashes.Tick();debris.Tick(frame_dt);
  const auto game=Pointer(base,0x8313A948u),ball=Pointer(base,game+2520);
  if(Valid(base,ball,156)){
    const auto p=Position(base,ball+144,1.f/256,-1);
    ProcessWaterContacts(base,p,frame_dt);SampleBall(base,p,frame_dt,false);
  }else pending_water_count=0;
  ProcessSlides(base);
  if(recording_ready){auto& frame=recorded_effects[recording_slot];frame.appearance=std::make_shared<AppearanceFrame>();frame.appearance->minute=appearance_minute;frame.appearance->water_time=effect_time;frame.appearance->period=appearance_period;frame.appearance->tick=motion_clock.load(std::memory_order_relaxed);frame.appearance->players=player_dirt;PublishEffects(frame);}
  WritePlayerSurfaceDiagnostics();
}

extern "C" void pes6_weather_replay_begin(uint8_t* base,uint32_t manager){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);
  wet_frame=pes6_rain_enabled()&&Wet(base);
  splashes.Density(pes6_torrential_selected());
  if(!Valid(base,manager,44)){ResetMotion();return;}
  const float cursor=Float(base,manager+36);
  if(!std::isfinite(cursor)){ResetMotion();return;}
  const float delta=cursor-last_cursor;
  if(!replay_mode){RememberLivePlayerDirt();SurfaceTrace("replay_enter",0,{},{});}
  if(!replay_mode||delta<0||delta>4){ResetMotion();frame_dt=0;replay_mode=true;}
  else {frame_dt=delta/60;effect_time+=frame_dt;if(!recording_bank){splashes.Tick(frame_dt);debris.Tick(frame_dt);}}
  last_cursor=cursor;
  const auto capacity=REX_LOAD_U16(manager+6),frame=REX_LOAD_U16(manager+4);
  replay_appearance.reset();
  if(capacity&&capacity<=600&&frame<capacity)
    if(const auto* saved=EffectsAtReplayFrame(base,manager,frame))replay_appearance=saved->appearance;
}
extern "C" void pes6_weather_replay_end(uint8_t* base,uint32_t manager){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);
  if(!replay_mode||!Valid(base,manager,44))return;
  // A highlight can have a frame table but no live ball object.
  if(RestoreRecordedEffects(base,manager,last_cursor))return;
  if(!Valid(base,manager+7272,4))return;
  const auto ball=Pointer(base,manager+7272);
  if(Valid(base,ball,32)){
    const Vec p=Position(base,ball+16,1.f/256,-1);
    if(frame_dt==0&&!ball_ready){last_ball=p;ball_ready=true;}
    else SampleBall(base,p,frame_dt,true);
  }
  if(frame_dt>0)ProcessSlides(base);
}

// Render passes only register addresses; pose inspection runs on simulation time.
extern "C" void pes6_weather_actor(uint8_t* base,uint32_t actor){
  if(!pes6_weather_enabled()||!actor)return;
  // Actor is live and already used by the native skinning function.
  if(REX_LOAD_U32(actor)!=0x820C7A80u)return;
  for(auto& slot:actors)if(slot.load(std::memory_order_relaxed)==actor)return;
  for(unsigned i=0;i<actors.size();i++){uint32_t empty=0;
    if(actors[i].compare_exchange_strong(empty,actor,std::memory_order_relaxed)){
      std::lock_guard lock(weather_mutex);
      const auto identity=PlayerRecordIdentity(base,actor),input=Pointer(base,actor+60);
      if(identity&&input){
        player_input_identities.insert_or_assign(input,identity);
        if(!replay_mode){auto& dirt=PlayerDirtForActor(i,actor,identity);dirt.input=input;}
      }
      return;
    }
  }
}
extern "C" void pes6_weather_forget(uint8_t* base,uint32_t actor){
  if(!pes6_weather_enabled())return;
  {
    std::lock_guard lock(weather_mutex);
    for(unsigned i=0;i<actors.size();i++)if(actors[i].load(std::memory_order_relaxed)==actor)RememberPlayerDirt(player_dirt[i]);
    if(const auto input=Pointer(base,actor+60))player_input_identities.erase(input);
  }
  const auto skeleton=Pointer(base,actor+68);
  if(skeleton)for(unsigned i=0;i<motion_controllers.size();i++){
    if(motion_skeletons[i].load(std::memory_order_relaxed)!=skeleton)continue;
    motion_controllers[i].store(0,std::memory_order_relaxed);
    motion_skeletons[i].store(0,std::memory_order_relaxed);
    recent_kick_clips[i].store(0,std::memory_order_relaxed);recent_keeper_clips[i].store(0,std::memory_order_relaxed);
    observed_motion_clips[i].store(0,std::memory_order_relaxed);
    motion_last_observed[i].store(0,std::memory_order_relaxed);
  }
  for(auto& slot:actors){uint32_t expected=actor;
    slot.compare_exchange_strong(expected,0,std::memory_order_relaxed);
  }
}
namespace {
void ProcessSlides(uint8_t* base){
  if(!active_rain)return;
  for(unsigned visited=0;visited<actors.size();++visited){
    const unsigned index=slide_cursor++%actors.size();
    const auto actor=actors[index].load(std::memory_order_relaxed);
    auto* track=&slides[index];
    if(!actor){*track={};continue;}
    if(track->actor!=actor){*track={};track->actor=actor;}
    if(track->time>=effect_time)continue;
    if(!Valid(base,actor,72)||REX_LOAD_U32(actor)!=0x820C7A80u||!base[actor+4])continue;
    const auto skeleton=REX_LOAD_U32(actor+68);
    if((!skeleton||skeleton!=track->skeleton)&&!Valid(base,skeleton,200))continue;
    const auto nodes=REX_LOAD_U32(skeleton+196);
    if((!nodes||nodes!=track->nodes)&&!Valid(base,nodes,4))continue;
    // Bone0 is the central pelvis; no foot/toe nodes are inspected.
    const auto pelvis=REX_LOAD_U32(nodes);
    if((!pelvis||pelvis!=track->pelvis)&&!Valid(base,pelvis,160))continue;
    track->skeleton=skeleton;track->nodes=nodes;track->pelvis=pelvis;
    const Vec p=Position(base,pelvis+144);
    if(!replay_mode){
      const auto identity=PlayerRecordIdentity(base,actor);
      auto& dirt=PlayerDirtForActor(index,actor,identity);dirt.position=p;dirt.input=REX_LOAD_U32(actor+60);
      if(identity&&dirt.input)player_input_identities.insert_or_assign(dirt.input,identity);
    }
    const float dt=effect_time-track->time;
    if(dt<=0)continue;track->time=effect_time;
    const Vec slide_direction{p.x-track->contact.previous.x,0,p.z-track->contact.previous.z};
    if(track->contact.Slide(p,dt)){
      if(pes6::graphics::EffectOptions().dirt&&!replay_mode){auto& dirt=player_dirt[index];dirt.amount=std::min(1.f,dirt.amount+.18f);dirt.pending=true;dirt.contact_tick=motion_clock.load(std::memory_order_relaxed);}
      ++player_surface_diagnostics.slides;
      EmitEvent(base,{EventKind::Slide,{p.x,0,p.z},slide_direction});
    }
    // Repeated goalkeeper dives/low body drags can touch cloth without a new
    // upright-to-fall transition. Keep this separate from particle emission.
    if(pes6::graphics::EffectOptions().dirt&&!replay_mode&&p.y<.65f&&std::hypot(slide_direction.x,slide_direction.z)>dt*.6f&&
       motion_clock.load(std::memory_order_relaxed)-player_dirt[index].contact_tick>20){
      auto& dirt=player_dirt[index];dirt.amount=std::min(1.f,dirt.amount+.12f);dirt.pending=true;dirt.contact_tick=motion_clock.load(std::memory_order_relaxed);
    }
  }
}
}

extern "C" void pes6_weather_impact(uint8_t* base,uint32_t position,uint32_t height,
                                     uint32_t z,float incoming){
  if(!pes6_rain_enabled())return;
  // Guard against the same physics routine being used for trajectories/previews.
  const auto game=Pointer(base,0x8313A948u),ball=Pointer(base,game+2520);
  if(!Valid(base,ball,552)||position!=ball+144||height!=ball+148||z!=ball+152||!Wet(base))return;
  if(!Valid(base,0x8313A93Eu,1)||base[0x8313A93Eu])return;
  const float x=Float(base,position)/256.f,world_z=Float(base,z)/256.f;
  std::lock_guard lock(weather_mutex);
  wet_frame=true;EmitEvent(base,{EventKind::Bounce,{x,0,world_z},{},incoming});
}

// Preserve ambient rain as native lines. Grass, soil and splash droplets form
// one separate XYZ+ARGB triangle batch, with real closed surfaces.
extern "C" void pes6_weather_draw(uint8_t* base,uint32_t rain,uint32_t descriptor){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);
  if(rain!=active_rain||!Valid(base,rain,112)||!Valid(base,descriptor,20))return;
  const auto vertices=Pointer(base,rain+104);
  if(!Valid(base,vertices,scratch_bytes))return;
  const bool wet=Wet(base);
  const bool heavy=wet&&pes6_torrential_selected();
  const uint32_t rain_count=wet?pes6_weather_drop_count():0;
  prepared_rain_enhanced=pes6_rain_enabled();
  const uint32_t rain_vertices=rain_count*(prepared_rain_enhanced?4:2);
  prepared_rain_vertices=rain_vertices;prepared_rain_primitives=rain_count*(prepared_rain_enhanced?2:1);
  if(prepared_rain_enhanced)Float(base,rain+84,-(heavy?torrential_fall_step:rain_fall_step));
  // Keep individual native rain lines and the selected population.
  if(wet&&!prepared_rain_enhanced){
    for(unsigned i=rain_count;i-->0;){
      const auto a=Position(base,vertices+i*24),b=Position(base,vertices+i*24+12);
      StoreWeatherVertex(base+vertices+i*32,a,0xFFFFFFFFu);
      StoreWeatherVertex(base+vertices+i*32+16,b,0xFFFFFFFFu);
    }
  }else if(wet){
    const float dx=Float(base,rain+44)-Float(base,rain+32),dz=Float(base,rain+52)-Float(base,rain+40);
    const float norm=std::hypot(dx,dz);
    const float half_width=heavy?torrential_rain_half_width:drizzle_rain_half_width;
    const Vec offset=norm>.001f?Vec{-dz/norm*half_width,0,dx/norm*half_width}:Vec{half_width,0,0};
    for(unsigned i=rain_count;i-->0;){
      const auto a=Position(base,vertices+i*24);auto b=Position(base,vertices+i*24+12);
      b=Add(a,Mul({b.x-a.x,b.y-a.y,b.z-a.z},heavy?torrential_rain_length:drizzle_rain_length));
      if(heavy){b.x+=(b.y-a.y)*.084f;b.z+=(b.y-a.y)*.024f;}
      const std::array<Vec,4> points{Add(a,offset),Add(b,offset),Add(a,Mul(offset,-1)),Add(b,Mul(offset,-1))};
      for(unsigned j=0;j<4;j++)StoreWeatherVertex(base+vertices+(i*4+j)*16,points[j],0xB8B8B8B8u);
    }
  }
  prepared_surface_vertices={};unsigned batch=0;
  auto solid=[&](const ParticleMesh& mesh,const Vec* translation=nullptr,uint32_t alpha=0){
    if(prepared_surface_vertices[batch]+mesh.count>surface_batch_vertices){if(batch+1==surface_batch_count)return;++batch;}
    auto* destination=base+vertices+(rain_vertices+batch*surface_batch_vertices+prepared_surface_vertices[batch])*16;
    for(unsigned i=0;i<mesh.count;i++){
      const auto& v=mesh.vertices[i];
      StoreWeatherVertex(destination,translation?Add(*translation,v.p):v.p,v.color|alpha);
      destination+=16;
    }
    prepared_surface_vertices[batch]+=mesh.count;
  };
  // This function holds weather_mutex: slot caches are safe across draws.
  static std::array<RoundWaterMeshCache,splash_limit> round_water_meshes;
  if(wet&&pes6_rain_enabled())for(const auto& d:splashes.particles)if(d.life>0){
    // Compact closed droplets, gently elongated along their velocity, with
    // a narrow size range. No velocity-length line or duplicated strokes.
    const float radius=(d.wide?.0075f:.005f)*d.size*(d.round?.75f:.45f)*(heavy?torrential_water_scale:1.f);
    const float fade=d.trail?1.f:std::clamp(d.life/.12f,0.f,1.f);
    const uint32_t color=(uint32_t((d.round?152.f:168.f)*fade)<<24)|0x00F1F6FAu;
    solid(round_water_meshes[&d-splashes.particles.data()].Get(radius,d.phase),&d.p,color&0xff000000u);
  }
  static std::array<RoundWaterMeshCache,net_drop_limit> net_meshes;
  if(wet&&pes6_rain_enabled()&&splashes.net_active)for(unsigned i=0;i<net_drop_limit;i++){
    const auto& d=splashes.net_particles[i];if(d.life<=0)continue;
    const uint32_t alpha=uint32_t(224.f*std::clamp(d.life/.08f,0.f,1.f))<<24;
    const float radius=.0085f*(heavy?.95f*1.15f:1.f)*d.size;
    solid(net_meshes[i].Get(radius,d.phase),&d.p,alpha);
  }
  static std::array<SoilClodMeshCache,debris_limit> soil_meshes;
  for(const auto& p:debris.particles)if(pes6::graphics::EffectOptions().particles&&p.life>0){
    const float phase=p.phase+p.spin*p.age;
    if(p.soil){
      solid(soil_meshes[&p-debris.particles.data()].Get(p.p,.004275f*p.size,p.phase,phase));
    }else{
      const Vec axis{std::sin(phase)*.65f,.65f,std::cos(phase)*.4f};
      const float length=.055f*p.size;
      solid(Solid(p.p,axis,length*.5f,.007f*p.size,.0018f*p.size,phase,.12f,0xFF508037u));
    }
  }
  // Reserve every existing splash/turf/net particle first. This new layer has
  // its own bounded budget and cannot displace the original visual effects.
  // Player surfaces are submitted after the opaque actor pass, independently
  // of rain/grass. Opaque player draws must not overwrite alpha-blended marks.
  REX_STORE_U32(descriptor,vertices+rain_vertices*16);
  REX_STORE_U32(descriptor+4,prepared_surface_vertices[0]/3);REX_STORE_U32(descriptor+12,prepared_surface_vertices[0]);
  REX_STORE_U32(descriptor+16,0xFFFFFFFFu);
}
extern "C" bool pes6_weather_wet(uint8_t* base){return Wet(base);}
extern "C" void pes6_player_wet_material(uint8_t* base,uint32_t draw,uint32_t vector){
  if(!pes6_weather_enabled())return;
  if(!Valid(base,draw,36))return;
  const uint32_t object=REX_LOAD_U32(draw+28),epoch=player_surface_source_epoch.load(std::memory_order_acquire);
  struct OwnerClass {uint32_t owner=0,epoch=0;bool face=false;};
  static thread_local std::array<OwnerClass,128> owners;
  auto& classification=owners[((object>>4)^(object>>12))%owners.size()];
  if(classification.owner!=object||classification.epoch!=epoch){
    std::lock_guard lock(weather_mutex);const auto found=player_surface_sources.find(object);
    classification={object,epoch,found!=player_surface_sources.end()&&found->second.face};
  }
  if(!classification.face)return;
  std::lock_guard lock(weather_mutex);
  const auto pair=Pointer(base,draw+32),packet=Pointer(base,pair);
  if(!Valid(base,packet,8)||(REX_LOAD_U32(packet+4)&0xffffu)!=0||!Valid(base,vector,16))return;
  const auto* state=replay_mode&&replay_appearance?replay_appearance.get():nullptr;
  const float moisture=pes6::graphics::EffectOptions().Moisture(Wet(base),PlayerSweat(state?state->minute:appearance_minute,state?state->period:appearance_period));
  if(moisture<=0)return;
  // Native82202BF0 constant8: material80..88 (specular RGB), material40
  // (power). Modify ONLY the per-draw stack copy. Keep the game's real normal,
  // specular and diffuse textures, lighting and skin tone; never edit assets.
  std::array<float,4> original;for(unsigned i=0;i<4;i++){original[i]=Float(base,vector+i*4);if(!std::isfinite(original[i]))return;}
  for(unsigned i=0;i<3;i++)Float(base,vector+i*4,std::clamp(original[i]*(1+moisture*1.8f),0.f,4.f));
  if(original[3]>0)Float(base,vector+12,std::clamp(original[3]*(1+moisture*.9f),1.f,64.f));
  ++player_surface_diagnostics.material_updates;
}
extern "C" void pes6_player_surface_source(uint8_t* base,uint32_t object,uint32_t data){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);++player_surface_diagnostics.loads;
  auto model=BuildPlayerSurfaceModel(base,data);
  if(!model.samples.empty()){player_surface_sources.insert_or_assign(object,std::move(model));++player_surface_diagnostics.loaded_models;}
  else player_surface_sources.erase(object);
  player_surface_source_epoch.fetch_add(1,std::memory_order_release);
}
extern "C" void pes6_player_surface_resident(uint8_t* base,uint32_t object){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);const auto found=player_surface_sources.find(object);
  if(found!=player_surface_sources.end())if(const auto data=Pointer(base,object+12))player_surface_models.insert_or_assign(data,found->second);
}
extern "C" void pes6_player_surface_unload(uint8_t* base,uint32_t object){
  std::lock_guard lock(weather_mutex);
  if(player_surface_sources.empty()&&player_surface_models.empty())return;
  if(player_surface_sources.erase(object))player_surface_source_epoch.fetch_add(1,std::memory_order_release);
  if(const auto data=Pointer(base,object+12)){
    player_surface_models.erase(data);for(auto& instance:player_surface_instances)if(instance.data==data)instance={};
  }
}
extern "C" void pes6_player_surface_pose(uint8_t* base,uint32_t instance){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);++player_surface_diagnostics.pose;CapturePlayerSurfaceOnce(base,instance);
}
extern "C" void pes6_player_surface_draw(uint8_t* base,uint32_t draw){
  if(!pes6_weather_enabled())return;
  std::lock_guard lock(weather_mutex);++player_surface_diagnostics.draw;
  if(!Valid(base,draw,28))return;const auto instance=Pointer(base,draw+24);if(!instance)return;
  CapturePlayerSurfaceOnce(base,instance);
}
extern "C" bool pes6_surface_pending(uint8_t* base){
  if(Wet(base))return true;
  std::lock_guard lock(weather_mutex);
  for(const auto& instance:player_surface_instances)if(instance.address&&!instance.meshes.empty()&&std::abs(instance.time-effect_time)<.05f)return true;
  for(const auto& p:debris.particles)if(p.life>0)return true;
  return false;
}
// Surface geometry owns distinct GPU vertex/index resources. Uploading rain and
// surfaces to the same native per-frame buffer can invalidate an earlier draw.
extern "C" void pes6_surface_release(PPCContext& ctx,uint8_t* base,uint32_t owner){
  const auto saved=ctx;
  for(unsigned batch=0;batch<=surface_batch_count;batch++){
    auto& renderer=batch==3?player_surface_renderer:batch==2?third_surface_renderer:batch?second_surface_renderer:surface_renderer;
    auto& source=batch==3?player_surface_owner:batch==2?third_surface_owner:batch?second_surface_owner:surface_owner;
    if(!renderer||source!=owner)continue;
    if(batch==3&&player_surface_staging){
      ctx.r3.u64=player_surface_staging;sub_8280E7B8(ctx,base);player_surface_staging=0;
    }
    const auto released=renderer;renderer=source=0;
    ctx.r3.u64=released;ctx.r4.u64=1;sub_821959E0(ctx,base);
  }
  ctx=saved;
}
namespace {
uint32_t SurfaceRenderer(PPCContext& ctx,uint8_t* base,uint32_t source,unsigned batch){
  auto& handle=batch==3?player_surface_renderer:batch==2?third_surface_renderer:batch?second_surface_renderer:surface_renderer;
  auto& owner=batch==3?player_surface_owner:batch==2?third_surface_owner:batch?second_surface_owner:surface_owner;
  if(handle&&owner==source)return handle;
  if(handle)pes6_surface_release(ctx,base,owner);
  const auto saved=ctx;
  ctx.r3.u64=124;sub_826D61B0(ctx,base);const auto renderer=ctx.r3.u32;
  if(!renderer){ctx=saved;return 0;}
  std::memcpy(base+renderer,base+source,124);
  for(unsigned at:{12u,16u,120u})REX_STORE_U32(renderer+at,0);
  REX_STORE_U32(renderer+104,2);REX_STORE_U32(renderer+116,1);
  REX_STORE_U32(renderer+108,surface_triangle_capacity);
  REX_STORE_U32(renderer+112,surface_triangle_capacity*3);
  REX_STORE_U32(renderer+20,surface_triangle_capacity*3*16);
  for(unsigned at:{12u,16u}){
    ctx.r3.u64=surface_triangle_capacity*3*16;ctx.r4.u64=0;
    sub_82128480(ctx,base);REX_STORE_U32(renderer+at,ctx.r3.u32);
  }
  // Restore the established indexed submission path for visible surfaces.
  ctx.r3.u64=surface_triangle_capacity*3*2;ctx.r4.u64=0;
  sub_8212D8E8(ctx,base);REX_STORE_U32(renderer+120,ctx.r3.u32);
  if(!REX_LOAD_U32(renderer+12)||!REX_LOAD_U32(renderer+16)||!REX_LOAD_U32(renderer+120)){
    ctx.r3.u64=renderer;ctx.r4.u64=1;sub_821959E0(ctx,base);ctx=saved;return 0;
  }
  handle=renderer;owner=source;ctx=saved;return renderer;
}
}
extern "C" void pes6_surface_render(PPCContext& ctx,uint8_t* base){
  if(!pes6_weather_enabled()){sub_82195A80(ctx,base);return;}
  const auto renderer=ctx.r3.u32,descriptor=ctx.r4.u32;
  const auto parameter=ctx.r5.u64;
  const auto at=REX_LOAD_U32(descriptor),triangles=REX_LOAD_U32(descriptor+4),count=REX_LOAD_U32(descriptor+12);
  const auto scratch=Pointer(base,active_rain+104);
  const bool rain=prepared_rain_vertices&&at==scratch+prepared_rain_vertices*16;
  if(rain){
    REX_STORE_U32(descriptor,scratch);REX_STORE_U32(descriptor+4,prepared_rain_primitives);REX_STORE_U32(descriptor+12,prepared_rain_vertices);
    const auto old_layout=REX_LOAD_U32(renderer+104),old_type=REX_LOAD_U32(renderer+116);
    REX_STORE_U32(renderer+104,0);REX_STORE_U32(renderer+116,3);
    sub_82195A80(ctx,base);
    REX_STORE_U32(renderer+104,old_layout);REX_STORE_U32(renderer+116,old_type);
  }
  REX_STORE_U32(descriptor,at);REX_STORE_U32(descriptor+4,triangles);REX_STORE_U32(descriptor+12,count);
  for(unsigned batch=0;batch<surface_batch_count;batch++)if(prepared_surface_vertices[batch]){
    const auto mesh_renderer=SurfaceRenderer(ctx,base,renderer,batch);
    REX_STORE_U32(descriptor,at+batch*surface_batch_vertices*16);
    REX_STORE_U32(descriptor+4,prepared_surface_vertices[batch]/3);REX_STORE_U32(descriptor+12,prepared_surface_vertices[batch]);
    if(mesh_renderer){ctx.r3.u64=mesh_renderer;ctx.r4.u64=descriptor;ctx.r5.u64=parameter;sub_82195A80(ctx,base);}
  }
  REX_STORE_U32(descriptor,at);REX_STORE_U32(descriptor+4,triangles);REX_STORE_U32(descriptor+12,count);
}
extern "C" void pes6_player_surface_view_begin(uint8_t* base,uint32_t group,uint32_t pass){
  if(!pes6_weather_enabled()||group>1||pass>1)return;
  std::lock_guard lock(weather_mutex);++player_surface_view_epoch;
  const auto camera=Pointer(base,0x83139614u);
  player_surface_camera_ready=camera&&Valid(base,camera+320+624,12);
  if(player_surface_camera_ready)player_surface_camera=Position(base,camera+320+624);
}
extern "C" void pes6_player_surface_render(PPCContext& ctx,uint8_t* base,uint32_t group,uint32_t pass,uint32_t descriptor){
  if(!pes6_weather_enabled()||group>1||pass>1)return;
  std::lock_guard lock(weather_mutex);
  if(!active_rain||!Valid(base,active_rain,112)||!Valid(base,descriptor,20))return;
  const auto scratch=Pointer(base,active_rain+104),source=Pointer(base,active_rain+96),indices=Pointer(base,active_rain+108);
  const auto parameter=Pointer(base,0x82d60000u-29036u);
  if(!scratch||!source||!indices||!Valid(base,source,124)||!Valid(base,scratch,scratch_bytes))return;
  const auto saved=ctx;std::array<uint8_t,20> original;std::memcpy(original.data(),base+descriptor,20);
  if(player_surface_owner&&player_surface_owner!=source)pes6_surface_release(ctx,base,player_surface_owner);
  if(!player_surface_staging){
    ctx.r3.u64=surface_batch_vertices*16;sub_826D61B0(ctx,base);player_surface_staging=ctx.r3.u32;
  }
  const auto vertices=player_surface_staging;
  if(!vertices||!Valid(base,vertices,surface_batch_vertices*16)){ctx=saved;return;}
  bool pushed=false;uint32_t renderer=0;unsigned count=0;
  const auto flush=[&](){
    if(!count)return;
    if(!pushed){ctx.r3.u64=0;sub_82193900(ctx,base);pushed=true;renderer=SurfaceRenderer(ctx,base,source,3);}
    if(renderer){
      REX_STORE_U32(descriptor,vertices);REX_STORE_U32(descriptor+4,count/3);REX_STORE_U32(descriptor+8,indices);
      REX_STORE_U32(descriptor+12,count);REX_STORE_U32(descriptor+16,0xffffffffu);
      ctx.r3.u64=renderer;ctx.r4.u64=descriptor;ctx.r5.u64=parameter;sub_82195A80(ctx,base);
      player_surface_diagnostics.vertices+=count;
    }count=0;
  };
  for(bool face:{false,true})for(const auto& instance:player_surface_instances)if(instance.address&&instance.face==face&&instance.view==player_surface_view_epoch&&std::abs(instance.time-effect_time)<.05f)
    for(const auto& mesh:instance.meshes){
      if(count+mesh.count>54000)flush();
      for(unsigned i=0;i<mesh.count;i++){const auto& v=mesh.vertices[i];StoreWeatherVertex(base+vertices+count++*16,v.p,v.color);}
    }
  flush();if(!pushed)return;
  sub_82193A38(ctx,base);std::memcpy(base+descriptor,original.data(),20);ctx=saved;
}
extern "C" void pes6_surface_motion(uint8_t* base,uint32_t controller){
  if(!pes6_weather_enabled()||controller<0x10000||controller>0xdfffffc8u)return;
  struct ObservedController {uint32_t address=0,tick=~0u;unsigned slot=64;};
  static thread_local std::array<ObservedController,64> checked;
  const auto tick=motion_clock.load(std::memory_order_relaxed);
  // Controllers are aligned: the old low-bit hash mapped different players
  // onto the same entry and repeated VirtualQuery on every animation callback.
  const unsigned hash=((controller>>4)^(controller>>10)^(controller>>16))%checked.size();
  unsigned entry=hash;
  for(unsigned probe=0;probe<checked.size();++probe){
    const unsigned candidate=(hash+probe)%checked.size();
    if(!checked[candidate].address||checked[candidate].address==controller){entry=candidate;break;}
  }
  auto& check=checked[entry];
  // Many animation/render callbacks inspect the same controller in one tick.
  // Validate once per address/tick, and still read its current clip each call.
  if(check.address!=controller||check.tick!=tick){
    if(!Valid(base,controller,56))return;
    if(check.address!=controller)check={controller,tick};else check.tick=tick;
  }
  // Match controllers can attach a wrapper, not the render actor itself.
  // Register by skeleton instead of rejecting controller+0's vtable.
  if(!REX_LOAD_U32(controller+4))return;
  const auto skeleton=REX_LOAD_U32(controller+4);
  const auto clip=REX_LOAD_U16(controller+54);
  auto remember=[&](unsigned i){
    motion_last_observed[i].store(motion_observation_clock.fetch_add(1,std::memory_order_relaxed)+1,std::memory_order_relaxed);
    observed_motion_clips[i].store(clip,std::memory_order_relaxed);
    if(TurfKickMotion(clip)||TurfPassMotion(clip)){recent_kick_clips[i].store(clip,std::memory_order_relaxed);recent_kick_ticks[i].store(tick,std::memory_order_relaxed);}
    if(KeeperWaterMotion(clip)){recent_keeper_clips[i].store(clip,std::memory_order_relaxed);recent_keeper_ticks[i].store(tick,std::memory_order_relaxed);}
  };
  // The registry is unchanged for almost every callback. Still reread the
  // current clip so same-tick kick/save transitions remain observable.
  if(check.slot<motion_controllers.size()&&
     motion_controllers[check.slot].load(std::memory_order_relaxed)==controller&&
     motion_skeletons[check.slot].load(std::memory_order_relaxed)==skeleton){
    remember(check.slot);return;
  }
  // Keep the live controller for this skeleton: an old wrapper must not win
  // the first-match lookup after a set-piece controller replaces it.
  bool present=false;
  for(unsigned i=0;i<motion_controllers.size();i++){
    auto value=motion_controllers[i].load(std::memory_order_relaxed);
    if(value==controller){
      if(motion_skeletons[i].load(std::memory_order_relaxed)!=skeleton){recent_kick_clips[i].store(0,std::memory_order_relaxed);recent_keeper_clips[i].store(0,std::memory_order_relaxed);}
      motion_skeletons[i].store(skeleton,std::memory_order_relaxed);remember(i);check.slot=i;present=true;continue;
    }
    // Cached skeleton keys avoid O(players squared) OS memory queries per tick.
    if(value&&motion_skeletons[i].load(std::memory_order_relaxed)==skeleton)
      motion_controllers[i].compare_exchange_strong(value,0,std::memory_order_relaxed);
  }
  if(present)return;
  for(unsigned i=0;i<motion_controllers.size();i++){uint32_t empty=0;
    if(motion_controllers[i].compare_exchange_strong(empty,controller,std::memory_order_relaxed)){
      recent_kick_clips[i].store(0,std::memory_order_relaxed);recent_keeper_clips[i].store(0,std::memory_order_relaxed);
      motion_skeletons[i].store(skeleton,std::memory_order_relaxed);remember(i);check.slot=i;return;
    }
  }
  // Cinematics create new controllers without necessarily destroying the live
  // actor. Recycle the least recently observed slot instead of becoming full
  // forever after several replay/half-time scene changes.
  for(unsigned attempt=0;attempt<motion_controllers.size();++attempt){
    unsigned oldest=0;auto age=motion_last_observed[0].load(std::memory_order_relaxed);
    for(unsigned i=1;i<motion_controllers.size();++i){const auto seen=motion_last_observed[i].load(std::memory_order_relaxed);if(seen<age){oldest=i;age=seen;}}
    auto previous=motion_controllers[oldest].load(std::memory_order_relaxed);
    if(motion_controllers[oldest].compare_exchange_strong(previous,controller,std::memory_order_relaxed)){
      recent_kick_clips[oldest].store(0,std::memory_order_relaxed);recent_keeper_clips[oldest].store(0,std::memory_order_relaxed);
      motion_skeletons[oldest].store(skeleton,std::memory_order_relaxed);remember(oldest);check.slot=oldest;return;
    }
  }
}
namespace {
bool NearSavingKeeper(uint8_t* base,Vec ball,const Vec* previous=nullptr){
  const auto tick=motion_clock.load(std::memory_order_relaxed);
  for(unsigned i=0;i<motion_controllers.size();i++){
    const auto controller=motion_controllers[i].load(std::memory_order_relaxed);
    const auto observed=observed_motion_clips[i].load(std::memory_order_relaxed);
    const bool recently_saving=tick-recent_keeper_ticks[i].load(std::memory_order_relaxed)<=12&&KeeperWaterMotion(recent_keeper_clips[i].load(std::memory_order_relaxed));
    if(!KeeperWaterMotion(observed)&&!recently_saving)continue;
    if(!controller||!Valid(base,controller,56))continue;
    const auto clip=REX_LOAD_U16(controller+54);const bool current=KeeperWaterMotion(clip);
    if(!current&&(TurfKickMotion(clip)||TurfPassMotion(clip)||TurfAirKickMotion(clip)))continue;
    if(!current&&clip>14&&(clip<306||clip>637))continue;
    const bool recent=tick-recent_keeper_ticks[i].load(std::memory_order_relaxed)<=12&&KeeperWaterMotion(recent_keeper_clips[i].load(std::memory_order_relaxed));
    if(!current&&!recent)continue;
    const auto skeleton=Pointer(base,controller+4),nodes=Pointer(base,skeleton+196),pelvis=Pointer(base,nodes);
    if(!Valid(base,pelvis+144,12))continue;
    const auto p=Position(base,pelvis+144);
    if(std::hypot(p.x-ball.x,p.z-ball.z)>=3.2f||std::abs(p.y-ball.y)>=2.5f)continue;
    if(!previous)return true;
    // The palette's traversal order varies by player. A saving pose plus a
    // swept ball touching a real body node catches fast glove deflections
    // even when no large velocity change survives the end-of-tick sample.
    if(!current||ball.y<.18f||!Valid(base,nodes,50*4))continue;
    const Vec delta{ball.x-previous->x,ball.y-previous->y,ball.z-previous->z};
    const float length2=delta.x*delta.x+delta.y*delta.y+delta.z*delta.z;
    if(length2>4.f)continue;
    for(unsigned bone=1;bone<50;bone++){
      const auto node=REX_LOAD_U32(nodes+bone*4);if(!Valid(base,node+144,12))continue;
      const auto point=Position(base,node+144);
      const float t=length2>1e-8f?std::clamp(((point.x-previous->x)*delta.x+(point.y-previous->y)*delta.y+(point.z-previous->z)*delta.z)/length2,0.f,1.f):0;
      const float x=point.x-previous->x-delta.x*t,y=point.y-previous->y-delta.y*t,z=point.z-previous->z-delta.z*t;
      if(x*x+y*y+z*z<.24f*.24f)return true;
    }
  }
  return false;
}
void ProcessWaterContacts(uint8_t* base,Vec p,float dt){
  if(!wet_frame){water_contacts={};pending_keeper_contact={};pending_water_count=0;collision_cooldowns={};return;}
  for(auto& cooldown:collision_cooldowns)cooldown=std::max(0.f,cooldown-dt);
  net_shower_cooldown=std::max(0.f,net_shower_cooldown-dt);
  bool object_hit=false;
  for(unsigned i=0;i<pending_water_count;i++){
    const auto& contact=pending_water_contacts[i];const unsigned kind=contact.kind;
    if(kind>1)continue;object_hit=true;
    if(collision_cooldowns[kind]>0)continue;
    collision_cooldowns[kind]=.12f;
    Vec origin=contact.position,normal{};if(kind==0)PostWaterSurface(contact.position,origin,normal);
    EmitEvent(base,{kind==0?EventKind::RadialContact:EventKind::WaterContact,origin,normal,1,kind==0?48u:32u});
    if(kind==0){
      // A post strike shakes a few retained drops from the upper frame too.
      const Vec frame{contact.position.x>=0?52.5f:-52.5f,2.42f,std::clamp(contact.position.z,-3.55f,3.55f)};
      EmitEvent(base,{EventKind::FrameDrip,frame,{},0,8});
    }
  }
  pending_water_count=0;
  const auto previous=water_contacts.previous;const bool had_previous=water_contacts.ready;
  const auto sample=water_contacts.Sample(p,dt);
  const auto tick=motion_clock.load(std::memory_order_relaxed);
  if(object_hit)pending_keeper_contact={};
  else if(sample.keeper_impulse)pending_keeper_contact={true,sample.impact,sample.incoming,tick};
  if(pending_keeper_contact.active&&tick-pending_keeper_contact.tick>6)pending_keeper_contact={};
  if(pending_keeper_contact.active&&!object_hit&&NearSavingKeeper(base,pending_keeper_contact.position)){
    const auto contact=pending_keeper_contact;pending_keeper_contact={};
    water_contacts.keeper_cooldown=.22f;
    EmitEvent(base,{EventKind::RadialContact,contact.position,{-contact.incoming.x,-contact.incoming.y,-contact.incoming.z},1,48});
  }else if(!object_hit&&had_previous&&water_contacts.keeper_cooldown==0&&
      std::hypot(water_contacts.velocity.x,water_contacts.velocity.y,water_contacts.velocity.z)>1.f&&
      NearSavingKeeper(base,p,&previous)){
    pending_keeper_contact={};water_contacts.keeper_cooldown=.22f;
    const auto v=water_contacts.velocity;
    EmitEvent(base,{EventKind::RadialContact,p,{-v.x,-v.y,-v.z},1,48});
  }
  if(sample.goal_net&&net_shower_cooldown<=0){
    EmitEvent(base,{EventKind::NetShower,p,{},0,net_base_drops});net_shower_cooldown=.65f;
  }
}
unsigned TurfKickAllowed(uint8_t* base,Vec origin,Vec launch,bool* air){
  float nearest=2.4f*2.4f,nearest_air=nearest;unsigned result=0;
  const auto tick=motion_clock.load(std::memory_order_relaxed);
  // Animation controllers remain available even when an actor has not been
  // registered by a draw/LOD pass. Ground contacts must not depend on visibility.
  for(unsigned i=0;i<motion_controllers.size();i++){
    const auto controller=motion_controllers[i].load(std::memory_order_relaxed);
    if(!controller||!Valid(base,controller,56))continue;
    const auto skel=Pointer(base,controller+4),nodes=Pointer(base,skel+196),pelvis=Pointer(base,nodes);
    if(!Valid(base,pelvis,160))continue;
    const auto p=Position(base,pelvis+144);
    const float d=(p.x-origin.x)*(p.x-origin.x)+(p.z-origin.z)*(p.z-origin.z);
    if(!std::isfinite(d)||d>=2.4f*2.4f)continue;
      auto clip=uint32_t(REX_LOAD_U16(controller+54));
      if(TurfAirKickMotion(clip)){nearest_air=std::min(nearest_air,d);continue;}
      if(clip<=14&&tick-recent_kick_ticks[i].load(std::memory_order_relaxed)<=4){
        const auto previous=recent_kick_clips[i].load(std::memory_order_relaxed);
        if(TurfKickMotion(previous)||TurfPassMotion(previous))clip=previous;
      }
      const unsigned count=TurfPassMotion(clip)?(TurfLongPassMotion(clip)||launch.y>1.5f?50:30):TurfKickMotion(clip)?50:0;
      if(count&&d<nearest){nearest=d;result=count;}
  }
  if(nearest_air<2.4f*2.4f&&nearest_air<=nearest){if(air)*air=true;return 0;}
  // Unknown clips, receptions, headers and dribbling still cannot emit turf.
  return result;
}
}
// Called at the native post/board event, before sound filtering or SE volume.
// Queue only the position; emit at the weather tick into the replay frame ring.
extern "C" void pes6_weather_object_contact(uint8_t* base,unsigned kind){
  if(!pes6_rain_enabled()||kind>1||!Wet(base)||!Valid(base,0x8313A93Eu,1)||base[0x8313A93Eu])return;
  const auto game=Pointer(base,0x8313A948u),ball=Pointer(base,game+2520);
  if(!Valid(base,ball+144,12))return;
  const auto p=Position(base,ball+144,1.f/256,-1);
  if(!WaterWorld(p))return;
  std::lock_guard lock(weather_mutex);
  if(pending_water_count<pending_water_contacts.size())pending_water_contacts[pending_water_count++]={p,kind};
}
// Same contact that triggers the net sound. This also covers net hits whose
// initial goal-line crossing was not sampled. Visuals do not depend on SE volume.
extern "C" void pes6_weather_net_hit(uint8_t* base,float x,float y,float z){
  if(!pes6_rain_enabled()||!Wet(base)||!Valid(base,0x8313A93Eu,1)||base[0x8313A93Eu])return;
  const Vec p{x,y,z};if(!WaterWorld(p)||!pes6::net_sound::Contact::Surface({x,y,z}))return;
  std::lock_guard lock(weather_mutex);
  if(net_shower_cooldown>0)return;
  wet_frame=true;EmitEvent(base,{EventKind::NetShower,p,{},0,net_base_drops});net_shower_cooldown=.65f;
}
