#include <sstream>
#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "boot_catalog.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <windows.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <chrono>
#include <thread>
#include <array>
#include <algorithm>
#include <atomic>
#include "frame_meter.h"
#include "host_frame_pacer.h"
#include "presentation_cadence.h"
#include "player_editor_runtime.h"
#include "team_edit_runtime.h"
#include "kit_lettering_runtime.h"
#include <map>
#include "formation_text_layout.h"
extern "C" void pes6_editor_frame(uint8_t*);

namespace {
// Chelsea/Liverpool use their original archive kits and native descriptors.
// Keep the experimental assets on disk without loading them in the game.
constexpr bool custom_club_kits_enabled = false;
struct TimingLog {
  std::mutex mutex;
  bool stopped = false;
  bool timer_period = false;
};
TimingLog timing_log;
Pes6HostFramePacer frame_pacer;

using RuntimeImport = void (*)(PPCContext&, uint8_t*);

RuntimeImport FindRuntimeImport(const char* name) {
  HMODULE module = GetModuleHandleW(L"rexruntime.dll");
  auto proc = module ? GetProcAddress(module, name) : nullptr;
  if (!proc) REX_FATAL("PES6 stutter trace: missing runtime export {}", name);
  return reinterpret_cast<RuntimeImport>(proc);
}

int ParsePes6LangIndex(const char* env_name) {
  const char* val = std::getenv(env_name);
  if (!val || !*val) return -1;
  if (_stricmp(val, "en") == 0 || _stricmp(val, "e") == 0 || _stricmp(val, "eng") == 0) return 0;
  if (_stricmp(val, "fr") == 0 || _stricmp(val, "f") == 0 || _stricmp(val, "fre") == 0) return 1;
  if (_stricmp(val, "de") == 0 || _stricmp(val, "g") == 0 || _stricmp(val, "ger") == 0) return 2;
  if (_stricmp(val, "it") == 0 || _stricmp(val, "i") == 0 || _stricmp(val, "ita") == 0) return 3;
  if (_stricmp(val, "es") == 0 || _stricmp(val, "s") == 0 || _stricmp(val, "spa") == 0) return 4;
  return -1;
}
}  // namespace

void pes6_start_timing() {
  timing_log.timer_period = timeBeginPeriod(1) == TIMERR_NOERROR;
  pes6_target_fps.store(60, std::memory_order_relaxed);
  REXLOG_INFO("PES6 timing: Windows 1ms timer={}, original scene cadence (60/30 FPS)", timing_log.timer_period);
}

void pes6_stop_timing() {

  std::lock_guard lock(timing_log.mutex);
  timing_log.stopped = true;
  if (timing_log.timer_period) {
    timeEndPeriod(1);
    timing_log.timer_period = false;
  }
}

void pes6_pace_submission() {
  const int limit = Pes6OriginalSubmissionFps(
      pes6_requested_interval.load(std::memory_order_relaxed));
  pes6_target_fps.store(uint32_t(limit), std::memory_order_relaxed);
  frame_pacer.Wait(limit);
}

static void RecordPresentInterval(uint32_t requested) {
  // Scene requests determine the original 60/30 Hz cadence. Do not alter
  // the device field or the simulation clock.
  const auto previous = pes6_requested_interval.exchange(requested,
      std::memory_order_relaxed);
  if (requested != previous) {
    REXLOG_INFO("PES6 presentation interval: raw=0x{:08X}, refreshes={}",
        requested, Pes6PresentationRefreshes(requested));
  }
}

extern "C" void pes6_record_present_interval(PPCContext& ctx) {
  RecordPresentInterval(ctx.r4.u32);
}

extern "C" void pes6_override_language_set(PPCContext& ctx) {
  static const int text_lang = ParsePes6LangIndex("PES6_TEXT_LANG");
  static const int comm_lang = ParsePes6LangIndex("PES6_COMM_LANG");
  if (ctx.r4.u32 == 1 && text_lang >= 0) {
    ctx.r5.u64 = uint32_t(text_lang);
  } else if (ctx.r4.u32 == 0 && comm_lang >= 0) {
    ctx.r5.u64 = uint32_t(comm_lang);
  }
}

extern "C" void pes6_configure_render_size(PPCContext& ctx, uint8_t* base) {
  const char* width_text = std::getenv("PES6_RENDER_WIDTH");
  const char* height_text = std::getenv("PES6_RENDER_HEIGHT");
  if (!width_text || !height_text) return;
  const int width = std::atoi(width_text);
  const int height = std::atoi(height_text);
  // These direct FSR inputs must reach device initialization at scale 1.
  // Dividing them by 2 recreates the small guest surfaces that freeze matches.
  const bool direct_fsr_input =
      (width == 1536 && height == 864) || (width == 1728 && height == 972);
  if ((!direct_fsr_input && (width < 640 || width > 1280 || height < 360 || height > 720)) ||
      width % 32 != 0 || width * 9 != height * 16) {
    REXLOG_WARN("PES6 rejected invalid render size {}x{}", width, height);
    return;
  }
  const uint32_t config = ctx.r3.u32;
  if (config < 0x00010000u || config >= 0xE0000000u) return;
  REX_STORE_U32(config + 8, uint32_t(width));
  REX_STORE_U32(config + 12, uint32_t(height));
  for (uint32_t offset : {0u, 16u}) {
    REX_STORE_U32(0x82EA49C0u + offset, uint32_t(width));
    REX_STORE_U32(0x82EA49C4u + offset, uint32_t(height));
  }
  REXLOG_WARN("PES6 configured guest render size: {}x{}", width, height);
}

extern "C" void pes6_override_afs_language(PPCContext& ctx) {
  static const int text_lang = ParsePes6LangIndex("PES6_TEXT_LANG");
  static const int comm_lang = ParsePes6LangIndex("PES6_COMM_LANG");
  if (ctx.r3.u32 == 2 && text_lang >= 0) {
    ctx.r4.u64 = uint32_t(text_lang);
  } else if (ctx.r3.u32 == 3 && comm_lang >= 0) {
    ctx.r4.u64 = uint32_t(comm_lang);
  }
}

extern "C" void pes6_replay_transition_trace(PPCContext&,uint8_t*,unsigned);
extern "C" REX_FUNC(__imp__VdSwap) {
  pes6_editor_frame(base);
  using Swap = void (*)(PPCContext&, uint8_t*);
  static const Swap original = [] {
    if (HMODULE module = GetModuleHandleW(L"rexruntime.dll")) {
      if (auto proc = GetProcAddress(module, "__imp__VdSwap"))
        return reinterpret_cast<Swap>(proc);
    }
    REX_FATAL("PES6 frame meter: runtime VdSwap export unavailable");
    return static_cast<Swap>(nullptr);
  }();
  const uint32_t w_ptr = REX_LOAD_U32(ctx.r1.u32 + 0x54);
  const uint32_t h_ptr = REX_LOAD_U32(ctx.r1.u32 + 0x5C);
  const uint32_t width = w_ptr ? REX_LOAD_U32(w_ptr) : 1280u;
  const uint32_t height = h_ptr ? REX_LOAD_U32(h_ptr) : 720u;
  if (pes6_frontbuffer_width.load(std::memory_order_relaxed) != width ||
      pes6_frontbuffer_height.load(std::memory_order_relaxed) != height) {
    pes6_frontbuffer_width.store(width, std::memory_order_relaxed);
    pes6_frontbuffer_height.store(height, std::memory_order_relaxed);
    REXLOG_INFO("PES6 actual frontbuffer: {}x{}; GPU scale={}", width, height,
        pes6_render_scale.load(std::memory_order_relaxed));
  }
  pes6_pace_submission();
  original(ctx,base);
  pes6_frame_submissions.fetch_add(1,std::memory_order_relaxed);
}

extern "C" bool pes6_keyboard_active();
extern "C" REX_FUNC(__imp__XamInputGetState) {
  const uint32_t state = ctx.r5.u32;
  static const auto original = FindRuntimeImport("__imp__XamInputGetState");
  original(ctx, base);
  if ((pes6::editor::open.load(std::memory_order_relaxed)||pes6_keyboard_active()) && ctx.r3.u32 == 0 && state)
    std::memset(base + state + 4, 0, 12);
}


// Expanded boots are delivered as validated unpacked bundles. This avoids the
// archive's asynchronous compressed-buffer dispatcher for the added entries.
// Load the donor through the ORIGINAL font resource, before any glyph is
// cached. Switching returned font objects under the same font ID mixed old
// atlas glyphs with new advances/baselines when moving between menus.
extern "C" void pes6_init_pes2008_font(PPCContext&,uint8_t*){}
extern "C" uint32_t pes6_font_override(uint8_t*,uint32_t,uint32_t original){return original;}
static std::atomic<bool> pes6_pes2008_font_active{false};
extern "C" void pes6_pes2008_text_offset(uint8_t* base,uint32_t node){
  if(!pes6_pes2008_font_active.load(std::memory_order_relaxed)||REX_LOAD_U32(node+148)!=0)return;
  const float y=std::bit_cast<float>(REX_LOAD_U32(node+20));
  if(std::isfinite(y))REX_STORE_U32(node+20,std::bit_cast<uint32_t>(y-2.f));
}
extern "C" bool pes6_cloth_resource(PPCContext&,uint8_t*);
extern "C" bool pes6_setpiece_resource(PPCContext&,uint8_t*);
extern "C" bool pes6_load_expanded_boot(PPCContext& ctx,uint8_t* base){
  if(pes6_setpiece_resource(ctx,base))return true;
  if(pes6_cloth_resource(ctx,base))return true;
  const auto request=ctx.r3.u32;
  if(request&&REX_LOAD_U32(request+8)==125){
    static std::mutex font_mutex;static std::vector<uint8_t> font_bytes;
    std::lock_guard lock(font_mutex);
    if(font_bytes.empty()){
      wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();
      for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
      std::ifstream file(root/"data/patches/fonts/wkm_pes2008.bin",std::ios::binary|std::ios::ate);
      if(file&&file.tellg()>48){font_bytes.resize(size_t(file.tellg()));file.seekg(0);file.read(reinterpret_cast<char*>(font_bytes.data()),font_bytes.size());}
    }
    if(font_bytes.empty())return false;
    PPCContext alloc=ctx;alloc.r3.u64=font_bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);
    const uint32_t output=alloc.r3.u32;if(!output)return false;
    std::memcpy(base+output,font_bytes.data(),font_bytes.size());
    pes6_pes2008_font_active.store(true,std::memory_order_relaxed);
    const auto previous=REX_LOAD_U32(request+16);
    if(base[request+24]&&ctx.r4.u32){PPCContext release=ctx;release.r3.u64=ctx.r4.u32;sub_82A00AB0(release,base);}
    REX_STORE_U32(request+16,output);REX_STORE_U32(request+20,uint32_t(font_bytes.size()));
    if(previous&&previous!=output&&previous!=ctx.r4.u32){PPCContext release=ctx;release.r3.u64=previous;sub_82A00AB0(release,base);}
    ctx.r3.u64=1;return true;
  }
  const auto id=REX_LOAD_U32(request+8)&0x00ffffffu;
  // The native decoder table has only entries0(copy) and1(zlib).
  // Report malformed archive input instead of dispatching into arbitrary data.
  const bool customCrest=false; // Crests are replaced at the native per-image decoder.
  const bool editorKit=REX_LOAD_U32(request+8)==id&&pes6_team_info::HasAtlas(id);
  const bool customKit=editorKit||(custom_club_kits_enabled&&(REX_LOAD_U32(request+8)==5874||REX_LOAD_U32(request+8)==5875||REX_LOAD_U32(request+8)==5880||REX_LOAD_U32(request+8)==5881));
  const bool numberFont=custom_club_kits_enabled&&REX_LOAD_U32(request+8)==id&&((id>=1965&&id<=1968)||(id>=1977&&id<=1980));
  const bool nameFont=custom_club_kits_enabled&&REX_LOAD_U32(request+8)==id&&((id>=1145&&id<=1148)||(id>=1157&&id<=1160));
  const bool customFont=numberFont||nameFont;
  static const auto assetRoot=[](){wchar_t assetExe[MAX_PATH]{};GetModuleFileNameW(nullptr,assetExe,MAX_PATH);auto root=std::filesystem::path(assetExe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root;}();
  // Evaluate once per resource load, never per draw/bind. Catalog files are immutable during play.
  static const auto editorPaths=[&](){std::map<unsigned,std::filesystem::path> result;std::ifstream input(assetRoot/"data/editor/asset-runtime.txt");std::string line;while(std::getline(input,line)){std::istringstream row(line);std::string kind;unsigned index,entry;if(row>>kind>>index>>entry&&(kind=="BOOT"||kind=="BALL")){auto f=assetRoot/"data/editor/assets"/((kind=="BOOT"?"boots-":"balls-")+std::to_string(index)+".bin");if(std::filesystem::exists(f))result[entry]=f;}}return result;}();
  const auto editorAsset=editorPaths.find(id);const bool customEditorAsset=REX_LOAD_U32(request+8)==id&&editorAsset!=editorPaths.end();
  const bool ballPreview=REX_LOAD_U32(request+8)==0x02000105u;
  const bool ballMenu=REX_LOAD_U32(request+8)==id&&id>=184&&id<=188;
  const bool customBall=ballPreview||ballMenu||(REX_LOAD_U32(request+8)==id&&id>=6731&&id<=6734);
  const bool customBoot=id>=pes6::editor::custom_boot_first_entry&&id<pes6::editor::custom_boot_first_entry+pes6::editor::custom_boot_count;
  const bool customStadium=(id>=6727&&id<=6730)||(id>=3021&&id<=3073)||id==0x02000002u;
  if(!customEditorAsset&&!customBoot&&!customCrest&&!customKit&&!customFont&&!customBall&&!customStadium&&base[request+121]!=1&&ctx.r4.u32&&ctx.r5.u32>=16){
    const auto input=ctx.r4.u32;
    MEMORY_BASIC_INFORMATION info{};
    if(VirtualQuery(base+input,&info,sizeof(info))&&info.State==MEM_COMMIT&&
       !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&
       base+input+16<=static_cast<uint8_t*>(info.BaseAddress)+info.RegionSize){
      const auto codec=base[input+2];
      if(codec>1){
        REXLOG_ERROR("PES6 rejected resource codec {}: key=0x{:08X}, bytes={}, header=0x{:08X}/0x{:08X}",
          codec,REX_LOAD_U32(request+8),ctx.r5.u32,REX_LOAD_U32(input),REX_LOAD_U32(input+4));
        if(base[request+24]){PPCContext release=ctx;release.r3.u64=input;sub_82A00AB0(release,base);}
        ctx.r3.u64=0;return true;
      }
    }
  }

  if(!customEditorAsset&&!customBoot&&!customCrest&&!customKit&&!customFont&&!customBall)return false;
  // Raw-file requests already bypass the decoder in the native function.
  if(base[request+121]==1)return false;
  const unsigned slot=(customKit||customCrest||customFont||customBall)?id:30+id-pes6::editor::custom_boot_first_entry;
  const unsigned fontIndex=numberFont?(id<1970?id-1965:4+id-1977):(id<1150?8+id-1145:12+id-1157);
  const unsigned cacheIndex=customFont?pes6::editor::custom_boot_count+6+fontIndex:customKit?pes6::editor::custom_boot_count+2+(id==5874?0:id==5875?1:id==5880?2:3):customCrest?pes6::editor::custom_boot_count+(id-2710):id-pes6::editor::custom_boot_first_entry;
  static std::mutex boot_mutex;
  static std::map<unsigned,std::vector<uint8_t>> bundles;
  std::lock_guard lock(boot_mutex);
  auto& bytes=bundles[customEditorAsset?30000+id:customBall?20000+id:editorKit?10000+id:cacheIndex];
  if(bytes.empty()){
    wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
    auto root=std::filesystem::path(exe).parent_path();
    for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
    const auto path=customEditorAsset?editorAsset->second:customBall&&id==6734?root/"data/balls/teamgeist-2006/teamgeist.bundle":customBall?root/"data/balls/aerow-ii"/(ballPreview?"preview.bundle":ballMenu?"menu-"+std::to_string(id)+".bundle":std::string(id==6731?"pl":id==6732?"winter":"calcio")+".bundle"):editorKit?root/"data/editor/kits/packages"/(std::to_string(id)+".bin"):customFont?root/"data/patches/kits/fonts"/(std::to_string(id)+".bin"):customKit?root/"data/patches/kits/packages"/(std::to_string(id)+".bin"):customCrest?root/"data/patches/premier-clubs/packages"/(std::to_string(id)+".bin"):root/"data/editor/boots/packages"/("boot-"+std::to_string(slot)+".bin");
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file||file.tellg()<56||file.tellg()>4000000){REXLOG_WARN("PES6 custom boot {}: missing or invalid bundle",slot);ctx.r3.u64=0;return true;}
    bytes.resize(size_t(file.tellg()));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    const auto be=[&](size_t at){return (uint32_t(bytes[at])<<24)|(uint32_t(bytes[at+1])<<16)|(uint32_t(bytes[at+2])<<8)|bytes[at+3];};
    const unsigned count=ballPreview?20:ballMenu?1:customFont?(numberFont?3:1):customKit?2:customCrest?207:4;
    bool valid=bool(file)&&bytes.size()>=8+count*12&&be(0)==count&&be(4)==8;
    for(unsigned i=0;i<count&&valid;i++){const uint64_t off=be(8+i*12),length=be(12+i*12);valid=off>=8+count*12&&off+length<=bytes.size();}
    if(customKit&&valid)for(unsigned i=0;i<2&&valid;i++){const size_t off=be(8+i*12),length=be(12+i*12);valid=length==263184&&bytes[off]=='W'&&bytes[off+1]=='E'&&bytes[off+2]=='0'&&bytes[off+3]=='0'&&bytes[off+6]==3&&bytes[off+7]==8&&bytes[off+8]==2&&bytes[off+9]==0&&bytes[off+10]==2&&bytes[off+11]==0&&bytes[off+4]==0x10&&bytes[off+5]==4&&be(off+12)==0x00100410;}
    if(customKit&&!valid){bytes.clear();REXLOG_ERROR("PES6 kit {}: incompatible palette format; native archive fallback",id);return false;}
    if(!valid){bytes.clear();REXLOG_WARN("PES6 custom boot {}: rejected bundle layout",slot);ctx.r3.u64=0;return true;}
  }
  PPCContext alloc=ctx;alloc.r3.u64=bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);
  const uint32_t output=alloc.r3.u32;
  if(!output){ctx.r3.u64=0;return true;}
  std::memcpy(base+output,bytes.data(),bytes.size());
  const auto previous=REX_LOAD_U32(request+16);
  // Same ownership rule as8219E398 for the temporary file input.
  if(base[request+24]&&ctx.r4.u32){PPCContext release=ctx;release.r3.u64=ctx.r4.u32;sub_82A00AB0(release,base);}
  REX_STORE_U32(request+16,output);REX_STORE_U32(request+20,uint32_t(bytes.size()));
  if(previous&&previous!=output&&previous!=ctx.r4.u32){PPCContext release=ctx;release.r3.u64=previous;sub_82A00AB0(release,base);}
  REXLOG_WARN("PES6 native resource {}: direct bundle loaded, {} bytes",slot,bytes.size());
  ctx.r3.u64=1;return true;
}

#include "real_team_names.h"

extern "C" bool pes6_home_stadium_name(PPCContext& ctx,uint8_t* base){return pes6_team_info::HomeName(ctx,base);}
extern "C" void pes6_home_stadium_select(PPCContext&,uint8_t* base){pes6_team_info::SelectHome(base);}
extern "C" bool pes6_editor_name_resource(PPCContext& ctx,uint8_t* base){return pes6_team_info::LetteringResource(ctx,base,false);}
extern "C" bool pes6_editor_number_resource(PPCContext& ctx,uint8_t* base){return pes6_team_info::LetteringResource(ctx,base,true);}
extern "C" void pes6_editor_copy_lettering(PPCContext& ctx,uint8_t*){pes6_team_info::CopyLettering(ctx.r3.u32,ctx.r4.u32);}
extern "C" bool pes6_editor_lettering_recolor(uint32_t descriptor,bool numbers){return pes6_team_info::LetteringRecolor(descriptor,numbers);}
extern "C" bool pes6_editor_lettering_nativo(uint8_t* base,uint32_t descriptor,uint32_t texA,uint32_t texB,bool numbers){return pes6_team_info::LetteringNativo(base,descriptor,texA,texB,numbers);}
extern "C" void pes6_editor_kit_model(PPCContext& ctx,uint8_t* base){
 unsigned team=ctx.r5.u32&0xffffu;if(team==214||team==215){if(!pes6_team_info::Readable(base,0x83139448,4))return;const auto match=pes6_team_info::U32(base,0x83139448);if(!pes6_team_info::Readable(base,match,12))return;const auto object=pes6_team_info::U32(base,match+(team==214?4:8));if(!pes6_team_info::Readable(base,object,8))return;team=pes6_team_info::U16(base,object+6);}
 if(team>=189||!pes6_team_info::Readable(base,0x83139440,4))return;const auto db=pes6_team_info::U32(base,0x83139440);if(!db)return;
 PPCContext lookup=ctx;lookup.r3.u64=db;lookup.r4.u64=team;sub_821783E0(lookup,base);pes6_team_info::PatchModels(base,team,lookup.r3.u32);
}
extern "C" void pes6_editor_collar_groups(PPCContext& ctx,uint8_t* base,uint32_t object,uint32_t kit){
 unsigned team=0,v=0;if(!pes6::crest::Identity(kit,team,v)||team>=189||!pes6_team_info::Readable(base,kit,84))return;
 const auto selected=pes6_team_info::models[team][v];if(selected[0]<0||selected[1]<0||selected[1]>1)return;
 const unsigned collar=unsigned(selected[1]);base[kit+6]=uint8_t(collar);const PPCContext saved=ctx;
 // Both cloth AND its matching skin strip must switch, on the final mesh instances.
 for(unsigned group=38;group<=41;group++){ctx=saved;ctx.r3.u64=object;ctx.r4.u64=group;ctx.r5.u64=((group<40)==(collar==0))?0x40001111u:0x40000000u;sub_821A6F78(ctx,base);}ctx=saved;
}
extern "C" void pes6_generic_kit_colors(PPCContext& ctx,uint8_t* base){
 const unsigned team=ctx.r5.u32&0xffffu;
 if(team>=189||!pes6_team_info::Readable(base,0x83139440,4))return;
 const auto db=pes6_team_info::U32(base,0x83139440);if(!db)return;
 PPCContext lookup=ctx;lookup.r3.u64=db;lookup.r4.u64=team;sub_821783E0(lookup,base);
 pes6_team_info::PatchColors(base,team,lookup.r3.u32);
}
extern "C" void pes6_apply_real_team_names(PPCContext& ctx,uint8_t* base){
  static std::mutex names_mutex;
  static std::array<std::array<uint32_t,2>,std::size(real_team_names)> strings{};
  std::lock_guard lock(names_mutex);
  for(size_t i=0;i<std::size(real_team_names);i++){
    const auto& team=real_team_names[i];
    for(unsigned part=0;part<2;part++){
      const auto at=team.at+4*part,expected=part?team.shortExpected:team.expected;
      const auto current=REX_LOAD_U32(at);
      if(current!=expected&&current!=strings[i][part])continue;
      if(!strings[i][part]){
        const char* text=part?team.abbreviation:team.name;
        const auto length=std::strlen(text)+1;
        PPCContext allocation=ctx;allocation.r3.u64=length;sub_826D61B0(allocation,base);
        const auto address=allocation.r3.u32;if(!address)continue;
        std::memcpy(base+address,text,length);strings[i][part]=address;
      }
      REX_STORE_U32(at,strings[i][part]);
    }
  }
  ApplyExternalTeamInfo(ctx,base);
}

#include "premier_crest_map.h"
#include "competition_logo_runtime.h"
#include "competition_presentation.h"
extern "C" void pes6_replay_transition_trace(PPCContext&,uint8_t*,unsigned){}
#include "competition_texture_source.h"
#include "soft_crowd.h"
extern "C" void pes6_competition_texture_source(uint8_t* base,uint32_t source){CompetitionTextureSource(base,source);}
extern "C" void pes6_soft_crowd_source(uint8_t* base,uint32_t source){CompetitionTextureSource(base,source);SoftCrowdSource(base,source);}
#include "loading_logo_decode.h"
extern "C" bool pes6_decode_kit_preview(PPCContext&,uint8_t*);
#include "overlay_ui_tools.h"
#include <rex/system/kernel_state.h>
#include <rex/system/xthread.h>
#include "xbox_keyboard.h"

namespace {
pes6::ui_tools::KeyboardState* SharedKeyboard() {
 if (HMODULE mod = GetModuleHandleW(L"pes6-native-extension.dll")) {
  using Fn = pes6::ui_tools::KeyboardState* (*)();
  if (auto fn = reinterpret_cast<Fn>(GetProcAddress(mod, "Pes6GetKeyboardState"))) return fn();
 }
 return &pes6::ui_tools::g_keyboard;
}
}

extern "C" bool pes6_keyboard_active(){return SharedKeyboard()->active.load(std::memory_order_acquire);}
extern "C" void pes6_show_keyboard_ui(PPCContext& ctx, uint8_t* base){
 const uint32_t defPtr = ctx.r5.u32;
 const uint32_t outPtr = ctx.r8.u32;
 const uint32_t maxChars = ctx.r9.u32;
 const uint32_t ovPtr = ctx.r10.u32;
 if(!outPtr||!maxChars){ctx.r3.u64=87;return;}
 std::string initial;
 if(defPtr && maxChars){
  for(uint32_t i=0; i<maxChars && i<128; i++){
   uint16_t ch = (uint16_t(base[defPtr + i*2]) << 8) | base[defPtr + i*2 + 1];
   if(!ch) break;
   if(ch < 128) initial.push_back(char(ch));
   else if(ch < 2048){initial.push_back(char(0xc0|(ch>>6)));initial.push_back(char(0x80|(ch&63)));}
   else {initial.push_back(char(0xe0|(ch>>12)));initial.push_back(char(0x80|((ch>>6)&63)));initial.push_back(char(0x80|(ch&63)));}
  }
 }
 auto& kb = *SharedKeyboard();
 {
  std::lock_guard<std::mutex> lock(kb.mutex);
  if(kb.active.load(std::memory_order_acquire)){ctx.r3.u64=170;return;}
  kb.text = initial;
  kb.max_chars = maxChars ? maxChars - 1 : 63;
  kb.row = 0; kb.col = 1; kb.shift = false; kb.symbols = false; kb.accepted = false;
  kb.cursor = kb.text.size(); kb.accents = false; kb.left_trigger = kb.right_trigger = true;
  kb.prev_buttons = 0xFFFF;
  kb.last_move = GetTickCount64();
  kb.guest_base = base;
  kb.out_ptr = outPtr;
  kb.ov_ptr = ovPtr;
  kb.finished.store(false, std::memory_order_release);
  if(ovPtr){REX_STORE_U32(ovPtr,997u);REX_STORE_U32(ovPtr+4,0);REX_STORE_U32(ovPtr+8,rex::system::XThread::GetCurrentThreadHandle());}
  kb.active.store(true, std::memory_order_release);
 }
 if(!ShowXboxKeyboard(kb)){
  std::lock_guard<std::mutex> lock(kb.mutex);kb.active.store(false);kb.accepted=false;kb.guest_base=nullptr;kb.finished.store(true);
  if(ovPtr)if(auto* kernel=rex::system::KernelState::shared())kernel->CompleteOverlappedEx(ovPtr,0,1223,0);
  REXLOG_ERROR("PES6 keyboard: no runtime UI drawer; request cancelled without hiding the game");
  ctx.r3.u64=ovPtr?997:1223;return;
 }
 if(ovPtr)ctx.r3.u64=997;
 else {while(!kb.finished.load(std::memory_order_acquire))std::this_thread::sleep_for(std::chrono::milliseconds(1));ctx.r3.u64=kb.accepted?0:1223;}
}

extern "C" bool pes6_decode_premier_crest(PPCContext& ctx,uint8_t* base){

 if(DecodeLoadingLogo(ctx,base))return true;
 if(pes6_competition_presentation::Decode(ctx,base))return true;
 if(DecodeCompetitionLogo(ctx,base))return true;
 if(pes6_decode_kit_preview(ctx,base))return true;
 const uint32_t state=ctx.r3.u32,input=ctx.r4.u32;if(!state||!input)return false;
 const auto length=REX_LOAD_U32(input+8);bool candidate=false;for(const auto& r:premier_crest_sources)if(r.compressed==length)candidate=true;if(!candidate)return false;
 uint64_t hash=14695981039346656037ull;for(uint32_t i=0;i<length;i++)hash=(hash^base[input+16+i])*1099511628211ull;
 unsigned index=0;for(;index<std::size(premier_crest_sources);index++)if(premier_crest_sources[index].compressed==length&&premier_crest_sources[index].hash==hash)break;if(index==std::size(premier_crest_sources))return false;
 static std::mutex mutex;static std::array<std::vector<uint8_t>,std::size(premier_crest_sources)> cache;std::lock_guard lock(mutex);auto& bytes=cache[index];const auto& source=premier_crest_sources[index];
 if(bytes.empty()){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::ifstream file(root/"data/patches/premier-clubs"/(std::to_string(source.team)+"-"+std::to_string(source.size)+".we"),std::ios::binary|std::ios::ate);if(!file||file.tellg()!=16+source.size*source.size*4)return false;bytes.resize(size_t(file.tellg()));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!file||bytes[7]!=32){bytes.clear();return false;}}
 PPCContext alloc=ctx;alloc.r3.u64=bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);if(!alloc.r3.u32)return false;std::memcpy(base+alloc.r3.u32,bytes.data(),bytes.size());REX_STORE_U32(state+8,input+16);REX_STORE_U32(state+12,alloc.r3.u32);REX_STORE_U32(state+16,length);REX_STORE_U32(state+20,uint32_t(bytes.size()));ctx.r3.u64=1;/* Per-load diagnostic disabled: no disk logging on crest refresh. */return true;

}

// Licensed Adidas geometry/material setup, with Premier League name/number assets.
extern "C" void pes6_patch_club_kit(PPCContext& ctx,uint8_t* base,uint32_t kit,uint32_t team,uint32_t role,uint32_t variant){
 pes6_team_info::Load();const unsigned crestTeam=pes6_team_info::ResolveTeam(base,team);
 if(crestTeam<189&&role<2&&pes6_team_info::Readable(base,variant,1))pes6::crest::Register(kit,crestTeam,role+2*((base[variant]>>(role?0:3))&1));
 pes6_team_info::PatchAtlas(base,kit,team,role,variant);
 pes6_team_info::PatchLettering(base,kit,team,role,variant);
 pes6_team_info::PatchDescriptorModel(base,kit,team,role,variant);
 if(!custom_club_kits_enabled)return;
 team&=0xffffu;
 if((team!=69&&team!=72)||role>1||!kit||!variant)return;
 const bool away=((base[variant]>>(role?0:3))&1u)!=0;
 const uint32_t modelDonor=role&&!away?(team==69?8u:115u):153u;
 PPCContext donor=ctx;donor.r3.u64=kit;donor.r4.u64=role;donor.r5.u64=modelDonor;donor.r6.u64=variant;sub_822FB9E8(donor,base);
 PPCContext alloc=ctx;alloc.r3.u64=144;sub_826D61B0(alloc,base);const auto numbers=alloc.r3.u32;
 if(numbers){std::memset(base+numbers,0,144);PPCContext font=ctx;font.r3.u64=numbers;font.r4.u64=role;font.r5.u64=64;font.r6.u64=variant;sub_822FB9E8(font,base);std::memcpy(base+kit+52,base+numbers+52,28);if(role&&!away){base[kit+1]=1;std::memcpy(base+kit+2,base+numbers+2,2);}PPCContext release=ctx;release.r3.u64=numbers;sub_8280E7B8(release,base);}
 // Keep the native Premier League glyphs, but set each garment's ink separately.
 // Descriptor pairs: shirt number +56, shorts number +64, player name +72.
 REX_STORE_U16(kit+52,team*4u+role*2u+unsigned(away));
 const auto rgba5551=[](unsigned r,unsigned g,unsigned b){return uint16_t(((r>>3)<<11)|((g>>3)<<6)|((b>>3)<<1)|1);};
 const uint16_t white=0xffff,navy=rgba5551(18,56,118),red=rgba5551(187,32,54),dark=rgba5551(24,31,39);
 uint16_t shirt=white,shorts=white,name=white,outline=dark;
 if(role){if(away){shirt=name=team==69?navy:red;shorts=team==69?navy:red;outline=team==69?white:rgba5551(244,217,80);}}
 else if((team==69&&!away)||(team==72&&away)){shirt=name=dark;shorts=white;outline=white;}
 const auto color=[&](uint32_t offset,uint16_t value){REX_STORE_U32(kit+offset,(uint32_t(value)<<16)|(REX_LOAD_U32(kit+offset)&0xffffu));};
 color(56,shirt);color(60,outline);color(64,shorts);color(68,shorts==white?dark:white);color(72,name);color(76,outline);
 REX_STORE_U32(kit+12,team);
 // Resource-entry+16 contains the full kit descriptor. The texture cache
 // copies that descriptor to +192: mode +208 = kit+16, index +212 = kit+20.
 REX_STORE_U32(kit+16,0);REX_STORE_U32(kit+20,team*2u+role);
 static std::atomic<unsigned> reported{0};const unsigned bit=1u<<((team==69?0:2)+role);
 if(!(reported.fetch_or(bit)&bit))REXLOG_WARN("PES6 licensed kit: team={} role={} mode={} archive entry={}",team,role,REX_LOAD_U32(kit+16),5736+REX_LOAD_U32(kit+20));
}

#include "kit_preview_map.h"
extern "C" bool pes6_decode_kit_preview(PPCContext& ctx,uint8_t* base){
 // User previews are resolved from the original compressed source signature.
 const uint32_t editorState=ctx.r3.u32,editorInput=ctx.r4.u32;
 if(editorState&&editorInput){
  struct Row{uint32_t length;uint64_t hash;unsigned team;};
  static std::filesystem::path previewRoot;static const auto rows=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);previewRoot=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(previewRoot/"game/cv.img");i++)previewRoot=previewRoot.parent_path();std::vector<Row> result;std::ifstream in(previewRoot/"data/editor/kit-previews/map.txt");Row row;while(in>>std::dec>>row.length>>std::hex>>row.hash>>std::dec>>row.team)if(row.team<189&&row.length<4000000)result.push_back(row);return result;}();
  const auto compressed=REX_LOAD_U32(editorInput+8);bool candidate=false;for(const auto& row:rows)if(row.length==compressed)candidate=true;
  if(candidate){uint64_t hash=14695981039346656037ull;for(unsigned i=0;i<compressed;i++)hash=(hash^base[editorInput+16+i])*1099511628211ull;for(const auto& row:rows)if(row.length==compressed&&row.hash==hash){
   static std::mutex userMutex;static std::map<unsigned,std::vector<uint8_t>> cache;std::lock_guard lock(userMutex);auto& bytes=cache[row.team];
   if(bytes.empty()){std::ifstream in(previewRoot/"data/editor/kit-previews"/(std::to_string(row.team)+".bin"),std::ios::binary|std::ios::ate);if(!in||in.tellg()<32||in.tellg()>10000000)return false;bytes.resize(size_t(in.tellg()));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!in){bytes.clear();return false;}}
   PPCContext alloc=ctx;alloc.r3.u64=bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);if(!alloc.r3.u32)return false;std::memcpy(base+alloc.r3.u32,bytes.data(),bytes.size());REX_STORE_U32(editorState+8,editorInput+16);REX_STORE_U32(editorState+12,alloc.r3.u32);REX_STORE_U32(editorState+16,compressed);REX_STORE_U32(editorState+20,uint32_t(bytes.size()));ctx.r3.u64=1;return true;
  }}
 }
 // Independent HD selection previews. Do not enable the legacy kit overrides.
 const uint32_t state=ctx.r3.u32,input=ctx.r4.u32;if(!state||!input)return false;const auto length=REX_LOAD_U32(input+8);bool candidate=false;for(const auto& row:kit_preview_sources)if(row.compressed==length)candidate=true;if(!candidate)return false;
 uint64_t hash=14695981039346656037ull;for(uint32_t i=0;i<length;i++)hash=(hash^base[input+16+i])*1099511628211ull;
 const KitPreviewSource* source=nullptr;for(const auto& row:kit_preview_sources)if(row.compressed==length&&row.hash==hash){source=&row;break;}if(!source)return false;
 static std::mutex mutex;static std::array<std::vector<uint8_t>,std::size(kit_preview_clubs)> cache;std::lock_guard lock(mutex);auto& bytes=cache[source->club];
 if(bytes.empty()){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::ifstream file(root/"data/patches/kits/previews/hd"/(std::string(kit_preview_clubs[source->club])+".bin"),std::ios::binary|std::ios::ate);if(!file||file.tellg()!=8388696)return false;bytes.resize(size_t(file.tellg()));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());if(!file){bytes.clear();return false;}
 const auto be=[&](size_t p){return uint32_t(bytes[p])<<24|uint32_t(bytes[p+1])<<16|uint32_t(bytes[p+2])<<8|bytes[p+3];};bool valid=be(0)==2&&be(4)==8;for(unsigned i=0;i<2;i++){const size_t at=32+i*4194320;valid=valid&&be(8+i*12)==at&&be(12+i*12)==4194320&&std::memcmp(bytes.data()+at,"WE00",4)==0&&bytes[at+6]==3&&bytes[at+7]==32&&be(at+8)==0x04000400&&be(at+12)==16;}if(!valid){bytes.clear();return false;}}
 PPCContext alloc=ctx;alloc.r3.u64=bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);if(!alloc.r3.u32)return false;std::memcpy(base+alloc.r3.u32,bytes.data(),bytes.size());REX_STORE_U32(state+8,input+16);REX_STORE_U32(state+12,alloc.r3.u32);REX_STORE_U32(state+16,length);REX_STORE_U32(state+20,uint32_t(bytes.size()));ctx.r3.u64=1;REXLOG_WARN("PES6 licensed uniform-selection preview: {}",kit_preview_clubs[source->club]);return true;
}

#include "menu_carousel.h"
#include "edit_menu_effects.inl"
