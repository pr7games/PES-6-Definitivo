#pragma once
#include "weather_particles.h"
#include <bit>
#include <immintrin.h>
namespace pes6::weather {
// XYZ+ARGB must be big-endian in guest memory. One shuffle/store preserves
// every input bit, including the alpha word; no floating-point conversion.
inline void StoreWeatherVertex(uint8_t* destination,Vec p,uint32_t color){
  const auto xyz=_mm_setr_ps(p.x,p.y,p.z,0.f);
  const auto words=_mm_blend_epi16(_mm_castps_si128(xyz),_mm_set_epi32(int(color),0,0,0),0xc0);
  const auto bytes=_mm_shuffle_epi8(words,_mm_setr_epi8(3,2,1,0,7,6,5,4,11,10,9,8,15,14,13,12));
  _mm_storeu_si128(reinterpret_cast<__m128i*>(destination),bytes);
}
}
