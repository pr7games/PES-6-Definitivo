#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include <cstring>
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <array>
#include <vector>
#include <bit>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include "stadium_sunset.inl"
#include "stadium_environment.inl"
#include "stadium_lightmaps.inl"
#include "stadium_adverts.inl"
#include "stadium_editor.inl"

namespace {
uint32_t tables=0;
unsigned literalVariant=0;
std::vector<uint32_t> konamiRequiredTextures;
bool KonamiActive(uint8_t* base){const auto c=REX_LOAD_U32(0x83139360u);return c&&base[c+130]==8;}
void RequireModelTextures(uint8_t* base,uint32_t data){
  const auto count=REX_LOAD_U32(data+124),table=REX_LOAD_U32(data+128);
  for(uint32_t i=0;i<count;i++){
    const auto id=REX_LOAD_U32(data+table+i*16+12);
    const auto at=std::lower_bound(konamiRequiredTextures.begin(),konamiRequiredTextures.end(),id);
    if(at==konamiRequiredTextures.end()||*at!=id)konamiRequiredTextures.insert(at,id);
  }
}
bool Init(PPCContext& original,uint8_t* base){
  if(tables)return true;
  PPCContext allocation=original;allocation.r3.u64=512;
  sub_826D61B0(allocation,base);tables=allocation.r3.u32;
  if(!tables)return false;
  std::memset(base+tables,0,512);
  // Konami, the eight original venues, and Wembley (ID 10); UI10 is Random (ID 9).
  constexpr uint8_t forward[]{8,3,2,5,0,4,6,1,7,10,9};
  constexpr uint8_t inverse[]{4,7,2,1,5,3,6,8,0,10,9};
  std::memcpy(base+tables,forward,sizeof(forward));
  std::memcpy(base+tables+16,inverse,sizeof(inverse));
  std::memcpy(base+tables+32,base+0x82D4FCE0u,96);
  std::memcpy(base+tables+128,base+0x82D4FCE0u,12);
  REX_STORE_U32(tables+132,84000);REX_STORE_U32(tables+136,2005);
  std::memcpy(base+tables+152,base+0x82D4FCE0u,12);
  REX_STORE_U32(tables+156,90000);REX_STORE_U32(tables+160,2007);
  constexpr char name[]="Konami Stadium",wembleyName[]="Wembley Stadium",preview[]="stadium_04";
  std::memcpy(base+tables+168,name,sizeof(name));
  std::memcpy(base+tables+188,wembleyName,sizeof(wembleyName));
  std::memcpy(base+tables+208,base+0x82D4FD40u,32);
  REX_STORE_U32(tables+240,tables+168);
  REX_STORE_U32(tables+248,tables+188);
  std::memcpy(base+tables+256,preview,sizeof(preview));
  std::memcpy(base+tables+288,base+0x82B55DF4u,12);
  REX_STORE_U32(tables+300,tables+256);
  REX_STORE_U32(tables+308,tables+256);
  REX_STORE_U32(tables+324,tables+256);
  REX_STORE_U32(tables+328,tables+256);
  REXLOG_INFO("PES6 stadium: Konami, Wembley, and all eight original venues");
  return true;
}
}

extern "C" uint32_t pes6_stadium_table(PPCContext& ctx,uint8_t* base,unsigned table){
  if(!Init(ctx,base))return table==0?0x82007488u:table==1?0x82007494u:table==2?0x82D4FCE0u:0x82D4FD40u;
  if(table==3)std::memcpy(base+tables+208,base+0x82D4FD40u,32);
  ApplyStadiumEdits(ctx,base,tables);
  return tables+(table==0?0:table==1?16:table==2?32:table==3?208:288);
}
extern "C" bool pes6_stadium_name(PPCContext& ctx,uint8_t* base){
  // Still valid for old saved matches; the menu handles Random separately.
  if((ctx.r3.u32>=9&&ctx.r3.u32!=10)||!Init(ctx,base))return false;
  ApplyStadiumEdits(ctx,base,tables);
  if(auto address=ctx.r3.u32<stadiumEdits.size()?stadiumEdits[ctx.r3.u32].nameAddress:0){ctx.r3.u64=address;return true;}
  if(ctx.r3.u32==10){ctx.r3.u64=tables+188;return true;}
  if(ctx.r3.u32!=8)return false;
  ctx.r3.u64=tables+168;return true;
}
extern "C" uint32_t pes6_stadium_resource(uint32_t byteIndex,uint32_t original){
  if(byteIndex>=128&&byteIndex<144)return 6727+(byteIndex-128)/4;
  if(byteIndex>=160&&byteIndex<176)return 6727+(byteIndex-160)/4;
  return original;
}

// The imported shell occupies native sector17, but contains the entire venue.
// Native camera-sector suppression must not discard that combined instance.
extern "C" void pes6_stadium_shell_visible(PPCContext& ctx,uint8_t* base){
  const auto config=REX_LOAD_U32(0x83139360u);
  if(!config||(base[config+130]!=8&&base[config+130]!=10&&base[config+130]!=5))return;
  const auto scene=REX_LOAD_U32(0x831393A8u);
  if(!scene)return;
  const auto instances=REX_LOAD_U32(scene+472);
  if(!instances)return;
  if(base[config+130]==5)return; // Restore native near-stand visibility.
  if(base[config+130]==5){
    // Wide cameras can see the near lower terrace from above/behind. Keep
    // its original seat mesh present instead of exposing the grass below.
    if(ctx.r3.u32<instances||(ctx.r3.u32-instances)%20)return;
    const auto slot=(ctx.r3.u32-instances)/20;
    if(slot!=17&&slot!=20&&slot!=23&&slot!=26)return;
  }else if(base[config+130]==8||base[config+130]==10){
    const auto slot=(ctx.r3.u32-instances)/20;
    if(ctx.r3.u32<instances||ctx.r3.u32!=instances+slot*20)return;
    if(slot!=17&&slot!=18&&slot!=20&&slot!=21&&slot!=23&&slot!=24&&slot!=26&&slot!=27&&slot!=31)return;
  }else if(ctx.r3.u32!=instances+17*20&&ctx.r3.u32!=instances+20*20&&ctx.r3.u32!=instances+23*20&&ctx.r3.u32!=instances+26*20)return;
  const auto instance=REX_LOAD_U32(ctx.r3.u32);
  if(!instance)return;
  if(base[config+130]==5)REX_STORE_U32(instance+284,0);
  REX_STORE_U16(ctx.r3.u32+16,0);
  REX_STORE_U16(ctx.r3.u32+18,0);
  const auto model=REX_LOAD_U32(instance+248);
  if(!model)return;
  const auto data=REX_LOAD_U32(model+12);
  const auto visibleGroups=REX_LOAD_U32(instance+252);
  const auto visibleBounds=REX_LOAD_U32(instance+256);
  if(data&&visibleGroups){const auto n=REX_LOAD_U32(data+64);if(n<=16)std::memset(base+visibleGroups,1,n);}
  if(data&&visibleBounds){const auto n=REX_LOAD_U32(data+88);if(n<=16)std::memset(base+visibleBounds,1,n);}
}

// Restore native PES6 near-side, height and back-side crowd rejection.
extern "C" bool pes6_stadium_teatro_crowd(uint8_t*){return false;}
extern "C" bool pes6_stadium_teatro_row_occludes(uint8_t*,uint32_t){return false;}

extern "C" bool pes6_stadium_bundle(PPCContext& ctx,uint8_t* base){
  const uint32_t request=ctx.r3.u32,key=REX_LOAD_U32(request+8);
  const bool preview=key==0x02000002u;
  const auto config=REX_LOAD_U32(0x83139360u);
  const bool wembley=config&&base[config+130]==10;
  if(stadiumLightingContext&&config)sunsetOvercast=konamiSunset&&base[config+131]!=0;
  const bool nativeSunsetLight=stadiumLightingContext&&pes6_stadium_sunset()&&config&&base[config+130]<8&&key>=3021&&key<=3073;
  const bool nativeLight=stadiumLightingContext&&!nativeSunsetLight&&config&&base[config+130]<8&&key>=3021&&key<=3073;
  const bool lighting=stadiumLightingContext&&key>=3021&&key<=3032&&config&&(base[config+130]==8||wembley);
  const bool lightProfile=lighting||nativeSunsetLight||nativeLight;
  const bool nativePitch=stadiumLightingContext&&config&&base[config+130]<8&&key>=11+unsigned(base[config+130])*4&&key<15+unsigned(base[config+130])*4;
  if(!preview&&!lightProfile&&!nativePitch&&(key<6727||key>6730))return false;
  if(base[request+121]==1)return false;
  const bool winter=!preview&&!lightProfile&&config&&base[config+133]!=0;
  const unsigned nativeVariant=preview||lightProfile?0:nativePitch?(key-11)%4:key-6727;
  // The guest request table is hour*2+weather. External assets are instead
  // weather*2+hour: key+1 is wet DAY, not dry NIGHT. Read the decoded match
  // weather directly, including drizzle/torrential, rather than guessing it
  // from resource-key parity. Wet dusk deliberately uses the full wet-day set.
  const bool wet=config&&base[config+131]!=0;
  const unsigned variant=preview||lightProfile?0:
    config?(konamiSunset?(wet?2:4):(wet?2:0)+(stadiumNight?1:0)):
    ((nativeVariant&1)*2+(nativeVariant>>1));
  const unsigned index=nativeLight?208+key-3021:nativePitch?82+unsigned(base[config+130])*12+(winter?6:0)+variant:nativeSunsetLight?29+key-3021:lighting?(wembley?192+key-3021:17+key-3021):wembley?(preview?190:178+(winter?6:0)+variant):preview?12:winter?6+variant:variant;
  if(!preview&&!lightProfile&&!nativePitch)literalVariant=variant;
  static std::mutex mutex;std::lock_guard lock(mutex);
  static std::array<std::vector<uint8_t>,272> cache;
  auto& bytes=cache[index];
  if(bytes.empty()){
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
    auto root=std::filesystem::path(exe).parent_path();
    for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
    auto file=nativeLight?root/"data/stadiums/native-lighting"/("lighting-"+std::to_string(key)+".bundle"):nativePitch?root/"data/stadiums/pes2009-pitches"/("stadium-"+std::to_string(base[config+130]))/("variant-"+std::to_string(variant)+(winter?"-winter":"")+".bundle"):
      root/(nativeSunsetLight?"data/stadiums/sunset":wembley?"data/stadiums/wembley":"data/stadiums/konami")/(nativeSunsetLight?"lighting-"+std::to_string(key)+".bundle":lighting?"lighting-"+std::to_string(key-3021)+".bundle":preview?"preview.bundle":"variant-"+std::to_string(variant)+(winter?"-winter":"")+".bundle");
    if(wembley&&winter&&!std::filesystem::exists(file)){
      file=root/"data/stadiums/wembley"/("variant-"+std::to_string(variant)+".bundle");
    }
    std::ifstream input(file,std::ios::binary|std::ios::ate);
    if(!input||input.tellg()<56||input.tellg()>64000000){REXLOG_ERROR("PES6 stadium bundle unavailable: key=0x{:08X}",key);ctx.r3.u64=0;return true;}
    bytes.resize(size_t(input.tellg()));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    const auto be=[&](size_t at){return uint32_t(bytes[at])<<24|uint32_t(bytes[at+1])<<16|uint32_t(bytes[at+2])<<8|bytes[at+3];};
    const unsigned expected=lightProfile?2:4;
    bool valid=bool(input)&&be(0)==expected&&be(4)==8;
    for(unsigned i=0;i<expected&&valid;i++){const uint64_t at=be(8+i*12),n=be(12+i*12);valid=at>=8+expected*12&&n&&at+n<=bytes.size();}
    if(!valid){bytes.clear();REXLOG_ERROR("PES6 stadium bundle invalid: key=0x{:08X}",key);ctx.r3.u64=0;return true;}
  }
  PPCContext allocation=ctx;allocation.r3.u64=bytes.size();allocation.r4.u64=16;sub_82A00AC8(allocation,base);
  const auto output=allocation.r3.u32;
  if(!output){ctx.r3.u64=0;return true;}
  std::memcpy(base+output,bytes.data(),bytes.size());
  // Step2 acts on this private copy, never on cached/original profiles.
  if(lightProfile&&(stadiumNight||wet||konamiSunset))StadiumAdjustLight(base+output,bytes.size());
  const auto previous=REX_LOAD_U32(request+16),input=ctx.r4.u32;
  if(base[request+24]&&input){PPCContext release=ctx;release.r3.u64=input;sub_82A00AB0(release,base);}
  REX_STORE_U32(request+16,output);REX_STORE_U32(request+20,uint32_t(bytes.size()));
  if(previous&&previous!=output&&previous!=input){PPCContext release=ctx;release.r3.u64=previous;sub_82A00AB0(release,base);}
  REXLOG_WARN("PES6 stadium external bundle loaded: key=0x{:08X}, bytes={}, venue={}, variant={}, wet={}, logicalNight={}, dusk={}",key,bytes.size(),config?unsigned(base[config+130]):99,variant,wet,stadiumNight,konamiSunset);
  ctx.r3.u64=1;return true;
}

// Native instances for donor geometry, with architecture batched by material.
// These assets deliberately
// bypass the stadium's fixed 34-entry model table, never its model renderer.
namespace {
struct LiteralPiece {
  uint32_t model,instance,record,pass;
  std::vector<int> scoreGroups;
  bool architecture=false;
  std::vector<StadiumAdvertStream> adverts;
  std::vector<pes6::stadium_net::PositionStream> advertDescriptors;
  float advertPhase=-1.0f;
  uint32_t data=0,visibleGroups=0,visibleBounds=0,groupCount=0,boundCount=0;
  void Show(uint8_t* base) const {
    if(visibleGroups&&groupCount<=256)std::memset(base+visibleGroups,1,groupCount);
    if(visibleBounds&&boundCount<=256)std::memset(base+visibleBounds,1,boundCount);
  }
};
struct LiteralVenue {
  uint32_t scene=0,payload=0,records=0;
  uint64_t loggedNativePasses=0;
  std::vector<LiteralPiece> pieces;
  std::vector<uint32_t> textures;
  std::array<std::vector<size_t>,49> drawLists;
} literal;
thread_local bool drawingLiteral=false;
bool LiteralActive(uint8_t* base){const auto config=REX_LOAD_U32(0x83139360u);return config&&(base[config+130]==8||base[config+130]==10);}
uint32_t LiteralCall(PPCContext& original,uint8_t* base,uint32_t function,uint32_t a,uint32_t b=0,uint32_t c=0,uint32_t d=0,uint32_t e=0){
  PPCContext ctx=original;ctx.r3.u64=a;ctx.r4.u64=b;ctx.r5.u64=c;ctx.r6.u64=d;ctx.r7.u64=e;
  REX_CALL_INDIRECT_FUNC(function);return ctx.r3.u32;
}
std::string LiteralName(uint8_t* base,uint32_t at){const uint64_t word=REX_LOAD_U64(at);std::string s;for(int i=0;i<10;i++){const auto c=(word>>(54-i*6))&63;if(c)s+=char(c<27?c+96:c+20);}return s;}
void LiteralRelease(PPCContext& ctx,uint8_t* base){
  REXLOG_WARN("Konami literal: releasing {} models and {} textures",literal.pieces.size(),literal.textures.size());
  for(const auto& p:literal.pieces)if(p.instance){const auto vt=REX_LOAD_U32(p.instance);LiteralCall(ctx,base,REX_LOAD_U32(vt),p.instance,1);}
  for(const auto& p:literal.pieces)if(p.model)LiteralCall(ctx,base,0x82100000u,p.model);
  for(const auto t:literal.textures)if(t)LiteralCall(ctx,base,0x8218FC78u,t);
  if(literal.records)LiteralCall(ctx,base,0x8280E7B8u,literal.records);
  if(literal.payload)LiteralCall(ctx,base,0x82A00AB0u,literal.payload);
  literal={};
  stadiumAdvertTick=0;
  stadiumAdvertReplayTick=0;
  stadiumAdvertInReplay=false;
}
}
extern "C" void pes6_stadium_prune_model(PPCContext& ctx,uint8_t* base){
  if(!KonamiActive(base))return;
  const auto data=ctx.r3.u32;
  if(REX_LOAD_U32(data+40)||ctx.r30.u32==31)RequireModelTextures(base,data);
  if(!REX_LOAD_U32(data+40)){
    REX_STORE_U32(data+48,0);REX_STORE_U32(data+116,0);
  }
}
extern "C" bool pes6_stadium_unused_texture(PPCContext& ctx,uint8_t* base){
  if(!KonamiActive(base)||konamiRequiredTextures.empty())return false;
  // These shading services read their resources independently of mesh bindings.
  if(ctx.r24.u32==22||ctx.r24.u32==24)return false;
  return !std::binary_search(konamiRequiredTextures.begin(),konamiRequiredTextures.end(),uint32_t(REX_LOAD_U16(ctx.r3.u32+4)));
}
extern "C" void pes6_stadium_begin_models(PPCContext&,uint8_t* base){
  if(KonamiActive(base))konamiRequiredTextures.clear();
}
extern "C" void pes6_stadium_literal_release(PPCContext& ctx,uint8_t* base){
  const bool imported=literal.scene&&literal.scene==ctx.r3.u32;
  const auto matchScene=REX_LOAD_U32(0x831393A8u);
  if(imported)LiteralRelease(ctx,base);
  // Native daytime/nighttime scenes may own no literal payload. End their
  // lighting scope too, so the optional gain cannot bleed into editors/menu.
  if(imported||(matchScene&&matchScene==ctx.r3.u32))stadiumLightingContext=false;
}
extern "C" void pes6_stadium_literal_load(PPCContext& ctx,uint8_t* base){
  // Scene objects can be reused when moving to a native venue. Release the
  // previous imported GPU resources before checking the newly selected venue.
  if(literal.scene)LiteralRelease(ctx,base);
  const bool nativeSunset=!LiteralActive(base)&&pes6_stadium_sunset();
  if(!LiteralActive(base)&&!nativeSunset)return;
  const auto scene=REX_LOAD_U32(0x831393A8u);if(!scene)return;
  wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();
  for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
  const auto config=REX_LOAD_U32(0x83139360u);
  const bool wembley=config&&base[config+130]==10;
  const bool winter=config&&base[config+133]!=0;
  auto file=nativeSunset?root/"data/stadiums/sunset"/("stadium-"+std::to_string(base[config+130])+"-sky.bundle"):root/(wembley?"data/stadiums/wembley":"data/stadiums/konami")/("literal-"+std::to_string(literalVariant)+(winter?"-winter":"")+".bundle");
  if(nativeSunset&&winter){const auto seasonal=root/"data/stadiums/sunset"/("stadium-"+std::to_string(base[config+130])+"-sky-winter.bundle");if(std::filesystem::exists(seasonal))file=seasonal;}
  if(wembley&&winter&&!std::filesystem::exists(file))file=root/"data/stadiums/wembley"/("literal-"+std::to_string(literalVariant)+".bundle");
  std::ifstream input(file,std::ios::binary|std::ios::ate);
  if(!input||input.tellg()<32||input.tellg()>64000000){REXLOG_ERROR("Konami literal: missing payload");return;}
  std::vector<uint8_t> bytes(size_t(input.tellg()));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
  auto be=[&](size_t at)->uint32_t{if(at+4>bytes.size())return 0;return uint32_t(bytes[at])<<24|uint32_t(bytes[at+1])<<16|uint32_t(bytes[at+2])<<8|bytes[at+3];};
  if(!input||be(0)!=2||be(4)!=8)return;
  auto mo=be(8),ml=be(12),to=be(20),tl=be(24);
  if(uint64_t(mo)+ml>bytes.size()||uint64_t(to)+tl>bytes.size()||ml<8||tl<8||be(mo+4)!=8||be(to+4)!=8)return;
  auto models=be(mo),textures=be(to);if(!models||models>256||textures>256||8+models*12>ml||8+textures*12>tl)return;
  for(const auto at:{mo,to})for(uint32_t i=0;i<be(at);i++){const auto off=be(at+8+i*12),n=be(at+12+i*12),len=at==mo?ml:tl;if(off<8+be(at)*12||!n||uint64_t(off)+n>len)return;}
  // Only upload textures referenced by models that will actually be drawn.
  // Ground/lines already belong to the main native bundle; supplemental
  // lightmap receivers are disabled and need no duplicate GPU resources.
  std::vector<uint32_t> requiredTextures;
  std::vector<uint32_t> activeModels,activeTextures;
  const auto unusedPass=[wembley](uint32_t pass){return pass==3||pass==4||pass==7||(wembley?(pass>=52&&pass<=70):(pass>=50&&pass<=70));};
  for(uint32_t i=0;i<models;i++){
    if(unusedPass(be(mo+16+i*12)))continue;
    const auto at=mo+be(mo+8+i*12),n=be(at+124),table=at+be(at+128);
    const auto length=be(mo+12+i*12);
    if(length<192||uint64_t(be(at+128))+uint64_t(n)*16>length)return;
    activeModels.push_back(i);
    for(uint32_t j=0;j<n;j++)requiredTextures.push_back(be(table+j*16+12));
  }
  std::sort(requiredTextures.begin(),requiredTextures.end());
  requiredTextures.erase(std::unique(requiredTextures.begin(),requiredTextures.end()),requiredTextures.end());
  for(uint32_t i=0;i<textures;i++){
    const auto data=to+be(to+8+i*12),length=be(to+12+i*12);
    if(length<16)return;
    const auto id=uint32_t(bytes[data+4])<<8|bytes[data+5];
    if(std::binary_search(requiredTextures.begin(),requiredTextures.end(),id))activeTextures.push_back(i);
  }
  // Repack only live payloads before allocating guest memory. Individual
  // model/texture bytes and tags remain exact; internal model offsets are
  // relative to that model, so moving it does not rewrite any geometry.
  const auto put=[](std::vector<uint8_t>& out,size_t at,uint32_t value){
    out[at]=uint8_t(value>>24);out[at+1]=uint8_t(value>>16);out[at+2]=uint8_t(value>>8);out[at+3]=uint8_t(value);
  };
  const auto compact=[&](uint32_t start,const std::vector<uint32_t>& selected){
    size_t size=8+selected.size()*12;
    for(auto i:selected)size+=be(start+12+i*12);
    std::vector<uint8_t> out(size);put(out,0,uint32_t(selected.size()));put(out,4,8);
    size_t cursor=8+selected.size()*12;
    for(size_t j=0;j<selected.size();j++){
      const auto row=start+8+selected[j]*12,n=be(row+4),offset=be(row);
      put(out,8+j*12,uint32_t(cursor));put(out,12+j*12,n);put(out,16+j*12,be(row+8));
      std::memcpy(out.data()+cursor,bytes.data()+start+offset,n);cursor+=n;
    }
    return out;
  };
  const auto originalBytes=bytes.size();
  if(KonamiActive(base)){
    auto liveModels=compact(mo,activeModels),liveTextures=compact(to,activeTextures);
    mo=32;ml=uint32_t(liveModels.size());to=mo+ml;tl=uint32_t(liveTextures.size());
    std::vector<uint8_t> live(to+tl);put(live,0,2);put(live,4,8);
    put(live,8,mo);put(live,12,ml);put(live,16,be(16));
    put(live,20,to);put(live,24,tl);put(live,28,be(28));
    std::memcpy(live.data()+mo,liveModels.data(),ml);std::memcpy(live.data()+to,liveTextures.data(),tl);
    bytes=std::move(live);models=be(mo);textures=be(to);
  }
  literal.scene=scene;literal.payload=LiteralCall(ctx,base,0x82A00AC8u,uint32_t(bytes.size()),16);
  literal.records=LiteralCall(ctx,base,0x826D61B0u,models*20);
  if(!literal.payload||!literal.records){LiteralRelease(ctx,base);return;}
  std::memcpy(base+literal.payload,bytes.data(),bytes.size());std::memset(base+literal.records,0,models*20);
  literal.pieces.reserve(models);literal.textures.reserve(textures);
  if(KonamiActive(base))REXLOG_WARN("Konami live payload: {} -> {} bytes; {} models, {} textures",originalBytes,bytes.size(),models,textures);
  if(KonamiActive(base)){
    konamiRequiredTextures.insert(konamiRequiredTextures.end(),requiredTextures.begin(),requiredTextures.end());
    std::sort(konamiRequiredTextures.begin(),konamiRequiredTextures.end());
    konamiRequiredTextures.erase(std::unique(konamiRequiredTextures.begin(),konamiRequiredTextures.end()),konamiRequiredTextures.end());
  }
  for(uint32_t i=0;i<textures;i++){
    const auto data=literal.payload+to+be(to+8+i*12),id=uint32_t(base[data+4])<<8|base[data+5];
    if(!std::binary_search(requiredTextures.begin(),requiredTextures.end(),id))continue;
    const auto t=LiteralCall(ctx,base,0x8218E880u,data);
    if(!t){LiteralRelease(ctx,base);return;}literal.textures.push_back(t);
    LiteralCall(ctx,base,0x8218FCE0u,t);LiteralCall(ctx,base,0x8218FD90u,t);
  }
  const std::array<std::string,13> nativeGroups{"model","champion1","champion2","common","ex","goal","half","konami","light","match","stats","title","welcome"};
  for(uint32_t i=0;i<models;i++){
    const auto data=literal.payload+mo+be(mo+8+i*12),pass=be(mo+16+i*12);
    // The user's A/B capture isolates the regression to these supplemental
    // coplanar models (216 extra submissions/frame). The base donor surfaces
    // already render the venue; do not instantiate the duplicate receivers.
    if(unusedPass(pass))continue;
    const auto model=LiteralCall(ctx,base,0x822015D0u,data,1,0,0,0);
    if(!model){LiteralRelease(ctx,base);return;}
    literal.pieces.push_back({model,0,literal.records+i*20,pass,{}});auto& p=literal.pieces.back();
    if(pass>=37&&pass<=40){
      p.adverts=StadiumAdvertStreams(base,data);p.advertDescriptors.reserve(p.adverts.size());
      for(const auto& stream:p.adverts)p.advertDescriptors.push_back(stream.stream);
    }
    const auto memory=LiteralCall(ctx,base,0x826D61B0u,304);
    if(!memory){LiteralRelease(ctx,base);return;}
    p.instance=LiteralCall(ctx,base,0x821FEF80u,memory,model);if(!p.instance){LiteralRelease(ctx,base);return;}
    // Literal models and their visibility allocations live until release.
    // Cache immutable metadata rather than chasing guest pointers per draw.
    p.data=REX_LOAD_U32(model+12);p.visibleGroups=REX_LOAD_U32(p.instance+252);p.visibleBounds=REX_LOAD_U32(p.instance+256);
    p.groupCount=REX_LOAD_U32(p.data+64);p.boundCount=REX_LOAD_U32(p.data+88);
    REX_STORE_U32(p.record,p.instance);REX_STORE_U32(p.instance+284,0);
    p.architecture=pass==16||(wembley&&(pass==17||pass==18));
    if(pass==17)for(unsigned ti=0;ti<REX_LOAD_U32(data+124);ti++){const auto id=REX_LOAD_U32(data+REX_LOAD_U32(data+128)+ti*16+12);if(id>=26000&&id<27000)p.architecture=true;}
    // Goals and deformable nets belong to the original PES6 service objects.
    if(pass==31){
      const auto n=REX_LOAD_U32(data+64),groups=REX_LOAD_U32(data+68);
      for(uint32_t j=0;j<n;j++){
        auto s=LiteralName(base,data+groups+j*48);if(s.size()>3&&(s[0]=='g'||s[0]=='i'))s=s.substr(3);
        if(s=="champio")s=j==7?"champion1":"champion2";
        // PES2008's vision group is the video screen. PES6 calls its
        // corresponding native render target10400; it has no named group.
        int match=s=="vision"?-2:-1;for(size_t k=0;k<nativeGroups.size();k++)if(s==nativeGroups[k])match=int(k);
        p.scoreGroups.push_back(match);
      }
    }
  }
  // Resolve dispatch once, preserving the original submission order exactly.
  for(unsigned slot=0;slot<literal.drawLists.size();slot++)for(size_t i=0;i<literal.pieces.size();i++){
    const auto pass=literal.pieces[i].pass;
    if(pass==slot||(slot==15&&(pass==16||(pass>=50&&pass<=55&&(!wembley||pass!=51))||(wembley&&(pass==31||(pass>=37&&pass<=40)))))||
       (slot>=37&&slot<=40&&(pass==slot+20||pass==slot+30)))literal.drawLists[slot].push_back(i);
  }
  REXLOG_WARN("Konami literal: loaded {} original mesh instances, {} original textures, variant {}",literal.pieces.size(),literal.textures.size(),literalVariant);
}
extern "C" bool pes6_stadium_literal_draw(PPCContext& ctx,uint8_t* base){
  // Restore the sky; it was ruled out by the user's black-background capture.
  if(!drawingLiteral&&stadiumLightingContext&&!literal.scene){
    const auto scene=REX_LOAD_U32(0x831393A8u),records=scene?REX_LOAD_U32(scene+472):0;
    if(records&&ctx.r3.u32==records+34*20){
      const auto record=ctx.r3.u32,camera=REX_LOAD_U32(0x83139614u);
      uint8_t position[12];std::memcpy(position,base+record+4,12);
      if(camera)std::memcpy(base+record+4,base+camera+944,12);
      drawingLiteral=true;stadiumBackgroundPass=true;stadiumBackgroundDepthTest=true;
      sub_8214D498(ctx,base);
      stadiumBackgroundPass=false;stadiumBackgroundDepthTest=false;drawingLiteral=false;
      std::memcpy(base+record+4,position,12);return true;
    }
  }
  // PES2009 diffuse pitch textures already contain the venue's baked shadow.
  // The old PES6 slot14 receiver multiplies another venue-shadow footprint
  // over them. Keep dynamic actor/ball/net shadows and all other passes.
  if(!drawingLiteral&&stadiumLightingContext){
    const auto config=REX_LOAD_U32(0x83139360u);
    if(config&&!EditedPitch(base)&&base[config+130]<8&&base[config+130]!=5&&pes6_stadium_lighting_hour(base,base[config+132])==0){
      const auto scene=REX_LOAD_U32(0x831393A8u),records=scene?REX_LOAD_U32(scene+472):0;
      if(records&&ctx.r3.u32==records+14*20)return true;
    }
  }
  if(drawingLiteral||!literal.scene)return false;
  const auto records=REX_LOAD_U32(literal.scene+472),record=ctx.r3.u32;
  if(record<records||record>=records+49*20||(record-records)%20)return false;
  const auto slot=(record-records)/20;
  if((LiteralActive(base)||pes6_stadium_sunset())&&slot==0){
    // Submit the background before the field and architecture. Its depth
    // must not erase roof geometry farther away than the native sky dome.
    drawingLiteral=true;stadiumBackgroundPass=true;
    // Every sky stays at far depth, including native sunset domes.
    stadiumBackgroundDepthTest=true;
    for(const auto idx:literal.drawLists[34]){
      auto& p=literal.pieces[idx];
      std::memcpy(base+p.record+4,base+records+34*20+4,12);
      const auto camera=REX_LOAD_U32(0x83139614u);
      if(camera)std::memcpy(base+p.record+4,base+camera+944,12);
      REX_STORE_U16(p.record+16,0);REX_STORE_U16(p.record+18,0);
      p.Show(base);
      PPCContext draw=ctx;draw.r3.u64=p.record;sub_8214D498(draw,base);
    }
    stadiumBackgroundPass=false;stadiumBackgroundDepthTest=false;drawingLiteral=false;
    return LiteralActive(base);
  }
  if(slot==34)return true;
  if(!LiteralActive(base)){
    if(!pes6_stadium_sunset()||slot!=34)return false;
    drawingLiteral=true;
    for(const auto idx:literal.drawLists[34]){
      auto& p=literal.pieces[idx];std::memcpy(base+p.record+4,base+record+4,12);PPCContext draw=ctx;draw.r3.u64=p.record;sub_8214D498(draw,base);}
    drawingLiteral=false;return true;
  }
  // Tag16 stores permanent architecture, but it is submitted with the known
  // opaque sideline pass15. Do not submit it a second time in native slot16.
  if(slot==16||slot==36)return true;
  // Donor category4 is a coplanar ground-lighting overlay. Its pale blue
  // lightmap (e.g. RGB239/247/255) is not an opaque ground diffuse: submitting
  // it in native pass4 paints over the correctly textured pitch in pass3.
  // Keep original data for a later multiply/lightmap-material translation.
  // The San Siro outer floor (0) and baked receiver (14) still overlap the
  // donor ground at Y~=0. Donor adboard shadow cards (41..44) are also Y=0:
  // recoding their white matte alone did not resolve the user's flicker.
  // Keep these incompatible ground passes out until their depth/blend route
  // is adapted. This does not suppress the donor board faces or goal meshes.
  if(slot==0||slot==5||slot==6||slot==14||(slot>=41&&slot<=44)){
    return true;
  }
  const auto config=REX_LOAD_U32(0x83139360u);
  const bool isWembley=config&&base[config+130]==10;
  if(isWembley&&slot!=3&&slot!=4&&slot!=7&&slot!=15&&slot!=35)return true;
  // Donor base/alpha overlays/lines use native slots3/4/7.
  // Slot4 now contains only donor p_color/p_pat alpha overlays. The opaque
  // category4 literal lightmap remains suppressed by this native return path.
  // Sky and static goalposts come from PES2008 in literal slots34/35.
  // The separate PES6 deformable-net draw runs through 8216E190 as usual.
  if(slot==3||slot==4||slot==7){
    if(slot==3||slot==4||slot==7){
      REX_STORE_U16(record+16,0);REX_STORE_U16(record+18,0);
      const auto inst=REX_LOAD_U32(record),model=inst?REX_LOAD_U32(inst+248):0,data=model?REX_LOAD_U32(model+12):0;
      if(data)for(const auto pair:{std::pair<uint32_t,uint32_t>{252,64},{256,88}}){const auto at=REX_LOAD_U32(inst+pair.first),n=REX_LOAD_U32(data+pair.second);if(at&&n<=256)std::memset(base+at,1,n);}
    }
    if(!(literal.loggedNativePasses&(uint64_t(1)<<slot))){
      literal.loggedNativePasses|=uint64_t(1)<<slot;
      REXLOG_WARN("Konami stage1 native draw route: slot={}, instance=0x{:08X}",slot,REX_LOAD_U32(record));
    }
    if(isWembley&&slot==3){
      // Original Wembley ground composition: paving, its lightmap, then grass
      // apron. The apron must not be overwritten later in architecture slot15.
      drawingLiteral=true;
      for(const unsigned pass:{18u,51u})for(auto& p:literal.pieces)if(p.pass==pass){
        REX_STORE_U16(p.record+16,0);REX_STORE_U16(p.record+18,0);
        const auto data=REX_LOAD_U32(p.model+12),vg=REX_LOAD_U32(p.instance+252),vb=REX_LOAD_U32(p.instance+256);
        if(data&&vg&&REX_LOAD_U32(data+64)<=256)std::memset(base+vg,1,REX_LOAD_U32(data+64));
        if(data&&vb&&REX_LOAD_U32(data+88)<=256)std::memset(base+vb,1,REX_LOAD_U32(data+88));
        StadiumLightmapState state(base,pass==51,false,pass==18);
        PPCContext draw=ctx;draw.r3.u64=p.record;sub_8214D498(draw,base);state.Restore(ctx,base);
      }
      StadiumLightmapState state(base,false,false,true);
      PPCContext draw=ctx;sub_8214D498(draw,base);state.Restore(ctx,base);
      drawingLiteral=false;return true;
    }
    return false;
  }
  if(literal.drawLists[slot].empty())return true;
  drawingLiteral=true;
  if(slot==15&&!(literal.loggedNativePasses&(uint64_t(1)<<15))){literal.loggedNativePasses|=uint64_t(1)<<15;const auto count=std::count_if(literal.pieces.begin(),literal.pieces.end(),[](const auto& p){return p.architecture;});REXLOG_WARN("Konami architecture recovery: dispatch {} one-diffuse / 32-byte models in approved sideline pass15, approved benches/perimeter retained",count);}
  for(const auto pieceIndex:literal.drawLists[slot]){
    auto& p=literal.pieces[pieceIndex];
    if(!p.adverts.empty()){
      const auto phase=StadiumAdvertPhase(base);
      if(phase!=p.advertPhase&&StadiumAdvertUpload(ctx,base,p.model,p.adverts,p.advertDescriptors,phase))p.advertPhase=phase;
    }
    if(p.architecture||p.pass>=50||p.pass==31||p.pass==35||(isWembley&&p.pass>=37&&p.pass<=40)){REX_STORE_U16(p.record+16,0);REX_STORE_U16(p.record+18,0);}
    if(!isWembley&&(slot==31||slot==34||slot==35||(slot>=37&&slot<=44)))std::memcpy(base+p.record+4,base+record+4,12);
    if(p.pass>=52&&p.pass<=55)std::memcpy(base+p.record+4,base+records+(p.pass-11)*20+4,12);
    if(!isWembley&&slot>=37&&slot<=40&&(p.pass==slot+20||p.pass==slot+30))std::memcpy(base+p.record+4,base+record+4,12);
    const auto data=p.data,vg=p.visibleGroups;
    if(p.architecture&&!(literal.loggedNativePasses&(uint64_t(1)<<16))){literal.loggedNativePasses|=uint64_t(1)<<16;REXLOG_WARN("Konami architecture recovery: pre-native gate enabled={} packetLists=({},{}) sourcePackets={} instance=0x{:08X}",unsigned(base[p.instance+160]),REX_LOAD_U32(p.model+48),REX_LOAD_U32(p.model+52),REX_LOAD_U32(data+40),p.instance);}
    p.Show(base);
    if(p.pass==31&&vg){
      const auto native=REX_LOAD_U32(records+31*20),states=native?REX_LOAD_U32(native+252):0;
      const auto nativeModel=native?REX_LOAD_U32(native+248):0,nativeData=nativeModel?REX_LOAD_U32(nativeModel+12):0;
      const uint32_t nativeGroupsCount=nativeData?REX_LOAD_U32(nativeData+64):0;
      for(size_t j=0;j<p.scoreGroups.size();j++){
        const int k=p.scoreGroups[j];
        if(isWembley){base[vg+j]=(k==-2||k==0)?1:0;}
        else{base[vg+j]=k==-2?1:k>=0?(states&&uint32_t(k)<nativeGroupsCount?base[states+k]:uint8_t(k==0||k==7||k==8)):0;}
      }
    }
    const bool advertAlpha=p.pass>=57&&p.pass<=60;
    const bool advertMultiply=p.pass>=67&&p.pass<=70;
    StadiumLightmapState lightmapState(base,(p.pass>=50&&p.pass<=55)||advertAlpha||advertMultiply,advertAlpha,isWembley&&p.pass==18);
    PPCContext draw=ctx;draw.r3.u64=p.record;sub_8214D498(draw,base);lightmapState.Restore(ctx,base);
  }
  drawingLiteral=false;return true;
}



extern "C" bool pes6_camera_is_cinematic(uint8_t*);
extern "C" bool pes6_stadium_skip_cinematic_shade(uint8_t* base){
  return stadiumLightingContext&&pes6_camera_is_cinematic(base);
}
