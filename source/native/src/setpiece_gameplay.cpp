#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "setpiece_gameplay.h"
#include "converted_motion.h"
#include "weather_memory.h"
#if __has_include(<rex/system/kernel_state.h>)
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#define PES6_SETPIECE_GUEST_PAGE_TABLE 1
#endif
#include <windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cmath>
#include <map>
#include <mutex>
#include <array>
#include <sstream>
#include <tuple>
namespace {
std::atomic<uint32_t> fk_file{0},pk_file{0},pkfk_file{0};
std::array<std::atomic<uint32_t>,10> extra_files{};
std::array<std::vector<uint8_t>,10> extra_bundles;
std::array<std::atomic<int>,4210> configured_fk{},configured_pk{};
std::array<unsigned,4210> openplay_percent{};
std::atomic<uint64_t> openplay_sequence{0};
thread_local unsigned openplay_player=0;
std::atomic<bool> openplay_trial{false};
std::array<std::vector<uint8_t>,13> openplay_bodies;
std::array<unsigned,13> openplay_contacts{};
std::map<uint32_t,uint32_t> cr7_chop_until;
thread_local uint32_t openplay_owner=0;
thread_local bool openplay_playback=false;
thread_local int openplay_recorded_styles[2]={-1,-1};
thread_local int openplay_style_override=-1;
std::mutex openplay_mutex;
std::map<std::tuple<uintptr_t,uint32_t,uint32_t,uint32_t,unsigned>,uint32_t> openplay_motions;
std::map<uint32_t,uint32_t> openplay_original_motions;
std::map<uint32_t,unsigned> openplay_motion_styles;
std::map<uint32_t,bool> openplay_live_decisions;
std::atomic<bool> replay_probe_enabled{false};
std::ofstream replay_probe_file;
std::mutex replay_probe_mutex;
unsigned replay_probe_rows=0;
std::array<unsigned,16> replay_probe_other{};
std::atomic<unsigned> selected{0}; // Native bank4 is serialized by the engine.
std::filesystem::path log_path;
std::vector<uint8_t> fk_bundle,pk_bundle,pkfk_bundle;
std::map<uint32_t,bool> initial_placed;
std::mutex initial_placement_mutex;
thread_local std::map<uint32_t,unsigned> placement_depth;
uint32_t BE(const std::vector<uint8_t>& b,size_t at){return uint32_t(b[at])<<24|uint32_t(b[at+1])<<16|uint32_t(b[at+2])<<8|b[at+3];}
bool ReadBundle(const std::filesystem::path& file,std::vector<uint8_t>& bytes,unsigned leaves){
  std::ifstream input(file,std::ios::binary|std::ios::ate);
  if(!input||input.tellg()<64||input.tellg()>4000000)return false;
  bytes.resize(size_t(input.tellg()));input.seekg(0);
  if(!input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))return false;
  if(BE(bytes,0)!=1||BE(bytes,4)!=8)return false;
  const uint64_t off=BE(bytes,8),size=BE(bytes,12);
  if(off<20||size<24||off+size>bytes.size())return false;
  if(bytes[off+2]!=0||std::memcmp(bytes.data()+off+3,"WESYS",5))return false;
  const auto inner=off+16;
  if(BE(bytes,inner)!=leaves||BE(bytes,inner+4)!=8||size<16+8+leaves*12)return false;
  for(unsigned i=0;i<leaves;i++){
    const uint64_t at=BE(bytes,inner+8+i*12),length=BE(bytes,inner+12+i*12);
    if(at<8+leaves*12||length<64||at+length>size-16)return false;
    if(bytes[inner+at+40]!=0xcc||bytes[inner+at+41]!=0xcc)return false;
  }
  return true;
}
bool Valid(uint8_t* base,uint32_t at,size_t n){
  if(at<0x10000||uint64_t(at)+n>=0xe0000000ull)return false;
#ifdef PES6_SETPIECE_GUEST_PAGE_TABLE
  if(auto* kernel=rex::runtime::current_kernel_state()){
    if(auto* memory=kernel->memory()){
      rex::memory::HeapAllocationInfo info{};
      return pes6::weather::ReadableGuestRange(*memory,base,at,n,info);
    }
  }
#endif
  for(const auto* p=base+at;n;){MEMORY_BASIC_INFORMATION info{};if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;const auto take=std::min(n,size_t(static_cast<const uint8_t*>(info.BaseAddress)+info.RegionSize-p));if(!take)return false;p+=take;n-=take;}return true;
}
bool Live(uint8_t*,uint32_t at,size_t n){return at>=0x10000&&uint64_t(at)+n<0xe0000000ull;}
uint32_t U(uint8_t* base,uint32_t at){return REX_LOAD_U32(at);}
float Float(uint8_t* base,uint32_t at){const auto bits=U(base,at);float value;std::memcpy(&value,&bits,4);return value;}
void StoreFloat(uint8_t* base,uint32_t at,float value){uint32_t bits;std::memcpy(&bits,&value,4);REX_STORE_U32(at,bits);}
void Log(const std::string&){}
uint32_t PenaltyFile(unsigned style){if(style==5)return pk_file;if(style==6)return pkfk_file;if(style>=7&&style<=16)return extra_files[style-7];return 0;}
bool ImportedPenaltyBank(uint32_t file){if(!file)return false;for(unsigned style=5;style<=16;style++)if(PenaltyFile(style)==file)return true;return false;}
unsigned PlayerPenalty(uint8_t* base,uint32_t player,uint32_t stats){const auto cache=U(base,player+248);return cache>=5&&cache<=16?cache:unsigned(base[stats]&7);}
int ConfiguredStyle(uint8_t* base,uint32_t player,unsigned which){
  // Live penalty report identifies Rooney as BE16(actor+132)=160. The old
  // roster+114 offset points into a vtable, not a player ID (often 0xea60).
  // Native 8223B630 and 825A5B14 read this same actor field as player identity.
  const unsigned id=(unsigned(base[player+132])<<8)|base[player+133];
  if(!id||id>=configured_pk.size())return -1;
  return (which?configured_pk[id]:configured_fk[id]).load(std::memory_order_relaxed)-1;
}
uint32_t SelectedFile(unsigned kind){return kind==1?fk_file.load():kind>=2?PenaltyFile(kind+3):0;}
uint32_t ActualBank(uint8_t* base){
  if(!Valid(base,0x83139354u,4))return 0;const auto manager=U(base,0x83139354u);if(!Valid(base,manager,4))return 0;
  const auto table=U(base,manager);if(!Valid(base,table,12))return 0;const auto entries=U(base,table+4),end=U(base,table+8);if(end<entries||end-entries<20||!Valid(base,entries+16,4))return 0;
  const auto bank=U(base,entries+16);return Valid(base,bank,412)&&base[bank+408]?U(base,bank+8):0;
}
}
namespace pes6::editor {
void InitializeSetPieceGameplay(const std::string& root){
  openplay_percent.fill(100);openplay_sequence=0;
  {std::ifstream frequencies(std::filesystem::path(root)/"data/editor/openplay-frequency.txt");std::string signature;std::getline(frequencies,signature);if(signature=="PES6_OPENPLAY_FREQUENCY_V1"){unsigned id,percent;while(frequencies>>id>>percent)if(id>0&&id<4210&&percent<=100)openplay_percent[id]=percent;}}
  replay_probe_enabled=false;replay_probe_file.close();replay_probe_rows=0;replay_probe_other.fill(0);
  const auto probeRequest=std::filesystem::path(root)/"data/editor/probe-manual-replay-once.txt";
  if(std::filesystem::exists(probeRequest)){
   replay_probe_file.open(std::filesystem::path(root)/"data/editor/manual-replay-probe.csv",std::ios::trunc);
   if(replay_probe_file){replay_probe_file<<"stage,lr,object,payload,replay,mode,owner,prevId,currentId,packetPrev,packetCurrent,prevMotion,currentMotion,prevRaw,currentRaw,prevStyle,currentStyle,clockPrevBits,clockCurrentBits,recordedPrevStyle,recordedCurrentStyle,ctr,arg3,arg4\n";replay_probe_file.flush();replay_probe_enabled=true;std::error_code ec;std::filesystem::remove(probeRequest,ec);}
  }

  openplay_trial=false;for(auto& body:openplay_bodies)body.clear();openplay_contacts.fill(0);openplay_motions.clear();openplay_original_motions.clear();openplay_motion_styles.clear();openplay_live_decisions.clear();
  {const auto folder=std::filesystem::path(root)/"data/editor";std::ifstream setting(folder/"openplay-shot-trial.txt");std::string signature;
   if((setting>>signature)&&signature=="PES6_OPENPLAY_ASSIGNED_FK_100_V1"){
    std::ifstream manifest(folder/"motion-recipes/openplay-shot-styles.txt");std::string version,name;unsigned style=0,contact=0,frames=0;unsigned ready=0;
    if((manifest>>version)&&version=="PES6_OPENPLAY_FK_STYLES_V1")while(manifest>>style>>name>>contact>>frames){
     if(style>=11||name!="free-kick-"+std::to_string(style+1)+"-openplay.motion"||!contact||contact>=frames||frames>180)continue;
     std::ifstream body(folder/"motion-recipes"/name,std::ios::binary|std::ios::ate);if(!body||body.tellg()<208||body.tellg()>1024*1024)continue;
     auto& bytes=openplay_bodies[style];bytes.resize(size_t(body.tellg()));body.seekg(0);body.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
     if(body&&BE(bytes,0)==0xff010001u&&BE(bytes,4)==(frames<<16|0x5036u)&&BE(bytes,8)==120&&BE(bytes,96)==0xff010002u){openplay_contacts[style]=contact;++ready;}else bytes.clear();
    }openplay_trial=ready==11;
   }}
  cr7_chop_until.clear();
  for(unsigned i=11;i<13;i++){
   const auto name=i==11?"cr7-chop-368.motion":"cr7-stepovers-817.motion";
   std::ifstream in(std::filesystem::path(root)/"data/editor/motion-recipes"/name,std::ios::binary|std::ios::ate);
   if(!in||in.tellg()<208||in.tellg()>1024*1024)continue;
   auto& bytes=openplay_bodies[i];bytes.resize(size_t(in.tellg()));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
   if(!in||BE(bytes,0)!=0xff010001u||(BE(bytes,4)&65535)!=0x5036||BE(bytes,8)!=120||BE(bytes,96)!=0xff010002u)bytes.clear();
  }
  for(auto& v:configured_fk)v=0;for(auto& v:configured_pk)v=0;
  for(auto& id:extra_files)id=0;for(auto& bytes:extra_bundles)bytes.clear();
  {std::lock_guard lock(initial_placement_mutex);initial_placed.clear();}
  fk_file=0;pk_file=0;pkfk_file=0;pkfk_bundle.clear();selected=0;fk_bundle.clear();pk_bundle.clear();log_path=std::filesystem::path(root)/"data/editor/setpiece-gameplay.log";
  std::ifstream manifest(std::filesystem::path(root)/"data/editor/motion-recipes/gameplay-banks.txt");std::string signature;uint32_t fk=0,pk=0,pkfk=0;
  if(!(manifest>>signature>>fk>>pk)||(signature!="PES6_SETPIECE_GAMEPLAY_V1"&&signature!="PES6_SETPIECE_GAMEPLAY_V2")||fk==pk||!fk||!pk)return;
  if(signature=="PES6_SETPIECE_GAMEPLAY_V2"&&(!(manifest>>pkfk)||!pkfk||pkfk==fk||pkfk==pk))return;
  std::ifstream archive(std::filesystem::path(root)/"game/cv.img",std::ios::binary|std::ios::ate);const auto size=archive.tellg();archive.seekg(0);uint8_t header[8]{};if(!archive.read(reinterpret_cast<char*>(header),8)||std::memcmp(header,"AFS\0",4))return;
  const uint32_t count=uint32_t(header[4])|(uint32_t(header[5])<<8)|(uint32_t(header[6])<<16)|(uint32_t(header[7])<<24);
  for(const auto id:{fk,pk,pkfk}){if(!id)continue;if(id>=count)return;archive.seekg(8+uint64_t(id)*8);uint32_t entry[2]{};if(!archive.read(reinterpret_cast<char*>(entry),8)||uint64_t(entry[0])+entry[1]>uint64_t(size)||entry[1]<1024)return;}
  const auto folder=std::filesystem::path(root)/"data/editor/motion-recipes";
  if(!ReadBundle(folder/"cr7-fk11.bundle",fk_bundle,17)||!ReadBundle(folder/"cr7-pk6.bundle",pk_bundle,11)){
    fk_bundle.clear();pk_bundle.clear();Log("Imported set pieces disabled: missing or invalid direct bundles");return;
  }
  if(pkfk&&ReadBundle(folder/"cr7-fk11-penalty.bundle",pkfk_bundle,11))pkfk_file=pkfk;
  std::ifstream extras(folder/"extended-penalty-banks.txt");std::string sig,line;
  if(std::getline(extras,sig)&&sig=="PES6_EXTENDED_PENALTIES_V1")while(std::getline(extras,line)){
    std::istringstream row(line);unsigned style=0,frames=0,contact=0;uint32_t id=0;std::string name;
    if(!(row>>style>>id>>name>>frames>>contact)||style<7||style>16||id>=count||id==fk||id==pk||id==pkfk||frames<=contact||contact<8||name!="fk"+std::to_string(style-6)+"-penalty.bundle")continue;
    archive.clear();archive.seekg(8+uint64_t(id)*8);uint32_t entry[2]{};if(!archive.read(reinterpret_cast<char*>(entry),8)||uint64_t(entry[0])+entry[1]>uint64_t(size)||entry[1]<1024)continue;
    if(ReadBundle(folder/name,extra_bundles[style-7],11))extra_files[style-7]=id;
  }
  fk_file=fk;pk_file=pk;Log("Imported set pieces enabled, direct resource route: FK11="+std::to_string(fk)+" PK6="+std::to_string(pk));
}
extern "C" bool pes6_setpiece_resource(PPCContext& ctx,uint8_t* base){
  const auto request=ctx.r3.u32;
  if(!Valid(base,request,122))return false;
  const auto id=U(base,request+8);
  const std::vector<uint8_t>* bytes=nullptr;
  if(fk_file&&id==fk_file)bytes=&fk_bundle;
  else if(pk_file&&id==pk_file)bytes=&pk_bundle;
  else if(pkfk_file&&id==pkfk_file)bytes=&pkfk_bundle;
  if(!bytes)for(unsigned i=0;i<10;i++)if(extra_files[i]&&id==extra_files[i]){bytes=&extra_bundles[i];break;}
  if(!bytes||base[request+121]==1)return false;
  // Added AFS resources cannot use the original async compressed input path.
  // Publish an independently allocated native bundle, exactly as expanded boots.
  PPCContext alloc=ctx;alloc.r3.u64=bytes->size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);
  const auto output=alloc.r3.u32;
  if(!output){Log("Imported setpiece direct allocation failed");ctx.r3.u64=0;return true;}
  std::memcpy(base+output,bytes->data(),bytes->size());
  const auto previous=U(base,request+16),input=ctx.r4.u32;
  if(base[request+24]&&input){PPCContext release=ctx;release.r3.u64=input;sub_82A00AB0(release,base);}
  REX_STORE_U32(request+16,output);REX_STORE_U32(request+20,uint32_t(bytes->size()));
  if(previous&&previous!=output&&previous!=input){PPCContext release=ctx;release.r3.u64=previous;sub_82A00AB0(release,base);}
  Log("Imported setpiece direct bundle published: id="+std::to_string(id)+" bytes="+std::to_string(bytes->size()));
  ctx.r3.u64=1;return true;
}
void RegisterPlayerSetPieces(unsigned id,int fk,int pk){if(id&&id<4210){configured_fk[id].store(fk+1,std::memory_order_relaxed);configured_pk[id].store(pk+1,std::memory_order_relaxed);}}
void ClearExtendedPenalties(){}
bool SetPiecePenaltyReady(unsigned style){return style<=4||(style<=16&&PenaltyFile(style)!=0);}
bool SetPieceFKPenaltyReady(){return pkfk_file.load()!=0;}
bool SetPieceGameplayReady(){return fk_file.load()!=0&&pk_file.load()!=0;}
}
PES6SetPieceStyleAlias::PES6SetPieceStyleAlias(uint8_t* base,uint32_t value,bool wrapped,bool select_bank):base_(base){
  if(!pes6::editor::SetPieceGameplayReady())return;
  uint32_t player=value;if(wrapped){if(!Live(base,value,4))return;const auto ref=U(base,value);if(!Live(base,ref,4))return;player=U(base,ref);}
  if(!Live(base,player,632)||!Live(base,0x8313a948u,4))return;const auto match=U(base,0x8313a948u);if(!Live(base,match,457))return;
  const unsigned mode=base[match+456];if(mode!=1&&mode!=4&&mode!=8)return;
  const auto stats=U(base,player+628);if(!Live(base,stats,4))return;const int configured=ConfiguredStyle(base,player,mode==1?0:1);const auto cached=U(base,player+248);const auto sourcePenalty=configured>=0?unsigned(configured):PlayerPenalty(base,player,stats);const unsigned penalty=configured>=0?unsigned(configured):(sourcePenalty!=(base[stats]&7)?sourcePenalty:(cached>=5&&cached<=16?cached:sourcePenalty));const unsigned fk=configured>=0?unsigned(configured):((base[stats]>>3)&15);const unsigned kind=mode==1?(fk==10?1:0):(PenaltyFile(penalty)?penalty-3:0);
  if(select_bank){const auto previous=selected.exchange(kind);if(previous!=kind)Log("Selected imported bank kind="+std::to_string(kind));}
  if(configured>=0)REX_STORE_U32(player+248,unsigned(configured));
}
PES6SetPieceStyleAlias::~PES6SetPieceStyleAlias(){}
extern "C" uint32_t pes6_setpiece_dispatch_style(uint8_t* base,uint32_t style){
  // Expanded native style table. The actor retains the full index permanently;
  // only action dispatch reuses a compatible engine route for each new bank.
  static constexpr unsigned penalty[17]{0,1,2,3,4,1,1,1,1,1,1,1,1,1,1,1,1};
  static constexpr unsigned freeKick[11]{0,1,2,3,4,5,6,7,8,9,0};
  const auto match=U(base,0x8313a948u);if(!Live(base,match,457))return style;
  const auto mode=base[match+456];if(mode==1&&style<11)return freeKick[style];if((mode==4||mode==8)&&style<17)return penalty[style];return style;
}
extern "C" uint32_t pes6_setpiece_load_file(uint32_t bank,uint32_t file){const auto kind=selected.load(),id=SelectedFile(kind);return bank==4&&id&&file==(kind==1?2509u:3011u)?id:file;}
extern "C" uint32_t pes6_setpiece_lookup_file(uint32_t file){if(fk_file&&file==fk_file)return 2509;return ImportedPenaltyBank(file)?3011:file;}
extern "C" uint32_t pes6_setpiece_query_file(uint32_t bank,uint32_t file){if(bank!=4)return file;const auto kind=selected.load(),id=SelectedFile(kind),native=kind==1?2509u:3011u;if(id&&file==id)return native;if(id&&file==native)return id;return file;}
extern "C" uint32_t pes6_setpiece_penalty_shot(uint8_t* base,uint32_t clip){return ImportedPenaltyBank(ActualBank(base))&&(clip==238||clip==247||clip==248)?1740:clip;}
extern "C" uint32_t pes6_setpiece_reverse_clip(uint8_t* base,uint32_t clip){return clip==1740&&ImportedPenaltyBank(ActualBank(base))?238:clip;}
extern "C" uint32_t pes6_setpiece_native_style(uint8_t* base,uint32_t player,uint32_t style){
  if(!pes6::editor::SetPieceGameplayReady()||!Live(base,player,632)||!Live(base,0x8313a948u,4))return style;
  const auto match=U(base,0x8313a948u),stats=U(base,player+628);if(!Live(base,match,457)||!Live(base,stats,4))return style;
  const unsigned mode=base[match+456];const int configured=ConfiguredStyle(base,player,mode==1?0:1);
  if(mode==1)return configured>=0?unsigned(configured):((base[stats]>>3)&15);
  if(mode==4||mode==8)return configured>=0?unsigned(configured):PlayerPenalty(base,player,stats);return style;
}
extern "C" uint32_t pes6_converted_sampler_frames(uint8_t* base,uint32_t header,uint32_t frames){
  // Called after the guest sampler has already read header+4. Native curves
  // must not pay for VirtualQuery for every channel of every player.
  if(header<0x10000||header>0xdffffff0u||base[header+6]!=0x50||base[header+7]!=0x36)return frames;
  return pes6::editor::ConvertedSamplerFrames(frames,uint16_t(U(base,header+4)),U(base,header+8),U(base,header+12));
}
extern "C" void pes6_penalty_initial_placement(uint8_t*,uint32_t){}
PES6PenaltyPlacementScope::PES6PenaltyPlacementScope(uint8_t* base,uint32_t player):base_(base),player_(player){}
PES6PenaltyPlacementScope::~PES6PenaltyPlacementScope(){}
extern "C" bool pes6_penalty_hold_finish(uint8_t* base,uint32_t object,uint32_t requested,bool wrapper){
  if(!pes6::editor::SetPieceGameplayReady()||requested==1734||requested==1735||!Live(base,object,160)||!Live(base,0x8313a948u,4))return false;
  const auto match=U(base,0x8313a948u);if(!Live(base,match,457)||(base[match+456]!=4&&base[match+456]!=8))return false;
  // Replay restores its own recorded sequence; this guard is live gameplay.
  if(!Live(base,0x83139895u,1)||base[0x83139895u])return false;
  const auto controller=wrapper?U(base,object+84):object;
  if(!Live(base,controller,160))return false;const auto clip=U(base,controller+16);
  if(!Live(base,clip,28)||(U(base,clip+16)&128))return false;
  const auto motion=U(base,clip+4);if(!Live(base,motion,12))return false;
  const auto curve=U(base,motion+4),raw=U(base,motion+8);
  if(!Live(base,curve,28)||!Live(base,raw,64))return false;
  const auto header=U(base,curve+24);if(!Live(base,header,16)||U(base,header)!=0xff010001u||(U(base,header+4)&65535u)!=0x5036u)return false;
  const auto frames=U(base,header+4)>>16,contact=U(base,raw+36)>>16;
  if(!frames||contact<8||contact>=frames)return false;
  const float clock=Float(base,clip+12);
  const bool hold=std::isfinite(clock)&&clock>=float(contact)/30.f&&clock+1e-5f<float(frames)/30.f;
  if(hold){static thread_local uint32_t lastObject=0,lastRequested=0;if(lastObject!=object||lastRequested!=requested){Log("PK6 preserve original follow-through: target="+std::to_string(requested)+" clock="+std::to_string(clock));lastObject=object;lastRequested=requested;}}
  return hold;
}

namespace {
void Put16(std::vector<uint8_t>& b,size_t at,unsigned value){b[at]=uint8_t(value>>8);b[at+1]=uint8_t(value);}
void Put32(std::vector<uint8_t>& b,size_t at,uint32_t value){for(unsigned i=0;i<4;i++)b[at+i]=uint8_t(value>>(24-8*i));}
unsigned Half(const std::vector<uint8_t>& b,size_t at){return unsigned(b[at])<<8|b[at+1];}
unsigned Align16(unsigned n){return (n+15u)&~15u;}
bool OpenPlayShotClip(unsigned clip){return (clip>=235&&clip<=263)||(clip>=1634&&clip<=1660);}
bool CR7StepClip(unsigned clip){return clip==100||clip==101||clip==102||clip==617;}
bool CR7ChopClip(unsigned clip){return clip==98||clip==99;}
bool RecordedAssignedClip(unsigned clip){return OpenPlayShotClip(clip)||CR7StepClip(clip)||CR7ChopClip(clip);}
int StyleFromActor(uint8_t* base,uint32_t actor,uint32_t controller){
 if(!Live(base,actor,632))return -1;const auto wrapper=U(base,actor+624);if(!Live(base,wrapper,88)||U(base,wrapper+84)!=controller)return -1;
 const unsigned id=(unsigned(base[actor+132])<<8)|base[actor+133];if(!id||id>=4210)return -1;
 const int configured=configured_fk[id].load(std::memory_order_relaxed)-1;const auto stats=U(base,actor+628);
 const int style=configured>=0?configured:(Live(base,stats,1)?int((base[stats]>>3)&15):-1);if(style>=0&&style<11){openplay_player=id;return style;}return -1;
}
int AssignedOpenPlayStyle(uint8_t* base,uint32_t match,uint32_t controller){
 if(openplay_owner){const int style=StyleFromActor(base,openplay_owner,controller);if(style>=0)return style;}
 for(unsigned i=0;i<23;i++){const auto actor=U(base,match+2560+i*12);const int style=StyleFromActor(base,actor,controller);if(style>=0)return style;}return -1;
}
}
extern "C" uint32_t pes6_openplay_motion(PPCContext& ctx,uint8_t* base,uint32_t controller,uint32_t clip,uint32_t original){
 if(!openplay_trial.load(std::memory_order_relaxed))return original;
 if(!RecordedAssignedClip(clip)){if(!openplay_playback){std::lock_guard lock(openplay_mutex);openplay_live_decisions.erase(controller);}return original;}
 const auto match=U(base,0x8313a948u);if(!Live(base,match,2840)||(!openplay_playback&&base[match+456]!=0)||!Live(base,original,12))return original;
 int style=openplay_style_override;
 if(style<0){openplay_player=0;const int assigned=AssignedOpenPlayStyle(base,match,controller);
  if(OpenPlayShotClip(clip))style=assigned;
  else if(openplay_player==446){if(CR7StepClip(clip))style=12;else{const auto until=cr7_chop_until.find(controller);if(until!=cr7_chop_until.end()&&int32_t(until->second-U(base,match+2152))>=0)style=11;}}
 }
 // CR7 test uses the complete PES6 stepover; speed is set at its native start.
 if(style==12)return original;
 if(style<0||style>=13)return original;const auto& openplay_body=openplay_bodies[unsigned(style)];if(openplay_body.empty())return original;
 std::lock_guard lock(openplay_mutex);
 if(!openplay_playback&&style<11){
  if(openplay_original_motions.count(original))return original;
  auto decision=openplay_live_decisions.find(controller);
  if(decision==openplay_live_decisions.end()){
   const unsigned percent=openplay_percent[openplay_player];bool chosen=percent==100;
   if(percent>0&&percent<100){uint64_t random=openplay_sequence.fetch_add(1,std::memory_order_relaxed)+0x9e3779b97f4a7c15ull+uint64_t(openplay_player)*0x100000001b3ull;random=(random^(random>>30))*0xbf58476d1ce4e5b9ull;random=(random^(random>>27))*0x94d049bb133111ebull;random^=random>>31;chosen=random%100<percent;}
   decision=openplay_live_decisions.emplace(controller,chosen).first;
  }
  if(!decision->second)return original;
 }
 if(const auto previous=openplay_original_motions.find(original);previous!=openplay_original_motions.end())original=previous->second;
 const auto sourceRaw=U(base,original+8);if(!Live(base,sourceRaw,64))return original;
 const unsigned bodyOffset=(unsigned(base[sourceRaw+52])<<8)|base[sourceRaw+53],contact=(unsigned(base[sourceRaw+36])<<8)|base[sourceRaw+37];
 if(bodyOffset<64||bodyOffset>16384||!Live(base,sourceRaw+bodyOffset,16))return original;
 const unsigned frames=(unsigned(base[sourceRaw+bodyOffset+4])<<8)|base[sourceRaw+bodyOffset+5];
 if((style<11&&(!contact||contact>=frames))||!frames||frames>180)return original;
 const auto key=std::make_tuple(uintptr_t(base),original,sourceRaw,clip,unsigned(style));
 if(const auto cached=openplay_motions.find(key);cached!=openplay_motions.end())return cached->second;
 const unsigned count=frames*4+1,table=Align16(8+24*4);std::vector<uint8_t> chunks(table);Put32(chunks,0,0xff010002u);
 for(unsigned channel=0;channel<23;channel++){
  const unsigned sourceChannel=96+BE(openplay_body,104+4*channel);if(sourceChannel+16>openplay_body.size())return original;
  const unsigned keys=Half(openplay_body,sourceChannel+4),dataAt=BE(openplay_body,sourceChannel+12),data=Align16(16+count*2),size=Align16(data+count*16),at=unsigned(chunks.size());
  if(!keys||uint64_t(sourceChannel)+dataAt+uint64_t(keys)*16>openplay_body.size())return original;
  chunks.resize(at+size);Put32(chunks,8+4*channel,at);Put16(chunks,at,Half(openplay_body,sourceChannel));Put16(chunks,at+4,count);chunks[at+6]=openplay_body[sourceChannel+6];Put32(chunks,at+8,16);Put32(chunks,at+12,data);
  for(unsigned sample=0;sample<count;sample++){const int sourceKey=std::clamp(int(sample)+(style<11?(int(openplay_contacts[unsigned(style)])-int(contact))*4:0),0,int(keys)-1);Put16(chunks,at+16+sample*2,sample);std::memcpy(chunks.data()+at+data+sample*16,openplay_body.data()+sourceChannel+dataAt+unsigned(sourceKey)*16,16);}
 }
 std::vector<uint8_t> bytes(bodyOffset+96+chunks.size());std::memcpy(bytes.data(),base+sourceRaw,bodyOffset);std::memcpy(bytes.data()+bodyOffset,openplay_body.data(),96);Put16(bytes,bodyOffset+4,frames);std::memcpy(bytes.data()+bodyOffset+96,chunks.data(),chunks.size());
 PPCContext allocation=ctx;allocation.r3.u64=bytes.size();allocation.r4.u64=16;sub_82A00AC8(allocation,base);const auto raw=allocation.r3.u32;if(!raw)return original;
 std::memcpy(base+raw,bytes.data(),bytes.size());allocation=ctx;allocation.r3.u64=12;allocation.r4.u64=16;sub_82A00AC8(allocation,base);const auto motion=allocation.r3.u32;
 if(!motion){PPCContext release=ctx;release.r3.u64=raw;sub_82A00AB0(release,base);return original;}
 std::memset(base+motion,0,12);REX_STORE_U32(motion,0x820aa24cu);PPCContext construct=ctx;construct.r3.u64=motion;construct.r4.u64=raw;sub_8214A548(construct,base);
 if(construct.r3.u32||!U(base,motion+4)){PPCContext release=ctx;release.r3.u64=motion;sub_82A00AB0(release,base);release.r3.u64=raw;sub_82A00AB0(release,base);return original;}
 openplay_motions.emplace(key,motion);openplay_original_motions[motion]=original;openplay_motion_styles[motion]=unsigned(style);return motion;
}

PES6OpenPlayActorScope::PES6OpenPlayActorScope(uint32_t actor):previous_(openplay_owner){openplay_owner=actor;}
PES6OpenPlayActorScope::~PES6OpenPlayActorScope(){openplay_owner=previous_;}
extern "C" void pes6_cr7_chop_arm(uint8_t* base,uint32_t actor){
 if(!Live(base,actor,632)||((unsigned(base[actor+132])<<8)|base[actor+133])!=446)return;
 const auto wrapper=U(base,actor+624),match=U(base,0x8313a948u);if(!Live(base,wrapper,88)||!Live(base,match,2156))return;
 const auto controller=U(base,wrapper+84);if(controller)cr7_chop_until[controller]=U(base,match+2152)+45;
}
extern "C" void pes6_openplay_replay_pose(PPCContext& ctx,uint8_t* base,uint32_t controller){
 if(!openplay_trial.load(std::memory_order_relaxed)||!Live(base,controller,56))return;
 const auto previousId=(unsigned(base[controller+52])<<8)|base[controller+53],currentId=(unsigned(base[controller+54])<<8)|base[controller+55];
 if(!RecordedAssignedClip(previousId)&&!RecordedAssignedClip(currentId))return;
 const auto clip=U(base,controller+16);if(!Live(base,clip,28))return;
 const auto previous=U(base,clip),current=U(base,clip+4);
 struct PlaybackScope {bool previous;PlaybackScope():previous(openplay_playback){openplay_playback=true;}~PlaybackScope(){openplay_playback=previous;}} playback;
 const auto savedOverride=openplay_style_override;
 openplay_style_override=openplay_recorded_styles[0];const auto convertedPrevious=openplay_style_override<0||openplay_style_override==15?previous:pes6_openplay_motion(ctx,base,controller,previousId,previous);
 openplay_style_override=openplay_recorded_styles[1];const auto convertedCurrent=openplay_style_override<0||openplay_style_override==15?current:pes6_openplay_motion(ctx,base,controller,currentId,current);
 openplay_style_override=savedOverride;
 if(convertedPrevious!=previous)REX_STORE_U32(clip,convertedPrevious);if(convertedCurrent!=current)REX_STORE_U32(clip+4,convertedCurrent);
}
extern "C" uint32_t pes6_openplay_decode_id(uint32_t id){
 const auto clip=id&2047u,style=(id>>11)&15u;
 return (id&0x8000u)&&(style<13||style==15)&&RecordedAssignedClip(clip)?clip:id;
}
extern "C" double pes6_replay_wrap_clock(double clock,double duration){
 if(!(duration>0)||!std::isfinite(clock)||!std::isfinite(duration)||clock<=duration*8)return clock;
 const float wrapped=float(std::fmod(clock,duration));
 // The native loop keeps the end keyframe on exact multiples, not time zero.
 return wrapped>0?double(wrapped):duration;
}
PES6OpenPlayReplayScope::PES6OpenPlayReplayScope(uint8_t* base,uint32_t payload){
 for(unsigned i=0;i<2;i++){previous_[i]=openplay_recorded_styles[i];openplay_recorded_styles[i]=-1;
  if(Live(base,payload,22)){const auto at=payload+18+i*2;const unsigned id=(unsigned(base[at])<<8)|base[at+1];if(pes6_openplay_decode_id(id)!=id)openplay_recorded_styles[i]=int((id>>11)&15u);}}
}
PES6OpenPlayReplayScope::~PES6OpenPlayReplayScope(){for(unsigned i=0;i<2;i++)openplay_recorded_styles[i]=previous_[i];}
extern "C" void pes6_openplay_record(uint8_t* base,uint32_t controller,uint32_t payload){
 if(!openplay_trial.load(std::memory_order_relaxed)||!Live(base,controller,56)||!Live(base,payload,22))return;
 const auto clip=U(base,controller+16);if(!Live(base,clip,8))return;
 for(unsigned side=0;side<2;side++){
  const auto at=payload+18+2*side;const unsigned id=(unsigned(base[at])<<8)|base[at+1];if(!RecordedAssignedClip(id))continue;
  const auto motion=U(base,clip+4*side);int style=15;
  {std::lock_guard lock(openplay_mutex);const auto found=openplay_motion_styles.find(motion);if(found!=openplay_motion_styles.end())style=int(found->second);}
  if(style>=0){const unsigned tagged=0x8000u|(unsigned(style)<<11)|id;base[at]=uint8_t(tagged>>8);base[at+1]=uint8_t(tagged);}
 }
}
extern "C" void pes6_openplay_record_compact(uint8_t* base,uint32_t controller,uint32_t payload){
 if(!openplay_trial.load(std::memory_order_relaxed)||!Live(base,controller,56)||!Live(base,payload,14))return;
 const unsigned id=(unsigned(base[payload+12])<<8)|base[payload+13];if(!RecordedAssignedClip(id))return;
 const auto clip=U(base,controller+16);if(!Live(base,clip,8))return;
 std::lock_guard lock(openplay_mutex);const auto style=openplay_motion_styles.find(U(base,clip+4));const unsigned chosenStyle=style==openplay_motion_styles.end()?15u:style->second;
 const unsigned tagged=0x8000u|(chosenStyle<<11)|id;base[payload+12]=uint8_t(tagged>>8);base[payload+13]=uint8_t(tagged);
}
extern "C" uint32_t pes6_openplay_compact_motion(PPCContext& ctx,uint8_t* base,uint32_t controller,uint32_t payload,uint32_t original){
 if(!Live(base,payload,14))return original;const unsigned id=(unsigned(base[payload+12])<<8)|base[payload+13],decoded=pes6_openplay_decode_id(id);
 if(decoded==id||!RecordedAssignedClip(decoded))return original;
 if(decoded!=id&&((id>>11)&15u)==15)return original;
 const auto savedStyle=openplay_style_override;const auto savedPlayback=openplay_playback;
 openplay_style_override=decoded!=id?int((id>>11)&15u):-1;openplay_playback=true;
 const auto motion=pes6_openplay_motion(ctx,base,controller,decoded,original);
 openplay_style_override=savedStyle;openplay_playback=savedPlayback;return motion;
}
extern "C" void pes6_openplay_probe(PPCContext& ctx,uint8_t* base,uint32_t stage,uint32_t object,uint32_t payload){
 if(!replay_probe_enabled.load(std::memory_order_relaxed))return;
 const auto half=[&](uint32_t p)->unsigned{return Live(base,p,2)?(unsigned(base[p])<<8)|base[p+1]:65535u;};
 const auto word=[&](uint32_t p)->uint32_t{return Live(base,p,4)?U(base,p):0u;};
 const unsigned previousId=half(object+52),currentId=half(object+54),packetPrevious=half(payload+18),packetCurrent=half(payload+20);
 const bool shot=OpenPlayShotClip(pes6_openplay_decode_id(previousId))||OpenPlayShotClip(pes6_openplay_decode_id(currentId))||(payload&&(OpenPlayShotClip(pes6_openplay_decode_id(packetPrevious))||OpenPlayShotClip(pes6_openplay_decode_id(packetCurrent))));
 std::lock_guard lock(replay_probe_mutex);if(!replay_probe_enabled)return;
 if(!shot&&(stage>=replay_probe_other.size()||replay_probe_other[stage]++>=(stage>=13?512u:32u)))return;
 if(replay_probe_rows>=8192){replay_probe_file.flush();replay_probe_file.close();replay_probe_enabled=false;return;}
 const auto clip=word(object+16),previous=clip?word(clip):0,current=clip?word(clip+4):0,match=word(0x8313a948u);
 int ps=-1,cs=-1;{std::lock_guard motionLock(openplay_mutex);const auto p=openplay_motion_styles.find(previous),c=openplay_motion_styles.find(current);if(p!=openplay_motion_styles.end())ps=int(p->second);if(c!=openplay_motion_styles.end())cs=int(c->second);}
 replay_probe_file<<stage<<','<<std::hex<<uint32_t(ctx.lr)<<','<<object<<','<<payload<<','<<std::dec<<(Live(base,0x83139895u,1)?unsigned(base[0x83139895u]):255)<<','<<(Live(base,match,457)?unsigned(base[match+456]):255)<<','<<std::hex<<openplay_owner<<','<<std::dec<<previousId<<','<<currentId<<','<<packetPrevious<<','<<packetCurrent<<','<<std::hex<<previous<<','<<current<<','<<(previous?word(previous+8):0)<<','<<(current?word(current+8):0)<<','<<std::dec<<ps<<','<<cs<<','<<std::hex<<(clip?word(clip+8):0)<<','<<(clip?word(clip+12):0)<<','<<std::dec<<openplay_recorded_styles[0]<<','<<openplay_recorded_styles[1]<<','<<std::hex<<ctx.ctr.u32<<','<<ctx.r3.u32<<','<<ctx.r4.u32<<'\n';
 ++replay_probe_rows;if(replay_probe_rows%64==0)replay_probe_file.flush();
}
