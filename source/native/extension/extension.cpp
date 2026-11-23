// Experimental native-host bridge. Installed only after separate validation.
#include "../generated/default/pes6_funcs.h"
#include "../src/player_editor_runtime.h"
#include "../src/host_frame_pacer.h"
#include "../src/presentation_cadence.h"
#include "../src/frame_meter.h"
#include <windows.h>
#include <MinHook.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <chrono>
extern "C" void pes6_editor_frame(uint8_t*);
void Pr7OverlayPaint();
#include "../src/overlay_ui_tools.h"
extern "C" void pes6_show_keyboard_ui(PPCContext&,uint8_t*);
bool Pr7PreparePresentation(uintptr_t,const std::filesystem::path&,std::ofstream&);

extern "C" void pes6_native_sampler_fast(PPCContext&,uint8_t*);
extern "C" void sub_8214D498_native_venue(PPCContext&,uint8_t*);
extern "C" void sub_8215F610_native_venue(PPCContext&,uint8_t*);
extern "C" void sub_8216BC78_native_venue(PPCContext&,uint8_t*);
extern "C" void sub_8216C710_native_venue(PPCContext&,uint8_t*);
extern "C" void sub_8216C3A0_native_venue(PPCContext&,uint8_t*);
namespace {
struct Mapping {uint32_t guest,rva;};
constexpr Mapping mappings[]={
#include "generated/originals.inc"
};
struct Hook {uint32_t guest,rva;PPCFunc* function;const char* name;};
const Hook hooks[]={
#include "generated/hooks.inc"
};
uintptr_t host=0;
std::map<uint32_t,PPCFunc*> originals;
std::map<FARPROC,FARPROC> runtime_originals;
std::mutex initialize_mutex;
bool initialized=false;
std::filesystem::path status_path;
std::filesystem::path project_root;
using ImGuiRender=void(*)();
ImGuiRender original_imgui_render=nullptr;
void Pr7ImGuiRender(){Pr7OverlayPaint();original_imgui_render();}
PPCFunc* original_present=nullptr;
void Pr7Present(PPCContext& ctx,uint8_t* base){
 // Keep the native renderer's complete Present implementation. Only restore
 // the scene's original submission cadence, which its fixed 60 Hz path loses.
 // The engine also writes D3DDevice::PresentationInterval inline, bypassing
 // sub_82115B30. Read the effective value before Present consumes the device.
 // Observing only that setter leaves cutscenes paced at the last menu value.
 auto interval=pes6_requested_interval.load(std::memory_order_relaxed);
 const uint32_t device=ctx.r3.u32;
 uint32_t encoded=0;SIZE_T read=0;
 const bool from_device=device>=0x10000u&&device<0xdfff0000u&&
     ReadProcessMemory(GetCurrentProcess(),base+device+13220u,
                       &encoded,sizeof(encoded),&read)&&read==sizeof(encoded);
 if(from_device){
  interval=_byteswap_ulong(encoded);
  pes6_requested_interval.store(interval,std::memory_order_relaxed);
 }
 const int fps=Pes6OriginalSubmissionFps(interval);
 static Pes6HostFramePacer pacer;
 pacer.Wait(fps);
 pes6_target_fps.store(fps,std::memory_order_relaxed);
 static uint32_t reported=~0u;
 if(reported!=interval){
  reported=interval;
  std::ofstream report(project_root/"data/native-renderer/extension-cadence-status.txt",std::ios::trunc);
  report<<"scene_interval="<<interval<<"\ntarget_fps="<<fps
        <<"\ninterval_source="<<(from_device?"d3d_device":"setter_fallback")
        <<"\nnative_present=preserved\n";
  std::ofstream history(project_root/"data/native-renderer/extension-cadence-transitions.txt",std::ios::app);
  history<<"uptime_ms="<<GetTickCount64()<<" scene_interval="<<interval
         <<" target_fps="<<fps<<" source="<<(from_device?"d3d_device":"setter_fallback")<<'\n';
 }
 original_present(ctx,base);
}
void Pr7Input(PPCContext& ctx,uint8_t* base){
 const auto inputResult=ctx.r5.u32;
 __imp__XamInputGetState(ctx,base);
 if(pes6::ui_tools::g_keyboard.active.load(std::memory_order_acquire)){if(ctx.r3.u32==0&&inputResult)std::memset(base+inputResult+4,0,12);return;}
 static uint32_t reported=0;
 static uint32_t pending_db=0,initialized_db=0;
 static std::chrono::steady_clock::time_point ready_after{};
 static uint32_t last_flags=~0u;
 const auto db=REX_LOAD_U32(0x83139440u);
 if(!db||db<0x10000||db>=0xdff00000u)return;
 if(pending_db!=db){pending_db=db;ready_after=std::chrono::steady_clock::now()+std::chrono::milliseconds(100);}
 if(std::chrono::steady_clock::now()<ready_after)return;
 const auto record=db+446u*152u;
 uint8_t bytes[156]{};SIZE_T read=0;
 if(!ReadProcessMemory(GetCurrentProcess(),base+record,bytes,sizeof(bytes),&read)||read!=sizeof(bytes))return;
 if(!bytes[4]||strnlen(reinterpret_cast<const char*>(bytes+4),45)==45)return;
 // The title allocates the database before filling it. Input polling can run
 // during that interval; don't publish an empty database as an applied revision.
 if(initialized_db!=db){pes6::editor::Initialize(project_root.string());initialized_db=db;}
 pes6_editor_frame(base);
 if(!ReadProcessMemory(GetCurrentProcess(),base+record,bytes,sizeof(bytes),&read)||read!=sizeof(bytes))return;
 const uint32_t flags=(uint32_t(bytes[155])<<24)|(uint32_t(bytes[108])<<16)|(uint32_t(bytes[109])<<8)|bytes[113];
 if(reported==db&&last_flags==flags)return;
 reported=db;
 last_flags=flags;
 std::ofstream log(status_path,std::ios::trunc);
 log<<"database="<<std::hex<<db<<std::dec<<"\nplayer=446\nname="<<std::string(reinterpret_cast<const char*>(bytes+4),strnlen(reinterpret_cast<const char*>(bytes+4),45))<<"\nboot="<<unsigned(bytes[155])
    <<"\nheight="<<148u+(unsigned(bytes[108])>>2)<<"\nshirt="<<(unsigned(bytes[109])&1u)<<"\nsleeves="<<(unsigned(bytes[113])&3u)<<"\n";
}
}

PPCFunc* Pr7Original(uint32_t guest){
 if(const auto i=originals.find(guest);i!=originals.end())return i->second;
 const auto* i=std::lower_bound(std::begin(mappings),std::end(mappings),guest,[](const auto& a,uint32_t b){return a.guest<b;});
 if(i==std::end(mappings)||i->guest!=guest)std::abort();
 return reinterpret_cast<PPCFunc*>(host+i->rva);
}

FARPROC WINAPI Pr7GetProcAddress(HMODULE module,LPCSTR name){
 const auto result=::GetProcAddress(module,name);
 if(const auto i=runtime_originals.find(result);i!=runtime_originals.end())return i->second;
 return result;
}

extern "C" __declspec(dllexport) DWORD WINAPI Pr7Initialize(){
 std::lock_guard lock(initialize_mutex);if(initialized)return 1;
 host=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 wchar_t image[MAX_PATH]{};GetModuleFileNameW(nullptr,image,MAX_PATH);
 auto root=std::filesystem::path(image).parent_path();
 for(int i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
 project_root=root;
 std::filesystem::create_directories(root/"data/native-renderer");
 std::ofstream log(root/"data/native-renderer/extension.log",std::ios::trunc);
 status_path=root/"data/native-renderer/extension-player-status.txt";
 log<<"Experimental PR7 native compatibility bridge\n";
 if(MH_Initialize()!=MH_OK){log<<"MinHook initialization failed\n";return 0;}
 std::map<uint32_t,PPCFunc*> prepared;
 for(const auto& hook:hooks){
  void* trampoline=nullptr;
  const auto status=MH_CreateHook(reinterpret_cast<void*>(host+hook.rva),reinterpret_cast<void*>(hook.function),&trampoline);
  if(status!=MH_OK){log<<hook.name<<": "<<MH_StatusToString(status)<<"\n";MH_Uninitialize();return 0;}
  prepared[hook.guest]=reinterpret_cast<PPCFunc*>(trampoline);
  log<<hook.name<<" prepared\n";
 }
 originals=std::move(prepared);
 // Exact-build native replacement of the guest D3D Present entry. Wrapping
 // this entry preserves Plume rendering and back-pressure to the game loop.
 void* present_trampoline=nullptr;
 if(MH_CreateHook(reinterpret_cast<void*>(host+0x37909e0u),reinterpret_cast<void*>(&Pr7Present),&present_trampoline)!=MH_OK){
  log<<"Scene cadence bridge preparation failed\n";MH_Uninitialize();originals.clear();return 0;
 }
 original_present=reinterpret_cast<PPCFunc*>(present_trampoline);
 originals[0x82121e40u]=original_present;
 log<<"Native scene cadence bridge prepared (original 60/30 Hz)\n";
 if(const auto runtime=GetModuleHandleW(L"rexruntime.dll")){
  const auto target=::GetProcAddress(runtime,"__imp__XamInputGetState");
  void* trampoline=nullptr;
  if(!target||MH_CreateHook(reinterpret_cast<void*>(target),reinterpret_cast<void*>(&Pr7Input),&trampoline)!=MH_OK){
   log<<"Input bridge preparation failed\n";MH_Uninitialize();originals.clear();return 0;
  }
  runtime_originals[target]=reinterpret_cast<FARPROC>(trampoline);
  const auto keyboard=::GetProcAddress(runtime,"__imp__XamShowKeyboardUI");
  void* keyboard_trampoline=nullptr;
  if(!keyboard||MH_CreateHook(reinterpret_cast<void*>(keyboard),reinterpret_cast<void*>(&pes6_show_keyboard_ui),&keyboard_trampoline)!=MH_OK){log<<"Keyboard bridge preparation failed\n";MH_Uninitialize();originals.clear();return 0;}
  log<<"System keyboard bridge prepared\n";
  const auto render=::GetProcAddress(runtime,"?Render@ImGui@@YAXXZ");
  void* render_trampoline=nullptr;
  if(!render||MH_CreateHook(reinterpret_cast<void*>(render),reinterpret_cast<void*>(&Pr7ImGuiRender),&render_trampoline)!=MH_OK){log<<"Overlay bridge preparation failed\n";MH_Uninitialize();originals.clear();return 0;}
  original_imgui_render=reinterpret_cast<ImGuiRender>(render_trampoline);
 }
 pes6::editor::Initialize(root.string());
 if(!Pr7PreparePresentation(host,root,log)){MH_Uninitialize();originals.clear();return 0;}
 // MH_EnableHook applies its thread-freeze transaction after every trampoline is ready.
 const auto enabled=MH_EnableHook(MH_ALL_HOOKS);
 if(enabled!=MH_OK){log<<"Enable failed: "<<MH_StatusToString(enabled)<<"\n";MH_Uninitialize();originals.clear();return 0;}
 initialized=true;log<<std::size(hooks)<<" hooks enabled\n";return 1;
}
