#pragma once
#include <set>
namespace pes6_replay_transition_diagnostic {
inline void Event(PPCContext& ctx,uint8_t* base,unsigned kind){
 using namespace pes6_competition_presentation;
 static const bool enabled=std::filesystem::exists(Root()/"trace.enabled");if(!enabled)return;
 static std::mutex mutex;std::lock_guard lock(mutex);
 static const auto folder=[](){auto p=Root()/"trace"/std::to_string(GetCurrentProcessId());std::filesystem::create_directories(p);return p;}();
 static std::ofstream log(folder/"events.tsv");static bool header=false;static unsigned events=0,dumps=0;static size_t total=0;static std::set<std::pair<unsigned,uint64_t>> seen;
 if(!header){log<<"tick\tkind\tlr\tr3\tr4\tr5\tr6\tcompetition\treplay\timage_file\n";header=true;}
 if(kind==9){static ULONGLONG previous=0;const auto now=GetTickCount64();if(now-previous<1000)return;previous=now;}
 if(events>=32000&&kind!=9&&kind!=10)return;
 const unsigned replay=Readable(base,0x8313A93Eu,1)?base[0x8313A93Eu]:255;
 // Textures are recorded even before replay starts: the transition can preload.
 std::string file;const uint32_t source=kind==2?ctx.r4.u32:ctx.r3.u32;
 if(kind==10){
  const auto request=ctx.r3.u32,input=ctx.r4.u32;const size_t size=ctx.r5.u32;
  if(!Readable(base,request,124)||!size||size>16000000||!Readable(base,input,size))return;
  const unsigned key=REX_LOAD_U32(request+8);uint64_t hash=14695981039346656037ull;for(size_t i=0;i<size;i++)hash=(hash^base[input+i])*1099511628211ull;
  if(!seen.emplace(kind,hash).second)return;
  if(dumps<2048&&total+size<=192000000){std::ostringstream name;name<<"resource-"<<std::hex<<key<<'-'<<hash<<".bin";file=name.str();std::ofstream out(folder/file,std::ios::binary);out.write(reinterpret_cast<char*>(base+input),size);dumps++;total+=size;}
 }
 if(kind>=11){static std::set<std::tuple<unsigned,uint32_t,uint32_t,uint32_t>> calls;if(calls.size()>12000)return;if(!calls.emplace(kind,uint32_t(ctx.lr),ctx.r3.u32,ctx.r4.u32).second)return;}
 if(kind==1||kind==2){
  if(!Readable(base,source,16)||std::memcmp(base+source,"WE00",4))return;
  size_t size=0;const unsigned type=base[source+7],w=REX_LOAD_U16(source+8),h=REX_LOAD_U16(source+10);
  if(type==8&&w&&h&&w<=4096&&h<=4096)size=1040ull+size_t(w)*h;
  else if((type==24||type==32)&&w&&h&&w<=4096&&h<=4096)size=16ull+size_t(w)*h*(type/8);
  else if(type==255&&Readable(base,source,144)&&!std::memcmp(base+source+16,"DDS ",4)){
   const auto le=[&](unsigned p){return uint32_t(base[source+p])|uint32_t(base[source+p+1])<<8|uint32_t(base[source+p+2])<<16|uint32_t(base[source+p+3])<<24;};
   unsigned dw=le(32),dh=le(28),mips=le(44);if(dw&&dh&&dw<=4096&&dh<=4096&&mips<=16){size=144;const bool dxt1=!std::memcmp(base+source+100,"DXT1",4);for(unsigned i=0;i<std::max(1u,mips);i++){size+=size_t((dw+3)/4)*((dh+3)/4)*(dxt1?8:16);dw=std::max(1u,dw/2);dh=std::max(1u,dh/2);}}
  }
  if(!size||size>16000000||!Readable(base,source,size))return;
  uint64_t hash=14695981039346656037ull;for(size_t i=0;i<size;i++)hash=(hash^base[source+i])*1099511628211ull;
  if(!seen.emplace(kind,hash).second)return;
  if(dumps<1024&&total+size<=96000000){std::ostringstream name;name<<std::hex<<hash<<".we";file=name.str();std::ofstream out(folder/file,std::ios::binary);out.write(reinterpret_cast<char*>(base+source),size);dumps++;total+=size;}
 }
 log<<GetTickCount64()<<'\t'<<kind<<'\t'<<std::hex<<ctx.lr<<'\t'<<ctx.r3.u32<<'\t'<<ctx.r4.u32<<'\t'<<ctx.r5.u32<<'\t'<<ctx.r6.u32<<std::dec<<'\t'<<Current(base)<<'\t'<<replay<<'\t'<<file<<'\n';log.flush();events++;
}
}
extern "C" void pes6_replay_transition_trace(PPCContext& ctx,uint8_t* base,unsigned kind){pes6_replay_transition_diagnostic::Event(ctx,base,kind);}
