#pragma once
#include <array>
#include <algorithm>
#include <cstdint>

namespace pes6::editor {
// Packed Xbox 360 appearance, consumed by 822F3218 -> instance+112..124.
// The 13 signed controls index the native 15-entry (-7..7) proportion tables.
struct BodyField { const char* id; const char* label; unsigned byte,shift,mask; int minimum,maximum,bias; };
inline constexpr std::array<BodyField,15> body_fields{{
  {"height","Altura (cm)",0,2,63,148,211,148},
  {"weight","Peso (kg)",1,1,127,40,127,0},
  {"head_length","Largo de cabeza",2,0,15,-7,7,0},
  {"head_width","Ancho de cabeza",2,4,15,-7,7,0},
  {"head_depth","Profundidad de cabeza",23,0,15,-7,7,0},
  {"neck_length","Largo de cuello",19,4,15,-7,7,0},
  {"neck_width","Grosor de cuello",18,0,15,-7,7,0},
  {"shoulder_height","Altura de hombros",22,0,15,-7,7,0},
  {"shoulder_width","Ancho de hombros",23,4,15,-7,7,0},
  {"chest","Grosor de pecho",19,0,15,-7,7,0},
  {"waist","Grosor de cintura",20,0,15,-7,7,0},
  {"arms","Grosor de brazos",20,4,15,-7,7,0},
  {"thighs","Grosor de muslos",21,4,15,-7,7,0},
  {"calves","Grosor de pantorrillas",21,0,15,-7,7,0},
  {"leg_length","Largo de piernas",22,4,15,-7,7,0}
}};
inline constexpr int body_unset=-100;
inline int ReadBodyField(const uint8_t* p,unsigned index){
  const auto& f=body_fields[index];int value=(p[f.byte]>>f.shift)&f.mask;
  if(f.minimum<0&&value>=8)value-=16;
  return value+f.bias;
}
inline void WriteBodyField(uint8_t* p,unsigned index,int value){
  const auto& f=body_fields[index];value=std::clamp(value,f.minimum,f.maximum)-f.bias;
  const unsigned mask=f.mask<<f.shift;
  p[f.byte]=uint8_t((p[f.byte]&~mask)|((unsigned(value)&f.mask)<<f.shift));
}
} // namespace pes6::editor
