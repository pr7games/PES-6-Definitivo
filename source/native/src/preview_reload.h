#pragma once
#include <cstdint>
namespace pes6::editor::preview {
inline bool CanReload(bool loading,bool resources_ready,uint64_t elapsed){
  return elapsed>=40&&(!loading||resources_ready);
}
inline int SliderStep(int selected,int current,int direction,int maximum=63){
  const int value=(selected==0?current:selected-1)+direction;
  const int count=maximum+1;
  return ((value%count)+count)%count+1;
}
}
