#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cwchar>

namespace pes6::nr {
// Private, per-game mapping written only after the external engine completes
// new frames. No launcher preference or environment flag can mark NR active.
struct Heartbeat {
  uint32_t magic,version,pid,active;
  uint64_t tick,completed;
  uint32_t worker,fps_milli,reserved,sequence;
};
static_assert(sizeof(Heartbeat)==48 && offsetof(Heartbeat,sequence)==44);
inline bool Fresh(const Heartbeat& h,uint32_t pid,uint64_t now) {
  return h.magic==0x50534E52u && h.version==2 && h.pid==pid && h.active==1 &&
      h.completed>0 && h.worker>0 && !(h.sequence&1) && now>=h.tick && now-h.tick<=3000;
}
class Status {
 public:
  Status() noexcept : pid_(GetCurrentProcessId()) {
    swprintf_s(mapping_name_, L"Local\\PES6_NR_%lu", static_cast<unsigned long>(pid_));
  }
  ~Status(){if(view_)UnmapViewOfFile(view_);if(mapping_)CloseHandle(mapping_);}
  double Fps(){return Active()?fps_:0;}
  bool Active(){
    const auto now=GetTickCount64();
    if(now-last_poll_<250)return active_;
    last_poll_=now;active_=false;fps_=0;
    if(!view_){
      mapping_=OpenFileMappingW(FILE_MAP_READ,FALSE,mapping_name_);
      if(!mapping_)return false;
      view_=static_cast<const uint8_t*>(MapViewOfFile(mapping_,FILE_MAP_READ,0,0,sizeof(Heartbeat)));
      if(!view_){CloseHandle(mapping_);mapping_=nullptr;return false;}
    }
    // Seqlock prevents a half-written heartbeat from reporting a false state.
    const auto* sequence=reinterpret_cast<const volatile uint32_t*>(view_+44);
    const auto before=*sequence;
    if(before&1)return false;
    MemoryBarrier();
    Heartbeat h{};std::memcpy(&h,view_,sizeof(h));
    MemoryBarrier();
    if(before!=*sequence || h.sequence!=before)return false;
    active_=Fresh(h,pid_,now);
    if(active_ && h.fps_milli<=1000000)fps_=double(h.fps_milli)/1000;
    return active_;
  }
 private:
  uint32_t pid_=0;
  wchar_t mapping_name_[64]{};
  HANDLE mapping_=nullptr;
  const uint8_t* view_=nullptr;
  uint64_t last_poll_=0;
  bool active_=false;
  double fps_=0;
};
inline Status& Reader(){static Status status;return status;}
inline bool Active(){return Reader().Active();}
inline double OutputFps(){return Reader().Fps();}
}
