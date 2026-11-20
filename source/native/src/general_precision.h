#pragma once
#include <cstdint>
// Remaining error, independent of shot tuning.100 restores original behaviour.
inline constexpr unsigned pes6_pass_error_percent=40;
inline constexpr unsigned pes6_centre_error_percent=40;
inline constexpr unsigned pes6_touch_error_percent=40;
inline int32_t pes6_precision_error(int32_t error,unsigned percent){return int32_t(int64_t(error)*percent/100);}
inline uint32_t pes6_precision_heading(uint32_t nominal,uint32_t actual,unsigned percent){
 const uint32_t wrapped=(actual-nominal)&65535u;
 const int32_t delta=wrapped>=32768u?int32_t(wrapped)-65536:int32_t(wrapped);
 return uint32_t(int64_t(nominal)+pes6_precision_error(delta,percent))&65535u;
}
