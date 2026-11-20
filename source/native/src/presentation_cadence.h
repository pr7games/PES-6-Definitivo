#pragma once
#include <algorithm>
#include <cstdint>

// XDK presentation interval values are flags, not a number of refreshes.
// The generated D3D wait maps 0/1 -> one refresh, 2 -> two, 4 -> three.
constexpr uint32_t Pes6PresentationRefreshes(uint32_t interval) {
  switch (interval) {
    case 2: return 2;
    case 4: return 3;
    case 8: return 4;
    default: return 1;
  }
}

constexpr int Pes6OriginalSubmissionFps(uint32_t interval) {
  return 60 / static_cast<int>(Pes6PresentationRefreshes(interval));
}
