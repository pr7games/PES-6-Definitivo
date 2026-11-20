#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>
#include <set>
#include <cstring>
#include <istream>
namespace pes6::editor {
inline bool ValidHairFit(const std::array<int,3>& fit){return fit[0]>=-30&&fit[0]<=30&&fit[1]>=-30&&fit[1]<=30&&fit[2]>=-20&&fit[2]<=20;}
inline bool ReadHairFit(std::istream& row,std::array<int,3>& fit){for(int& value:fit)if(!(row>>value))return false;return ValidHairFit(fit);}
inline bool FitHairGeometry(std::span<uint8_t> m,const std::array<int,3>& fit){
  if(!ValidHairFit(fit)||m.size()<128||std::memcmp(m.data(),"KTMDL",5))return false;
  const auto valid=[&](int64_t at,uint64_t n){return at>=0&&uint64_t(at)<=m.size()&&n<=m.size()-uint64_t(at);};
  const auto u32=[&](size_t p){return uint32_t(m[p])<<24|uint32_t(m[p+1])<<16|uint32_t(m[p+2])<<8|m[p+3];};
  const auto f32=[&](size_t p){return std::bit_cast<float>(u32(p));};
  const auto store=[&](size_t p,float f){const auto u=std::bit_cast<uint32_t>(f);for(unsigned i=0;i<4;i++)m[p+i]=uint8_t(u>>(24-i*8));};
  struct Attribute{size_t at;unsigned semantic;};std::vector<Attribute> attrs;std::set<size_t> seen;
  const auto count=u32(40),meshes=u32(44);
  if(count>4096||!valid(meshes,uint64_t(count)*96))return false;
  for(unsigned i=0;i<count;i++){
    const size_t mesh=meshes+i*96;
    for(unsigned rel:{20u,36u}){
      const auto streams=u32(mesh+rel-4);const int64_t table=int64_t(mesh)+int32_t(u32(mesh+rel));
      if(streams>4096||!valid(table,uint64_t(streams)*32))return false;
      for(unsigned j=0;j<streams;j++){
        const size_t s=size_t(table)+j*32;if(int32_t(u32(s+20))!=0)continue;
        const int64_t vertices=int64_t(s)+int32_t(u32(s)),decl=int64_t(s)+int32_t(u32(s+12));
        const auto n=u32(s+4);const unsigned stride=m[s+9],elements=m[s+10];
        if(n>1000000||!stride||!valid(vertices,uint64_t(n)*stride)||!valid(decl,elements*4))return false;
        for(unsigned e=0;e<elements;e++){
          const size_t d=size_t(decl)+e*4;const unsigned semantic=m[d+3],offset=m[d+1],type=m[d+2];
          if(semantic!=16&&semantic!=18&&semantic!=30&&semantic!=31)continue;
          if(type!=2||offset+12>stride)return false;
          for(unsigned v=0;v<n;v++){
            const size_t p=size_t(vertices)+v*stride+offset;if(!seen.insert(p).second)continue;
            for(unsigned k=0;k<3;k++)if(!std::isfinite(f32(p+k*4)))return false;
            attrs.push_back({p,semantic});
          }
        }
      }
    }
  }
  // Finish all validation before writing, so malformed input stays unchanged.
  if(attrs.empty())return false;
  if(fit==std::array<int,3>{0,0,0})return true;
  const float scale=1.f+fit[2]*.01f;
  for(const auto& a:attrs){
    float x=f32(a.at),y=f32(a.at+4),z=f32(a.at+8);
    if(a.semantic==16){x*=scale;y+=fit[0]*.001f;z+=fit[1]*.001f;}
    else if(fit[2]){
      x*=a.semantic==18?1.f/scale:scale;
      const float n=std::sqrt(x*x+y*y+z*z);if(n>1e-8f){x/=n;y/=n;z/=n;}
    }else continue;
    store(a.at,x);store(a.at+4,y);store(a.at+8,z);
  }
  return true;
}
}
