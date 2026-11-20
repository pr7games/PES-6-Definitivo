#pragma once
#include "menu_carousel_logic.h"
#include "menu_texture_upload.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <random>
#include <windows.h>
namespace {
std::atomic<uint32_t> menu_adriano_texture{0};
std::atomic<uint32_t> menu_adriano_gpu{0};
std::atomic<uint32_t> menu_adriano_resource{0};
std::array<std::atomic<uint32_t>,Pes6MenuCarouselClock::portrait_count-1> menu_carousel_resources{};
std::array<uint32_t,Pes6MenuCarouselClock::portrait_count-1> menu_carousel_gpus{};
std::atomic<unsigned> menu_carousel_index{0};
std::atomic<uint32_t> menu_portrait_mesh_scratch{0};
std::atomic<bool> menu_carousel_visible{false};
std::atomic<bool> main_menu_font_active{false};
std::array<uint32_t,Pes6MenuCarouselClock::portrait_count-1> menu_carousel_textures{};
std::mutex menu_carousel_mutex;
Pes6MenuCarouselClock menu_carousel_clock;
thread_local bool menu_carousel_loading=false;
thread_local unsigned menu_rgba_scope_depth=0;
bool MenuReadable(uint8_t* base,uint32_t at,size_t size){
  if(at<0x10000||uint64_t(at)+size>=0xe0000000ull)return false;
  const uint8_t* p=base+at;
  struct Region{const uint8_t* start=nullptr,*end=nullptr;};
  thread_local std::array<Region,4> cache{};
  thread_local unsigned next_slot=0;
  for(const auto& r:cache){if(r.start&&p>=r.start&&p+size<=r.end)return true;}
  MEMORY_BASIC_INFORMATION info{};
  if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
  const auto* rs=static_cast<const uint8_t*>(info.BaseAddress);const auto* re=rs+info.RegionSize;
  cache[next_slot]={rs,re};next_slot=(next_slot+1)&3u;
  return p+size<=re;
}
std::filesystem::path MenuCarouselRoot(){
  wchar_t file[MAX_PATH]{};GetModuleFileNameW(nullptr,file,MAX_PATH);auto root=std::filesystem::path(file).parent_path();
  for(unsigned i=0;i<7&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root;
}
void MenuTrace(const char* event,uint32_t a=0,uint32_t b=0){
#ifndef PES6_MENU_CAROUSEL_FIXTURE
  std::ofstream out(MenuCarouselRoot()/"data/menu-carousel/runtime.log",std::ios::app);
  if(out)out<<GetTickCount64()<<" "<<event<<" "<<std::hex<<a<<" "<<b<<"\n";
#endif
}
void LoadCarouselTextures(PPCContext ctx,uint8_t* base){
  if(menu_carousel_loading)return;menu_carousel_loading=true;
  Pes6MenuRgbaScope rgba_scope;
  const auto root=MenuCarouselRoot();
  if(!menu_portrait_mesh_scratch.load()){PPCContext alloc=ctx;alloc.r3.u64=64;sub_826D61B0(alloc,base);menu_portrait_mesh_scratch.store(alloc.r3.u32,std::memory_order_release);}
  for(unsigned i=0;i<menu_carousel_textures.size();i++){
    if(menu_carousel_textures[i])continue;
    const auto filename=root/"data/menu-carousel"/((i<9?"0":"")+std::to_string(i+1)+".we");
    std::ifstream input(filename,std::ios::binary|std::ios::ate);if(!input)continue;
    const auto bytes=input.tellg();if(bytes!=16+512*512*4&&bytes!=16+1024*1024*4)continue;
    const unsigned size=unsigned(std::streamoff(bytes));
    PPCContext alloc=ctx;alloc.r3.u64=size+64;sub_826D61B0(alloc,base);const uint32_t data=alloc.r3.u32;
    if(!data)continue;
    if(!MenuReadable(base,data,size+64)){PPCContext release=ctx;release.r3.u64=data;sub_8280E7B8(release,base);continue;}
    input.seekg(0);input.read(reinterpret_cast<char*>(base+data),size);
    if(input&&std::memcmp(base+data,"WE00",4)==0&&base[data+6]==3&&base[data+7]==32&&
       (REX_LOAD_U16(data+8)==512||REX_LOAD_U16(data+8)==1024)&&
       REX_LOAD_U16(data+10)==REX_LOAD_U16(data+8)&&size==16+unsigned(REX_LOAD_U16(data+8))*unsigned(REX_LOAD_U16(data+8))*4&&REX_LOAD_U32(data+12)==16){
      const auto name=std::string("pes6_menu_carousel_")+std::to_string(i+1);std::memcpy(base+data+size,name.c_str(),name.size()+1);
      PPCContext load=ctx;load.r3.u64=data;load.r4.u64=data+size;sub_82736390(load,base);
      if(load.r3.u32&&MenuReadable(base,load.r3.u32,44)){
        menu_carousel_textures[i]=load.r3.u32;const auto texture=REX_LOAD_U32(load.r3.u32+36);
        if(MenuReadable(base,texture,56)){
          menu_carousel_gpus[i]=texture;PPCContext filter=ctx;filter.r3.u64=texture;filter.r4.u64=0;filter.r5.u64=3;sub_8218FB18(filter,base);
          // Filtering may allocate or replace the GPU handle. Read AFTER it.
          menu_carousel_resources[i]=REX_LOAD_U32(texture+20);MenuTrace("portrait-loaded",i+1,menu_carousel_resources[i]);
        }
      }
    }
    PPCContext release=ctx;release.r3.u64=data;sub_8280E7B8(release,base);
  }
  menu_carousel_loading=false;
}
}
#include "loading_logo.h"
extern "C" void pes6_menu_rgba_scope(bool enter){
  if(enter)++menu_rgba_scope_depth;
  else if(menu_rgba_scope_depth)--menu_rgba_scope_depth;
}
extern "C" bool pes6_menu_rgba_pixels(uint8_t* base,uint32_t texture,uint32_t output){
  if(!menu_rgba_scope_depth)return false;
  if(!MenuReadable(base,texture,56)||REX_LOAD_U32(texture+28)!=3)return false;
  const unsigned width=REX_LOAD_U16(texture+46),height=REX_LOAD_U16(texture+48);
  if(!width||!height||width>2048||height>2048)return false;
  const auto input=REX_LOAD_U32(texture+12);const size_t size=size_t(width)*height*4;
  if(!MenuReadable(base,input,size)||!MenuReadable(base,output,size))return false;
  if(uint64_t(input)<uint64_t(output)+size&&uint64_t(output)<uint64_t(input)+size)return false;
  const auto* pixels=base+input;auto* converted=base+output;
  // Exact byte order of 8218F830's WE32 loop (guest ARGB). A native loop can
  // vectorize this shuffle instead of executing PPC rotates per pixel.
  for(size_t i=0;i<size;i+=4){
    converted[i]=pixels[i+3];converted[i+1]=pixels[i];
    converted[i+2]=pixels[i+1];converted[i+3]=pixels[i+2];
  }
  return true;
}
extern "C" bool pes6_loading_logo_source(uint8_t* base,uint32_t source){
  return !pes6_loading_logo::loading&&MenuReadable(base,source,16)&&pes6_loading_logo::Source(base,source);
}
extern "C" void pes6_loading_logo_gpu(PPCContext& ctx,uint8_t* base,bool recognized){
  pes6_loading_logo::GpuLoaded(ctx,base,recognized);
}
extern "C" void pes6_loading_logo_gpu_released(uint32_t gpu){pes6_loading_logo::GpuReleased(gpu);}
extern "C" void pes6_menu_carousel_loaded(PPCContext& ctx,uint8_t* base,uint32_t source){
  pes6_loading_logo::Loaded(ctx,base,source);
  if(menu_carousel_loading||!ctx.r3.u32||!MenuReadable(base,source,36))return;
  if(std::memcmp(base+source,"WE00",4)||REX_LOAD_U16(source+4)!=0x4ec3||base[source+7]!=255||
     std::memcmp(base+source+16,"DDS ",4)||std::memcmp(base+source+28,"\x00\x02\x00\x00\x00\x02\x00\x00",8))return;
  std::lock_guard lock(menu_carousel_mutex);menu_adriano_texture.store(ctx.r3.u32,std::memory_order_release);
  if(MenuReadable(base,ctx.r3.u32,44)){const auto gpu=REX_LOAD_U32(ctx.r3.u32+36);menu_adriano_gpu.store(gpu,std::memory_order_release);
    if(MenuReadable(base,gpu,56))menu_adriano_resource.store(REX_LOAD_U32(gpu+20),std::memory_order_release);
  }
  MenuTrace("original-ui",ctx.r3.u32,menu_adriano_resource.load());
  LoadCarouselTextures(ctx,base);
}
extern "C" void pes6_menu_carousel_released(uint8_t* base,uint32_t texture){
  pes6_loading_logo::Released(base,texture);
  if(texture!=menu_adriano_texture.load(std::memory_order_acquire)||!MenuReadable(base,texture,44))return;
  if(REX_LOAD_U32(texture+40)<=1){main_menu_font_active.store(false,std::memory_order_release);menu_adriano_texture.store(0,std::memory_order_release);menu_adriano_resource.store(0,std::memory_order_release);menu_adriano_gpu.store(0,std::memory_order_release);}
}
extern "C" uint32_t pes6_menu_carousel_texture(uint32_t texture){
  if(!texture||texture!=menu_adriano_texture.load(std::memory_order_acquire))return texture;
  const auto index=menu_carousel_index.load(std::memory_order_acquire);
  return index&&menu_carousel_textures[index-1]?menu_carousel_textures[index-1]:texture;
}

extern "C" void pes6_menu_carousel_gpu_loaded(PPCContext& ctx,uint8_t* base,uint32_t source){
  if(menu_carousel_loading||!ctx.r3.u32||!MenuReadable(base,source,36))return;
  if(std::memcmp(base+source,"WE00",4)||REX_LOAD_U16(source+4)!=0x4ec3||base[source+7]!=255||
     std::memcmp(base+source+16,"DDS ",4)||std::memcmp(base+source+28,"\x00\x02\x00\x00\x00\x02\x00\x00",8))return;
  menu_adriano_gpu.store(ctx.r3.u32,std::memory_order_release);
  if(MenuReadable(base,ctx.r3.u32,56))menu_adriano_resource.store(REX_LOAD_U32(ctx.r3.u32+20),std::memory_order_release);
  MenuTrace("original-gpu-constructor",ctx.r3.u32,menu_adriano_resource.load());
}
extern "C" void pes6_menu_carousel_bind(PPCContext& ctx,uint8_t* base){
  pes6_loading_logo::Bind(ctx,base,GetTickCount64());
  if(ctx.r3.u32!=0)return;
  const auto resource=menu_adriano_resource.load(std::memory_order_acquire);
  if(!resource||ctx.r4.u32!=resource||menu_carousel_loading)return;
  const auto index=menu_carousel_index.load(std::memory_order_acquire);
  if(index&&menu_carousel_resources[index-1])ctx.r4.u64=menu_carousel_resources[index-1];
}

// The menu controller, rather than arbitrary texture binds, owns timing and
// visibility. Its constructor leaves GPU handles at zero until later upload.
void MenuCarouselTickAt(PPCContext& ctx,uint8_t* base,uint32_t controller,uint64_t now){
  const auto state=REX_LOAD_U32(controller+164);
  const auto node=REX_LOAD_U32(controller+180);
  main_menu_font_active.store(state<=4&&!base[controller+196],std::memory_order_release);
  const bool visible=(state==3||state==4)&&!base[controller+196]&&node&&base[node+53];
  const bool was_visible=menu_carousel_visible.exchange(visible,std::memory_order_acq_rel);
  if(visible&&!was_visible)MenuTrace("native-menu-visible",controller,node);
  if(!visible){std::lock_guard lock(menu_carousel_mutex);menu_carousel_clock.Leave();return;}
  unsigned before,index;bool restart;
  {
    std::lock_guard lock(menu_carousel_mutex);
    const auto gpu=menu_adriano_gpu.load(std::memory_order_acquire);
    // Refresh owned objects once per menu update.
    if(gpu)menu_adriano_resource.store(REX_LOAD_U32(gpu+20),std::memory_order_release);
    if(gpu&&!menu_carousel_textures[0])LoadCarouselTextures(ctx,base);
    for(unsigned i=0;i<menu_carousel_gpus.size();i++)if(menu_carousel_gpus[i])menu_carousel_resources[i]=REX_LOAD_U32(menu_carousel_gpus[i]+20);
    if(!menu_carousel_clock.seen){
      std::mt19937 rng{std::random_device{}()};
      std::shuffle(menu_carousel_clock.order.begin(),menu_carousel_clock.order.end(),rng);
    }
    before=menu_carousel_clock.index;index=menu_carousel_clock.Visit(now,0);
    restart=before!=index&&menu_carousel_clock.timed_change;
    menu_carousel_index.store(index,std::memory_order_release);
  }
  if(before!=index)MenuTrace(restart?"timed-change":"return-change",index,menu_adriano_resource.load());
  if(restart){
    // Exactly the portrait/group entrance sequence from native 826EFC98.
    // No fabricated opacity or vertex animation: use its original keys 1..40.
    PPCContext animation=ctx;animation.r3.u64=node;animation.f1.f64=1.f;sub_8272C000(animation,base);
    animation.r3.u64=node;animation.r4.u64=1;sub_8272C080(animation,base);
    animation.r3.u64=node;animation.r4.u64=40;sub_8272C088(animation,base);
    animation.r3.u64=node;animation.r4.u64=0;sub_8272C090(animation,base);
    animation.r3.u64=node;animation.r4.u64=1;sub_8272C0A0(animation,base);
  }
}
extern "C" bool pes6_main_menu_font_active(){
  return main_menu_font_active.load(std::memory_order_acquire);
}
extern "C" void pes6_menu_carousel_tick(PPCContext& ctx,uint8_t* base,uint32_t controller){
  MenuCarouselTickAt(ctx,base,controller,GetTickCount64());
}
extern "C" uint32_t pes6_menu_carousel_draw(PPCContext&,uint8_t* base,uint32_t,uint32_t texture){
  texture=pes6_loading_logo::Draw(texture,GetTickCount64());
  if(!texture)return texture;
  const auto gpu=menu_adriano_gpu.load(std::memory_order_acquire);
  if(!gpu)return texture;
  if(texture!=menu_adriano_texture.load(std::memory_order_acquire)){
    if(REX_LOAD_U32(texture+36)!=gpu)return texture;
    menu_adriano_texture.store(texture,std::memory_order_release);
  }
  return pes6_menu_carousel_texture(texture);
}


// All new portraits fill the native height. This compensates their horizontal
// atlas resampling, widening only their real mesh to the left. Adriano is stock.
extern "C" void pes6_menu_portrait_mesh(PPCContext& ctx,uint8_t* base,uint32_t mesh,uint32_t owner,uint32_t interpolated){
  const auto submit=[&](){if(interpolated)sub_8273BEA0(ctx,base);else sub_8273BCE8(ctx,base);};
  if(!menu_carousel_index.load(std::memory_order_acquire)||base[mesh+3]!=3){submit();return;}
  const uint32_t original_gpu=menu_adriano_gpu.load(std::memory_order_acquire),resource=REX_LOAD_U32(owner+128);
  if(!original_gpu||!resource){submit();return;}
  const uint32_t textures=REX_LOAD_U32(resource+40),texture=textures?REX_LOAD_U32(textures+12):0;
  if(!texture||REX_LOAD_U32(texture+36)!=original_gpu){submit();return;}
  const auto count=interpolated?ctx.r9.u32:ctx.r8.u32;
  const auto scratch=menu_portrait_mesh_scratch.load(std::memory_order_acquire);
  if(count!=4||!scratch){submit();return;}
  const auto copy=[&](uint32_t source,uint32_t target){
    std::memcpy(base+target,base+source,24);
    for(unsigned i=0;i<4;i++){
      const int x=int16_t(REX_LOAD_U16(source+i*6));
      if(x!=0&&x!=422*16)return false;
      // Both native panels share the same horizontal endpoints. Right anchor
      // stays fixed; 422 ->592 units is exact in the native 1/16 format.
      REX_STORE_U16(target+i*6,uint16_t(422*16+(x-422*16)*592/422));
    }return true;
  };
  if(!copy(ctx.r4.u32,scratch)||(interpolated&&!copy(ctx.r5.u32,scratch+24))){submit();return;}
  ctx.r4.u64=scratch;if(interpolated)ctx.r5.u64=scratch+24;
  submit();
}
