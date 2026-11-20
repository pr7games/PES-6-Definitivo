#pragma once
#include <array>
#include <algorithm>
#include <cmath>
// Preserve size/aspect. Move the source's right and bottom cuts to the true
// viewport edges, with a sub-pixel bleed to avoid a filtered seam.
inline std::array<float,2> MenuPortraitEdgeOffset(const std::array<std::array<float,2>,4>& points){
  float right=points[0][0],bottom=points[0][1];
  for(const auto& p:points){if(!std::isfinite(p[0])||!std::isfinite(p[1]))return {};right=(std::max)(right,p[0]);bottom=(std::min)(bottom,p[1]);}
  return {1.002f-right,-1.002f-bottom};
}
