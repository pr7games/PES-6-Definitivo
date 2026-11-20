#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include <cstring>
#include <bit>
#include <filesystem>
#include <fstream>
#include <windows.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <map>

namespace {
uint32_t ball_tables=0;
struct CompetitionNameRow{std::string text;uint32_t guest=0;};
std::map<uint32_t,CompetitionNameRow> competition_names;
void ReadCompetitionNames(const std::filesystem::path& root){std::ifstream in(root/"data/editor/competitions/names.txt");uint32_t id;std::string name;while(in>>id>>std::quoted(name))if(!name.empty()&&name.size()<=63)competition_names[id]={name,0};}
bool CompetitionName(PPCContext& ctx,uint8_t* base){
 static bool loaded=false;if(!loaded){loaded=true;
#ifndef PES6_BALL_TEST
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();ReadCompetitionNames(root);
#endif
 }
 const auto it=competition_names.find(ctx.r3.u32);if(it==competition_names.end())return false;auto& row=it->second;
 if(!row.guest){PPCContext alloc=ctx;alloc.r3.u64=row.text.size()+1;sub_826D61B0(alloc,base);if(!alloc.r3.u32)return false;row.guest=alloc.r3.u32;std::memcpy(base+row.guest,row.text.c_str(),row.text.size()+1);}ctx.r3.u64=row.guest;return true;
}

struct BallRow{unsigned id,entry,preview;std::string name;};
std::vector<BallRow> editor_balls;
void ReadEditorBalls(const std::filesystem::path& root){
 std::vector<BallRow> result;std::ifstream in(root/"data/editor/asset-runtime.txt");std::string line;bool seen[64]{};
 while(std::getline(in,line)){std::istringstream row(line);std::string kind,name;unsigned id,entry,preview;
  if(row>>kind>>id>>entry>>preview>>std::quoted(name)&&kind=="BALL"&&id<64&&id!=15&&entry<10000&&preview<20&&!name.empty()&&name.size()<64&&!seen[id]){result.push_back({id,entry,preview,name});seen[id]=true;}}
 if(result.size()<19)return;for(unsigned id=0;id<20;id++)if(id!=15&&!seen[id])return;
 editor_balls=std::move(result);
}
void LoadEditorBalls(){
 static bool loaded=false;if(loaded)return;loaded=true;
#ifndef PES6_BALL_TEST
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();ReadEditorBalls(root);
#endif
}
unsigned Playable(){LoadEditorBalls();return editor_balls.empty()?19:unsigned(editor_balls.size());}
bool choice_loaded=false;
unsigned custom_choice=0;
std::filesystem::path choice_file;
void LoadChoice(){
 if(choice_loaded)return;choice_loaded=true;
#ifndef PES6_BALL_TEST
 wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
 auto root=std::filesystem::path(exe).parent_path();
 for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
 choice_file=root/"data/balls/selection.txt";
 std::ifstream in(choice_file);unsigned saved=0;in>>saved;
 LoadEditorBalls();if(saved>=16&&((editor_balls.empty()&&saved<=19)||std::any_of(editor_balls.begin(),editor_balls.end(),[&](const BallRow& b){return b.id==saved;})))custom_choice=saved;
#endif
}
bool InitBalls(PPCContext& original,uint8_t* base){
 LoadEditorBalls();if(ball_tables)return true;
 PPCContext allocation=original;allocation.r3.u64=8192;sub_826D61B0(allocation,base);
 const auto t=allocation.r3.u32;if(!t)return false;
 std::memset(base+t,0,8192);
 // Preserve all15 physical native IDs and the persisted Random sentinel15.
 std::memcpy(base+t,base+0x82B562FCu,15);
 std::memcpy(base+t+256,base+0x82B56348u,15*4);
 std::memcpy(base+t+768,base+0x82B563C8u,15*4);
 constexpr const char* names[]={"Total 90 Aerow Ii Pl","Total 90 Aerow Ii Winter","Total 90 Aerow Ii Calcio","Teamgeist Wc Germany 2006"};
 for(unsigned i=0;i<4;i++){
  base[t+15+i]=16+i;
  // Expanded native UI animations keep original0..15 and add three frames.
  REX_STORE_U32(t+256+(15+i)*4,std::bit_cast<uint32_t>(float(16+i)));
  REX_STORE_U32(t+768+(15+i)*4,0x007F0001u+i);
  std::strcpy(reinterpret_cast<char*>(base+t+1280+i*64),names[i]);
 }
 if(!editor_balls.empty())for(unsigned i=0;i<editor_balls.size();i++){const auto& b=editor_balls[i];base[t+i]=b.id;REX_STORE_U32(t+256+i*4,std::bit_cast<uint32_t>(float(b.preview)));REX_STORE_U32(t+768+i*4,0x007F0001u+i);std::strcpy(reinterpret_cast<char*>(base+t+1280+i*64),b.name.c_str());}
 const unsigned random=Playable();base[t+random]=15;
 REX_STORE_U32(t+256+random*4,REX_LOAD_U32(0x82B56348u+15*4));
 REX_STORE_U32(t+768+random*4,REX_LOAD_U32(0x82B563C8u+15*4));
 ball_tables=t;
 REXLOG_INFO("PES6 balls: {} playable balls + Random",Playable());
 return true;
}
}
extern "C" uint32_t pes6_ball_table(PPCContext& ctx,uint8_t* base,unsigned kind){
 if(!InitBalls(ctx,base))return kind==0?0x82B562FCu:kind==1?0x82B56348u:0x82B563C8u;
 return ball_tables+(kind==0?0:kind==1?256:768);
}
extern "C" uint32_t pes6_ball_count(){return ball_tables?Playable()+1:16;}
extern "C" bool pes6_ball_name(PPCContext& ctx,uint8_t* base){
 if(CompetitionName(ctx,base))return true;
 const auto message=ctx.r3.u32;
 if(message<0x007F0001u||message>=0x007F0001u+(editor_balls.empty()?4:Playable())||!InitBalls(ctx,base))return false;
 ctx.r3.u64=ball_tables+1280+(message-0x007F0001u)*64;return true;
}
extern "C" uint32_t pes6_ball_load_choice(uint32_t native);
extern "C" uint32_t pes6_ball_resource(uint32_t id){
 // Match descriptors may still carry the four-bit save alias. Resolve it at
 // the final resource boundary too, including paths bypassing82745508.
 const auto selected=pes6_ball_load_choice(id);LoadEditorBalls();
 unsigned entry=selected>=16&&selected<=19?6731+selected-16:5720+selected;
 for(const auto& b:editor_balls)if(b.id==selected){entry=b.entry;break;}
 REXLOG_INFO("PES6 match ball: descriptor={}, selected={}, resource={}",id,selected,entry);
 return entry;
}
// The options file owns only four bits for the ball ID. Keep its format intact:
// persist the native Nike12 alias there and remember the extra choice separately.
extern "C" uint32_t pes6_ball_save_choice(uint32_t id){
 LoadChoice();const auto next=id>=16&&id<64?id:0;
 if(custom_choice!=next){custom_choice=next;
#ifndef PES6_BALL_TEST
  auto temporary=choice_file;temporary+=L".tmp";
  std::ofstream out(temporary,std::ios::trunc);out<<next<<'\n';out.close();
  if(!out||!MoveFileExW(temporary.c_str(),choice_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))REXLOG_WARN("PES6 ball preference could not be persisted");
#endif
 }
 return next?12:id;
}
extern "C" uint32_t pes6_ball_load_choice(uint32_t native){LoadChoice();return native==12&&custom_choice?custom_choice:native;}

// UI positions retain native gameplay identities; only the League selector moves Premier first.
extern "C" uint32_t pes6_league_index(uint8_t* base,uint32_t scene,uint32_t index){static constexpr unsigned order[]{4,0,1,2,3,5};return scene&&base[scene+194]==7&&index<6?order[index]:index;}
