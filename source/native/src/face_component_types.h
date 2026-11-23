#pragma once
#include "player_face_fields.h"
namespace pes6::editor {
// Port presets made from native morph/texture controls, not invented native
// resource IDs. Persist the actual40 values so V8 and the external editor
// retain them. Fine editing automatically shows Actual/personalizado.
struct FaceComponentType {
 const char* id;const char* label;unsigned group,count;
 std::array<unsigned,6> fields;
 std::array<std::array<int,6>,6> types;
};
inline constexpr std::array<FaceComponentType,8> face_component_types{{
 {"face_head_type","Tipo de cabeza",1,3,{9,10,11},{{{0,0,0},{0,-3,1},{0,3,-1},{2,-1,2},{-2,2,0},{1,1,-3}}}},
 {"face_brow_type","Tipo de cejas",2,5,{12,13,14,15,16},{{{0,0,0,0,0},{1,2,-3,0,0},{-2,-2,3,0,1},{2,0,0,3,-1},{-1,1,-1,-3,2},{0,-3,2,1,-2}}}},
 {"face_eye_type","Tipo de ojos",3,6,{17,18,19,20,21,22},{{{0,0,0,0,0,0},{0,0,-3,2,0,1},{1,0,3,-1,0,0},{-2,1,0,2,1,-2},{3,-1,-1,0,-1,2},{0,1,1,-3,2,-1}}}},
 {"face_nose_type","Tipo de nariz",4,4,{23,24,25,3},{{{0,0,0,0},{1,-3,2,0},{-1,3,0,1},{2,0,3,1},{-2,1,-2,2},{0,2,2,2}}}},
 {"face_cheek_type","Tipo de mejillas",5,2,{26,27},{{{0,0},{3,1},{-3,-1},{1,3},{-1,-3},{2,-2}}}},
 {"face_mouth_type","Tipo de boca",6,6,{28,29,30,31,32,5},{{{0,0,0,0,0,0},{-3,0,0,-2,-1,0},{3,0,0,1,1,1},{0,1,1,3,3,1},{1,-1,-1,-3,-3,2},{-1,1,2,0,2,2}}}},
 {"face_jaw_type","Tipo de mandibula y menton",7,3,{33,34,35},{{{0,0,0},{2,1,-3},{-2,0,3},{1,3,1},{-1,-3,-1},{3,-1,2}}}},
 {"face_ear_type","Tipo de orejas",8,4,{36,37,38,39},{{{0,0,0,0},{0,3,2,0},{0,-3,-2,0},{2,0,0,1},{-2,1,3,-1},{1,-1,-1,2}}}}
}};
inline auto FindFaceComponentType(std::string_view id){return std::find_if(face_component_types.begin(),face_component_types.end(),[&](const auto& f){return id==f.id;});}
template<class T> void ResetFaceComponent(T& c,const FaceComponentType& type){for(unsigned i=0;i<type.count;i++)c.face[type.fields[i]]=face_unset;}
template<class T> int FaceComponentSelection(const T& c,const std::array<int,face_fields.size()>& original,const FaceComponentType& type){
 for(unsigned option=0;option<type.types.size();option++){
  bool same=true;for(unsigned i=0;i<type.count;i++){const auto at=type.fields[i];if((c.face[at]==face_unset?original[at]:c.face[at])!=type.types[option][i]){same=false;break;}}
  if(same)return int(option+1);
 }
 return 0;
}
}
