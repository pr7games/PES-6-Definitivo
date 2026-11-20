#include "konami_hemisphere_data.h"
#include <algorithm>

namespace {
thread_local float stadiumHemisphereShade = 0.0f;
bool StadiumHemisphereActive(uint8_t* base);
}

extern "C" void pes6_stadium_hemisphere_shade(PPCContext& ctx, uint8_t* base) {
  const auto config = REX_LOAD_U32(0x83139360u);
  if (config && base[config + 130] == 8) {
    stadiumHemisphereShade = std::clamp(float(ctx.f1.f64), 0.0f, 1.0f);
  }
}

extern "C" void pes6_stadium_hemisphere_constants(uint8_t* base, uint32_t constants, bool pixel) {
  const auto config = REX_LOAD_U32(0x83139360u);
  const bool active = config && StadiumHemisphereActive(base);
  const unsigned weather = active && base[config + 131] == 1 ? 1 : 0;
  const unsigned time = active && base[config + 132] == 1 ? 1 : 0;
  const unsigned variant = pes6_stadium_sunset() ? 4 + weather : weather * 2 + time;
  const auto& colors = konamiHemisphereColors[variant];
  const auto target = constants + ((pixel ? 368 : 112) + 248) * 16;
  for (unsigned channel = 0; channel < 4; ++channel) {
    float mean = 0.0f, difference = 0.0f;
    if (active && channel < 3) {
      const auto upper = colors[0][0][channel] * (1.0f - stadiumHemisphereShade) +
        colors[1][0][channel] * stadiumHemisphereShade;
      const auto lower = colors[0][1][channel] * (1.0f - stadiumHemisphereShade) +
        colors[1][1][channel] * stadiumHemisphereShade;
      mean = (upper + lower) * 0.5f;
      difference = (upper - lower) * 0.5f;
    }
    REX_STORE_U32(target + channel * 4, std::bit_cast<uint32_t>(mean));
    REX_STORE_U32(target + 16 + channel * 4, std::bit_cast<uint32_t>(difference));
  }
  // One dirty bit covers four float4 registers. Registers248/249 share bit1.
  const auto dirty = constants + (pixel ? 8 : 0);
  REX_STORE_U64(dirty, REX_LOAD_U64(dirty) | 2ull);
}
