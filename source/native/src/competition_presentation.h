#pragma once
#include <sstream>
#include <iomanip>
extern "C" uint32_t pes6_ball_save_choice(uint32_t id);
// Competition identity follows the same save getters as native 827A0938.
// No cached "last cup": exhibition/Master League must never inherit an override.
namespace pes6_competition_presentation {
inline bool Readable(uint8_t* base,uint32_t at,size_t n){MEMORY_BASIC_INFORMATION m{};return at&&uint64_t(at)+n<=0x100000000ull&&VirtualQuery(base+at,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&base+at+n<=static_cast<uint8_t*>(m.BaseAddress)+m.RegionSize;}
inline int Current(uint8_t* base){
 if(!Readable(base,0x82B5A23Cu,8))return -1;
 const auto save=REX_LOAD_U32(0x82B5A23Cu),manager=REX_LOAD_U32(0x82B5A240u);
 if(!Readable(base,manager,84)||!Readable(base,save,16))return -1;
 const auto mode=REX_LOAD_U32(manager+80);
 if(mode==2){switch(base[save+14]){case 0:case 9:return 7;case 1:case 10:return 8;case 2:return 9;case 3:return 10;case 4:return 11;case 5:case 6:case 8:return 13;case 58:return 12;default:return -1;}}
 if(mode==1&&Readable(base,save+56306,1)){switch(base[save+56306]){case 11:case 13:return 0;case 15:return 3;case 18:return 1;case 20:return 5;case 24:return 2;case 26:return 4;default:return -1;}}
 return -1;
}
inline const std::filesystem::path& Root(){static const auto root=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto p=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(p/"game/cv.img");i++)p=p.parent_path();return p/"data/editor/competitions/presentation";}();return root;}
inline bool Filename(const std::string& s){return !s.empty()&&s.size()<100&&s.find_first_not_of("0123456789-cefgisk.binwe")==std::string::npos&&s.find("..") == std::string::npos;}
inline uint32_t DefaultBall(uint8_t* base,uint32_t fallback){
 const auto id=Current(base);if(id<0)return fallback;
 static const auto choices=[](){std::map<int,unsigned> r;std::ifstream in(Root()/"balls.txt");int id;unsigned ball;while(in>>id>>ball)if(id>=0&&id<=13&&ball<64&&ball!=15)r[id]=ball;return r;}();
 const auto it=choices.find(id);if(it==choices.end())return fallback;
 //82714348 used to change only the highlighted selector row. Synchronize
 // the native match option as82745430 does, retaining every unrelated bit.
 const auto save=REX_LOAD_U32(0x82B5A23Cu);
 if(Readable(base,save+18332,4)){
  const auto alias=pes6_ball_save_choice(it->second);
  const auto option=REX_LOAD_U32(save+18332);
  REX_STORE_U32(save+18332,(option&~0x3C00u)|((alias&15u)<<10));
 }
 return it->second;
}
inline bool Decode(PPCContext& ctx,uint8_t* base){
 const int id=Current(base);if(id<0)return false;const uint32_t state=ctx.r3.u32,input=ctx.r4.u32;
 if(!Readable(base,state,24)||!Readable(base,input,16)||std::memcmp(base+input+3,"WESYS",5))return false;
 struct Row{int id;unsigned size;uint64_t hash;std::string file;};
 static const auto rows=[](){std::vector<Row> r;std::ifstream in(Root()/"packages.txt");Row x;while(in>>std::dec>>x.id>>x.size>>std::hex>>x.hash>>x.file)if(x.id>=0&&x.id<=13&&x.size<=64000000&&Filename(x.file))r.push_back(x);return r;}();
 const unsigned size=REX_LOAD_U32(input+8);bool candidate=false;for(const auto& row:rows)if(row.id==id&&row.size==size)candidate=true;if(!candidate||!Readable(base,input,size_t(size)+16))return false;
 uint64_t hash=14695981039346656037ull;for(unsigned i=0;i<size;i++)hash=(hash^base[input+16+i])*1099511628211ull;
 for(const auto& row:rows)if(row.id==id&&row.size==size&&row.hash==hash){
  static std::mutex mutex;static std::map<std::string,std::vector<uint8_t>> cache;std::lock_guard lock(mutex);auto& bytes=cache[row.file];
  if(bytes.empty()){std::ifstream in(Root()/"packages"/row.file,std::ios::binary|std::ios::ate);if(!in||in.tellg()<20||in.tellg()>64000000)return false;bytes.resize(size_t(in.tellg()));in.seekg(0);in.read(reinterpret_cast<char*>(bytes.data()),bytes.size());const auto be=[&](size_t p){return uint32_t(bytes[p])<<24|uint32_t(bytes[p+1])<<16|uint32_t(bytes[p+2])<<8|bytes[p+3];};bool valid=bool(in)&&be(4)==8;const unsigned count=be(0);valid=valid&&count>0&&count<=4096&&8ull+12ull*count<=bytes.size();for(unsigned i=0;i<count&&valid;i++){const uint64_t offset=be(8+i*12),length=be(12+i*12);valid=!length||(offset>=8ull+count*12ull&&offset+length<=bytes.size());}if(!valid){bytes.clear();return false;}}
  PPCContext alloc=ctx;alloc.r3.u64=bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);if(!alloc.r3.u32)return false;std::memcpy(base+alloc.r3.u32,bytes.data(),bytes.size());REX_STORE_U32(state+8,input+16);REX_STORE_U32(state+12,alloc.r3.u32);REX_STORE_U32(state+16,size);REX_STORE_U32(state+20,uint32_t(bytes.size()));ctx.r3.u64=1;return true;
 }
 return false;
}
// The commercial builder reads WE panels directly (821608E8), bypassing the
// normal GPU texture constructor. Preserve its original allocation/free pair.
inline uint32_t Image(PPCContext& ctx,uint8_t* base,uint32_t source,bool panel){
 const int id=Current(base);if(!Readable(base,source,16)||std::memcmp(base+source,"WE00",4))return source;
 const unsigned texture=REX_LOAD_U16(source+4);if(!panel&&texture!=25009&&texture!=25014&&texture!=41220)return source;
 struct Row{int id;char kind;unsigned texture;std::string file;};static const auto rows=[](){std::vector<Row> r;std::ifstream in(Root()/"images.txt");Row x;while(in>>x.id>>x.kind>>x.texture>>x.file)if(Filename(x.file)&&(x.kind=='P'||x.kind=='K'))r.push_back(x);return r;}();
 // Global commercial rows (-1/P) also cover exhibition and Master League.
 // Imported atlas overrides remain strictly competition-specific.
 for(const auto& row:rows)if((row.id==id||(panel&&row.id==-1))&&row.texture==texture&&row.kind==(panel?'P':'K')){
  static std::mutex mutex;struct Cached{uint32_t guest=0;std::vector<uint8_t> bytes;};static std::map<std::string,Cached> cache;std::lock_guard lock(mutex);auto& c=cache[row.file];
  if(c.bytes.empty()){std::ifstream in(Root()/"images"/row.file,std::ios::binary|std::ios::ate);if(!in||in.tellg()<16||in.tellg()>16000000)return source;c.bytes.resize(size_t(in.tellg()));in.seekg(0);in.read(reinterpret_cast<char*>(c.bytes.data()),c.bytes.size());if(!in||!Readable(base,source,16)||std::memcmp(c.bytes.data(),base+source,16)){c.bytes.clear();return source;}}
  if(std::memcmp(c.bytes.data(),base+source,16))return source;
  if(!c.guest){PPCContext alloc=ctx;alloc.r3.u64=c.bytes.size();alloc.r4.u64=16;sub_82A00AC8(alloc,base);if(!alloc.r3.u32)return source;c.guest=alloc.r3.u32;std::memcpy(base+c.guest,c.bytes.data(),c.bytes.size());}return c.guest;
 }
 return source;
}
}
extern "C" uint32_t pes6_competition_default_ball(uint8_t* base,uint32_t fallback){return pes6_competition_presentation::DefaultBall(base,fallback);}
extern "C" uint32_t pes6_competition_image(PPCContext& ctx,uint8_t* base,uint32_t source,bool panel){return pes6_competition_presentation::Image(ctx,base,source,panel);}
