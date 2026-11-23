#include <cstddef>
#include <cstdint>
#include <limits>
// Same compatibility ABI as native/src/main.cpp for the pinned SDK libraries.
#ifndef PES6_USE_SYSTEM_STL_HELPERS
extern "C" size_t __stdcall __std_find_first_of_trivial_pos_1(const void* haystack,size_t length,const void* needles,size_t count) noexcept {
 const auto* b=static_cast<const uint8_t*>(haystack);const auto* s=static_cast<const uint8_t*>(needles);
 for(size_t i=0;i<length;i++)for(size_t j=0;j<count;j++)if(b[i]==s[j])return i;
 return std::numeric_limits<size_t>::max();
}
extern "C" size_t __stdcall __std_find_last_of_trivial_pos_1(const void* haystack,size_t length,const void* needles,size_t count) noexcept {
 const auto* b=static_cast<const uint8_t*>(haystack);const auto* s=static_cast<const uint8_t*>(needles);
 for(size_t i=length;i>0;i--)for(size_t j=0;j<count;j++)if(b[i-1]==s[j])return i-1;
 return std::numeric_limits<size_t>::max();
}
#endif
