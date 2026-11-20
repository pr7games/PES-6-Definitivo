#pragma once
#include <array>
#include <atomic>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <map>
#include "kit_crest_runtime.h"
#include "team_management.h"
namespace pes6_team_info {
struct Text {std::string value;uint32_t address=0;};
struct Home {int model=-1;Text name;};
inline std::array<std::array<Text,2>,205> teams;
inline std::array<Text,8> legacy_stadiums;
inline std::array<Home,205> homes;
inline std::array<std::array<std::array<int,21>,4>,205> colors;
inline std::array<std::array<std::array<int,2>,4>,205> models;
inline std::array<std::array<bool,4>,205> atlases{};
inline std::filesystem::path kit_root;
inline std::array<int,378> kit_ready;
struct Lettering {std::array<int,9> values{{-1,-1,-1,-1,-1,-1,-1,-1,1}};bool Enabled()const{for(unsigned i=0;i<8;i++)if(values[i]>=0)return true;return values[8]!=1;}};
inline std::array<std::array<Lettering,4>,205> lettering;
inline constexpr std::array<unsigned,8> design_offsets{84,88,87,86,85,89,90,91};
inline constexpr std::array<unsigned,8> design_counts{2,114,114,42,24,60,30,12};
inline std::array<std::array<std::array<int,8>,4>,205> designs;
inline std::atomic<bool> loaded{false};
inline std::array<bool,205> model_changes{}, color_changes{}, model_touched{}, color_touched{};
inline void RefreshOverrides(unsigned id){
 bool model=false,color=false;
 for(unsigned v=0;v<4;v++){
  model|=models[id][v][0]>=0;
  for(auto value:designs[id][v])model|=value>=0;
  for(auto value:colors[id][v])color|=value>=0;
 }
 model_changes[id]=model;color_changes[id]=color;
}
inline bool Readable(uint8_t* base,uint32_t at,size_t n){
 if(at<0x10000||uint64_t(at)+n>0xe0000000ull)return false;
 MEMORY_BASIC_INFORMATION m{};return VirtualQuery(base+at,&m,sizeof(m))&&m.State==MEM_COMMIT&&!(m.Protect&(PAGE_NOACCESS|PAGE_GUARD))&&base+at+n<=static_cast<uint8_t*>(m.BaseAddress)+m.RegionSize;
}
inline uint32_t U32(uint8_t* base,uint32_t at){return uint32_t(base[at])<<24|uint32_t(base[at+1])<<16|uint32_t(base[at+2])<<8|base[at+3];}
inline unsigned U16(uint8_t* base,uint32_t at){return unsigned(base[at])<<8|base[at+1];}
inline void Load(const std::filesystem::path& profile={}){
 if(loaded.load(std::memory_order_acquire))return;
 static std::mutex mutex;std::lock_guard<std::mutex> lock(mutex);
 if(loaded)return;pes6::teams::Load();if(!pes6::teams::loaded)return;
 struct Publish {~Publish(){for(unsigned id=0;id<205;id++)RefreshOverrides(id);loaded.store(true,std::memory_order_release);}} publish;
 kit_ready.fill(-1);for(auto& team:models)for(auto& variant:team)variant.fill(-1);for(auto& team:colors)for(auto& variant:team)variant.fill(-1);
 for(auto& team:designs)for(auto& variant:team)variant.fill(-1);
 homes[115].model=0;homes[115].name.value="San Siro";
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();
 for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
 kit_root=profile.empty()?root:profile.parent_path();
 std::ifstream in(profile.empty()?root/"data/editor/team-info.txt":profile,std::ios::binary);std::string line;std::getline(in,line);if(!line.empty()&&line.back()=='\r')line.pop_back();
 const bool legacy=line=="PES6_TEAM_INFO_V1";if(!legacy&&line!="PES6_TEAM_INFO_V2")return;
 const auto valid=[](const std::string& s,size_t max){return !s.empty()&&s.size()<=max&&std::none_of(s.begin(),s.end(),[](unsigned char c){return c<32;});};
 while(std::getline(in,line)){
  std::istringstream row(line);std::string kind,name,short_name;unsigned id;if(!(row>>kind>>id))continue;
  if(kind=="TEAM"){
   if(legacy)id+=7;if(id<teams.size()&&(row>>std::quoted(name)>>std::quoted(short_name))&&valid(name,63)&&valid(short_name,8)){teams[id][0].value=name;teams[id][1].value=short_name;}
  }else if(kind=="STADIUM"&&id<8&&(row>>std::quoted(name))&&valid(name,63))legacy_stadiums[id].value=name;
  else if(kind=="HOME"&&id<homes.size()){
   int model;if((row>>model>>std::quoted(name))&&model>=0&&model<8&&valid(name,63)){homes[id].model=model;homes[id].name.value=name;}
  }else if((kind=="KIT_LETTERING"||kind=="KIT_LETTERING_V2")&&id<lettering.size()){unsigned v;std::array<int,9> x{};if(!(row>>v)||v>=4)continue;bool valid=true;for(auto& n:x)if(!(row>>n))valid=false;for(unsigned i=0;i<2;i++)if(x[i]!=-1&&!((x[i]>=1&&x[i]<=int(i?5:12))||(x[i]>=1000&&x[i]<1820)))valid=false;for(unsigned i=2;i<8;i++)if(x[i]<-1||x[i]>65535)valid=false;if(x[8]!=0&&x[8]!=1)valid=false;if(valid){if(kind=="KIT_LETTERING"){std::swap(x[2],x[3]);std::swap(x[4],x[5]);std::swap(x[6],x[7]);}lettering[id][v].values=x;}
  }else if(kind=="KIT_ATLAS"&&id<atlases.size()){unsigned v,active;if((row>>v>>active)&&v<4&&active==1)atlases[id][v]=true;
  }else if(kind=="KIT_CREST"&&id<models.size()){unsigned v,m;if((row>>v>>m)&&v<4&&m<3)pes6::crest::modes[id][v]=int(m);
  }else if(kind=="KIT_MODEL"&&id<models.size()){unsigned v,m,c;if((row>>v>>m>>c)&&v<4&&m<256&&c<2)models[id][v]={int(m),int(c)};
  }else if(kind=="KIT_DESIGN"&&id<designs.size()){
   unsigned v,slot,value;if((row>>v>>slot>>value)&&v<4&&slot<8&&value<design_counts[slot])designs[id][v][slot]=int(value);
  }else if(kind=="KIT_COLOR"&&id<colors.size()){
   unsigned v,slot,value;if((row>>v>>slot>>value)&&v<4&&slot<21&&value<=65535)colors[id][v][slot]=value;
  }
 }
}
inline uint32_t Address(Text& text,PPCContext& ctx,uint8_t* base){
 if(text.value.empty())return 0;if(!text.address){PPCContext allocation=ctx;allocation.r3.u64=text.value.size()+1;sub_826D61B0(allocation,base);if(!allocation.r3.u32)return 0;text.address=allocation.r3.u32;std::memcpy(base+text.address,text.value.c_str(),text.value.size()+1);}return text.address;
}
inline int HomeTeam(uint8_t* base){
 if(!Readable(base,0x83139448,4))return -1;auto match=U32(base,0x83139448);if(!Readable(base,match,12))return -1;auto team=U32(base,match+4);if(!Readable(base,team,8))return -1;unsigned id=U16(base,team+6);return id<teams.size()?int(id):-1;
}
inline bool HomeName(PPCContext& ctx,uint8_t* base){
 Load();const int team=HomeTeam(base);if(team<0||homes[team].model!=int(ctx.r3.u32))return false;
 if(auto text=Address(homes[team].name,ctx,base)){ctx.r3.u64=text;return true;}return false;
}
inline void SelectHome(uint8_t* base){
 Load();int team=HomeTeam(base);if(team<0||homes[team].model<0||!Readable(base,0x83139438,4))return;
 const auto config=U32(base,0x83139438);if(Readable(base,config,16))base[config+12]=uint8_t(homes[team].model);
}
inline unsigned ResolveTeam(uint8_t* base,unsigned team){team&=0xffffu;if(team!=214&&team!=215)return team;if(!Readable(base,0x83139448,4))return 999;const auto match=U32(base,0x83139448);if(!Readable(base,match,12))return 999;const auto object=U32(base,match+(team==214?4:8));return Readable(base,object,8)?U16(base,object+6):999;}
inline bool PackageReady(unsigned entry){if(entry<5736||entry>=6114)return false;static std::mutex mutex;std::lock_guard<std::mutex> lock(mutex);auto& ready=kit_ready[entry-5736];if(ready>=0)return ready!=0;ready=0;std::ifstream file(kit_root/"data/editor/kits/packages"/(std::to_string(entry)+".bin"),std::ios::binary|std::ios::ate);if(!file||file.tellg()!=526400)return false;uint8_t header[32]{};file.seekg(0);file.read(reinterpret_cast<char*>(header),32);const auto be=[](const uint8_t* p){return uint32_t(p[0])<<24|uint32_t(p[1])<<16|uint32_t(p[2])<<8|p[3];};if(!file||be(header)!=2||be(header+4)!=8)return false;for(unsigned i=0;i<2;i++){const auto off=be(header+8+i*12),size=be(header+12+i*12);if(off!=32+i*263184||size!=263184)return false;uint8_t image[16]{};file.seekg(off);file.read(reinterpret_cast<char*>(image),16);if(!file||std::memcmp(image,"WE00",4)||image[6]!=3||image[7]!=8||image[8]!=2||image[9]||image[10]!=2||image[11]||image[4]!=0x10||image[5]!=4||image[12]!=0||image[13]!=16||image[14]!=4||image[15]!=16)return false;}ready=1;return true;}
inline bool HasAtlas(unsigned entry){Load();if(entry<5736||entry>=6114)return false;const unsigned team=(entry-5736)/2,role=(entry-5736)&1;return (atlases[team][role]||atlases[team][role+2])&&PackageReady(entry);}
inline void PatchAtlas(uint8_t* base,uint32_t kit,unsigned team,unsigned role,uint32_t variant){Load();team=ResolveTeam(base,team);if(team>=189||role>1||!Readable(base,variant,1)||!Readable(base,kit,144))return;const unsigned v=role+2*((base[variant]>>(role?0:3))&1);if(!atlases[team][v]||!PackageReady(5736+team*2+role))return;
 // Use the licensed atlas path; native glyphs, ink colors and model survive.
 for(unsigned off=16;off<=48;off+=4){const unsigned value=off==16?0:off==20?team*2+role:0xffffffffu;base[kit+off]=value>>24;base[kit+off+1]=value>>16;base[kit+off+2]=value>>8;base[kit+off+3]=value;}
}
inline void PatchDescriptorModel(uint8_t* base,uint32_t kit,unsigned team,unsigned role,uint32_t variant){
 Load();team=ResolveTeam(base,team);if(team>=189||role>1||!Readable(base,variant,1)||!Readable(base,kit,144))return;
 const unsigned flags=base[variant],v=role+2*((flags>>(role?0:3))&1);const auto& selected=models[team][v];
 // Actual collar selects deri1/deri2 via descriptor+6 (record+84).
 // Descriptor+5/record+102 select back-number meshes, NOT the collar.
 // Do not derive the collar from a generic model-remapping table or depend
 // on an earlier database patch: this is the final render descriptor.
 if(selected[0]<0||selected[0]>255||selected[1]<0||selected[1]>1)return;
 base[kit]=uint8_t(selected[0]);base[kit+6]=uint8_t(selected[1]);
}
inline std::mutex uniform_mutex;
inline std::map<uint32_t,std::array<uint8_t,432>> original_uniforms;
inline const std::array<uint8_t,432>& CaptureUniform(uint8_t* base,uint32_t record){
 auto [it,inserted]=original_uniforms.try_emplace(record);
 if(inserted)std::memcpy(it->second.data(),base+record,432);
 return it->second;
}
inline std::array<uint8_t,432> OriginalUniform(uint8_t* base,uint32_t record){
 std::lock_guard lock(uniform_mutex);return CaptureUniform(base,record);
}
inline void PatchModels(uint8_t* base,unsigned team,uint32_t record){
 Load();if(team>=models.size()||(!model_changes[team]&&!model_touched[team])||!Readable(base,record,432))return;
 model_touched[team]=true;
 std::lock_guard lock(uniform_mutex);const auto& original=CaptureUniform(base,record);
 for(unsigned v=0;v<4;v++){for(auto offset:design_offsets)base[record+v*108+offset]=original[v*108+offset];base[record+v*108+101]=original[v*108+101];}
 if(U16(base,record+104)==0xffff)for(unsigned v=0;v<4;v++)for(unsigned slot=0;slot<8;slot++)if(designs[team][v][slot]>=0)base[record+v*108+design_offsets[slot]]=uint8_t(designs[team][v][slot]);
 for(unsigned v=0;v<4;v++)if(models[team][v][0]>=0){base[record+v*108+101]=uint8_t(models[team][v][0]);base[record+v*108+84]=uint8_t(models[team][v][1]);}
}
inline void PatchColors(uint8_t* base,unsigned team,uint32_t record){
 Load();if(team>=colors.size()||(!color_changes[team]&&!color_touched[team])||!Readable(base,record,432)||U16(base,record+104)!=0xffff)return;
 color_touched[team]=true;
 std::lock_guard lock(uniform_mutex);const auto& original=CaptureUniform(base,record);
 for(unsigned v=0;v<4;v++)for(unsigned slot=0;slot<21;slot++){const auto offset=v*108+slot*4;base[record+offset]=original[offset];base[record+offset+1]=original[offset+1];}
 for(unsigned v=0;v<4;v++)for(unsigned slot=0;slot<21;slot++)if(colors[team][v][slot]>=0){const auto at=record+v*108+slot*4;const auto word=(unsigned(colors[team][v][slot])&0xfffeu)|(U16(base,at)&1u);base[at]=word>>8;base[at+1]=word;}
}
}
inline void ApplyExternalTeamInfo(PPCContext& ctx,uint8_t* base,const std::filesystem::path& profile={}){
 using namespace pes6_team_info;Load(profile);
 for(unsigned id=0;id<teams.size();id++)for(unsigned part=0;part<2;part++)if(auto address=Address(teams[id][part],ctx,base))REX_STORE_U32(0x82D55218u+id*24+part*4,address);
 for(unsigned id=0;id<8;id++)if(auto address=Address(legacy_stadiums[id],ctx,base))REX_STORE_U32(0x82D4FD40u+id*4,address);
}
