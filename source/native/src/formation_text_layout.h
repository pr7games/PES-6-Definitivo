#pragma once
#include <cmath>
#include <unordered_map>

// Native formation/substitution widgets only. WKM bitmaps and global text
// placement are never changed. Native layout+85 selects vertical centering
// in 82734108, using that particular widget's own box height.
namespace pes6_formation_text_layout {
inline bool Readable(uint8_t* base,uint32_t p,size_t n){
  if(p<0x10000u||uint64_t(p)+n>0x100000000ull)return false;
  MEMORY_BASIC_INFORMATION info{};
  return VirtualQuery(base+p,&info,sizeof(info))&&info.State==MEM_COMMIT&&
    !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&
    base+p+n<=static_cast<uint8_t*>(info.BaseAddress)+info.RegionSize;
}
inline float F(uint8_t* base,uint32_t p){return std::bit_cast<float>(REX_LOAD_U32(p));}
inline void Put(uint8_t* base,uint32_t p,float value){REX_STORE_U32(p,std::bit_cast<uint32_t>(value));}
struct Label {
  uint32_t parent=0,font=0;
  float original_scale=1,applied_scale=1,original_height=0,applied_height=0;
  uint8_t original_align=0;
};
inline std::unordered_map<uint32_t,Label> labels;
inline uint32_t manager_seen=0;
inline void Restore(uint8_t* base,uint32_t node,const Label& saved){
  if(!Readable(base,node,272)||REX_LOAD_U32(node+64)!=saved.parent||REX_LOAD_U32(node+148)!=saved.font)return;
  if(std::abs(F(base,node+152)-saved.applied_scale)<1e-6f)Put(base,node+152,saved.original_scale);
  if(std::abs(F(base,node+112)-saved.applied_height)<1e-6f)Put(base,node+112,saved.original_height);
  if(base[node+181]==1)base[node+181]=saved.original_align;
  base[node+52]|=0x67;
}
inline bool Owns(uint8_t* base,uint32_t manager,uint32_t node){
  // The same native controller is used in Edit -> Management, pre-match
  // formation and in-match substitutions. Its pitch resource also owns the
  // player list, information cells, instructions and formation submenus.
  if(!Readable(base,manager,7876))return false;
  uint32_t roots[2]{},resources[2]{};
  for(unsigned side=0;side<2;side++){
    const auto pitch=REX_LOAD_U32(manager+392+side*3776+3704);
    if(!Readable(base,pitch,1732))continue;
    const auto root=REX_LOAD_U32(pitch+1728);
    if(!Readable(base,root,224))continue;
    roots[side]=root;resources[side]=REX_LOAD_U32(root+128);
  }
  // Text+128 belongs to its text-layout object, not to the UI resource!
  // Follow parent+64 and inspect resource+128 only on container objects.
  auto parent=REX_LOAD_U32(node+64);
  for(unsigned depth=0;depth<24&&Readable(base,parent,132);depth++){
    if(parent==roots[0]||parent==roots[1])return true;
    const auto vtable=REX_LOAD_U32(parent);
    if(Readable(base,vtable,28)&&REX_LOAD_U32(vtable+24)==0x8272c7c0u){
      const auto resource=REX_LOAD_U32(parent+128);
      if(resource&&((resources[0]&&resource==resources[0])||(resources[1]&&resource==resources[1])))return true;
    }
    const auto next=REX_LOAD_U32(parent+64);if(next==parent)break;parent=next;
  }
  return false;
}
inline void Apply(uint8_t* base,uint32_t node){
  const auto manager=REX_LOAD_U32(0x83139808u);
  if(manager!=manager_seen){
    for(const auto& [label,saved]:labels)Restore(base,label,saved);
    labels.clear();manager_seen=manager;
  }
  // No tree walk or native glyph invalidation for steady-state frames.
  if(!manager||!base[node+52]||!Readable(base,node,272))return;
  auto it=labels.find(node);
  const auto parent=REX_LOAD_U32(node+64),font=REX_LOAD_U32(node+148);
  if(it!=labels.end()&&(it->second.parent!=parent||it->second.font!=font)){labels.erase(it);it=labels.end();}
  if(it==labels.end()&&!Owns(base,manager,node))return;
  const float current=F(base,node+152),height=F(base,node+112),cell_height=F(base,node+212);
  if(!std::isfinite(current)||current<.25f||current>2.f)return;
  const float box=(std::isfinite(cell_height)&&cell_height>=8.f&&cell_height<=48.f)?cell_height:height;
  if(!std::isfinite(box)||box<8.f||box>48.f)return; // not a single-row cell
  if(it==labels.end())it=labels.emplace(node,Label{parent,font,current,current,height,box,base[node+181]}).first;
  auto& saved=it->second;
  // If native styling explicitly changes, use that new size as the baseline.
  if(std::abs(current-saved.applied_scale)>1e-6f)saved.original_scale=current;
  const float desired=std::max(.25f,saved.original_scale-1.f/24.f);
  const bool changed=std::abs(current-desired)>1e-6f||std::abs(height-box)>1e-6f||base[node+181]!=1;
  if(!changed)return;
  saved.applied_scale=desired;saved.applied_height=box;
  Put(base,node+152,desired);Put(base,node+112,box);base[node+181]=1;
  base[node+52]|=0x67;
}
}
extern "C" void pes6_formation_text_cell(uint8_t* base,uint32_t node){pes6_formation_text_layout::Apply(base,node);}
