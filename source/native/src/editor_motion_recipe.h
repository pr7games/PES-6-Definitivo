#pragma once
#include <algorithm>
#include <cstdint>
#include <array>
#include <cstring>
#include <cmath>
#include <vector>
namespace pes6::editor::preview {
// PES2009 822BDE18 / 822BDED8: first phase starts at frame0 and its
// explicit marker is frame4. Ordinary transitions blend for three frames.
inline constexpr float setup_marker_seconds=4.f/60.f;
inline constexpr float native_blend_seconds=3.f/60.f;
inline bool KickMirror(uint8_t stats0,bool kicking){return kicking&&(stats0&0x80u)!=0;}
inline float PhaseDelta(float delta,float clock,float marker){
  delta=std::max(0.f,delta);return marker>0?std::min(delta,std::max(0.f,marker-clock)):delta;
}
inline bool PhaseMarkerReached(float clock,float marker){return marker>0&&clock+1e-6f>=marker;}
struct RecipePhase {uint32_t source_id=0,offset=0,bytes=0;float start=0,marker=0,blend=0;};
struct MotionRecipe {std::array<RecipePhase,3> phases{};unsigned count=0;};
inline bool ReadMotionRecipe(const std::vector<uint8_t>& b,MotionRecipe& out){
  out={};if(b.size()<16||b.size()>1024*1024||std::memcmp(b.data(),"P6RCP001",8))return false;
  const auto le=[&](size_t p){return uint32_t(b[p])|(uint32_t(b[p+1])<<8)|(uint32_t(b[p+2])<<16)|(uint32_t(b[p+3])<<24);};
  const auto fl=[&](size_t p){const auto u=le(p);float f;std::memcpy(&f,&u,4);return f;};
  const auto count=le(8);if(count<2||count>3||b.size()<16+count*24)return false;
  size_t expected=16+count*24;
  for(unsigned i=0;i<count;i++){
    const size_t p=16+i*24;auto& v=out.phases[i];v={le(p),le(p+12),le(p+16),fl(p+4),fl(p+8),fl(p+20)};
    if(v.offset!=expected||v.bytes<256||uint64_t(v.offset)+v.bytes>b.size()||!std::isfinite(v.start)||!std::isfinite(v.marker)||!std::isfinite(v.blend)||v.start<0||v.marker<0||v.blend<0||v.blend>1||v.marker>60)return false;
    const auto at=v.offset;const auto be=[&](size_t p){return (uint32_t(b[at+p])<<24)|(uint32_t(b[at+p+1])<<16)|(uint32_t(b[at+p+2])<<8)|b[at+p+3];};
    if(b[at+40]!=0xcc||b[at+41]!=0xcc||b[at+52]!=0||b[at+53]!=64||be(64)!=0xff010001u||b[at+70]!=0x50||b[at+71]!=0x36||be(72)!=120||be(76)!=1||be(80)!=32||be(84)!=96||be(96)!=0xff010003u||b[at+100]!=0||b[at+101]!=22||be(160)!=0xff010002u)return false;
    const unsigned frames=(unsigned(b[at+68])<<8)|b[at+69];if(!frames||frames>1800||v.start>float(frames)/30||v.marker>float(frames)/30)return false;
    for(unsigned k=0;k<24;k++){
      const uint32_t off=be(168+k*4);if(k==23){if(off)return false;break;}
      if(off<112||uint64_t(160)+off+16>v.bytes)return false;
      const auto ch=160+off,type=(unsigned(b[at+ch])<<8)|b[at+ch+1],n=(unsigned(b[at+ch+4])<<8)|b[at+ch+5];
      if((type!=1&&type!=4)||b[at+ch+6]>=22||n!=frames*4+1||be(ch+8)!=16||uint64_t(ch)+be(ch+12)+uint64_t(n)*16>v.bytes||be(ch+12)<16+n*2)return false;
    }
    expected+=v.bytes;
  }
  if(expected!=b.size())return false;out.count=count;return true;
}
// A marked phase exits only at its recipe marker; a zero marker waits for the
// native endpoint. Never OR an earlier completion flag into a marked phase.
inline bool RecipePhaseEnded(float clock,float marker,bool native_end){return marker>0?PhaseMarkerReached(clock,marker):native_end;}
}
