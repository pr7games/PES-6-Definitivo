// pes6 - ReXGlue Recompiled Project

#include "generated/default/pes6_init.h"
#include "pes6_app.h"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

// The pinned SDK's spdlog was built with newer MSVC string algorithms.
// These implementations provide the same byte-position ABI while
// retaining the installed 2019 STL.
#ifndef PES6_USE_SYSTEM_STL_HELPERS
extern "C" size_t __stdcall __std_find_first_of_trivial_pos_1(
    const void* haystack, size_t length, const void* needles, size_t count) noexcept {
  if (length == 0 || count == 0) return std::numeric_limits<size_t>::max();
  const auto* bytes = static_cast<const uint8_t*>(haystack);
  const auto* set = static_cast<const uint8_t*>(needles);
  if (count == 1) {
    const void* match = std::memchr(bytes, set[0], length);
    return match ? static_cast<size_t>(static_cast<const uint8_t*>(match) - bytes)
                 : std::numeric_limits<size_t>::max();
  }
  if (length >= 16) {
    uint64_t bitmap[4]{};
    for (size_t j = 0; j < count; ++j) {
      const uint8_t c = set[j];
      bitmap[c >> 6] |= (uint64_t{1} << (c & 63));
    }
    for (size_t i = 0; i < length; ++i) {
      const uint8_t c = bytes[i];
      if ((bitmap[c >> 6] >> (c & 63)) & 1u) return i;
    }
    return std::numeric_limits<size_t>::max();
  }
  for (size_t i = 0; i < length; ++i)
    for (size_t j = 0; j < count; ++j)
      if (bytes[i] == set[j]) return i;
  return std::numeric_limits<size_t>::max();
}

extern "C" size_t __stdcall __std_find_last_of_trivial_pos_1(
    const void* haystack, size_t length, const void* needles, size_t count) noexcept {
  if (length == 0 || count == 0) return std::numeric_limits<size_t>::max();
  const auto* bytes = static_cast<const uint8_t*>(haystack);
  const auto* set = static_cast<const uint8_t*>(needles);
  if (length >= 16 && count > 1) {
    uint64_t bitmap[4]{};
    for (size_t j = 0; j < count; ++j) {
      const uint8_t c = set[j];
      bitmap[c >> 6] |= (uint64_t{1} << (c & 63));
    }
    for (size_t i = length; i > 0; --i) {
      const uint8_t c = bytes[i - 1];
      if ((bitmap[c >> 6] >> (c & 63)) & 1u) return i - 1;
    }
    return std::numeric_limits<size_t>::max();
  }
  for (size_t i = length; i > 0; --i)
    for (size_t j = 0; j < count; ++j)
      if (bytes[i - 1] == set[j]) return i - 1;
  return std::numeric_limits<size_t>::max();
}

#endif
REX_DEFINE_APP(pes6, Pes6App::Create)
