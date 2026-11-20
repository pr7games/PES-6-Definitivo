#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

inline constexpr size_t camera_menu_descriptor_size=0x58;
// The executable descriptor is read-only. Build a private writable copy,
// including the BF terminator, and expand only its camera-list command.
inline bool MakeCameraMenuDescriptor(const uint8_t* source,uint8_t* destination){
  if(!source||!destination||source==destination||
     source[20]!=0xA5||source[21]!=0||source[22]!=0||source[23]!=12||
     source[76]!=0xBF||source[77]!=0||source[78]!=0||source[79]!=0)return false;
  std::memcpy(destination,source,camera_menu_descriptor_size);
  destination[23]=14;
  return true;
}

inline constexpr uint32_t CameraNativeChoice(uint32_t row){
  return row==4||row==5?3:row>=6&&row<14?row-2:row;
}
inline constexpr uint32_t CameraMenuChoice(uint32_t type,uint32_t variant){
  return type==3&&variant?3+variant:type>=4&&type<12?type+2:type;
}
inline constexpr float CameraDistanceScale(uint32_t variant){
  return variant==1?1.125f:variant==2?1.25f:1.f;
}
