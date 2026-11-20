#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "camera_menu_descriptor.h"
#include "weather_memory.h"
#if __has_include(<rex/system/kernel_state.h>)
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#define PES6_CAMERA_GUEST_PAGE_TABLE 1
#endif
#include <windows.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>

// UI rows4/5 are Wide variants; rows6..13 map to original native types4..11.
// Persist native type3 and store the extra choice separately so option saves
// and all native camera dispatchers retain their original valid ranges.
namespace {
constexpr uint32_t wide_type=3,wide_table=0x820744C8u+32*wide_type;
bool loaded=false;
uint32_t wide_variant=0;
uint32_t active_match_camera=0;
uint32_t private_table=0;
uint32_t private_menu_descriptor=0;
uint32_t extended_wrapper=0;
std::filesystem::path preference;

bool Valid(uint8_t* base,uint32_t at,size_t bytes){
  if(at<0x10000||uint64_t(at)+bytes>=0xe0000000ull)return false;
#ifdef PES6_CAMERA_GUEST_PAGE_TABLE
  if(auto* kernel=rex::runtime::current_kernel_state()){
    if(auto* memory=kernel->memory()){
      rex::memory::HeapAllocationInfo info{};
      return pes6::weather::ReadableGuestRange(*memory,base,at,bytes,info);
    }
  }
#endif
  auto* p=base+at;
  while(bytes){MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||
       (info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const auto n=std::min(bytes,size_t(static_cast<uint8_t*>(info.BaseAddress)+info.RegionSize-p));
    if(!n)return false;p+=n;bytes-=n;
  }return true;
}
uint32_t Native(PPCContext& original,uint8_t* base,uint32_t function,
                uint32_t a,uint32_t b=0,uint32_t c=0){
  PPCContext ctx=original;ctx.r3.u64=a;ctx.r4.u64=b;ctx.r5.u64=c;
  // All calls here target known native functions or a validated UI vtable.
  REX_CALL_INDIRECT_FUNC(function);
  return ctx.r3.u32;
}
void Load(){
  if(loaded)return;loaded=true;
  wchar_t executable[MAX_PATH]{};GetModuleFileNameW(nullptr,executable,MAX_PATH);
  auto root=std::filesystem::path(executable).parent_path();
  for(int i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");++i)root=root.parent_path();
  preference=root/"data/camera-wide.txt";
  std::ifstream in(preference);std::string value;in>>value;
  wide_variant=value=="wide-far"?2:value=="wide-mid"?1:0;
}
void Select(uint32_t value){
  Load();if(wide_variant==value)return;wide_variant=value;
  std::error_code ec;std::filesystem::create_directories(preference.parent_path(),ec);
  auto temporary=preference;temporary+=L".tmp";
  std::ofstream out(temporary,std::ios::trunc);out<<(value==2?"wide-far":value==1?"wide-mid":"original")<<'\n';
  out.close();
  if(!out||!MoveFileExW(temporary.c_str(),preference.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
    REXLOG_WARN("PES6 camera: could not persist wide camera variant");
  REXLOG_INFO("PES6 camera: {}",value==2?"Amplia lejana (distance x1.25)":value==1?"Amplia 2 (distance x1.125)":"original camera selection");
}
float Float(uint8_t* base,uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));}
void Float(uint8_t* base,uint32_t at,float v){REX_STORE_U32(at,std::bit_cast<uint32_t>(v));}
}

extern "C" void pes6_camera_menu_labels(PPCContext& ctx,uint8_t* base,uint32_t owner){
  Load();
  const auto wrapper=ctx.r3.u32,descriptor=ctx.r4.u32;
  extended_wrapper=0;
  // Exact native camera descriptor: type list op A5, twelve original names.
  if(descriptor!=0x82074A48u||!Valid(base,descriptor,camera_menu_descriptor_size)||
     !Valid(base,wrapper,144)||!Valid(base,owner,76))return;
  const auto instruction=REX_LOAD_U32(descriptor+20);
  if(instruction!=0xA500000Cu)return;
  if(!private_menu_descriptor)
    private_menu_descriptor=Native(ctx,base,0x826D61B0u,camera_menu_descriptor_size);
  if(!Valid(base,private_menu_descriptor,camera_menu_descriptor_size)||
     !MakeCameraMenuDescriptor(base+descriptor,base+private_menu_descriptor))return;
  constexpr const char* labels[]={"Amplia 2","Amplia lejana"};
  uint32_t texts[2]{};
  // Allocate both before moving any original owned string pointers.
  for(unsigned i=0;i<2;i++){
    const auto size=std::strlen(labels[i])+1;
    texts[i]=Native(ctx,base,0x82A00AC8u,uint32_t(size+4),16);
    if(!Valid(base,texts[i],size)){
      for(unsigned j=0;j<=i;j++)if(texts[j])Native(ctx,base,0x82A00AB0u,texts[j]);
      return;
    }
    std::memcpy(base+texts[i],labels[i],size);
  }
  // Move pointers rather than copying strings: each slot retains one owner.
  for(int i=11;i>=4;--i)REX_STORE_U32(wrapper+16+(i+2)*4,REX_LOAD_U32(wrapper+16+i*4));
  REX_STORE_U32(wrapper+16+4*4,texts[0]);
  REX_STORE_U32(wrapper+16+5*4,texts[1]);
  // Pass the private descriptor to8246B5F8. Never write the image at82074A5C,
  // including failure paths; allocation failure leaves the original12 rows.
  ctx.r4.u64=private_menu_descriptor;
  extended_wrapper=wrapper;
  REXLOG_INFO("PES6 camera: native selector extended to14 entries (Amplia, Amplia 2, Amplia lejana)");
}

extern "C" void pes6_camera_menu_ready(PPCContext& ctx,uint8_t* base,uint32_t owner){
  Load();if(!Valid(base,owner,76))return;
  const auto wrapper=REX_LOAD_U32(owner+4);
  if(!extended_wrapper||wrapper!=extended_wrapper)return;
  const auto combo=REX_LOAD_U32(owner+40);
  if(!Valid(base,combo,4))return;
  const auto vtable=REX_LOAD_U32(combo);
  if(!Valid(base,vtable,80))return;
  // Native vtable+76 selects a row; it does not store the camera config.
  Native(ctx,base,REX_LOAD_U32(vtable+76),combo,CameraMenuChoice(base[owner+36],wide_variant));
  Native(ctx,base,0x8246F640u,owner);
}

extern "C" uint32_t pes6_camera_native_choice(uint32_t choice){
  return extended_wrapper?CameraNativeChoice(choice):choice;
}

extern "C" uint32_t pes6_camera_choice(uint32_t choice){
  if(!extended_wrapper){Select(0);return choice;}
  Select(choice==4?1:choice==5?2:0);
  return CameraNativeChoice(choice);
}

extern "C" void pes6_camera_parameters(PPCContext& ctx,uint8_t* base,uint32_t camera){
  const bool camera_valid=Valid(base,camera,200);
  if(camera_valid)active_match_camera=camera;
  Load();
  const float distance_scale=CameraDistanceScale(wide_variant);
  // Only live match Wide. Do not alter editor, replay or cinematic cameras.
  if(!wide_variant||ctx.r3.u32!=wide_table||!camera_valid||
     REX_LOAD_U32(camera+192)!=0||REX_LOAD_U32(camera+196)!=wide_type)return;
  if(!private_table)private_table=Native(ctx,base,0x826D61B0u,32);
  if(!Valid(base,private_table,32))return;
  std::memcpy(base+private_table,base+wide_table,32);
  //824AB6A0: eye.y=table4; target.y=table8; eye.z=target.z+table12.
  // Pull back along the same vertical/depth direction; preserve tracking,
  // angle selection, field bounds, projection and the original shared table.
  const float target_y=Float(base,wide_table+8);
  Float(base,private_table+4,target_y+(Float(base,wide_table+4)-target_y)*distance_scale);
  Float(base,private_table+12,Float(base,wide_table+12)*distance_scale);
  ctx.r3.u64=private_table;
  static bool reported=false;
  if(!reported){reported=true;REXLOG_INFO("PES6 camera: extended wide native parameters active");}
}

extern "C" bool pes6_camera_is_cinematic(uint8_t* base){
  return !active_match_camera||!Valid(base,active_match_camera,200)||REX_LOAD_U32(active_match_camera+192)!=0;
}
