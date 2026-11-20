#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <istream>
#include "hair_catalog.h"
namespace pes6::editor {
inline int ReadHairModel(const uint8_t* p){return ((p[4]&31)<<6)|(p[5]>>2);}
inline void WriteHairModel(uint8_t* p,int id){
  p[4]=uint8_t((p[4]&224)|((id>>6)&31));
  p[5]=uint8_t((p[5]&3)|((id&63)<<2));
}
inline bool ValidHairModel(int id){return id==-1||std::find(hair_models.begin(),hair_models.end(),id)!=hair_models.end();}
inline int HairGroup(int model){
  const auto it=std::find(hair_models.begin(),hair_models.end(),model);
  return it==hair_models.end()?0:hair_groups[it-hair_models.begin()];
}
inline unsigned HairGroupSize(int group){return group<1||group>4?0:unsigned(std::count(hair_groups.begin(),hair_groups.end(),group));}
inline int HairGroupModel(int group,unsigned index){
  if(group<1||group>4)return -1;
  for(unsigned i=0;i<hair_models.size();i++)if(hair_groups[i]==group){if(index==0)return hair_models[i];--index;}
  return -1;
}
inline int HairColor8(int value){return (std::clamp(value,0,63)*255+31)/63;}
inline int HairColor6(int value){return (std::clamp(value,0,255)*63+127)/255;}
inline std::array<float,3> HairMaterialColor(const std::array<uint8_t,3>& rgb){
  const float boost=std::max({rgb[0],rgb[1],rgb[2]})*.15f;
  return {(rgb[0]+boost)/255.f,(rgb[1]+boost)/255.f,(rgb[2]+boost)/255.f};
}
inline bool ReadHairExtension(std::istream& row,int& model,std::array<int,3>& color,int maximum=63){
  if(!(row>>model))return false;
  if(model==0)model=-1; // Earlier V5 exposed an empty package as bald.
  if(!ValidHairModel(model))return false;
  for(int& value:color)if(!(row>>value)||value<-1||value>maximum)return false;
  return true;
}
inline std::array<int,3> ReadHairColor(const uint8_t* p){return {p[15]>>2,p[16]>>2,((p[16]&3)<<4)|(p[17]>>4)};}
inline void WriteHairColor(uint8_t* p,const std::array<int,3>& color){
  if(color[0]>=0)p[15]=uint8_t((p[15]&3)|(std::clamp(color[0],0,63)<<2));
  if(color[1]>=0)p[16]=uint8_t((p[16]&3)|(std::clamp(color[1],0,63)<<2));
  if(color[2]>=0){const int b=std::clamp(color[2],0,63);p[16]=uint8_t((p[16]&252)|(b>>4));p[17]=uint8_t((p[17]&15)|((b&15)<<4));}
}
}
