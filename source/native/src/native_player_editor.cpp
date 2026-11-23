extern "C" bool pes6_keyboard_active();
#include "boot_catalog.h"
#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "player_editor_runtime.h"
#include "native_player_preview.h"
#include "edit_menu_effects.h"
#include "menu_texture_upload.h"
#include "preview_reload.h"
#include <windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

// Independent guest scenes, rendered exclusively by PES6's native UI engine.
// edit_basic is never used as the canvas for the new screens.
namespace {
using namespace pes6::editor;
enum class Page { Categories, Appearance, Boots, Accessories, Animations, Face, Hair, FaceDetail, Information };
struct Quad { uint32_t widget=0,primitive=0; };
struct Cell { Quad border,fill,image,slider_track,slider_cursor;std::array<Quad,16> slider_segments;uint32_t label=0,value=0,button=0; };
struct Scene { std::array<uint32_t,2> arrows{};Quad divider;uint32_t root=0,title=0,heading=0,preview_status=0,cat_ok_label=0,info_def_label=0;Quad panel,scroll_track,scroll_thumb,cat_top_bar,cat_ok_fill,info_def_fill;std::array<Cell,12> cells; };
struct Editor {
  uint32_t owner=0,original=0,original_geometry=0,text=0,measure=0,font=0;
  uint16_t player=0;
  Page page=Page::Categories;
  unsigned selected=0,offset=0,total=0;
  unsigned category_selected=0;
  unsigned face_group=0;
  int pulse=-1,nationality_drawn_country=-1;bool nationality_drawn_info=false;
  int focus=0; // -1: native OK; 0: section content; +1: native Default
  bool active=false;
  bool editing=false;
  int direct_info_subpage=0; // 0: none, 1: opening, 2: inside native submenu
  float font_size=0;
  float label_x=-321,value_x=-97,arrow_y=0,arrow_left=-15;
  std::array<uint32_t,2> nationality_arrows{};
  std::array<Scene,unsigned(Page::Information)> scenes;
  std::vector<Accessory> appearance;
  std::map<std::string,float> text_widths;
} editor;
std::array<uint32_t,boot_capacity> boot_textures{};
// Read-only geometry descriptor shared by our frames, retained for the UI's
// deferred disposal. A single allocation is reused for the whole process.
uint32_t frame_geometry_template=0,information_geometry_template=0;

bool Valid(uint8_t* base,uint32_t address,size_t length) {
  if(address<0x10000||uint64_t(address)+length>=0xe0000000ull)return false;
  const uint8_t* p=base+address;
  struct Region{const uint8_t* start=nullptr,*end=nullptr;};
  thread_local std::array<Region,4> cache{};
  thread_local unsigned next_slot=0;
  for(const auto& r:cache){if(r.start&&p>=r.start&&p+length<=r.end)return true;}
  while(length){MEMORY_BASIC_INFORMATION info{};
    if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const auto* rs=static_cast<const uint8_t*>(info.BaseAddress);const auto* re=rs+info.RegionSize;
    cache[next_slot]={rs,re};next_slot=(next_slot+1)&3u;
    const size_t n=std::min(length,size_t(re-p));
    if(!n)return false;p+=n;length-=n;
  }return true;
}
uint32_t Native(PPCContext& context,uint8_t* base,PPCFunc* fn,
                uint32_t a=0,uint32_t b=0,uint32_t c=0,uint32_t d=0) {
  PPCContext call=context;call.r3.u64=a;call.r4.u64=b;call.r5.u64=c;call.r6.u64=d;
  fn(call,base);return call.r3.u32;
}
void Float(uint8_t* base,uint32_t at,float value){REX_STORE_U32(at,std::bit_cast<uint32_t>(value));}
uint32_t Allocate(PPCContext& ctx,uint8_t* base,unsigned size) {
  const uint32_t p=Native(ctx,base,sub_826D61B0,size);if(p)std::memset(base+p,0,size);return p;
}
// The original root owns compiled geometry. Copy its descriptor and only the
// seven nationality-box records; never modify the shared original resource.
void StripNationality(PPCContext& ctx,uint8_t* base,uint32_t root){
  const uint32_t source=REX_LOAD_U32(root+140);
  if(!Valid(base,source,24))return;
  const unsigned count=REX_LOAD_U16(source);
  const uint32_t table=REX_LOAD_U32(source+12);
  if(count<32||count>128||!Valid(base,table,count*4))return;
  if(!frame_geometry_template)frame_geometry_template=Allocate(ctx,base,4096);
  if(!frame_geometry_template)return;
  const uint32_t target=frame_geometry_template,newTable=target+32;
  std::memcpy(base+target,base+source,24);
  std::memcpy(base+newTable,base+table,count*4);
  REX_STORE_U32(target+12,newTable);
  uint32_t cursor=newTable+count*4;
  // Record 27 is the Default button and must remain intact.
  for(unsigned index:{18u,24u,25u,26u,28u,29u,30u,31u}){
    const uint32_t record=REX_LOAD_U32(table+index*4);
    if(!Valid(base,record,28))continue;
    std::memcpy(base+cursor,base+record,28);if(index!=18)base[cursor]=0;
    REX_STORE_U32(newTable+index*4,cursor);cursor+=28;
  }
  REX_STORE_U32(root+140,target);base[root+52]|=0x23;
}
// Information retains its nationality box, but gets a private silhouette record.
void PrepareInformationGeometry(PPCContext& ctx,uint8_t* base){
  const auto source=REX_LOAD_U32(editor.original+140);
  if(!Valid(base,source,24))return;
  const unsigned count=REX_LOAD_U16(source);const auto table=REX_LOAD_U32(source+12);
  if(count<19||count>128||!Valid(base,table,count*4))return;
  const auto shape=REX_LOAD_U32(table+18*4);if(!Valid(base,shape,28))return;
  if(!information_geometry_template)information_geometry_template=Allocate(ctx,base,4096);
  if(!information_geometry_template)return;
  const auto target=information_geometry_template,newTable=target+32,record=newTable+count*4;
  std::memcpy(base+target,base+source,24);std::memcpy(base+newTable,base+table,count*4);
  std::memcpy(base+record,base+shape,28);
  REX_STORE_U32(target+12,newTable);REX_STORE_U32(newTable+18*4,record);
  editor.original_geometry=source;REX_STORE_U32(editor.original+140,target);base[editor.original+52]|=7;
}
void Text(PPCContext& ctx,uint8_t* base,uint32_t node,const std::string& value) {
  if(!node)return;
  const size_t length=std::min<size_t>(value.size(),1023);
  std::memcpy(base+editor.text,value.data(),length);base[editor.text+length]=0;
  const uint32_t table=REX_LOAD_U32(node+96);
  if(!Valid(base,table,20))return;
  if(auto* setter=rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(table+16)))
    Native(ctx,base,setter,node+96,editor.text);
  base[node+52]|=0x63;
}
void PlaceText(uint8_t* base,uint32_t node,float x,float y,float width,float height=12.f) {
  Float(base,node+32,x);Float(base,node+36,y);
  Float(base,node+108,width);Float(base,node+112,height);
  Float(base,node+208,width);Float(base,node+212,height);
  base[node+52]|=0x63;
}
void FontScale(uint8_t* base,uint32_t node,float size){Float(base,node+152,size);base[node+52]|=0x63;}
float ReadFloat(uint8_t* base,uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));}
void Parameter(PPCContext& ctx,uint8_t* base,uint32_t node,float value){
  PPCContext call=ctx;call.r3.u64=node;call.f1.f64=value;sub_8272C000(call,base);
}
uint32_t VectorItem(uint8_t* base,uint32_t node,unsigned offset,unsigned index){
  const auto begin=REX_LOAD_U32(node+offset),end=REX_LOAD_U32(node+offset+4);
  return begin&&end>=begin+4*(index+1)&&Valid(base,begin+4*index,4)?REX_LOAD_U32(begin+4*index):0;
}
void HideVector(uint8_t* base,uint32_t node,unsigned offset){
  for(unsigned i=0;i<128;i++){auto child=VectorItem(base,node,offset,i);if(!child)break;base[child+53]=0;}
}
uint32_t Label(PPCContext& ctx,uint8_t* base,uint32_t root,uint32_t sample) {
  const auto node=Allocate(ctx,base,272);if(!node)return 0;
  Native(ctx,base,sub_82738328,node,0,root);
  // Only the borrowed font atlas and scalar style are shared; strings and
  // glyph buffers are independently owned. Every row uses the same font size.
  REX_STORE_U32(node+148,REX_LOAD_U32(sample+148));
  REX_STORE_U32(node+152,REX_LOAD_U32(sample+152));
  REX_STORE_U32(node+176,0);base[node+180]=0;base[node+181]=0;
  REX_STORE_U32(node+48,0x171717ffu);base[node+83]=0;
  return node;
}
// The same temporary text layout used by 8272DA10 for the native help bar.
// Font size is an atlas scale, not a character width; never estimate width
// from its numeric value or from the count of UTF-8 bytes/characters.
float TextWidth(PPCContext& ctx,uint8_t* base,const std::string& value){
  if(value.empty()||!editor.measure)return 0; // font index zero is valid
  if(const auto found=editor.text_widths.find(value);found!=editor.text_widths.end())return found->second;
  const auto layout=editor.measure;
  Native(ctx,base,sub_82733D98,layout);
  REX_STORE_U32(layout,0x820bdbccu);
  REX_STORE_U32(layout+52,editor.font);Float(base,layout+56,editor.font_size);
  std::memcpy(base+editor.text,value.c_str(),std::min<size_t>(value.size()+1,1024));
  Native(ctx,base,sub_82734708,layout,editor.text);
  Native(ctx,base,sub_8273C320,layout);
  const float width=ReadFloat(base,layout+36);
  Native(ctx,base,sub_8273C0B0,layout);
  if(width>0&&width<10000){editor.text_widths.emplace(value,width);return width;}
  return 0;
}
std::string RowLabel(const std::string& label){
  if(label=="Profundidad de cabeza")return "Fondo de cabeza";
  if(label=="Mu\xc3\xb1" "equera izquierda")return "Mu\xc3\xb1" "equera izq.";
  if(label=="Mu\xc3\xb1" "equera derecha")return "Mu\xc3\xb1" "equera der.";
  if(label=="Brazalete izquierdo")return "Brazalete izq.";
  if(label=="Brazalete derecho")return "Brazalete der.";
  if(label=="Malla bajo pantal\xc3\xb3n")return "Malla interior";
  if(label=="Saque de volea (portero)")return "Volea (portero)";
  return label;
}
Quad NewQuad(PPCContext& ctx,uint8_t* base,uint32_t root,bool textured=false) {
  Quad q;q.widget=Native(ctx,base,sub_827372B0,textured?1:0,root,0xffff);
  if(q.widget)q.primitive=Native(ctx,base,sub_827333F8,q.widget);
  return q;
}
void Rect(uint8_t* base,Quad q,float x,float y,float width,float height,uint32_t top,uint32_t bottom) {
  if(!q.primitive)return;
  const std::array<float,8> points{x,y,x+width,y,x,y+height,x+width,y+height};
  for(unsigned i=0;i<4;i++){
    Float(base,q.primitive+80+i*16,points[i*2]);Float(base,q.primitive+84+i*16,points[i*2+1]);
    Float(base,q.primitive+88+i*16,0);Float(base,q.primitive+92+i*16,1);
    REX_STORE_U32(q.primitive+144+i*4,i<2?top:bottom);
  }
  base[q.primitive+52]|=7;base[q.widget+52]|=3;
}
void Show(Quad q,uint8_t* base,bool visible){if(q.widget)base[q.widget+53]=visible;}
uint32_t NewArrow(PPCContext& ctx,uint8_t* base,uint32_t parent,uint32_t sample){
  if(!Valid(base,sample,256))return 0;
  const auto node=Allocate(ctx,base,256);if(!node)return 0;
  Native(ctx,base,sub_82733690,node,REX_LOAD_U32(sample+240),parent,0xffff);
  base[node+53]=0;return node;
}
void Arrow(PPCContext& ctx,uint8_t* base,uint32_t node,bool visible,float x,float y,bool enabled){
  if(!node)return;
  base[node+53]=visible;if(!visible)return;
  Float(base,node+32,x);Float(base,node+36,y);base[node+52]|=0x63;
  Native(ctx,base,sub_82733740,node,enabled?0:1);
}
std::filesystem::path AssetRoot() {
  wchar_t file[MAX_PATH]{};GetModuleFileNameW(nullptr,file,MAX_PATH);
  auto root=std::filesystem::path(file).parent_path();
  for(int n=0;n<6&&!std::filesystem::exists(root/"game/cv.img");n++)root=root.parent_path();
  return root;
}
void LoadBoots(PPCContext& ctx,uint8_t* base) {
  const auto root=AssetRoot();EnsureBootCatalog();
  for(unsigned i=0;i<boot_count;i++){
    if(boot_textures[i])continue;
    std::ifstream file(root/"data/editor/boot-previews"/(std::to_string(i)+".we"),std::ios::binary|std::ios::ate);
    if(!file)file.open(root/"data/editor/boots"/("boot-"+std::to_string(2716+i)+".we"),std::ios::binary|std::ios::ate);
    if(!file)continue;const auto length=file.tellg();if(length<32||length>2000000)continue;
    const auto data=Allocate(ctx,base,unsigned(length));if(!data)continue;
    file.seekg(0);file.read(reinterpret_cast<char*>(base+data),length);
    if(!file){Native(ctx,base,sub_8280E7B8,data);continue;}
    // Menu WE textures use raw RGBA32, unlike the WE/DDS actor packages.
    // Reject incompatible headers before the native loader dereferences them.
    const unsigned width=(unsigned(base[data+8])<<8)|base[data+9];
    const unsigned height=(unsigned(base[data+10])<<8)|base[data+11];
    if(std::memcmp(base+data,"WE00",4)!=0||base[data+6]!=3||base[data+7]!=32||
       width!=512||height!=256||uint64_t(length)!=16ull+uint64_t(width)*height*4){
      OutputDebugStringA("PES6 editor: incompatible boot thumbnail skipped\n");
      Native(ctx,base,sub_8280E7B8,data);continue;
    }
    const std::string name="pes6_editor_boot_"+std::to_string(i);
    std::memcpy(base+editor.text,name.c_str(),name.size()+1);
    // Existing WE/DDS texture loader and native UI texture cache.
    {Pes6MenuRgbaScope rgba_scope;boot_textures[i]=Native(ctx,base,sub_82736390,data,editor.text);}
    if(boot_textures[i]){
      const auto texture=REX_LOAD_U32(boot_textures[i]+36);
      if(Valid(base,texture,56))Native(ctx,base,sub_8218FB18,texture,0,3);
    }
    Native(ctx,base,sub_8280E7B8,data);
  }
}
constexpr unsigned bootColumns=3, bootRows=3;
unsigned Capacity(){return editor.page==Page::Boots?bootColumns*bootRows:editor.page==Page::Categories?10:(editor.page==Page::Appearance||editor.page==Page::Accessories||editor.page==Page::Animations||editor.page==Page::Face||editor.page==Page::FaceDetail||editor.page==Page::Hair)?11:9;}
unsigned BootMaxOffset(unsigned count){
  const auto rows=(count+bootColumns-1)/bootColumns;
  return rows>bootRows?(rows-bootRows)*bootColumns:0;
}
std::array<std::string,2> BootLabel(const std::string& name){
  constexpr size_t limit=22;
  if(name.size()<=limit && (name.find('/')==std::string::npos || name.find(' ')==std::string::npos))return {name,{}};
  const auto slash=name.find('/');
  const auto variant=slash==std::string::npos?std::string::npos:name.rfind(' ',slash);
  auto split=variant;
  if(split==std::string::npos||split>limit)split=name.rfind(' ',limit);
  if(split==std::string::npos||split==0)split=std::min(limit,name.size());
  while(split<name.size()&&split>0&&(static_cast<unsigned char>(name[split])&0xc0)==0x80)--split;
  auto rest=name.substr(std::min(split+(split<name.size()&&name[split]==' '),name.size()));
  if(rest.size()>limit){
    size_t cut=limit-3;
    while(cut>0&&(static_cast<unsigned char>(rest[cut])&0xc0)==0x80)--cut;
    rest=rest.substr(0,cut)+"...";
  }
  return {name.substr(0,split),rest};
}
bool AppearancePage(){
  return editor.page==Page::Appearance||editor.page==Page::Hair||editor.page==Page::FaceDetail;
}
bool HeadPage(){
  return editor.page==Page::Face||editor.page==Page::FaceDetail||editor.page==Page::Hair;
}
std::vector<Accessory> Items(const Snapshot& s){
  if(editor.page==Page::Face){
    std::vector<Accessory> groups;
    for(unsigned i=0;i<face_groups.size();i++)groups.push_back({"face_group_"+std::to_string(i),face_groups[i],{"Abrir"},0,true});
    return groups;
  }
  if(AppearancePage()){
    std::vector<Accessory> result;
    for(const auto& item:s.appearance){
      const bool hair=item.id=="hair"||item.id.starts_with("hair_");
      const bool face=item.id=="face"||item.id.starts_with("face_");
      if(editor.page==Page::Hair&&hair)result.push_back(item);
      if(editor.page==Page::Appearance&&!hair&&!face)result.push_back(item);
      if(editor.page==Page::FaceDetail&&face){
        const auto field=FindFaceField(item.id);
        const auto component=FindFaceComponentType(item.id);
        const unsigned group=component!=face_component_types.end()?component->group:field==face_fields.end()?0:field->group;
        if(group==editor.face_group)result.push_back(item);
      }
    }
    return result;
  }
  return editor.page==Page::Animations?s.animations:s.accessories;
}
unsigned Total(const Snapshot& s){
  if(editor.page==Page::Categories)return 10;
  if(editor.page==Page::Boots)return unsigned(s.boots.size());
  return unsigned(Items(s).size());
}
bool Input(PPCContext& ctx,uint8_t* base,unsigned key,bool repeat=false){
  const auto input=REX_LOAD_U32(editor.owner+168);
  return input&&Native(ctx,base,repeat?sub_8272E038:sub_8272DF98,input,key)!=0;
}
bool Held(PPCContext& ctx,uint8_t* base,unsigned key){
  const auto input=REX_LOAD_U32(editor.owner+168);
  return input&&Native(ctx,base,sub_8272E0D8,input,key)!=0;
}
void Sound(PPCContext& ctx,uint8_t* base,unsigned kind){
  const auto manager=Native(ctx,base,sub_82241290);
  if(Valid(base,manager,32))Native(ctx,base,sub_82728928,manager,kind,REX_LOAD_U32(manager+28));
}
void ActivateField(PPCContext& ctx,uint8_t* base){
  const auto items=Items(ReadPlayer(editor.player));
  if(editor.selected>=items.size()||!items[editor.selected].available)return;
  if(BeginFieldEdit(editor.player))editor.editing=true;
}
void Visibility(uint8_t* base){
  const bool information=editor.page==Page::Information;
  base[editor.original+53]=information;
  for(unsigned i=0;i<editor.scenes.size();i++)base[editor.scenes[i].root+53]=!information&&unsigned(editor.page)==i;
}
void Footer(PPCContext& ctx,uint8_t* base,const std::string& value){
  std::string rich=value;
  const std::array<std::pair<const char*,const char*>,4> buttons{{
    {"A:","\x11" "ka"},{"B:","\x11" "kb"},{"X:","\x11" "kg"},{"Y:","\x11" "kh"}}};
  for(const auto& [plain,icon]:buttons){
    size_t at=0;while((at=rich.find(plain,at))!=std::string::npos){
      // Match a complete prompt token, never the B: suffix inside RB:.
      if(at&&rich[at-1]!=' '&&rich[at-1]!='\n'){at+=2;continue;}
      rich.replace(at,2,icon);at+=3;
    }
  }
  std::memcpy(base+editor.text,rich.c_str(),std::min<size_t>(rich.size()+1,1024));
  const auto footer=Native(ctx,base,sub_8272DEF0);
  if(footer)Native(ctx,base,sub_8272DA10,footer,editor.text);
}
void Categories(PPCContext& ctx,uint8_t* base){
  preview::Rest();editor.editing=false;editor.direct_info_subpage=0;
  editor.page=Page::Categories;editor.selected=editor.category_selected;
  editor.offset=0;editor.focus=0;
}
void Pulse(uint8_t* base){
  if(editor.page==Page::Information||(editor.focus!=0&&!(editor.page==Page::Categories&&editor.focus==-1)))return;
  const unsigned phase=unsigned(GetTickCount64()%960)/40;
  const unsigned brightness=phase<=12?phase:24-phase;
  if(editor.pulse==int(brightness))return;
  editor.pulse=int(brightness);
  auto& prim=(editor.page==Page::Categories&&editor.focus==-1)
      ?editor.scenes[0].cat_ok_fill.primitive
      :editor.scenes[unsigned(editor.page)].cells[editor.selected-editor.offset].fill.primitive;
  const uint32_t color=0xff0000ffu|((196+brightness*2)<<16)|((35+brightness)<<8);
  for(unsigned i=0;i<4;i++)REX_STORE_U32(prim+144+i*4,color);
  base[prim+52]|=3;
}
void Refresh(PPCContext& ctx,uint8_t* base){
  Visibility(base);preview::Visible(true);preview::Container(editor.page==Page::Information?editor.original:editor.scenes[unsigned(editor.page)].root);preview::Frame(editor.page==Page::Information?3:HeadPage()?9:(editor.page==Page::Categories||editor.page==Page::Appearance||editor.page==Page::Accessories||editor.page==Page::Animations)?6:0);if(editor.page==Page::Information)return;
  pes6_edit_foreground_tree(base,editor.scenes[unsigned(editor.page)].root);
  const auto snapshot=ReadPlayer(editor.player);
  const unsigned capacity=Capacity(),total=Total(snapshot);editor.total=total;if(!total&&editor.page!=Page::Boots)return;
  editor.selected=total?std::min(editor.selected,total-1):0;
  if(editor.page==Page::Boots){
    if(editor.selected<editor.offset)editor.offset=(editor.selected/bootColumns)*bootColumns;
    else if(editor.selected>=editor.offset+capacity)editor.offset=(editor.selected/bootColumns)*bootColumns-(capacity-bootColumns);
    editor.offset=std::min(editor.offset,BootMaxOffset(total));
  }else if(editor.selected<editor.offset)editor.offset=editor.selected;
  else if(editor.selected>=editor.offset+capacity)editor.offset=editor.selected-capacity+1;
  editor.pulse=-1;
  auto& scene=editor.scenes[unsigned(editor.page)];
  const std::array<std::string,8> names{"","Apariencia","Botas","Accesorios","Animaciones","Cara","Peinados",face_groups[editor.face_group]};
  if(scene.heading)Text(ctx,base,scene.heading,snapshot.player_name);
  base[scene.preview_status+53]=0;
  std::string title=names[unsigned(editor.page)];
  if(editor.page==Page::Boots)title+="   -   "+BootName(snapshot.boot_index);
  FontScale(base,scene.title,editor.font_size);
  Text(ctx,base,scene.title,title);
  const bool compact_list=editor.page==Page::Appearance||editor.page==Page::Accessories||editor.page==Page::Animations||HeadPage();
  const bool compact_boots=editor.page==Page::Boots;
  const bool compact_ui=compact_list||compact_boots;
  base[scene.title+53]=editor.page!=Page::Categories&&!compact_ui;
  Show(scene.divider,base,editor.page!=Page::Categories&&!compact_ui);
  Rect(base,scene.divider,editor.value_x-32,-156,1.5f,285,0xbcbcb8ff,0xbcbcb8ff);
  const std::array<std::string,10> categories{"Informaci\xc3\xb3n","Posici\xc3\xb3n","Habilidad","Habilidad especial","Apariencia","Botas","Vestimenta","Movimiento","Cara","Peinados"};
  const auto items=Items(snapshot);
  if(editor.focus==0){
    if(editor.page==Page::Boots)preview::Frame(10);
    else if(HeadPage())preview::Frame(9);
    else if(editor.selected<items.size()){
      const auto& field=items[editor.selected].id;
      if(editor.page==Page::Accessories)preview::Frame(field=="ankle_tape"||field=="undershorts"?8:7);
      if(editor.page==Page::Appearance){
        if(field=="thighs"||field=="calves"||field=="leg_length")preview::Frame(8);
        else if(field!="height"&&field!="weight"&&field!="waist")preview::Frame(7);
      }
    }
  }
  // The two original native controls stay in their original positions (hidden on compact_ui and Categories).
  for(unsigned pageIndex:{0u,2u}){
    const auto form=Native(ctx,base,sub_8272F650,scene.root,pageIndex);
    base[form+53]=editor.page!=Page::Categories&&!compact_ui;
    // Focus belongs to the form's cursor, not the row's resource parameter.
    if(editor.page!=Page::Categories&&!compact_ui)
      Native(ctx,base,sub_8272E288,form,editor.focus==(pageIndex==0?-1:1)?1:0);
  }
  if(compact_ui){
    Rect(base,scene.cat_top_bar,-332,-180,372,28,0x383c44f0u,0x22262cf0u);
    Rect(base,scene.panel,-332,-152,372,326,0xe9e9e7ff,0xd9d9d6ff);
    PlaceText(base,scene.heading,-322,-177,350,24);
    FontScale(base,scene.heading,editor.font_size*.90f);
    REX_STORE_U32(scene.heading+48,0xffffffffu);
    const std::string sectionName=editor.page==Page::Boots?"Botas":editor.page==Page::Appearance?"Apariencia":editor.page==Page::Accessories?"Vestimenta":editor.page==Page::Animations?"Movimiento":editor.page==Page::Face?"Cara":editor.page==Page::Hair?"Peinados":face_groups[editor.face_group];
    Text(ctx,base,scene.heading,snapshot.player_name+"  -  "+sectionName);
    Show(scene.cat_top_bar,base,true);
    const bool ok_sel=editor.focus==-1,def_sel=editor.focus==1;
    const uint32_t ok_col=ok_sel?0xffc62bffu:0xc2c2c0ffu;
    const uint32_t def_col=def_sel?0xffc62bffu:0xc2c2c0ffu;
    Rect(base,scene.info_def_fill,-316,143,160,24,def_col,def_col);
    Show(scene.info_def_fill,base,true);
    if(scene.info_def_label){
      base[scene.info_def_label+53]=1;
      PlaceText(base,scene.info_def_label,-308,145,144,20);
      FontScale(base,scene.info_def_label,editor.font_size*.82f);
      REX_STORE_U32(scene.info_def_label+48,0x181818ffu);
      Text(ctx,base,scene.info_def_label,"\x11kh Por defecto");
    }
    Rect(base,scene.cat_ok_fill,-120,143,120,24,ok_col,ok_col);
    Show(scene.cat_ok_fill,base,true);
    if(scene.cat_ok_label){
      base[scene.cat_ok_label+53]=1;
      const float ok_w=TextWidth(ctx,base,"OK")*0.90f;
      PlaceText(base,scene.cat_ok_label,-60.f-ok_w*0.5f,145,120,20);
      FontScale(base,scene.cat_ok_label,editor.font_size*.90f);
      REX_STORE_U32(scene.cat_ok_label+48,0x181818ffu);
      Text(ctx,base,scene.cat_ok_label,"OK");
    }
  }
  if(editor.page==Page::Categories){
    Rect(base,scene.cat_top_bar,-332,-180,320,28,0x383c44f0u,0x22262cf0u);
    Rect(base,scene.panel,-332,-152,320,326,0xe9e9e7ff,0xd9d9d6ff);
    PlaceText(base,scene.heading,-322,-177,300,24);
    FontScale(base,scene.heading,editor.font_size*.90f);
    REX_STORE_U32(scene.heading+48,0xffffffffu);
    const bool ok_sel=editor.focus==-1;
    const uint32_t ok_col=ok_sel?0xffc62bffu:0xc2c2c0ffu;
    Rect(base,scene.cat_ok_fill,-232,132,120,24,ok_col,ok_col);
    Show(scene.cat_top_bar,base,true);Show(scene.cat_ok_fill,base,true);
    Show(scene.info_def_fill,base,false);
    if(scene.info_def_label)base[scene.info_def_label+53]=0;
    if(scene.cat_ok_label){
      base[scene.cat_ok_label+53]=1;
      const float ok_w=TextWidth(ctx,base,"OK")*0.90f;
      PlaceText(base,scene.cat_ok_label,-172.f-ok_w*0.5f,134,120,20);
      FontScale(base,scene.cat_ok_label,editor.font_size*.90f);
      REX_STORE_U32(scene.cat_ok_label+48,0x181818ffu);
      Text(ctx,base,scene.cat_ok_label,"OK");
    }
  }
  Show(scene.scroll_track,base,total>capacity);
  Show(scene.scroll_thumb,base,total>capacity);
  if(total>capacity){
    const float scrollH=compact_ui?284.f:254.f;
    const float scrollY=compact_ui?-145.f:-128.f;
    const float scrollX=compact_ui?32.f:168.f;
    const float height=scrollH*float(capacity)/float(total);
    const float maxOffset=float(editor.page==Page::Boots?BootMaxOffset(total):total-capacity);
    Rect(base,scene.scroll_track,scrollX,scrollY,compact_ui?3.f:2.f,scrollH,0xa7a59dff,0xa7a59dff);
    Rect(base,scene.scroll_thumb,scrollX,scrollY+(scrollH-height)*float(editor.offset)/maxOffset,compact_ui?3.f:2.f,height,0xe9a900ff,0xe9a900ff);
  }
  for(auto arrow:scene.arrows)Arrow(ctx,base,arrow,false,0,0,true);
  for(unsigned i=0;i<12;i++){
    auto& cell=scene.cells[i];const unsigned index=editor.offset+i;
    const bool visible=i<capacity&&index<total,selected=index==editor.selected&&editor.focus==0;
    const bool slider=visible&&editor.page==Page::Hair&&index<items.size()&&items[index].slider_value>=0;
    Show(cell.slider_track,base,slider);Show(cell.slider_cursor,base,slider);
    for(auto segment:cell.slider_segments)Show(segment,base,slider);
    Show(cell.border,base,visible);Show(cell.fill,base,visible);Show(cell.image,base,visible&&editor.page==Page::Boots);
    base[cell.label+53]=visible;base[cell.value+53]=visible&&editor.page!=Page::Categories;
    if(cell.button)base[cell.button+53]=visible;
    if(!visible)continue;
    float x=-328,y=-130+float(i)*29.f,w=495,h=24;
    if(editor.page==Page::Categories){
      const float rowY=-145+float(i)*26.f;
      const uint32_t color=selected?0xffc62bff:(i%2?0xefefedff:0xd9d9d6ff);
      Rect(base,cell.border,-327,rowY-1,310,25,selected?0xffffffff:0xe9e9e7ff,selected?0xffffffff:0xe9e9e7ff);
      Rect(base,cell.fill,-326,rowY,308,23,color,color);
      PlaceText(base,cell.label,-318,rowY+1,292,22);
      FontScale(base,cell.label,editor.font_size*.88f);
      Text(ctx,base,cell.label,categories[index]);continue;
    }
    if(compact_list){
      const float rowY=-145+float(i)*26.f;
      const auto& item=items[index];
      const bool adjusting=selected&&editor.editing&&item.available;
      std::string displayed=item.selected>=0&&item.selected<int(item.choices.size())?item.choices[item.selected]:"--";
      if(displayed.starts_with("Por defecto"))displayed="Por defecto";
      const uint32_t color=selected?0xffc62bffu:(i%2?0xefefedffu:0xd9d9d6ffu);
      Rect(base,cell.border,-327,rowY-1,352,24,selected?0xffffffffu:0xe9e9e7ffu,selected?0xffffffffu:0xe9e9e7ffu);
      Rect(base,cell.fill,-326,rowY,350,22,color,color);
      PlaceText(base,cell.label,-318,rowY+1,170,20);
      FontScale(base,cell.label,editor.font_size*.85f);
      REX_STORE_U32(cell.label+48,item.available?0x181818ffu:0x888880ffu);
      Text(ctx,base,cell.label,RowLabel(item.label));
      PlaceText(base,cell.value,-140,rowY+1,158,20);
      FontScale(base,cell.value,editor.font_size*.85f);
      REX_STORE_U32(cell.value+48,item.available?0x181818ffu:0x888880ffu);
      if(slider){
        const int maximum=int(item.choices.size())-2;
        const int level=std::clamp(item.slider_value,0,maximum);
        Rect(base,cell.slider_track,-140,rowY+3,124,15,0x777775ff,0xf7f7f0ff);
        for(unsigned k=0;k<16;k++){
          const bool lit=int(k)*maximum/16<=level;
          Rect(base,cell.slider_segments[k],-138+k*7.5f,rowY+5,6.6f,11,
            lit?0xffd756ff:0xc6c6c2ff,lit?0xe8a41cff:0x959590ff);
        }
        Rect(base,cell.slider_cursor,-138+118.f*float(level)/float(maximum),rowY+2,2,17,0xffffffff,0xffffffff);
        PlaceText(base,cell.value,-10,rowY+1,32,20);FontScale(base,cell.value,editor.font_size*.85f);
        const std::string sVal=std::to_string(level)+(item.selected==0?" D":"");
        if(adjusting){
          const bool canBoth=item.choices.size()>1;
          Arrow(ctx,base,scene.arrows[0],true,-155.f,rowY+1.f,canBoth);
          Arrow(ctx,base,scene.arrows[1],true,16.f,rowY+1.f,canBoth);
        }
        Text(ctx,base,cell.value,sVal);
        continue;
      }
      if(adjusting){
        const float vw=TextWidth(ctx,base,displayed)*0.85f;
        const bool canBoth=item.choices.size()>1;
        Arrow(ctx,base,scene.arrows[0],true,-155.f,rowY+1.f,canBoth);
        Arrow(ctx,base,scene.arrows[1],true,-140.f+vw+8.f,rowY+1.f,canBoth);
      }
      Text(ctx,base,cell.value,displayed);
      continue;
    }
    if(editor.page==Page::Boots){x=-326+float(i%bootColumns)*118;y=-144+float(i/bootColumns)*90;w=114;h=84;}
    const uint32_t fill=selected?0xffc62bff:(i%2?0xefefedff:0xd9d9d6ff);
    Rect(base,cell.border,x-1,y-1,w+2,h+2,selected?0xffffffff:0xe9e9e7ff,selected?0xffffffff:0xe9e9e7ff);
    Rect(base,cell.fill,x,y,w,h,fill,fill);
    if(editor.page==Page::Boots){
      const auto& boot=snapshot.boots[index];
      const auto texture=boot.appearance_index<boot_count?boot_textures[boot.appearance_index]:0;
      if(texture){
        Native(ctx,base,sub_82733030,cell.image.primitive,texture);
        Rect(base,cell.image,x+4,y+4,w-8,60,0xffffffff,0xffffffff);
      }else Show(cell.image,base,false);
      const auto lines=BootLabel(boot.name);
      PlaceText(base,cell.label,x+4,y+64,w-8,9);
      PlaceText(base,cell.value,x+4,y+73,w-8,9);
      FontScale(base,cell.label,editor.font_size*.43f);
      FontScale(base,cell.value,editor.font_size*.43f);
      Text(ctx,base,cell.label,lines[0]);
      Text(ctx,base,cell.value,lines[1]);
      base[cell.value+53]=!lines[1].empty();
    }else{
      PlaceText(base,cell.label,editor.label_x,y+1,editor.value_x-editor.label_x-22,24);FontScale(base,cell.label,editor.font_size);
      Text(ctx,base,cell.label,RowLabel(items[index].label));
      REX_STORE_U32(cell.label+48,items[index].available?0x171717ffu:0x888880ffu);
      FontScale(base,cell.value,editor.font_size);
      const auto& item=items[index];
      const bool adjusting=selected&&editor.editing&&item.available;
      std::string displayed=item.selected>=0&&item.selected<int(item.choices.size())?item.choices[item.selected]:"--";
      if(displayed.starts_with("Por defecto"))displayed="Por defecto";
      const float valueX=editor.value_x;
      const float maxWidth=x+w-valueX-24;
      float width=TextWidth(ctx,base,displayed);
      if(width>maxWidth){
        do{
          if(displayed.empty())break;
          size_t end=displayed.size()-1;
          while(end&&(static_cast<unsigned char>(displayed[end])&0xc0)==0x80)--end;
          displayed.resize(end);
        }while(!displayed.empty()&&TextWidth(ctx,base,displayed+"...")>maxWidth);
        displayed+="...";width=TextWidth(ctx,base,displayed);
      }
      PlaceText(base,cell.value,valueX,y+1,x+w-valueX-18,24);
      const float right=slider?x+w-10:valueX+width+10;
      const bool canDecrease=item.choices.size()>1;
      const bool canIncrease=item.choices.size()>1;
      // Hide once before the row loop; subsequent rows must not hide the
      // selected field's indicators. Arrows flank the measured text bounds.
      if(adjusting){
        Arrow(ctx,base,scene.arrows[0],true,valueX+editor.arrow_left,y+1,canDecrease);
        Arrow(ctx,base,scene.arrows[1],true,right,y+1,canIncrease);
      }
      REX_STORE_U32(cell.value+48,item.available?0x171717ffu:0x888880ffu);
      if(slider){
        // Match the native camera-angle gauge: silver surround, sixteen
        // separated gold blocks, bright cursor. All are native UI primitives.
        const int maximum=int(item.choices.size())-2;
        const int level=std::clamp(item.slider_value,0,maximum);
        Rect(base,cell.slider_track,valueX,y+4,179,15,0x777775ff,0xf7f7f0ff);
        for(unsigned k=0;k<16;k++){
          const bool lit=int(k)*maximum/16<=level;
          Rect(base,cell.slider_segments[k],valueX+2+k*10.8f,y+6,9.8f,11,
            lit?0xffd756ff:0xc6c6c2ff,lit?0xe8a41cff:0x959590ff);
        }
        Rect(base,cell.slider_cursor,valueX+2+172.f*float(level)/float(maximum),y+3,2,17,0xffffffff,0xffffffff);
        PlaceText(base,cell.value,valueX+185,y+1,65,24);FontScale(base,cell.value,editor.font_size);
        Text(ctx,base,cell.value,std::to_string(level)+(item.selected==0?" D":""));continue;
      }
      Text(ctx,base,cell.value,displayed);
    }
  }
  std::string footer=editor.focus==1?editor.page==Page::FaceDetail?"A: Restaurar cara completa  B: Volver":"A: Restaurar apartado  B: Volver":"A: Seleccionar  B: Volver";
  if(editor.focus==0&&(editor.page==Page::Appearance||editor.page==Page::Accessories||editor.page==Page::Animations))footer="Izq./Der.: Cambiar  B: Volver";
  if(editor.page==Page::Appearance)footer="Izq./Der.: Cambiar  Arr./Ab.: Desplazar  B: Volver";
  if(HeadPage()&&editor.focus==0)footer="Izq./Der.: Ajustar  B: Volver";
  if(editor.page==Page::Face&&editor.focus==0)footer="A: Abrir apartado  B: Volver";
  if(editor.focus==0&&editor.page==Page::FaceDetail&&editor.face_group!=0&&!items.empty()&&!items.front().available)footer="Activa Construir cara en Base y piel  B: Volver";
  if(editor.page==Page::Boots&&!total)footer+="    No hay botas visibles";
  if(editor.page==Page::Boots&&total)footer+="  LT/RT: Página  "+std::to_string(editor.offset+1)+"-"+std::to_string(std::min(total,editor.offset+capacity))+" / "+std::to_string(total);
  if(editor.page==Page::FaceDetail&&total>capacity)footer+="    "+std::to_string(editor.offset+1)+"-"+std::to_string(std::min(total,editor.offset+capacity))+" / "+std::to_string(total);
  if(editor.focus==0&&(AppearancePage()||editor.page==Page::Accessories||editor.page==Page::Animations))
    footer=editor.editing?"Izq./Der.: Cambiar  A: Confirmar  B: Cancelar":"A: Editar  Arr./Ab.: Navegar  B: Volver";
  footer+="  LB/RB: Girar";
  if(editor.page!=Page::Categories&&editor.page!=Page::Face)footer+="  Y: Por defecto";
  if(editor.page==Page::Animations)footer+="  X: Animar/Pausar";
  Footer(ctx,base,footer);
  Pulse(base);
}
// Replace the original height row while keeping its native focus/navigation.
bool Nationality(PPCContext& ctx,uint8_t* base,bool input){
  const auto form=Native(ctx,base,sub_8272F650,editor.original,1);
  if(!Valid(base,form,482))return false;
  for(unsigned i=0;i<32;i++){
    const auto label=VectorItem(base,form,312,i);if(!label)break;
    const auto text=REX_LOAD_U32(label+100);
    if(!Valid(base,text,64))continue;
    const std::string caption(reinterpret_cast<const char*>(base+text),strnlen(reinterpret_cast<const char*>(base+text),63));
    if(_strnicmp(caption.c_str(),"Height",6)==0||_strnicmp(caption.c_str(),"Altura",6)==0||_strnicmp(caption.c_str(),"Estatura",8)==0)Text(ctx,base,label,"Nacionalidad");
  }
  int country=ReadNationality(editor.player);if(country<0)return false;
  const auto row=Native(ctx,base,sub_8272E410,form,1);
  const auto value=VectorItem(base,row,312,0);
  const std::string name=country<int(nationality_names.size())?nationality_names[country]:"Nacionalidad "+std::to_string(country);
  const bool info=editor.page==Page::Information&&!REX_LOAD_U32(editor.owner+196);
  if(value){
    base[value+53]=info;
    if(editor.nationality_drawn_country!=country||editor.nationality_drawn_info!=info){REX_STORE_U32(value+148,editor.font);FontScale(base,value,editor.font_size);}
    Float(base,value+108,245);Float(base,value+208,245);
    REX_STORE_U32(value+176,0);base[value+180]=0;base[value+181]=0;
    if(editor.nationality_drawn_country!=country||editor.nationality_drawn_info!=info){editor.nationality_drawn_country=country;editor.nationality_drawn_info=info;Text(ctx,base,value,name);}
  }
  const int selected=int16_t(REX_LOAD_U16(form+478))*int16_t(REX_LOAD_U16(form+476))+int16_t(REX_LOAD_U16(form+480));
  const bool adjusting=info&&REX_LOAD_U32(editor.owner+192)==1&&selected==1&&editor.editing;
  // +16/+20 are resolved native UI positions (also used by 8273B918).
  // +32/+36 alone are animation offsets, not the authored row location.
  float valueX=value?ReadFloat(base,value+16)-ReadFloat(base,editor.original+16):-101.f;
  float valueY=value?ReadFloat(base,value+20)-ReadFloat(base,editor.original+20):-117.f;
  if(valueX<-180||valueX>-60)valueX=-101.f;
  if(valueY<-140||valueY>-100)valueY=-117.f;
  const float width=TextWidth(ctx,base,name);
  for(unsigned direction=0;direction<2;direction++){
    const auto nativeArrow=VectorItem(base,row,376,direction);if(nativeArrow)base[nativeArrow+53]=0;
    Arrow(ctx,base,editor.nationality_arrows[direction],adjusting,
      direction?valueX+width+10:valueX-15,valueY,
      nationality_names.size()>1);
  }
  if(!input||REX_LOAD_U32(editor.owner+192)!=1||selected!=1)return false;
  Footer(ctx,base,editor.editing?"Izq./Der.: Nacionalidad  A: Confirmar  B: Cancelar  Y: Por defecto":"A: Editar nacionalidad  Arr./Ab.: Navegar  B: Volver  Y: Por defecto");
  const bool reset=Input(ctx,base,15);
  if(!editor.editing&&!reset){
    if(Input(ctx,base,8)&&BeginFieldEdit(editor.player)){editor.editing=true;Sound(ctx,base,0);return true;}
    return false;
  }
  if(editor.editing&&Input(ctx,base,9)){
    if(CancelFieldEdit(editor.player)){
      editor.editing=false;preview::Changed(ctx,base);RefreshNativeCatalog(base);Sound(ctx,base,1);
      const auto flag=VectorItem(base,editor.original,280,0);
      if(flag)Native(ctx,base,sub_82733B70,flag,0,unsigned(ReadNationality(editor.player)));
    }
    return true;
  }
  if(editor.editing&&Input(ctx,base,8)){EndFieldEdit();editor.editing=false;Sound(ctx,base,0);return true;}
  const int direction=Input(ctx,base,2,true)?-1:(Input(ctx,base,3,true)?1:0);
  if(!reset&&!direction)return true;
  const int next=reset?0:(country+direction+int(nationality_names.size()))%int(nationality_names.size())+1;
  if(Apply({Action::Information,editor.player,ReadCatalog()->generation,"nationality",next})){
    Sound(ctx,base,6);
    preview::Changed(ctx,base);
    RefreshNativeCatalog(base);
    country=ReadNationality(editor.player);
    if(value)Text(ctx,base,value,nationality_names[std::clamp(country,0,int(nationality_names.size())-1)]);
    const auto flag=VectorItem(base,editor.original,280,0);
    if(flag)Native(ctx,base,sub_82733B70,flag,0,unsigned(country));
  }
  return true;
}
void InformationSelectors(PPCContext& ctx,uint8_t* base){
  Nationality(ctx,base,false);
  const auto form=Native(ctx,base,sub_8272F650,editor.original,1);
  if(!Valid(base,form,482))return;
  const int selected=int16_t(REX_LOAD_U16(form+478))*int16_t(REX_LOAD_U16(form+476))+int16_t(REX_LOAD_U16(form+480));
  for(unsigned index=1;index<=5;index++){
    const auto row=Native(ctx,base,sub_8272E410,form,index);
    for(unsigned direction=0;direction<2;direction++){
      const auto arrow=VectorItem(base,row,376,direction);if(arrow)base[arrow+53]=0;
    }
  }
  if(REX_LOAD_U32(editor.owner+196))return;
  if(selected==1){
    Footer(ctx,base,editor.editing?"Izq./Der.: Cambiar  A: Confirmar  B: Cancelar  Y: Por defecto":"A: Editar  B: Volver  Y: Por defecto  Arr./Ab.: Navegar");
    return;
  }
  if(selected<2||selected>5||!base[editor.owner+224])return;
  const auto row=Native(ctx,base,sub_8272E410,form,unsigned(selected));
  const auto value=VectorItem(base,row,312,0);if(!Valid(base,value,184))return;
  const auto string=REX_LOAD_U32(value+100);if(!Valid(base,string,1))return;
  std::string text;
  for(unsigned i=0;i<511&&Valid(base,string+i,1)&&base[string+i];i++)text+=char(base[string+i]);
  const float x=ReadFloat(base,value+16)-ReadFloat(base,editor.original+16);
  const float y=ReadFloat(base,value+20)-ReadFloat(base,editor.original+20);
  Arrow(ctx,base,editor.nationality_arrows[0],true,x-15,y,true);
  Arrow(ctx,base,editor.nationality_arrows[1],true,x+TextWidth(ctx,base,text)+10,y,true);
  Footer(ctx,base,"Izq./Der.: Cambiar  A: Confirmar  B: Cancelar");
}
void Information(PPCContext& ctx,uint8_t* base){
  editor.page=Page::Information;editor.nationality_drawn_country=-1;Visibility(base);
  const auto informationForm=Native(ctx,base,sub_8272F650,editor.original,1);
  const auto heading=VectorItem(base,informationForm,312,6);
  if(heading)Text(ctx,base,heading,ReadPlayer(editor.player).player_name);
  REX_STORE_U32(editor.owner+192,1);
  Native(ctx,base,sub_8272F6C8,editor.original,1);
  Native(ctx,base,sub_822E59C0,editor.owner,1,1);
  Nationality(ctx,base,false);
}
void Change(PPCContext& ctx,uint8_t* base,int direction){
  const auto s=ReadPlayer(editor.player);const auto items=Items(s);
  if(editor.selected>=items.size())return;
  const auto& item=items[editor.selected];if(!item.available||item.choices.empty())return;
  const int count=int(item.choices.size()),value=item.slider_value>=0?preview::SliderStep(item.selected,item.slider_value,direction,count-2):(item.selected+direction+count)%count;
  if(value==item.selected)return;
  if(Apply({AppearancePage()?Action::Appearance:editor.page==Page::Animations?Action::Animation:Action::Accessory,editor.player,s.generation,item.id,value})){Sound(ctx,base,6);preview::Changed(ctx,base,item.id=="hair"||item.id.starts_with("hair_")||item.id=="face"||item.id.starts_with("face_"));if(editor.page==Page::Animations)preview::Rest();}
  Refresh(ctx,base);
}
void Destroy(PPCContext& ctx,uint8_t* base){
  EndFieldEdit();editor.active=false;
  preview::Leave(ctx,base);
  if(editor.original_geometry&&Valid(base,editor.original,144)){REX_STORE_U32(editor.original+140,editor.original_geometry);base[editor.original+52]|=7;}
  for(auto& scene:editor.scenes)if(scene.root&&Valid(base,scene.root,240)){
    // Native release unlinks the root and defers disposal until traversal ends.
    base[scene.root+53]=0;Native(ctx,base,sub_82735EC8,scene.root);scene.root=0;
  }
  for(auto arrow:editor.nationality_arrows)if(arrow)Native(ctx,base,sub_82735EC8,arrow);
  if(editor.measure)Native(ctx,base,sub_8280E7B8,editor.measure);
  if(editor.text)Native(ctx,base,sub_8280E7B8,editor.text);
  editor=Editor{};
}
} // namespace

// Keep native menu changes scoped to the editor; other game selectors retain
// their original behavior. Information choices and numeric abilities wrap.
extern "C" bool pes6_native_editor_circular(){
  return editor.active&&editor.page==Page::Information;
}
extern "C" bool pes6_native_editor_direct_subpage_exit(PPCContext& ctx,uint8_t* base){
  if(!editor.active||ctx.r31.u32!=editor.owner||!editor.direct_info_subpage)return false;
  Native(ctx,base,sub_822E6608,editor.owner);
  editor.direct_info_subpage=0;
  Categories(ctx,base);
  Refresh(ctx,base);
  return true;
}
extern "C" void pes6_native_editor_after_update(PPCContext& ctx,uint8_t* base,uint32_t owner){
  if(editor.active&&owner==editor.owner){
    const auto root=(editor.page==Page::Information&&(editor.direct_info_subpage||REX_LOAD_U32(editor.owner+196)||base[editor.owner+225]))?editor.original:editor.scenes[unsigned(editor.page)].root;
    // A page/field change keeps the same player image: no entrance replay.
    pes6_edit_effects(ctx,base,root,0x50000000u|unsigned(editor.player));
  }
  if(!editor.active||owner!=editor.owner||editor.page!=Page::Information)return;
  InformationSelectors(ctx,base);
  if(editor.direct_info_subpage){
    if(REX_LOAD_U32(editor.owner+164)!=5){
      Native(ctx,base,sub_822E6608,editor.owner);
      editor.direct_info_subpage=0;
      Categories(ctx,base);Refresh(ctx,base);return;
    }
    base[editor.original+53]=0;
    base[editor.scenes[0].root+53]=0;
    preview::Visible(false);
    return;
  }
  if(REX_LOAD_U32(editor.owner+196)||base[editor.owner+225]){
    base[editor.original+53]=0;
    base[editor.scenes[0].root+53]=0;
    preview::Visible(false);
    return;
  }
  // Render compact Information overlay on scenes[0] while hiding native original frame
  base[editor.original+53]=0;
  auto& s0=editor.scenes[0];
  base[s0.root+53]=1;
  Rect(base,s0.cat_top_bar,-332,-163,372,28,0x383c44f0u,0x22262cf0u);
  Rect(base,s0.panel,-332,-135,372,240,0xe9e9e7ff,0xd9d9d6ff);
  if(s0.heading){
    base[s0.heading+53]=1;
    PlaceText(base,s0.heading,-322,-160,350,24);
    FontScale(base,s0.heading,editor.font_size*.90f);
    REX_STORE_U32(s0.heading+48,0xffffffffu);
    Text(ctx,base,s0.heading,ReadPlayer(editor.player).player_name);
  }
  const auto form=Native(ctx,base,sub_8272F650,editor.original,1);
  const int sel=Valid(base,form,482)?int16_t(REX_LOAD_U16(form+478))*int16_t(REX_LOAD_U16(form+476))+int16_t(REX_LOAD_U16(form+480)):-1;
  const bool on_ok=REX_LOAD_U32(editor.owner+192)==0;
  static constexpr std::array<const char*,6> kInfoLabels{{"Nombre jugador","Nacionalidad","Edad","Pie dominante","Resistencia lesiones","Tipo de progreso"}};
  for(unsigned i=0;i<12;i++){
    auto& cell=s0.cells[i];
    const bool vis=i<6;
    Show(cell.border,base,vis);Show(cell.fill,base,vis);Show(cell.image,base,false);
    base[cell.label+53]=vis;base[cell.value+53]=vis;
    if(!vis)continue;
    const float rowY=-128+float(i)*26.f;
    const bool is_sel=(REX_LOAD_U32(editor.owner+192)==1)&&sel==int(i);
    const uint32_t bg=is_sel?0xffc62bffu:(i%2?0xefefedffu:0xd9d9d6ffu);
    Rect(base,cell.border,-327,rowY-1,362,24,is_sel?0xffffffffu:0xe9e9e7ffu,is_sel?0xffffffffu:0xe9e9e7ffu);
    Rect(base,cell.fill,-326,rowY,360,22,bg,bg);
    PlaceText(base,cell.label,-318,rowY+1,170,20);
    FontScale(base,cell.label,editor.font_size*.85f);
    REX_STORE_U32(cell.label+48,0x181818ffu);
    Text(ctx,base,cell.label,kInfoLabels[i]);
    std::string val="-";
    if(i==1){
      const int country=ReadNationality(editor.player);
      if(country>=0&&country<int(nationality_names.size()))val=nationality_names[country];
    }else if(Valid(base,form,482)){
      const auto row=Native(ctx,base,sub_8272E410,form,i);
      const auto vnode=VectorItem(base,row,312,0);
      const auto str=Valid(base,vnode,104)?REX_LOAD_U32(vnode+100):0;
      if(Valid(base,str,1))val=std::string(reinterpret_cast<const char*>(base+str),strnlen(reinterpret_cast<const char*>(base+str),63));
    }
    PlaceText(base,cell.value,-140,rowY+1,168,20);
    FontScale(base,cell.value,editor.font_size*.85f);
    REX_STORE_U32(cell.value+48,0x181818ffu);
    Text(ctx,base,cell.value,val);
  }
  for(auto arrow:editor.nationality_arrows)Arrow(ctx,base,arrow,false,0,0,true);
  const bool show_arrows=(REX_LOAD_U32(editor.owner+192)==1)&&((sel==1&&editor.editing)||(sel>=2&&sel<=5&&(REX_LOAD_U32(editor.owner+164)==4||base[editor.owner+224])));
  if(show_arrows){
    if(sel>=2)Footer(ctx,base,"Izq./Der.: Cambiar  A: Confirmar  B: Cancelar");
    std::string val;
    if(sel==1){
      const int country=ReadNationality(editor.player);
      if(country>=0&&country<int(nationality_names.size()))val=nationality_names[country];
    }else if(Valid(base,form,482)){
      const auto row=Native(ctx,base,sub_8272E410,form,unsigned(sel));
      const auto vnode=VectorItem(base,row,312,0);
      const auto str=Valid(base,vnode,104)?REX_LOAD_U32(vnode+100):0;
      if(Valid(base,str,1))val=std::string(reinterpret_cast<const char*>(base+str),strnlen(reinterpret_cast<const char*>(base+str),63));
    }
    const float ay=-128+float(sel)*26.f+1.f;
    const float vw=TextWidth(ctx,base,val)*0.85f;
    Arrow(ctx,base,s0.arrows[0],true,-155.f,ay,true);
    Arrow(ctx,base,s0.arrows[1],true,-140.f+vw+8.f,ay,true);
  }else{
    for(auto arrow:s0.arrows)Arrow(ctx,base,arrow,false,0,0,true);
  }
  const bool on_def=REX_LOAD_U32(editor.owner+192)==2;
  const uint32_t ok_col=on_ok?0xffc62bffu:0xc2c2c0ffu;
  const uint32_t def_col=on_def?0xffc62bffu:0xc2c2c0ffu;
  Rect(base,s0.info_def_fill,-316,66,160,24,def_col,def_col);
  Show(s0.info_def_fill,base,true);
  if(s0.info_def_label){
    base[s0.info_def_label+53]=1;
    PlaceText(base,s0.info_def_label,-308,68,144,20);
    FontScale(base,s0.info_def_label,editor.font_size*.82f);
    REX_STORE_U32(s0.info_def_label+48,0x181818ffu);
    Text(ctx,base,s0.info_def_label,"\x11kh Por defecto");
  }
  Rect(base,s0.cat_ok_fill,-120,66,120,24,ok_col,ok_col);
  Show(s0.cat_ok_fill,base,true);
  if(s0.cat_ok_label){
    base[s0.cat_ok_label+53]=1;
    const float ok_w=TextWidth(ctx,base,"OK")*0.90f;
    PlaceText(base,s0.cat_ok_label,-60.f-ok_w*0.5f,68,120,20);
    FontScale(base,s0.cat_ok_label,editor.font_size*.90f);
    REX_STORE_U32(s0.cat_ok_label+48,0x181818ffu);
    Text(ctx,base,s0.cat_ok_label,"OK");
  }
}
extern "C" void pes6_native_editor_leave(PPCContext& ctx,uint8_t* base){
  if(editor.owner==ctx.r3.u32){pes6_edit_effects_close(ctx,base);Destroy(ctx,base);}
}
extern "C" void pes6_native_editor_enter(PPCContext& ctx,uint8_t* base,uint32_t owner){
  if(editor.owner)Destroy(ctx,base);
  const auto original=REX_LOAD_U32(owner+172);if(!Valid(base,original,448))return;
  const auto form=Native(ctx,base,sub_8272F650,original,1);
  if(!Valid(base,form,486))return;
  const auto row=Native(ctx,base,sub_8272E410,form,0);
  if(!Valid(base,row,448))return;
  const auto texts=REX_LOAD_U32(row+312);if(!Valid(base,texts,4))return;
  const auto ageRow=Native(ctx,base,sub_8272E410,form,2);
  const auto sample=VectorItem(base,ageRow,312,0);if(!Valid(base,sample,272))return;
  RefreshNativeCatalog(base);
  const auto id=uint16_t(REX_LOAD_U32(0x831398A4u));if(!ReadPlayer(id).appearance_available)return;
  editor.owner=owner;editor.original=original;editor.player=id;
  editor.text=Allocate(ctx,base,1024);
  if(!editor.text){Destroy(ctx,base);return;}
  editor.font_size=ReadFloat(base,sample+152);
  editor.font=REX_LOAD_U32(sample+148);
  editor.measure=Allocate(ctx,base,128);
  if(!editor.measure){Destroy(ctx,base);return;}
  for(unsigned direction=0;direction<2;direction++){
    editor.nationality_arrows[direction]=NewArrow(ctx,base,original,VectorItem(base,ageRow,376,direction));
    if(!editor.nationality_arrows[direction]){Destroy(ctx,base);return;}
  }
  for(unsigned section=0;section<editor.scenes.size();section++){
    auto& scene=editor.scenes[section];
    // A separate, fully constructed native edit_basic scene supplies the real
    // frame, silhouette, OK and Default controls. Its information form is hidden.
    scene.root=Native(ctx,base,sub_8272FA80,REX_LOAD_U32(original+128),0,0,0xffffffff);
    if(!scene.root){Destroy(ctx,base);return;}
    base[scene.root+53]=0;
    for(unsigned pageIndex:{0u,2u}){
      const auto page=Native(ctx,base,sub_8272F650,scene.root,pageIndex);
      Native(ctx,base,sub_8272F3A0,page,1,1);
    }
    const auto content=Native(ctx,base,sub_8272F650,scene.root,1);
    base[content+53]=0;
    StripNationality(ctx,base,scene.root);
    const auto nationality=VectorItem(base,scene.root,312,0);if(nationality)base[nationality+53]=0;
    const auto emblem=VectorItem(base,scene.root,280,0);if(emblem)base[emblem+53]=0;
    scene.panel=NewQuad(ctx,base,scene.root);
    if(section==0||section==unsigned(Page::Appearance)||section==unsigned(Page::Boots)||section==unsigned(Page::Accessories)||section==unsigned(Page::Animations)||section==unsigned(Page::Face)||section==unsigned(Page::FaceDetail)||section==unsigned(Page::Hair)){
      static uint32_t empty_cat_geo=0;
      if(!empty_cat_geo){
        empty_cat_geo=Allocate(ctx,base,64);
        const auto src=REX_LOAD_U32(scene.root+140);
        if(empty_cat_geo&&Valid(base,src,24)){std::memcpy(base+empty_cat_geo,base+src,24);REX_STORE_U16(empty_cat_geo,0);}
      }
      if(empty_cat_geo){REX_STORE_U32(scene.root+140,empty_cat_geo);base[scene.root+52]|=0x23;}
      scene.cat_top_bar=NewQuad(ctx,base,scene.root);
      scene.cat_ok_fill=NewQuad(ctx,base,scene.root);
      scene.cat_ok_label=Label(ctx,base,scene.root,sample);
      scene.info_def_fill=NewQuad(ctx,base,scene.root);
      scene.info_def_label=Label(ctx,base,scene.root,sample);
    }else{
      Rect(base,scene.panel,-332,-157,503,287,0xe9e9e7ff,0xe9e9e7ff);
    }
    scene.scroll_track=NewQuad(ctx,base,scene.root);scene.scroll_thumb=NewQuad(ctx,base,scene.root);
    for(unsigned i=0;i<12;i++){
      auto& cell=scene.cells[i];
      cell.border=NewQuad(ctx,base,scene.root);cell.fill=NewQuad(ctx,base,scene.root);
      cell.image=NewQuad(ctx,base,scene.root,true);
      cell.label=Label(ctx,base,scene.root,sample);cell.value=Label(ctx,base,scene.root,sample);
      if(!cell.border.primitive||!cell.fill.primitive||!cell.image.primitive||!cell.label||!cell.value){Destroy(ctx,base);return;}
      if(section==unsigned(Page::Hair)){
        cell.slider_track=NewQuad(ctx,base,scene.root);cell.slider_cursor=NewQuad(ctx,base,scene.root);
        if(!cell.slider_track.primitive||!cell.slider_cursor.primitive){Destroy(ctx,base);return;}
        for(auto& segment:cell.slider_segments){segment=NewQuad(ctx,base,scene.root);if(!segment.primitive){Destroy(ctx,base);return;}}
      }
    }
    scene.divider=NewQuad(ctx,base,scene.root);
    for(unsigned direction=0;direction<2;direction++){
      scene.arrows[direction]=NewArrow(ctx,base,scene.root,VectorItem(base,ageRow,376,direction));
      if(!scene.arrows[direction]){Destroy(ctx,base);return;}
    }
    scene.title=Label(ctx,base,scene.root,sample);if(!scene.title){Destroy(ctx,base);return;}
    PlaceText(base,scene.title,-321,-153,483,22);
    FontScale(base,scene.title,editor.font_size);
    scene.heading=Label(ctx,base,scene.root,sample);
    if(!scene.heading){Destroy(ctx,base);return;}
    PlaceText(base,scene.heading,-65,-199,400,25);
    FontScale(base,scene.heading,editor.font_size*.90f);
    REX_STORE_U32(scene.heading+48,0xffffffffu);
    scene.preview_status=Label(ctx,base,scene.root,sample);
    if(!scene.preview_status){Destroy(ctx,base);return;}
    PlaceText(base,scene.preview_status,180,122,155,18);
    FontScale(base,scene.preview_status,editor.font_size*.50f);
  }
  PrepareInformationGeometry(ctx,base);
  LoadBoots(ctx,base);
  editor.active=true;preview::Enter(ctx,base,id);Refresh(ctx,base);
}
extern "C" bool pes6_native_editor_update(PPCContext& ctx,uint8_t* base){
  preview::Tick(ctx,base);
  if(!editor.active||ctx.r3.u32!=editor.owner||REX_LOAD_U32(editor.owner+172)!=editor.original)return false;
  if(frame_geometry_template){
    const auto table=REX_LOAD_U32(frame_geometry_template+12);
    const auto silhouette=REX_LOAD_U32(table+18*4);
    const bool visible=!preview::Ready();
    if(base[silhouette]!=visible){base[silhouette]=visible;for(auto& scene:editor.scenes)base[scene.root+52]|=7;}
  }
  if(editor.original_geometry&&information_geometry_template){
    const auto table=REX_LOAD_U32(information_geometry_template+12),shape=REX_LOAD_U32(table+18*4);
    const bool visible=!preview::Ready();
    if(base[shape]!=visible){base[shape]=visible;base[editor.original+52]|=7;}
  }
  for(auto& scene:editor.scenes){
    const bool visible=false;
    if(base[scene.preview_status+53]!=visible){base[scene.preview_status+53]=visible;if(visible)Text(ctx,base,scene.preview_status,preview::Status());}
  }
  const bool rotateLeft=Held(ctx,base,10),rotateRight=Held(ctx,base,11);
  const bool pose=editor.page==Page::Animations&&editor.focus==0&&Input(ctx,base,14);
  if(rotateLeft)preview::Rotate(-1);
  if(rotateRight)preview::Rotate(1);
  if(pose){const auto selection=ReadPlayer(editor.player);if(editor.selected<selection.animations.size())preview::Animation(ctx,base,selection.animations[editor.selected].id);}
  if(editor.page==Page::Information&&(rotateLeft||rotateRight||pose))return true;
  if(editor.page==Page::Information){
    if(editor.direct_info_subpage){
      if(REX_LOAD_U32(editor.owner+164)!=5){
        Native(ctx,base,sub_822E6608,editor.owner);
        editor.direct_info_subpage=0;
        Categories(ctx,base);Refresh(ctx,base);return true;
      }
      preview::Visible(false);
      return false;
    }
    preview::Container(editor.scenes[0].root);preview::Frame(6);
    const auto form=Native(ctx,base,sub_8272F650,editor.original,1);
    const int selected=Valid(base,form,482)?int16_t(REX_LOAD_U16(form+478))*int16_t(REX_LOAD_U16(form+476))+int16_t(REX_LOAD_U16(form+480)):-1;
    // Text entry keeps the preview behind the bottom keyboard.
    const bool textEntry=pes6_keyboard_active();
    if(textEntry)Visibility(base);
    preview::Visible(textEntry||(!REX_LOAD_U32(editor.owner+196)&&!base[editor.owner+225]));
    if(REX_LOAD_U32(editor.owner+196)==5){
      Apply({Action::Information,editor.player,ReadCatalog()->generation,"nationality",0});
      RefreshNativeCatalog(base);
      preview::Changed(ctx,base);
    }
    if(REX_LOAD_U32(editor.owner+196)||base[editor.owner+225]){Nationality(ctx,base,false);return false;}
    if(Nationality(ctx,base,true))return true;
    if(REX_LOAD_U32(editor.owner+164)!=4&&!base[editor.owner+224]&&!editor.editing){
      const auto focus_area=REX_LOAD_U32(editor.owner+192);
      if(focus_area==1){
        if(selected==5&&Input(ctx,base,1,true)){REX_STORE_U32(editor.owner+192,2);Sound(ctx,base,5);return true;}
        if(selected==0&&Input(ctx,base,0,true)){REX_STORE_U32(editor.owner+192,2);Sound(ctx,base,5);return true;}
      }else if(focus_area==2){
        if(Input(ctx,base,3,true)){REX_STORE_U32(editor.owner+192,0);Sound(ctx,base,5);return true;}
        if(Input(ctx,base,0,true)){REX_STORE_U32(editor.owner+192,1);if(Valid(base,form,482)){REX_STORE_U16(form+478,5);REX_STORE_U16(form+480,0);}Sound(ctx,base,5);return true;}
        if(Input(ctx,base,1,true)){REX_STORE_U32(editor.owner+192,1);if(Valid(base,form,482)){REX_STORE_U16(form+478,0);REX_STORE_U16(form+480,0);}Sound(ctx,base,5);return true;}
        if(Input(ctx,base,8))return false;
      }else if(focus_area==0){
        if(Input(ctx,base,2,true)){REX_STORE_U32(editor.owner+192,2);Sound(ctx,base,5);return true;}
        if(Input(ctx,base,0,true)){REX_STORE_U32(editor.owner+192,1);if(Valid(base,form,482)){REX_STORE_U16(form+478,5);REX_STORE_U16(form+480,0);}Sound(ctx,base,5);return true;}
        if(Input(ctx,base,1,true)){REX_STORE_U32(editor.owner+192,1);if(Valid(base,form,482)){REX_STORE_U16(form+478,0);REX_STORE_U16(form+480,0);}Sound(ctx,base,5);return true;}
      }
      if(Input(ctx,base,15)){
        Apply({Action::Information,editor.player,ReadCatalog()->generation,"nationality",0});
        RefreshNativeCatalog(base);
        preview::Changed(ctx,base);
        return false;
      }
    }
    const bool back=Input(ctx,base,9)&&REX_LOAD_U32(editor.owner+164)!=4&&!base[editor.owner+224]&&!editor.editing;
    const bool confirm=REX_LOAD_U32(editor.owner+192)==0&&Input(ctx,base,8);
    if(!back&&!confirm)return false;
    Native(ctx,base,sub_822E6608,editor.owner);
    Categories(ctx,base);
    Refresh(ctx,base);return true;
  }
  Visibility(base);preview::Visible(true);
  for(auto arrow:editor.nationality_arrows)Arrow(ctx,base,arrow,false,0,0,true);
  if(Input(ctx,base,9)){
    if(editor.editing){
      if(CancelFieldEdit(editor.player)){editor.editing=false;preview::Changed(ctx,base,true);RefreshNativeCatalog(base);Sound(ctx,base,1);}
      Refresh(ctx,base);return true;
    }
    Sound(ctx,base,1);
    if(editor.page==Page::Categories){
      // Keep scenes[0].root and preview visible until pes6_native_editor_leave destroys
      // this editor on the next tick, avoiding a 1-frame blank gap returning to roster.
      Native(ctx,base,sub_827290F8,editor.owner,8,editor.owner);return true;
    }
    if(editor.page==Page::FaceDetail){editor.page=Page::Face;editor.selected=editor.face_group;editor.offset=0;editor.focus=0;}
    else Categories(ctx,base);
    Refresh(ctx,base);return true;
  }
  // Reuse Refresh()'s item count on idle frames instead of rebuilding the
  // entire 4200-player Snapshot every tick. ReadPlayer runs only on actions.
  const unsigned total=editor.page==Page::Categories?10u:editor.total,capacity=Capacity();
  const auto bootSelection=[](const Snapshot& player){
    const auto it=std::find_if(player.boots.begin(),player.boots.end(),[&](const auto& boot){
      return boot.appearance_index==player.boot_index;
    });
    return it==player.boots.end()?0u:unsigned(it-player.boots.begin());
  };
  if(editor.focus==0&&editor.page!=Page::Categories&&editor.page!=Page::Face&&Input(ctx,base,15)){
    const auto s=ReadPlayer(editor.player);const auto items=Items(s);std::string field;
    const auto action=editor.page==Page::Boots?Action::Boot:editor.page==Page::Animations?Action::Animation:
      AppearancePage()?Action::Appearance:Action::Accessory;
    if(editor.page!=Page::Boots){if(editor.selected>=items.size())return true;field=items[editor.selected].id;}
    if(ResetField({action,editor.player,s.generation,field,0})){
      Sound(ctx,base,0);
      preview::Changed(ctx,base,HeadPage()||editor.page==Page::Boots);
      if(editor.page==Page::Animations)preview::Rest();
      RefreshNativeCatalog(base);
      if(editor.page==Page::Boots)editor.selected=bootSelection(ReadPlayer(editor.player));
    }
    Refresh(ctx,base);return true;
  }
  if(editor.editing){
    const int step=editor.page==Page::Hair&&!Input(ctx,base,2,false)&&!Input(ctx,base,3,false)?5:1;
    if(Input(ctx,base,2,true))Change(ctx,base,-step);
    else if(Input(ctx,base,3,true))Change(ctx,base,step);
    else if(Input(ctx,base,8)){EndFieldEdit();editor.editing=false;Sound(ctx,base,0);Refresh(ctx,base);}
    return true;
  }
  bool changed=false;
  const auto move=[&](int direction){editor.selected=unsigned((int(editor.selected)+int(total)+direction)%int(total));changed=true;};
  const bool compact_list=editor.page==Page::Appearance||editor.page==Page::Boots||editor.page==Page::Accessories||editor.page==Page::Animations||HeadPage();
  if(total&&Input(ctx,base,0,true)){
    const unsigned step=editor.page==Page::Boots?bootColumns:1;
    if(editor.page==Page::Categories){
      if(editor.focus==-1){editor.focus=0;editor.selected=total-1;}
      else if(editor.selected==0)editor.focus=-1;
      else editor.selected--;
    }else if(compact_list){
      if(editor.focus!=0){editor.focus=0;editor.selected=total-1;}
      else if(editor.selected<step)editor.focus=1;
      else editor.selected-=step;
    }else{
      if(editor.focus==-1)editor.focus=1;
      else if(editor.focus==1)editor.focus=0;
      else if(editor.selected<step)editor.focus=-1;
      else editor.selected-=step;
    }
    changed=true;
  }
  if(total&&Input(ctx,base,1,true)){
    const unsigned step=editor.page==Page::Boots?bootColumns:1;
    if(editor.page==Page::Categories){
      if(editor.focus==-1){editor.focus=0;editor.selected=0;}
      else if(editor.selected+1>=total)editor.focus=-1;
      else editor.selected++;
    }else if(compact_list){
      if(editor.focus!=0){editor.focus=0;editor.selected=0;}
      else if(editor.selected+step>=total)editor.focus=1;
      else editor.selected+=step;
    }else{
      if(editor.focus==1)editor.focus=-1;
      else if(editor.focus==-1)editor.focus=0;
      else if(editor.selected+step>=total)editor.focus=1;
      else editor.selected+=step;
    }
    changed=true;
  }
  if(compact_list){
    if(editor.focus==1&&Input(ctx,base,3,true)){editor.focus=-1;changed=true;}
    else if(editor.focus==-1&&Input(ctx,base,2,true)){editor.focus=1;changed=true;}
  }
  // 4/5 are left-stick directions, already included by native direction input.
  // Native shoulder/trigger actions are 10..13; accept either native preset.
  if(editor.page==Page::Boots){
    if(total&&Input(ctx,base,12)){
      editor.selected=editor.selected>=capacity?editor.selected-capacity:0;editor.focus=0;changed=true;
    }
    if(total&&Input(ctx,base,13)){
      editor.selected=std::min(total-1,editor.selected+capacity);editor.focus=0;changed=true;
    }
  }
  if(total&&editor.focus==0&&editor.page==Page::Boots){
    if(Input(ctx,base,2,true))move(-1);
    if(Input(ctx,base,3,true))move(1);
  }
  if(changed){Sound(ctx,base,5);Refresh(ctx,base);}
  Pulse(base);
  if(!Input(ctx,base,8))return true;
  Sound(ctx,base,0);
  if(editor.focus==1){if(ResetPlayer(editor.player,editor.page==Page::Hair?ResetSection::Hair:HeadPage()?ResetSection::Face:editor.page==Page::Appearance?ResetSection::Appearance:editor.page==Page::Boots?ResetSection::Boots:editor.page==Page::Accessories?ResetSection::Accessories:editor.page==Page::Animations?ResetSection::Animations:ResetSection::All)){preview::Changed(ctx,base,HeadPage()||editor.page==Page::Categories);RefreshNativeCatalog(base);}Refresh(ctx,base);return true;}
  if(editor.focus==-1){
    if(editor.page==Page::Categories){
      Native(ctx,base,sub_827290F8,editor.owner,8,editor.owner);return true;
    }
    if(editor.page==Page::FaceDetail){editor.page=Page::Face;editor.selected=editor.face_group;editor.offset=0;editor.focus=0;}
    else Categories(ctx,base);
    Refresh(ctx,base);return true;
  }
  if(editor.page==Page::Categories){
    editor.category_selected=editor.selected;
    if(editor.selected==0){editor.direct_info_subpage=0;Information(ctx,base);return true;}
    if(editor.selected>=1&&editor.selected<=3){
      editor.direct_info_subpage=1;
      Information(ctx,base);
      base[editor.original+53]=0;
      const auto form=Native(ctx,base,sub_8272F650,editor.original,1);
      if(Valid(base,form,482)){
        REX_STORE_U16(form+478,0);
        REX_STORE_U16(form+480,uint16_t(editor.selected+5));
      }
      preview::Visible(false);
      return false;
    }
    editor.page=static_cast<Page>(editor.selected-3);
    editor.selected=editor.page==Page::Boots?bootSelection(ReadPlayer(editor.player)):0;editor.offset=0;
    Refresh(ctx,base);return true;
  }
  if(editor.page==Page::Boots){
    const auto s=ReadPlayer(editor.player);if(editor.selected>=s.boots.size())return true;
    const auto& boot=s.boots[editor.selected];if(Apply({Action::Boot,editor.player,s.generation,{},boot.appearance_index}))preview::Changed(ctx,base);Refresh(ctx,base);
  }else if(editor.page==Page::Face){
    editor.face_group=editor.selected;editor.page=Page::FaceDetail;
    editor.selected=0;editor.offset=0;editor.focus=0;Refresh(ctx,base);
  }else {ActivateField(ctx,base);Refresh(ctx,base);}
  return true;
}

#include "native_team_editor.inl"
