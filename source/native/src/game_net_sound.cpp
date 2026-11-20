#include "../generated/default/pes6_pch.h"
#include "net_sound_contact.h"
#include <windows.h>
#include <mmsystem.h>
#include <array>
#include <atomic>
#include <bit>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include <limits>
extern "C" void pes6_weather_net_hit(uint8_t*,float,float,float);

namespace {
using namespace pes6::net_sound;
bool ReadNet(uint8_t* base,uint32_t at,void* p,size_t bytes){SIZE_T n=0;return at>=0x10000&&uint64_t(at)+bytes<0xe0000000ull&&ReadProcessMemory(GetCurrentProcess(),base+at,p,bytes,&n)&&n==bytes;}
uint32_t NetWord(uint8_t* base,uint32_t at){uint32_t n=0;return ReadNet(base,at,&n,4)?_byteswap_ulong(n):0;}
bool NetPosition(uint8_t* base,uint32_t at,Vec& p){std::array<uint32_t,3> w{};if(!ReadNet(base,at,w.data(),12))return false;p={std::bit_cast<float>(_byteswap_ulong(w[0]))/256.f,-std::bit_cast<float>(_byteswap_ulong(w[1]))/256.f,std::bit_cast<float>(_byteswap_ulong(w[2]))/256.f};return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
struct NetVoice {HWAVEOUT device=nullptr;WAVEHDR header{};std::vector<int16_t> pcm;};
struct NetAudio {
 std::mutex mutex;bool initialized=false,replaying=false,enabled=false;
 std::array<std::vector<int16_t>,3> samples;std::array<NetVoice,2> voices;
 Contact live,replay;float cursor=std::numeric_limits<float>::quiet_NaN();
 std::filesystem::path root;unsigned played=0;std::atomic<uint64_t> post_until{0};
 ~NetAudio(){for(auto& v:voices)if(v.device){waveOutReset(v.device);waveOutUnprepareHeader(v.device,&v.header,sizeof(v.header));waveOutClose(v.device);}}
 void Status(const char* state,float speed=0,unsigned volume=0,unsigned variant=0){
  std::error_code ec;std::filesystem::create_directories(root/L"data/diagnostics",ec);
  std::ofstream out(root/L"data/diagnostics/net-sound-status.txt",std::ios::trunc);
  out<<"state="<<state<<"\nplayed="<<played<<"\nlast_speed_mps="<<speed<<"\nsfx_volume="<<volume<<"\nvariant="<<variant<<"\nsource=original_net_cord_pcm\n";
 }
 void Init(){
  if(initialized)return;initialized=true;wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);root=std::filesystem::path(exe).parent_path();
  for(unsigned i=0;i<7&&!std::filesystem::exists(root/L"game/cv.img");i++)root=root.parent_path();
  size_t maximum=0;
  for(unsigned k=0;k<3;k++){
   std::ifstream in(root/L"data/audio/net"/("net-"+std::to_string(k)+".wav"),std::ios::binary);
   std::array<char,44> h{};in.read(h.data(),h.size());if(!in||std::memcmp(h.data(),"RIFF",4)||std::memcmp(h.data()+8,"WAVEfmt ",8)||std::memcmp(h.data()+36,"data",4)){Status("invalid_sample");return;}
   auto u16=[&](unsigned at){uint16_t n;std::memcpy(&n,h.data()+at,2);return n;};auto u32=[&](unsigned at){uint32_t n;std::memcpy(&n,h.data()+at,4);return n;};
   const auto bytes=u32(40);if(u16(20)!=1||u16(22)!=1||u16(34)!=16||u32(24)!=44100||bytes<1000||bytes>88200||bytes%2){Status("invalid_format");return;}
   samples[k].resize(bytes/2);in.read(reinterpret_cast<char*>(samples[k].data()),bytes);if(!in){Status("truncated_sample");return;}maximum=std::max(maximum,samples[k].size());
  }
  WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=2;format.nSamplesPerSec=44100;format.wBitsPerSample=16;format.nBlockAlign=4;format.nAvgBytesPerSec=176400;
  for(auto& v:voices){if(waveOutOpen(&v.device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR){v.device=nullptr;continue;}v.pcm.resize(maximum*2);v.header.lpData=reinterpret_cast<char*>(v.pcm.data());v.header.dwBufferLength=DWORD(v.pcm.size()*2);
   if(waveOutPrepareHeader(v.device,&v.header,sizeof(v.header))!=MMSYSERR_NOERROR){waveOutClose(v.device);v.device=nullptr;}else enabled=true;
  }
  Status(enabled?"ready":"audio_device_unavailable");
 }
 void Stop(){for(auto& v:voices)if(v.device)waveOutReset(v.device);}
 void Play(uint8_t* base,Vec p,float speed){
  if(!enabled||speed<=0||GetTickCount64()<post_until.load(std::memory_order_relaxed))return;
  DWORD foreground=0;GetWindowThreadProcessId(GetForegroundWindow(),&foreground);if(foreground&&foreground!=GetCurrentProcessId())return;
  // Native SE slider:824DF078(+80) ->2200000D ->82187504 stores here.
  const unsigned volume=std::min(NetWord(base,0x82f0d4c4u),15u);const float gain=Gain(speed,volume);if(gain<=0)return;
  const unsigned variant=(unsigned(std::abs(p.z)*3.f)+unsigned(std::max(0.f,p.y)*3.f))%3;
  for(auto& v:voices)if(v.device&&!(v.header.dwFlags&WHDR_INQUEUE)){
   const auto& pcm=samples[variant];for(size_t i=0;i<pcm.size();i++){const auto value=int16_t(std::clamp(std::lround(pcm[i]*gain),-32768l,32767l));v.pcm[i*2]=v.pcm[i*2+1]=value;}
   v.header.dwBufferLength=DWORD(pcm.size()*4);const auto result=waveOutWrite(v.device,&v.header,sizeof(v.header));
   if(result==MMSYSERR_NOERROR){played++;Status("played",speed,volume,variant);}else Status("audio_write_failed",speed,volume,variant);return;
  }
 }
};
NetAudio& NetSound(){static NetAudio audio;return audio;}
}
extern "C" void pes6_net_sound_tick(uint8_t* base){
 uint8_t replay=0;if(!ReadNet(base,0x8313a93eu,&replay,1))return;
 auto& s=NetSound();std::lock_guard lock(s.mutex);
 if(replay){if(!s.replaying)s.Stop();s.replaying=true;s.live.Reset();s.replay.Reset();return;}
 const auto game=NetWord(base,0x8313a948u),ball=game?NetWord(base,game+2520):0;Vec p;
 if(!ball||!NetPosition(base,ball+144,p))return;
 s.Init();
 if(s.replaying){s.Stop();s.replaying=false;s.replay.Reset();s.cursor=std::numeric_limits<float>::quiet_NaN();}
 const float speed=s.live.Sample(p,1.f/60);if(speed>0){pes6_weather_net_hit(base,p.x,p.y,p.z);s.Play(base,p,speed);}
}
extern "C" void pes6_net_sound_replay(uint8_t* base,uint32_t manager){
 uint8_t flag=0;if(!ReadNet(base,0x8313a93eu,&flag,1)||!flag)return;
 auto& s=NetSound();std::lock_guard lock(s.mutex);
 // Also stop an in-flight live net sample at replay entry, including highlights
 // with no valid replay ball. Never initialize or submit audio in this hook.
 if(!s.replaying)s.Stop();s.replaying=true;s.live.Reset();s.replay.Reset();
 s.cursor=std::numeric_limits<float>::quiet_NaN();
}
extern "C" void pes6_net_sound_cue(uint32_t cue){
 if(cue==0x8c000011u||cue==0x8c000012u)NetSound().post_until.store(GetTickCount64()+120,std::memory_order_relaxed);
}
