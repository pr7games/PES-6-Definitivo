#pragma once
#include <cstdint>
namespace pes6::editor {
// Converted resources retain frame30 duration for gameplay, but explicit keys
// use frame120 units. Only the sampler's key-range limit changes units.
inline uint32_t ConvertedSamplerFrames(uint32_t frames,uint16_t tag,uint32_t rate,uint32_t flags){
  return tag==0x5036&&rate==120&&(flags&1)&&frames<=1800?frames*4:frames;
}
}
