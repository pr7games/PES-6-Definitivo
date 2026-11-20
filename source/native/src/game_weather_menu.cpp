#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "weather_particles.h"
#include <atomic>
#include <cstring>
#include <cstdlib>
#include <windows.h>

extern "C" bool pes6_weather_enabled();
extern "C" bool pes6_rain_enabled();
extern "C" void pes6_player_match_end();
namespace {
std::atomic<bool> torrential{false};
uint32_t menu_data=0;
bool Valid(uint8_t* base,uint32_t at,size_t bytes){
  if(at<0x10000||uint64_t(at)+bytes>=0xe0000000ull)return false;
  MEMORY_BASIC_INFORMATION info{};
  return VirtualQuery(base+at,&info,sizeof(info))&&info.State==MEM_COMMIT&&
    !(info.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&
    base+at+bytes<=static_cast<uint8_t*>(info.BaseAddress)+info.RegionSize;
}
template<class Function>
uint32_t Native(PPCContext original,uint8_t* base,Function function,uint32_t a,uint32_t b=0,uint32_t c=0){
  original.r3.u64=a;original.r4.u64=b;original.r5.u64=c;
  function(original,base);return original.r3.u32;
}
bool Labels(PPCContext ctx,uint8_t* base){
  if(!menu_data)menu_data=Native(ctx,base,sub_826D61B0,80);
  if(!Valid(base,menu_data,80))return false;
  const auto* lang=std::getenv("PES6_TEXT_LANG");
  const char* light="Llovizna";const char* heavy="Lluvia torrencial";
  if(lang){
    if(!_stricmp(lang,"en")||!_stricmp(lang,"e")||!_stricmp(lang,"eng")){light="Drizzle";heavy="Torrential rain";}
    else if(!_stricmp(lang,"fr")||!_stricmp(lang,"f")||!_stricmp(lang,"fre")||!_stricmp(lang,"fra")){light="Bruine";heavy="Pluie torrentielle";}
    else if(!_stricmp(lang,"de")||!_stricmp(lang,"g")||!_stricmp(lang,"ger")||!_stricmp(lang,"deu")){light="Nieselregen";heavy="Starkregen";}
    else if(!_stricmp(lang,"it")||!_stricmp(lang,"i")||!_stricmp(lang,"ita")){light="Pioviggine";heavy="Pioggia torrenziale";}
    else if(!_stricmp(lang,"pt")||!_stricmp(lang,"p")||!_stricmp(lang,"por")){light="Chuvisco";heavy="Chuva torrencial";}
  }
  std::memcpy(base+menu_data+16,heavy,std::strlen(heavy)+1);
  std::memcpy(base+menu_data+48,light,std::strlen(light)+1);
  return true;
}
}
extern "C" bool pes6_torrential_selected(){return pes6_rain_enabled()&&torrential.load(std::memory_order_relaxed);}
extern "C" uint32_t pes6_weather_drop_count(){return !pes6_rain_enabled()?500:pes6_torrential_selected()?pes6::weather::torrential_drops:pes6::weather::drops;}
// Logical values remain 2=Random, 3=Torrential for native preference handling.
// The visible order is Clear, Rain, Torrential, Random, only with effects ON.
extern "C" uint32_t pes6_weather_menu_choice(uint32_t row){
  if(!pes6_rain_enabled())return row;
  return row==2?3:row==3?2:row;
}
extern "C" uint32_t pes6_weather_menu_row(uint32_t choice){
  if(!pes6_rain_enabled())return choice==3?1:choice;
  if(choice==1&&pes6_torrential_selected())choice=3;
  return choice==2?3:choice==3?2:choice;
}
extern "C" uint32_t pes6_weather_menu_key_index(uint32_t row){
  const auto choice=pes6_weather_menu_choice(row);
  return choice==3?1:choice<=2?choice:2;
}
extern "C" bool pes6_weather_menu_caption(PPCContext& ctx,uint8_t* base,uint32_t row){
  if(!pes6_rain_enabled()||(row!=1&&row!=2)||!Labels(ctx,base))return false;
  Native(ctx,base,sub_827378E8,ctx.r3.u32,menu_data+(row==1?48:16));return true;
}

// UI row 2 remains Random; row 3 explicitly opts into the extra visual mode.
// The native match configuration always receives the ordinary rain enum (1).
extern "C" bool pes6_weather_menu_resolve(uint8_t* base,uint32_t weather){
  if(!Valid(base,weather,1))return false;
  if(pes6_weather_enabled())pes6_player_match_end();
  const bool requested=base[weather]==3;
  const bool heavy=requested&&pes6_rain_enabled();
  torrential.store(heavy,std::memory_order_relaxed);
  if(requested)base[weather]=1;
  return heavy;
}
extern "C" void pes6_weather_normalize_saved(uint8_t* base){
  if(pes6_rain_enabled())return;
  // UI preference uses logical value3, with optional Random bit. Normalize
  // before any native resolver or menu can consume the persisted selection.
  constexpr uint32_t preference=0x82D8AA58u;
  if(Valid(base,preference,1)&&(base[preference]&0x7f)==3)base[preference]=1;
  constexpr uint32_t config_pointer=0x83139438u;
  if(Valid(base,config_pointer,4)){
    const auto config=REX_LOAD_U32(config_pointer);
    if(Valid(base,config,330))for(unsigned offset:{14u,326u})if((base[config+offset]&0x7f)==3)base[config+offset]=1;
  }
  torrential.store(false,std::memory_order_relaxed);
}
extern "C" void pes6_weather_menu_restore(uint8_t* base,uint32_t weather,bool heavy){
  if(heavy&&Valid(base,weather,1))base[weather]=3;
}
extern "C" void pes6_weather_menu_ready(PPCContext& ctx,uint8_t* base,uint32_t owner){
  if(!Valid(base,owner,152))return;
  if(pes6_weather_enabled())pes6_player_match_end();
  if(!pes6_rain_enabled())return;
  const auto root=REX_LOAD_U32(owner+148);
  if(!Valid(base,root,480))return;
  const auto panel=Native(ctx,base,sub_8272F650,root,1);
  if(!Valid(base,panel,468))return;
  const auto row=Native(ctx,base,sub_8272E410,panel,2);
  if(!Valid(base,row,348))return;
  const auto wrappers=REX_LOAD_U32(row+344);
  if(!Valid(base,wrappers,4))return;
  const auto combo=REX_LOAD_U32(wrappers);
  // Validated native class: +44 installs message IDs, +76 selects a row.
  if(!Valid(base,combo,288)||REX_LOAD_U32(combo)!=0x8200BD10u)return;
  if(!Labels(ctx,base))return;
  std::memcpy(base+menu_data,base+0x820074B8u,8);
  REX_STORE_U32(menu_data+8,REX_LOAD_U32(0x820074BCu));
  REX_STORE_U32(menu_data+12,REX_LOAD_U32(0x820074C0u));
  auto selected=Native(ctx,base,sub_8273FB30,combo);
  if(REX_LOAD_U32(combo+240)==3&&selected==2)selected=3;
  Native(ctx,base,sub_8273FC78,combo,menu_data,4);
  const auto list=REX_LOAD_U32(combo+276);
  if(!Valid(base,list,480))return;
  for(unsigned item:{1u,2u}){
    const auto entry=Native(ctx,base,sub_8272E4B0,list,0,item);
    if(!Valid(base,entry,316))return;
    const auto texts=REX_LOAD_U32(entry+312);
    if(!Valid(base,texts,4))return;
    const auto text=REX_LOAD_U32(texts);
    if(!Valid(base,text,208))return;
    Native(ctx,base,sub_827378E8,text+96,menu_data+(item==1?48:16));
  }
  Native(ctx,base,sub_82740098,combo,selected<4?selected:2);
  REXLOG_INFO("PES6 weather: localized drizzle/torrential selector (10000 heavy drops)");
}
