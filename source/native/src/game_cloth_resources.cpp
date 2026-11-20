#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "cloth_variant_map.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <vector>
extern "C" bool pes6_cloth_enabled();
// Load a private, prepared clothing bundle only while the launcher option is
// enabled. Match original compressed bytes so unrelated/newer kits fall back.
extern "C" bool pes6_cloth_resource(PPCContext& ctx,uint8_t* base){
 if(!pes6_cloth_enabled())return false;
 const uint32_t request=ctx.r3.u32,input=ctx.r4.u32;
 if(!request||!input||ctx.r5.u32<16||base[request+121]==1)return false;
 const uint32_t id=REX_LOAD_U32(request+8);
 if(id<2753||id>2944)return false;
 const auto& variant=cloth_variants[id-2753];
 if(variant.entry!=id||REX_LOAD_U32(input+8)!=variant.compressed||ctx.r5.u32<variant.compressed+16||REX_LOAD_U32(input+12)!=variant.sourceSize)return false;
 uint64_t hash=14695981039346656037ull;
 for(uint32_t i=0;i<variant.compressed;i++)hash=(hash^base[input+16+i])*1099511628211ull;
 if(hash!=variant.hash)return false;
 static std::mutex resource_mutex;std::lock_guard lock(resource_mutex);
 static const auto folder=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root/"data/cloth/models";}();
 static std::map<std::string,std::vector<uint8_t>> cache;
 auto& bytes=cache[variant.file];
 if(bytes.empty()){
  std::ifstream file(folder/variant.file,std::ios::binary|std::ios::ate);
  if(!file||file.tellg()!=variant.size)return false;
  bytes.resize(variant.size);file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
  auto be=[&](size_t a){return(uint32_t(bytes[a])<<24)|(uint32_t(bytes[a+1])<<16)|(uint32_t(bytes[a+2])<<8)|bytes[a+3];};
  bool valid=bool(file)&&bytes.size()>56&&be(0)==4&&be(4)==8;
  for(unsigned l=0;l<4&&valid;l++){const uint64_t at=be(8+l*12),size=be(12+l*12);valid=at>=56&&size>=128&&at+size<=bytes.size()&&std::memcmp(bytes.data()+at,"KTMDL",5)==0;}
  if(!valid){bytes.clear();return false;}
 }
 PPCContext allocation=ctx;allocation.r3.u64=bytes.size();allocation.r4.u64=16;sub_82A00AC8(allocation,base);
 const uint32_t output=allocation.r3.u32;if(!output)return false;
 std::memcpy(base+output,bytes.data(),bytes.size());
 const uint32_t previous=REX_LOAD_U32(request+16);
 // Follow the existing direct-bundle loader's ownership convention.
 if(base[request+24]){PPCContext release=ctx;release.r3.u64=input;sub_82A00AB0(release,base);}
 REX_STORE_U32(request+16,output);REX_STORE_U32(request+20,uint32_t(bytes.size()));
 if(previous&&previous!=output&&previous!=input){PPCContext release=ctx;release.r3.u64=previous;sub_82A00AB0(release,base);}
 ctx.r3.u64=1;return true;
}
