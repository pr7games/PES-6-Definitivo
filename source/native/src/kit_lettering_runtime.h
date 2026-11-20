#pragma once
#include <map>
namespace pes6_team_info {
inline std::map<uint32_t,unsigned> lettering_descriptors;
inline std::mutex lettering_mutex;
inline bool LetteringSpec(uint32_t descriptor,std::array<int,9>& values){
 std::lock_guard<std::mutex> lock(lettering_mutex);auto it=lettering_descriptors.find(descriptor);if(it==lettering_descriptors.end())return false;values=lettering[it->second/4][it->second%4].values;return true;
}
inline void CopyLettering(uint32_t destination,uint32_t source){
 pes6::crest::Copy(destination,source);
 std::lock_guard<std::mutex> lock(lettering_mutex);auto it=lettering_descriptors.find(source);if(it==lettering_descriptors.end())lettering_descriptors.erase(destination);else lettering_descriptors[destination]=it->second;
}
inline void StoreWord(uint8_t* base,uint32_t at,unsigned value){base[at]=value>>24;base[at+1]=value>>16;base[at+2]=value>>8;base[at+3]=value;}
inline void PatchLettering(uint8_t* base,uint32_t kit,unsigned team,unsigned role,uint32_t variant){
 Load();team=ResolveTeam(base,team);if(team>=189||role>1||!Readable(base,kit,144)||!Readable(base,variant,1))return;
 const unsigned v=role+2*((base[variant]>>(role?0:3))&1);auto& spec=lettering[team][v];
 {std::lock_guard<std::mutex> lock(lettering_mutex);if(!spec.Enabled()){lettering_descriptors.erase(kit);return;}lettering_descriptors[kit]=team*4+v;}
 // Include the team/variant in the native glyph cache key, including generics.
 const unsigned glyphKey=team*4+role*2+(v>>1);base[kit+52]=glyphKey>>8;base[kit+53]=glyphKey;
 const auto& x=spec.values;if(x[0]>=0)base[kit+54]=x[0]<1000?uint8_t(x[0]):0;if(x[1]>=0)base[kit+55]=x[1]<1000?uint8_t(x[1]):0;
 // Native descriptor fill/outline: name60/56, shirt number68/64, shorts76/72.
 const auto ink=[&](unsigned off,int value){if(value>=0)StoreWord(base,kit+off,(unsigned(value)<<16)|(U32(base,kit+off)&0xffffu));};
 ink(68,x[2]);ink(64,x[3]);ink(76,x[4]);ink(72,x[5]);ink(60,x[8]?x[2]:x[6]);ink(56,x[8]?x[3]:x[7]);
}
inline bool LetteringResource(PPCContext& ctx,uint8_t* base,bool numbers){
 const auto object=ctx.r3.u32;if(!Readable(base,object,numbers?36:28))return false;const auto descriptor=U32(base,object+(numbers?32:24));std::array<int,9> values;if(!LetteringSpec(descriptor,values))return false;const int style=values[numbers?0:1];if(style<0)return false;
 const unsigned entry=style>=1000?(numbers?1689:869)+unsigned(style-1000):(numbers?851:863)+unsigned(style);
 if(numbers){StoreWord(base,object+24,entry);StoreWord(base,object+28,style>=1000?unsigned(base[descriptor+81])+1:0);}else StoreWord(base,object+20,entry);return true;
}
inline bool LetteringRecolor(uint32_t descriptor,bool numbers){
 std::array<int,9> x;if(!LetteringSpec(descriptor,x))return false;if(numbers)return x[2]>=0||x[3]>=0||x[4]>=0||x[5]>=0;return x[x[8]?2:6]>=0||x[x[8]?3:7]>=0;
}
}
