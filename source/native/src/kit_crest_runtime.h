#pragma once
#include <array>
#include <map>
#include <mutex>
#include "kit_crest_models.h"
namespace pes6::crest {
inline std::array<std::array<int,4>,205> modes{};
inline std::map<uint32_t,unsigned> descriptors;
inline std::mutex mutex;
inline void Register(uint32_t descriptor,unsigned team,unsigned variant){if(team>=205||variant>=4)return;std::lock_guard lock(mutex);descriptors[descriptor]=team*4+variant;}
inline void Copy(uint32_t destination,uint32_t source){std::lock_guard lock(mutex);auto it=descriptors.find(source);if(it==descriptors.end())descriptors.erase(destination);else descriptors[destination]=it->second;}
inline int Mode(uint32_t descriptor){std::lock_guard lock(mutex);auto it=descriptors.find(descriptor);return it==descriptors.end()?0:modes[it->second/4][it->second%4];}
inline bool Identity(uint32_t descriptor,unsigned& team,unsigned& variant){std::lock_guard lock(mutex);auto it=descriptors.find(descriptor);if(it==descriptors.end())return false;team=it->second/4;variant=it->second%4;return true;}
inline bool Visible(unsigned model,int mode,bool legacy){if(model>=model_flags.size())return false;if(mode==2)return false;if(mode==1)return model_flags[model]==1||model_flags[model]==2;return model_flags[model]==1||(model_flags[model]==2&&legacy);}
}
