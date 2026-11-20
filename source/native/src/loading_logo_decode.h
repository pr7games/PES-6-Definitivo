#pragma once
// Called only on native decompression requests, before the stock texture decoder.
inline bool DecodeLoadingLogo(PPCContext& ctx,uint8_t* base){
 const uint32_t state=ctx.r3.u32,input=ctx.r4.u32;if(!state||!input)return false;
 struct Source{uint32_t length;uint64_t hash;std::string lang;unsigned entry;};
 static std::filesystem::path root;
 static const auto sources=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();std::ifstream in(root/"data/loading-logo/packages/sources.txt");std::vector<Source> result;Source s{};while(in>>std::dec>>s.length>>std::hex>>s.hash>>s.lang)if(s.length<10000000&&s.entry<3000&&s.lang.size()==1&&std::string("cefgis").find(s.lang)!=std::string::npos)result.push_back(s);return result;}();
 const auto length=REX_LOAD_U32(input+8);bool candidate=false;for(const auto& s:sources)if(s.length==length)candidate=true;if(!candidate)return false;
 uint64_t hash=14695981039346656037ull;for(unsigned i=0;i<length;i++)hash=(hash^base[input+16+i])*1099511628211ull;
 for(const auto& source:sources)if(source.length==length&&source.hash==hash){
  static std::mutex mutex;static std::map<std::string,std::vector<uint8_t>> cache;std::lock_guard lock(mutex);const auto key=source.lang+"-"+std::to_string(source.entry);auto& bytes=cache[key];
  if(bytes.empty()){std::ifstream in(root/"data/loading-logo/packages"/(source.lang+".bin"),std::ios::binary|std::ios::ate);if(!in||in.tellg()<20||in.tellg()>10000000)return false;bytes.resize(size_t(in.tellg()));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
   const auto be=[&](size_t at){return uint32_t(bytes[at])<<24|uint32_t(bytes[at+1])<<16|uint32_t(bytes[at+2])<<8|bytes[at+3];};bool valid=bool(in)&&be(4)==8;const unsigned count=be(0);valid=valid&&count>0&&count<4096&&8ull+count*12<=bytes.size();for(unsigned i=0;i<count&&valid;i++){const uint64_t at=be(8+i*12),size=be(12+i*12);valid=at>=8ull+count*12&&at+size<=bytes.size();}if(!valid){bytes.clear();return false;}}
  PPCContext alloc=ctx;alloc.r3.u64=bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);if(!alloc.r3.u32)return false;std::memcpy(base+alloc.r3.u32,bytes.data(),bytes.size());REX_STORE_U32(state+8,input+16);REX_STORE_U32(state+12,alloc.r3.u32);REX_STORE_U32(state+16,length);REX_STORE_U32(state+20,uint32_t(bytes.size()));ctx.r3.u64=1;return true;
 }
 return false;
}
