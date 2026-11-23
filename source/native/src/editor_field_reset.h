#pragma once
#include "player_body_fields.h"
#include "player_face_fields.h"
#include "face_component_types.h"
#include <string_view>
namespace pes6::editor {
enum class ResetKind { Boot, Animation, Appearance, Accessory };
// Pure edit-state operation: no guest memory, file IO or model destruction.
template<class T> bool ResetEditorField(T& c,ResetKind kind,std::string_view field){
  if(kind==ResetKind::Boot){c.boot=-1;return true;}
  if(kind==ResetKind::Animation){
    constexpr std::array<std::string_view,6> names{"free_kick","penalty","dribble","drop_kick","celebration1","celebration2"};
    const auto it=std::find(names.begin(),names.end(),field);
    if(it==names.end())return false;c.animations[it-names.begin()]=-1;return true;
  }
  if(kind==ResetKind::Appearance){
    if(field=="hair"||field=="hair_type"){c.hair_model=c.hair_source=-1;return true;}
    if(field=="face"){c.face_source=-1;return true;}
    if(field=="face_mode"){c.face_mode=-1;return true;}
    const auto component=FindFaceComponentType(field);
    if(component!=face_component_types.end()){ResetFaceComponent(c,*component);return true;}
    const auto face=FindFaceField(field);
    if(face!=face_fields.end()){c.face[face-face_fields.begin()]=face_unset;return true;}
    if(field=="hair_height"||field=="hair_depth"||field=="hair_width"){
      c.hair_fit[field=="hair_height"?0:field=="hair_depth"?1:2]=0;return true;
    }
    if(field=="hair_red"||field=="hair_green"||field=="hair_blue"){
      c.hair_color[field=="hair_red"?0:field=="hair_green"?1:2]=-1;return true;
    }
    const auto it=std::find_if(body_fields.begin(),body_fields.end(),[&](const BodyField& f){return field==f.id;});
    if(it==body_fields.end())return false;c.body[it-body_fields.begin()]=body_unset;return true;
  }
  if(kind!=ResetKind::Accessory)return false;
  if(field=="shirt")c.shirt=0;
  else if(field=="sleeves")c.sleeves=-1;
  else if(field=="wrist_left")c.wrist_left=-1;
  else if(field=="wrist_right")c.wrist_right=-1;
  else if(field=="bracelet_left")c.bracelet_left=-1;
  else if(field=="bracelet_right")c.bracelet_right=-1;
  else if(field=="undershorts")c.undershorts=-1;
  else if(field=="ankle_tape")c.ankle_tape=-1;
  else if(field=="necklace")c.necklace=-1;
  else return false;
  return true;
}
}
