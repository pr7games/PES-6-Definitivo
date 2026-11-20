#pragma once
#include <cstdlib>

namespace pes6::graphics {
struct Effects {
  bool rain=false,particles=false,dirt=false;
  bool Surfaces() const {return rain||particles||dirt;}
  float Moisture(bool wet,float sweat) const {
    return wet?(rain?1.f:0.f):(particles?sweat:0.f);
  }
};
inline bool ReadEffect(const char* name,bool fallback){
  const auto* value=std::getenv(name);
  return value&&value[0]?(value[0]=='1'&&value[1]=='\0'):fallback;
}
inline const Effects& EffectOptions(){
  static const Effects options=[](){
    const bool legacy=ReadEffect("PES6_ENHANCED_EFFECTS",false);
    return Effects{ReadEffect("PES6_IMPROVED_RAIN",legacy),
      ReadEffect("PES6_IMPROVED_PARTICLES",legacy),
      ReadEffect("PES6_PLAYER_DIRT",legacy)};
  }();
  return options;
}
}
