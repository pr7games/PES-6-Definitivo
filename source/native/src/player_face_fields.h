#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <string_view>

namespace pes6::editor {
// Xbox 360 appearance -> 821A7EA8 -> head+24..54 -> 822F8988.
// cv.img entry 5717 contains six base heads and 31 native position/normal
// delta channels per vertex. Labels follow the affected geometry of those
// channels, including the distinct inner/outer brow and eyelid controls.
struct FaceField {
  const char* id;
  const char* label;
  unsigned byte,shift,mask;
  int minimum,maximum;
  unsigned group;
};
inline constexpr int face_unset=-100;
inline constexpr std::array<const char*,9> face_groups{
  "Base y piel","Cabeza","Cejas","Ojos","Nariz","Mejillas","Boca y labios","Mandíbula y mentón","Orejas"};
inline constexpr std::array<FaceField,40> face_fields{{
  {"face_skin","Piel: tono",4,5,7,0,5,0},
  {"face_skin_texture","Textura superior A",40,3,31,0,14,0},
  {"face_face_texture","Textura superior B",39,3,31,0,14,0},
  {"face_mouth_texture","Nariz: textura",39,0,7,0,2,4},
  {"face_wrinkle_texture","Textura inferior",40,0,7,0,5,0},
  {"face_lip_texture","Boca: textura",41,3,31,0,2,6},
  {"face_eye_red","Iris: rojo",6,0,63,0,63,3},
  {"face_eye_green","Iris: verde",10,2,63,0,63,3},
  {"face_eye_blue","Iris: azul",11,2,63,0,63,3},
  {"face_head_position","Cabeza: altura",42,4,15,-7,7,1},
  {"face_head_width","Cabeza: ancho",24,4,15,-7,7,1},
  {"face_head_depth","Cabeza: profundidad",24,0,15,-7,7,1},
  {"face_brow_height","Cejas: altura",25,4,15,-7,7,2},
  {"face_brow_inner","Cejas: extremo interior",25,0,15,-7,7,2},
  {"face_brow_angle","Cejas: inclinación",26,4,15,-7,7,2},
  {"face_brow_spacing","Cejas: separación",26,0,15,-7,7,2},
  {"face_brow_depth","Cejas: profundidad",27,4,15,-7,7,2},
  {"face_eye_spacing","Ojos: separación",27,0,15,-7,7,3},
  {"face_eye_height","Ojos: altura",28,4,15,-7,7,3},
  {"face_eye_opening","Ojos: apertura",28,0,15,-7,7,3},
  {"face_eye_width","Ojos: ancho",29,4,15,-7,7,3},
  {"face_eye_depth","Ojos: profundidad",29,0,15,-7,7,3},
  {"face_eye_angle","Ojos: inclinación",30,4,15,-7,7,3},
  {"face_nose_height","Nariz: altura",30,0,15,-7,7,4},
  {"face_nose_width","Nariz: ancho",31,4,15,-7,7,4},
  {"face_nose_depth","Nariz: profundidad",31,0,15,-7,7,4},
  {"face_cheek_width","Mejillas: ancho",32,4,15,-7,7,5},
  {"face_cheek_depth","Mejillas: profundidad",32,0,15,-7,7,5},
  {"face_mouth_width","Boca: ancho",33,4,15,-7,7,6},
  {"face_mouth_height","Boca: altura",33,0,15,-7,7,6},
  {"face_mouth_depth","Boca: profundidad",34,4,15,-7,7,6},
  {"face_upper_lip","Labio superior: grosor",34,0,15,-7,7,6},
  {"face_lower_lip","Labio inferior: grosor",35,4,15,-7,7,6},
  {"face_chin_length","Mentón: largo",35,0,15,-7,7,7},
  {"face_chin_depth","Mentón: profundidad",36,4,15,-7,7,7},
  {"face_jaw_width","Mandíbula: ancho",36,0,15,-7,7,7},
  {"face_ear_height","Orejas: altura",37,4,15,-7,7,8},
  {"face_ear_size","Orejas: largo",37,0,15,-7,7,8},
  {"face_ear_width","Orejas: separación",38,4,15,-7,7,8},
  {"face_ear_depth","Orejas: profundidad",38,0,15,-7,7,8},
}};
inline int ReadFaceField(const uint8_t* p,unsigned index){
  const auto& f=face_fields[index];
  int value=(p[f.byte]>>f.shift)&f.mask;
  if(f.minimum<0&&value>=8)value-=16;
  return std::clamp(value,f.minimum,f.maximum);
}
inline std::array<int,face_fields.size()> ReadFace(const uint8_t* p){
  std::array<int,face_fields.size()> values;
  for(unsigned i=0;i<values.size();i++)values[i]=ReadFaceField(p,i);
  return values;
}
inline void WriteFaceField(uint8_t* p,unsigned index,int value){
  const auto& f=face_fields[index];
  const unsigned mask=f.mask<<f.shift;
  p[f.byte]=uint8_t((p[f.byte]&~mask)|((unsigned(value)&f.mask)<<f.shift));
}
inline auto FindFaceField(std::string_view id){
  return std::find_if(face_fields.begin(),face_fields.end(),[&](const FaceField& f){return id==f.id;});
}
}
