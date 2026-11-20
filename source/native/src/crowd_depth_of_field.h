#pragma once
#include <bit>
#include <cmath>
#include "weather_memory.h"
#if __has_include(<rex/system/kernel_state.h>)
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>
#define PES6_CROWD_DOF_GUEST_PAGE_TABLE 1
#endif
inline bool crowd_dof_lut_ready=false;
inline bool CrowdDofValid(uint8_t* base,uint32_t at,size_t n){
 if(at<0x10000u||uint64_t(at)+n>=0xe0000000ull)return false;
#ifdef PES6_CROWD_DOF_GUEST_PAGE_TABLE
 if(auto* kernel=rex::runtime::current_kernel_state()){
  if(auto* memory=kernel->memory()){
   rex::memory::HeapAllocationInfo info{};
   return pes6::weather::ReadableGuestRange(*memory,base,at,n,info);
  }
 }
#endif
 MEMORY_BASIC_INFORMATION m{};
 return VirtualQuery(base+at,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&base+at+n<=static_cast<uint8_t*>(m.BaseAddress)+m.RegionSize;
}

// One record per stage/process, for the requested end-to-end integration test.
// Never query or write a file repeatedly while drawing frames.
inline void CrowdDofTrace(uint8_t* base,unsigned stage,uint32_t object){
 static const bool trace_enabled=[](){const auto* v=std::getenv("PES6_CROWD_DOF_TRACE");return v&&v[0]=='1'&&v[1]==0;}();
 if(!trace_enabled)return;
 static std::mutex traceMutex;
 static uint64_t seen=0;
 std::lock_guard lock(traceMutex);
 if(stage>=16)return;
 const auto word=[&](uint32_t at){MEMORY_BASIC_INFORMATION m{};return at>=0x10000u&&at<0xdfff0000u&&VirtualQuery(base+at,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&base+at+4<=static_cast<uint8_t*>(m.BaseAddress)+m.RegionSize?uint32_t(REX_LOAD_U32(at)):0u;};
 const uint32_t scene=word(0x831393A8u);
 const unsigned scope=word(scene+544)?1:0;
 const uint64_t key=1ull<<(stage+scope*16);
 if(seen&key)return;
 seen|=key;
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
 auto root=std::filesystem::path(exe).parent_path();
 for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
 std::ofstream out(root/"analysis/crowd-softening/live-route-v3.txt",std::ios::app);
 const uint32_t renderer=(stage>=5&&stage<=8)?word(object+136):word(0x831395A0u);
 const char* enabled=std::getenv("PES6_SOFT_CROWD");
 out<<"stage="<<stage<<" stadium="<<scope<<" enabled="<<(enabled?enabled:"unset")<<" lut_ready="<<crowd_dof_lut_ready<<std::hex<<" object="<<object<<" renderer="<<renderer<<" scene="<<scene<<" crowd="<<word(scene+544)<<" active_flags="<<word(renderer+1268)<<" resource_flags="<<word(renderer+8848)<<" enabled_word="<<word(renderer+8852)<<" focus_bits="<<word(object+64)<<" near_bits="<<word(object+20)<<" far_bits="<<word(object+24)<<std::dec<<'\n';
}

// Use the game's depth-aware composition, not the final image/UI shader.
// 821AAC20 builds the native circle-of-confusion LUT and requests DOF in
// the renderer. Keep its focus distance, projection and resource lifecycle.
inline bool CrowdDofBegin(uint8_t* base,uint32_t camera,uint32_t* saved){
 static const bool enabled=[](){const auto* v=std::getenv("PES6_SOFT_CROWD");return v&&v[0]=='1'&&v[1]==0;}();
 if(!enabled||camera<0x10000u||camera>0xdfff0000u)return false;
 CrowdDofTrace(base,0,camera);
 if(!CrowdDofValid(base,0x831393A8u,4)||!CrowdDofValid(base,camera,180))return false;
 const uint32_t scene=REX_LOAD_U32(0x831393A8u);
 // A loaded crowd is the scope: frontend portraits and menu cameras stay native.
 if(!CrowdDofValid(base,scene,576)||!REX_LOAD_U32(scene+544))return false;
 const float focus=std::bit_cast<float>(uint32_t(REX_LOAD_U32(camera+64)));
 const float nearPlane=std::bit_cast<float>(uint32_t(REX_LOAD_U32(camera+20)));
 const float farPlane=std::bit_cast<float>(uint32_t(REX_LOAD_U32(camera+24)));
 // camera+64 is still in the guest's 1/256 world units here. The native
 // body converts it before building the LUT; comparing it with clip-plane
 // metres here rejected the real user's camera (0x462eb15c ~= 11180).
 if(!std::isfinite(focus)||!std::isfinite(nearPlane)||!std::isfinite(farPlane)||nearPlane<=0||farPlane<=nearPlane||focus<=0)return false;
 std::memcpy(saved,base+camera+160,20);
 CrowdDofTrace(base,1,camera);
 base[camera+160]=1;
 REX_STORE_U32(camera+164,std::bit_cast<uint32_t>(.35f));
 // This is the LUT normalization threshold, not the sampling radius.
 REX_STORE_U32(camera+168,std::bit_cast<uint32_t>(.002f));
 // No minimum blur in front of the focus plane or behind it: the optical
 // LUT determines coverage from depth, keeping the focal subject sharp.
 REX_STORE_U32(camera+172,0);
 REX_STORE_U32(camera+176,0);
 return true;
}
inline void CrowdDofEnd(uint8_t* base,uint32_t camera,const uint32_t* saved,float nativeFocus){
 CrowdDofTrace(base,2,camera);
 // The native LUT normally blurs both sides of the focus plane. Keep its
 // foreground half clear: this option is intended to soften the stands.
 const uint32_t renderer=REX_LOAD_U32(0x831395A0u);
 if(CrowdDofValid(base,renderer,1212)){
   const float lo=std::bit_cast<float>(uint32_t(REX_LOAD_U32(renderer+1204)));
   const float hi=std::bit_cast<float>(uint32_t(REX_LOAD_U32(renderer+1208)));
   const float focus=nativeFocus;
   if(std::isfinite(lo)&&std::isfinite(hi)&&hi>lo){
    // Deliberate background defocus rather than a barely visible optical CoC.
    // Ease from the focal plane to full blur over the far half of the LUT.
    const float range=hi-focus;
    if(range>0)for(unsigned i=0;i<256;i++){
     const float depth=lo+(hi-lo)*(float(i)/255.f);
     float t=(depth-focus)/range*1.75f;
     t=t<0?0:t>1?1:t;
     const float blur=t*t*(3.f-2.f*t);
     REX_STORE_U32(renderer+180+i*4,std::bit_cast<uint32_t>(blur));
    }
    crowd_dof_lut_ready=range>0;
   }
 }
 std::memcpy(base+camera+160,saved,20);
}

inline void CrowdDofPrepare(uint8_t* base,uint32_t renderer){
 static const bool enabled=[](){const auto* v=std::getenv("PES6_SOFT_CROWD");return v&&v[0]=='1'&&v[1]==0;}();
 if(!enabled||!crowd_dof_lut_ready||!CrowdDofValid(base,renderer,8856))return;
 if((REX_LOAD_U32(renderer+8848)&3)!=3||!REX_LOAD_U32(renderer+2432)||!REX_LOAD_U32(renderer+2436))return;
 // 132 is the author's dirty flag; 1268 is the render snapshot actually
 // consumed by the target selector and the post-scene dispatch.
 REX_STORE_U32(renderer+1268,REX_LOAD_U32(renderer+1268)|2u);
 base[renderer+8852]=1;
 CrowdDofTrace(base,11,renderer);
}

// Enlarge only the native DOF downsample/filter offsets, with stack-local
// restoration after the draw. No final-screen/UI blur and no extra draw passes.
inline bool CrowdDofFilterBegin(uint8_t* base,uint32_t renderer,uint32_t* saved){
 static const bool enabled=[](){const auto* v=std::getenv("PES6_SOFT_CROWD");return v&&v[0]=='1'&&v[1]==0;}();
 if(!enabled||!CrowdDofValid(base,renderer,8848))return false;
 CrowdDofTrace(base,3,renderer);
 if(!CrowdDofValid(base,0x831393A8u,4))return false;
 const uint32_t scene=REX_LOAD_U32(0x831393A8u);
 if(!CrowdDofValid(base,scene,576)||!REX_LOAD_U32(scene+544))return false;
 std::memcpy(saved,base+renderer+8720,128);
 for(unsigned i=0;i<32;i++){
  const uint32_t at=renderer+8720+i*4;
  const float offset=std::bit_cast<float>(uint32_t(REX_LOAD_U32(at)));
  if(std::isfinite(offset)&&std::fabs(offset)<.05f)REX_STORE_U32(at,std::bit_cast<uint32_t>(offset*3.f));
 }
 return true;
}
inline void CrowdDofFilterEnd(uint8_t* base,uint32_t renderer,const uint32_t* saved){
 std::memcpy(base+renderer+8720,saved,128);
}
