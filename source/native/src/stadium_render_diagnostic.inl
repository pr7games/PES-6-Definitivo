#include "frame_meter.h"
#include <chrono>
#include <iomanip>
namespace {
using StadiumDiagClock = std::chrono::steady_clock;
thread_local int stadiumDiagnosticPass = -1;
struct StadiumRenderDiagnostic {
  bool started=false, keyHeld=false, bypass=false, enabled=false;
  uint64_t tick=0, frames=0, draws=0, nativeDraws=0, extraDraws=0, samples=0;
  uint64_t work=0, wait=0, modelUs=0, extraUs=0, detailsCount=0;
  std::array<uint64_t,72> passDraws{};
  std::array<uint64_t,72> passLogged{};
  uint64_t nativeLogged=0;
  std::ofstream summary, details;
  void Frame(uint8_t* base) {
    if(!started){
      started=true;
      enabled=std::filesystem::exists(SunsetRoot()/"data/stadiums/konami/render-diagnostic.enabled");
      if(!enabled)return;
      const auto directory=SunsetRoot()/"data/logs";
      std::filesystem::create_directories(directory);
      const auto stats=directory/"konami-render-diagnostic.csv";
      const auto raw=directory/"konami-render-draws.csv";
      const bool statsHeader=!std::filesystem::exists(stats)||std::filesystem::file_size(stats)==0;
      const bool rawHeader=!std::filesystem::exists(raw)||std::filesystem::file_size(raw)==0;
      summary.open(stats,std::ios::app);details.open(raw,std::ios::app);
      if(statsHeader)summary<<"tick_ms,mode,replay,dusk,elapsed_ms,presented_frames,submission_fps,target_fps,requested_interval,work_us_avg,limit_wait_us_avg,gpu_draws,native_draws,extra_draws,literal_submit_us,extra_submit_us,width,height,render_scale,draws_by_pass\n";
      if(rawHeader)details<<"tick_ms,mode,replay,pass,override,gpu,depth_before,color_before,blend_before,depth_effective,color_effective,blend_effective,arg4,arg5\n";
      tick=GetTickCount64();frames=pes6_frame_submissions.load(std::memory_order_relaxed);
    }
    if(!enabled)return;
    const bool pressed=(GetAsyncKeyState(VK_SPACE)&0x8000);
    if(pressed&&!keyHeld){bypass=!bypass;summary<<"# switch tick="<<GetTickCount64()<<" mode="<<(bypass?"B_without_extra_passes":"A_all_passes")<<'\n';summary.flush();}
    keyHeld=pressed;
    work+=pes6_last_work_us.load(std::memory_order_relaxed);
    wait+=pes6_last_limit_wait_us.load(std::memory_order_relaxed);++samples;
    const auto now=GetTickCount64(),elapsed=now-tick;
    if(elapsed<1000)return;
    const auto current=pes6_frame_submissions.load(std::memory_order_relaxed);
    summary<<now<<','<<(bypass?'B':'A')<<','<<unsigned(base[0x8313A93Eu])<<','<<pes6_stadium_sunset()<<','<<elapsed<<','<<current-frames<<','<<double(current-frames)*1000.0/double(elapsed)<<','<<pes6_target_fps.load()<<','<<pes6_requested_interval.load()<<','<<(samples?work/samples:0)<<','<<(samples?wait/samples:0)<<','<<draws<<','<<nativeDraws<<','<<extraDraws<<','<<modelUs<<','<<extraUs<<','<<pes6_frontbuffer_width.load()<<','<<pes6_frontbuffer_height.load()<<','<<pes6_render_scale.load()<<',';
    for(size_t pass=0;pass<passDraws.size();++pass)if(passDraws[pass])summary<<pass<<'='<<passDraws[pass]<<'|';
    summary<<'\n';summary.flush();details.flush();
    frames=current;tick=now;draws=nativeDraws=extraDraws=samples=work=wait=modelUs=extraUs=0;passDraws.fill(0);
  }
  void Draw(uint8_t* base,uint32_t gpu,uint32_t depth,uint32_t color,uint32_t blend,bool overrideState,uint32_t arg4,uint32_t arg5){
    if(!enabled)return;
    ++draws;
    if(stadiumDiagnosticPass<0)++nativeDraws;
    else if(stadiumDiagnosticPass<72){++passDraws[stadiumDiagnosticPass];if(stadiumDiagnosticPass>=50)++extraDraws;}
    auto& logged=stadiumDiagnosticPass>=0&&stadiumDiagnosticPass<72?passLogged[stadiumDiagnosticPass]:nativeLogged;
    if(logged==tick||detailsCount>=4096)return;
    logged=tick;++detailsCount;
    details<<GetTickCount64()<<','<<(bypass?'B':'A')<<','<<unsigned(base[0x8313A93Eu])<<','<<stadiumDiagnosticPass<<','<<overrideState<<','<<std::hex<<gpu<<','<<depth<<','<<color<<','<<blend<<','<<REX_LOAD_U32(gpu+10420)<<','<<REX_LOAD_U32(gpu+10428)<<','<<REX_LOAD_U32(gpu+10424)<<std::dec<<','<<arg4<<','<<arg5<<'\n';
  }
} stadiumDiagnostic;
struct StadiumDiagnosticModel {
  int previous;
  unsigned pass;
  StadiumDiagClock::time_point start;
  StadiumDiagnosticModel(unsigned value):previous(stadiumDiagnosticPass),pass(value){
    stadiumDiagnosticPass=int(pass);
    if(stadiumDiagnostic.enabled)start=StadiumDiagClock::now();
  }
  ~StadiumDiagnosticModel(){
    if(stadiumDiagnostic.enabled){const auto us=std::chrono::duration_cast<std::chrono::microseconds>(StadiumDiagClock::now()-start).count();stadiumDiagnostic.modelUs+=us;if(pass>=50)stadiumDiagnostic.extraUs+=us;}
    stadiumDiagnosticPass=previous;
  }
};
}

