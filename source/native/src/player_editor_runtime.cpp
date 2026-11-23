#include "../generated/default/pes6_pch.h"
#include "../generated/default/pes6_funcs.h"
#include "player_editor_runtime.h"
#include "licensed_southamerica_names.h"
#include "setpiece_gameplay.h"
#include "boot_catalog.h"
#include "player_hair_fields.h"
#include "editor_field_reset.h"
#include "hair_geometry_fit.h"
#include "kit_crest_runtime.h"
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <array>
#include <tuple>
#include <optional>
#include <unordered_map>

namespace pes6::editor {
std::atomic<bool> open{false};
namespace {
struct Changes {
  std::string name;
  int face_source=-1,hair_source=-1;
  int nationality=-1;
  int face_mode=-1; // -1 preserves the selected original/preset, 0 builds a face.
  std::array<int,face_fields.size()> face=[](){std::array<int,face_fields.size()> values;values.fill(face_unset);return values;}();
  int hair_model=-1;
  std::array<int,3> hair_color{-1,-1,-1};
  std::array<int,3> hair_fit{0,0,0};
  int boot=-1, sleeves=-1, shirt=0, wrist_left=-1, wrist_right=-1,
      bracelet_left=-1, bracelet_right=-1, undershorts=-1, ankle_tape=-1, necklace=-1;
  std::array<int,6> animations{-1,-1,-1,-1,-1,-1};
  std::array<int,15> body=[](){std::array<int,15> values;values.fill(body_unset);return values;}();
};
constexpr std::array<const char*,6> animation_fields{"free_kick","penalty","dribble","drop_kick","celebration1","celebration2"};
constexpr std::array<const char*,6> animation_labels{"Tiro libre","Penalti","Carrera / regate","Saque de volea (portero)","Celebración 1","Celebración 2"};
constexpr std::array<const char*,11> free_kick_names{"Colocado","Potente","R. Carlos","Recoba","Beckham","Vuelta","Henry","Lampard","R10","Pirlo","CR7"};
constexpr std::array<const char*,17> penalty_names{"Repiqueteo","Pie al lado","Paradinha","Salto inicial","R10","CR7","CR7 v2","Colocado","Potente","R. Carlos","Recoba","Beckham","Vuelta","Henry","Lampard","R10 v2","Pirlo"};
constexpr std::array<int,6> animation_max{10,16,3,3,79,79};
std::mutex mutex;
std::map<uint16_t,Changes> changes;
struct FieldEditBackup{uint16_t id;bool existed;Changes value;};
std::optional<FieldEditBackup> field_edit_backup;
std::shared_ptr<Catalog> catalog=std::make_shared<Catalog>();
std::filesystem::path store_path;
std::string status;
std::atomic<uint64_t> revision{1};
uint64_t applied_revision=0;
std::atomic<uint64_t> published_revision{0};
std::atomic<uint32_t> published_db{0};
uint32_t observed_db=0;
uint32_t original_db=0;
struct Original {
  std::array<uint8_t,48> appearance;
  std::array<uint8_t,4> motions;
};
std::map<uint16_t,Original> original;
std::unordered_map<uint32_t,uint16_t> appearance_copies;
// Native match/replay code also builds appearance blocks inline. Such copies
// never pass through the memcpy hook, so an address alone is not an identity.
using HeadSignature=std::array<uint8_t,30>;
struct HeadSignatureHash {
  size_t operator()(const HeadSignature& key)const {
    size_t hash=14695981039346656037ull;
    for(auto value:key)hash=(hash^value)*1099511628211ull;
    return hash;
  }
};
std::unordered_map<HeadSignature,uint16_t,HeadSignatureHash> head_identities;
uint32_t head_identity_db=0;
uint64_t head_identity_revision=0;
struct DecodingHead{uint32_t target=0,source=0,db=0;uint16_t id=0;};
thread_local DecodingHead decoding_head;
HeadSignature HeadKey(const uint8_t* p){
  HeadSignature key{};
  std::memcpy(key.data(),p+4,8);key[1]&=252; // sleeves vary by match
  std::memcpy(key.data()+8,p+15,3);
  std::memcpy(key.data()+11,p+24,19);
  return key;
}
struct HeadEdit{bool tint=false;std::array<uint8_t,3> color{};std::array<int,3> fit{};};
std::unordered_map<uint32_t,HeadEdit> decoded_hair_tints;
std::function<void(uint16_t)> open_callback;
std::atomic<uint16_t> native_player{0};
bool Valid(const uint8_t* p,size_t n) {
  MEMORY_BASIC_INFORMATION info{};
  while(n) {
    if(!VirtualQuery(p,&info,sizeof(info))||info.State!=MEM_COMMIT||
       (info.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const size_t available=static_cast<const uint8_t*>(info.BaseAddress)+info.RegionSize-p;
    const size_t step=std::min(available,n); if(!step)return false;
    p+=step;n-=step;
  }
  return true;
}
uint32_t Database(uint8_t* base) {
  const uint32_t db=REX_LOAD_U32(0x83139440u);
  return db>=0x10000u && db<0xdff00000u && Valid(base+db,4210u*152+4) ? db:0;
}
std::string Name(uint8_t* base,uint32_t record) {
  const char* p=reinterpret_cast<const char*>(base+record+4);
  return std::string(p,strnlen(p,45));
}
void CaptureOriginal(uint8_t* base,uint32_t db,uint16_t id) {
  if(original_db!=db){original.clear();appearance_copies.clear();original_db=db;}
  if(!original.contains(id)) {
    Original bytes;
    std::memcpy(bytes.appearance.data(),base+db+152u*id+108,48);
    const auto* stats=base+db+152u*id+72;
    bytes.motions={stats[0],stats[1],stats[31],stats[32]};original[id]=bytes;
  }
}
std::array<int,15> ReadBody(const uint8_t* appearance){
  std::array<int,15> result;for(unsigned i=0;i<result.size();i++)result[i]=ReadBodyField(appearance,i);return result;
}
std::array<int,6> ReadAnimations(const uint8_t* stats) {
  return {int((stats[0]>>3)&15),int(stats[0]&7),int(stats[1]>>6),int((stats[1]>>4)&3),int(stats[31]>>1),int(stats[32]>>1)};
}
void PatchAnimations(uint8_t* stats,const Changes& c,unsigned id) {
  RegisterPlayerSetPieces(id,c.animations[0],c.animations[1]);
  const auto& a=c.animations;
  if(a[0]>=0&&(a[0]<=9||SetPieceGameplayReady()))stats[0]=(stats[0]&~0x78u)|(a[0]<<3);
  if(a[1]>=0&&SetPiecePenaltyReady(unsigned(a[1])))stats[0]=(stats[0]&~7u)|std::min(a[1],7);
  if(a[2]>=0)stats[1]=(stats[1]&~0xc0u)|(a[2]<<6);
  if(a[3]>=0)stats[1]=(stats[1]&~0x30u)|(a[3]<<4);
  if(a[4]>=0)stats[31]=(stats[31]&1u)|(a[4]<<1);
  if(a[5]>=0)stats[32]=(stats[32]&1u)|(a[5]<<1);
}
void Patch(uint8_t* p,const Changes& c,uint8_t* base) {
  const auto db=REX_LOAD_U32(0x83139440u);
  const auto donor=[&](int id)->const uint8_t*{if(id<1||id>=4210)return nullptr;const auto q=base+db+152u*id+108;return Valid(q,48)?q:nullptr;};
  const auto blend=[&](unsigned offset,uint8_t mask,const uint8_t* from){p[offset]=(p[offset]&~mask)|(from[offset]&mask);};
  if(const auto q=donor(c.face_source)){
    // Face kind / preset ID / skin group / facial morph controls (821A7EA8).
    blend(4,224,q);blend(7,254,q);blend(6,63,q);blend(10,252,q);blend(11,252,q);blend(8,255,q);blend(9,227,q);
    std::memcpy(p+24,q+24,16);blend(40,255,q);blend(41,255,q);blend(42,240,q);
  }
  if(const auto q=donor(c.hair_source)){
    blend(4,31,q);blend(5,252,q);
  }
  if(c.face_mode==0)p[9]&=~3u;
  for(unsigned i=0;i<face_fields.size();i++)if(c.face[i]!=face_unset)WriteFaceField(p,i,c.face[i]);
  if(c.hair_model>=0)WriteHairModel(p,c.hair_model);
  auto packed_color=c.hair_color;
  for(int& value:packed_color)if(value>=0)value=HairColor6(value);
  WriteHairColor(p,packed_color);
  for(unsigned i=0;i<body_fields.size();i++)if(c.body[i]!=body_unset)WriteBodyField(p,i,c.body[i]);
  if(c.nationality>=0){p[44]=uint8_t(c.nationality>>1);p[45]=uint8_t((p[45]&127)|((c.nationality&1)<<7));}
  if(c.boot>=0)p[47]=uint8_t(c.boot);
  if(c.sleeves>=0)p[5]=(p[5]&~3u)|uint8_t(c.sleeves+1);
  p[1]=(p[1]&~1u)|uint8_t(c.shirt);
  auto bit=[&](int at,int mask,int value){if(value>=0)p[at]=(p[at]&~mask)|(value?mask:0);};
  bit(12,8,c.wrist_left);bit(12,128,c.wrist_right);
  bit(13,8,c.bracelet_left);bit(13,128,c.bracelet_right);
  bit(14,128,c.undershorts);bit(14,8,c.ankle_tape);
  if(c.necklace>=0)p[10]=(p[10]&~3u)|uint8_t(c.necklace);
}
// Called with mutex held. Build once per edit/database revision, including
// unedited players so identical heads cannot accidentally inherit an edit.
void IndexHeadIdentities(uint8_t* base,uint32_t db){
  const auto current=revision.load(std::memory_order_relaxed);
  if(head_identity_db==db&&head_identity_revision==current)return;
  head_identities.clear();head_identities.reserve(8418);
  const auto add=[&](const uint8_t* appearance,uint16_t id){
    const auto [it,inserted]=head_identities.emplace(HeadKey(appearance),id);
    if(!inserted&&it->second!=id)it->second=0; // ambiguous, never guess
  };
  for(uint16_t id=1;id<4210;id++){
    const auto record=db+152u*id;
    if(Name(base,record).empty())continue;
    const auto* live=base+record+108;
    add(live,id);
    const auto saved=original.find(id);
    const auto* baseline=saved==original.end()?live:saved->second.appearance.data();
    add(baseline,id);
    const auto change=changes.find(id);
    if(change!=changes.end()&&Name(base,record)==change->second.name){
      std::array<uint8_t,48> edited;
      std::memcpy(edited.data(),baseline,48);Patch(edited.data(),change->second,base);
      add(edited.data(),id);
    }
  }
  head_identity_db=db;head_identity_revision=current;
}
uint16_t AppearanceId(uint8_t* base,uint32_t db,uint32_t source){
  if(source>=db+108&&(source-db-108)%152==0){
    const auto id=(source-db-108)/152;
    if(id>0&&id<4210)return uint16_t(id);
  }
  if(const auto it=appearance_copies.find(source);it!=appearance_copies.end())return it->second;
  IndexHeadIdentities(base,db);
  const auto it=head_identities.find(HeadKey(base+source));
  return it==head_identities.end()?0:it->second;
}
bool Save() {
  std::error_code ec;std::filesystem::create_directories(store_path.parent_path(),ec);
  if(ec){status="No se pudo crear la carpeta de edicion";return false;}
  auto temporary=store_path;temporary+=L".tmp";
  std::ofstream out(temporary,std::ios::binary|std::ios::trunc);
  out<<"PES6_PLAYER_APPEARANCE_V8\n";
  for(const auto& [id,c]:changes){
    out<<id<<' '<<std::quoted(c.name)<<' '<<c.boot<<' '<<c.sleeves<<' '<<c.shirt<<' '<<c.wrist_left<<' '<<c.wrist_right<<' '<<c.bracelet_left<<' '<<c.bracelet_right<<' '<<c.undershorts<<' '<<c.ankle_tape<<' '<<c.necklace;
    for(int value:c.animations)out<<' '<<value;
    for(int value:c.body)out<<' '<<value;
    out<<' '<<c.face_source<<' '<<c.hair_source<<' '<<c.hair_model;
    for(int value:c.hair_color)out<<' '<<value;
    for(int value:c.hair_fit)out<<' '<<value;
    out<<' '<<c.face_mode;
    for(int value:c.face)out<<' '<<value;
    out<<' '<<c.nationality;
    out<<'\n';
  }
  out.flush();if(!out){status="No se pudieron guardar los cambios";return false;}out.close();
  if(!MoveFileExW(temporary.c_str(),store_path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
    status="No se pudo actualizar el archivo de edicion";return false;
  }
  status="Guardado. Inicia un partido nuevo para comprobarlo.";return true;
}
const Player* Find(uint16_t id) {
  const auto i=std::find_if(catalog->players.begin(),catalog->players.end(),[&](const Player& p){return p.id==id;});
  return i==catalog->players.end()?nullptr:&*i;
}
}
static std::array<bool,boot_capacity> hidden_boots{};
void Initialize(const std::string& root) {
  InitializeSetPieceGameplay(root);
  std::lock_guard lock(mutex);store_path=std::filesystem::path(root)/"data/editor/player-appearance.txt";
  changes.clear();field_edit_backup.reset();++revision;EnsureBootCatalog();
  {std::ifstream names(std::filesystem::path(root)/"data/editor/boots/names.txt");unsigned index;std::string name;while(names>>index>>std::quoted(name))if(index<boot_count&&!name.empty())boot_names[index]=name;}
  hidden_boots.fill(false);
  {
    std::ifstream hidden(std::filesystem::path(root)/"data/editor/boots/hidden-in-game.txt");
    std::string line;
    while(std::getline(hidden,line)){
      std::istringstream row(line);unsigned index;
      if(row>>index && index<hidden_boots.size())hidden_boots[index]=true;
    }
  }
  std::ifstream in(store_path);std::string header;std::getline(in,header);
  const bool v8=header=="PES6_PLAYER_APPEARANCE_V8";
  const bool v7=v8||header=="PES6_PLAYER_APPEARANCE_V7";
  const bool v6=v7||header=="PES6_PLAYER_APPEARANCE_V6";
  const bool v5=v6||header=="PES6_PLAYER_APPEARANCE_V5";
  const bool v4=v5||header=="PES6_PLAYER_APPEARANCE_V4";
  const bool v3=v4||header=="PES6_PLAYER_APPEARANCE_V3";
  const bool v2=v3||header=="PES6_PLAYER_APPEARANCE_V2";
  if(!v2&&header!="PES6_PLAYER_APPEARANCE_V1")return;
  std::string line;
  while(std::getline(in,line)){
    uint32_t id;Changes c;std::istringstream row(line);
    if(!(row>>id>>std::quoted(c.name)>>c.boot>>c.sleeves>>c.shirt>>c.wrist_left>>c.wrist_right>>c.bracelet_left>>c.bracelet_right>>c.undershorts>>c.ankle_tape>>c.necklace))continue;
    bool valid=true;
    if(v2)for(size_t i=0;i<c.animations.size();i++)if(!(row>>c.animations[i])||c.animations[i]<-1||c.animations[i]>animation_max[i])valid=false;
    if(v3)for(size_t i=0;i<c.body.size();i++)if(!(row>>c.body[i])||(c.body[i]!=body_unset&&(c.body[i]<body_fields[i].minimum||c.body[i]>body_fields[i].maximum)))valid=false;
    if(v4&&(!(row>>c.face_source>>c.hair_source)||c.face_source<-1||c.face_source>=4210||c.hair_source<-1||c.hair_source>=4210))valid=false;
    if(v5&&!ReadHairExtension(row,c.hair_model,c.hair_color,v6?255:63))valid=false;
    if(v5&&!v6)for(int& value:c.hair_color)if(value>=0)value=HairColor8(value);
    if(v6&&!ReadHairFit(row,c.hair_fit))valid=false;
    if(v7){
      if(!(row>>c.face_mode)||c.face_mode<-1||c.face_mode>0)valid=false;
      for(unsigned i=0;i<c.face.size();i++){
        int& value=c.face[i];const auto& field=face_fields[i];
        if(!(row>>value)||(value!=face_unset&&(value<field.minimum||value>field.maximum)))valid=false;
      }
    }
    if(v8&&(!(row>>c.nationality)||c.nationality<-1||c.nationality>=int(nationality_names.size())))valid=false;
    auto binary=[](int v){return v>=-1&&v<=1;};
    if(valid&&id>0&&id<4210&&c.boot>=-1&&c.boot<int(boot_count)&&c.sleeves>=-1&&c.sleeves<2&&c.shirt>=0&&c.shirt<=1&&binary(c.wrist_left)&&binary(c.wrist_right)&&binary(c.bracelet_left)&&binary(c.bracelet_right)&&binary(c.undershorts)&&binary(c.ankle_tape)&&c.necklace>=-1&&c.necklace<3){changes[uint16_t(id)]=c;RegisterPlayerSetPieces(id,c.animations[0],c.animations[1]);}
  }
  ++revision;
}
std::shared_ptr<const Catalog> ReadCatalog(){std::lock_guard lock(mutex);return catalog;}
int PreviewAnimationStyle(uint16_t id,unsigned field,int native_style){
  std::lock_guard lock(mutex);const auto it=changes.find(id);
  return field<2&&it!=changes.end()&&it->second.animations[field]>=0?it->second.animations[field]:native_style;
}
void RefreshNativeCatalog(uint8_t* base) {
  const uint32_t db=Database(base);if(!db)return;
  std::lock_guard lock(mutex);
  auto next=std::make_shared<Catalog>();
  next->generation=catalog->generation+(observed_db!=db?1:0);
  for(uint16_t id=1;id<4210;id++){
    const uint32_t r=db+152u*id;auto name=Name(base,r);
    if(!name.empty())next->players.push_back({id,std::move(name),base[r+155],uint8_t(base[r+113]&3),ReadAnimations(base+r+72),ReadBody(base+r+108),ReadHairColor(base+r+108),ReadFace(base+r+108),uint8_t(base[r+117]&3),unsigned(base[r+152])*2u+(base[r+153]>>7)});
  }
  catalog=std::move(next);
}
std::string Status(){std::lock_guard lock(mutex);return status;}
void SetOpenCallback(std::function<void(uint16_t)> callback) {
  std::lock_guard lock(mutex);open_callback=std::move(callback);
}
int ReadNationality(uint16_t id){
  std::lock_guard lock(mutex);
  const auto* player=Find(id);if(!player)return -1;
  const auto found=changes.find(id);
  if(found!=changes.end()&&found->second.name==player->name&&found->second.nationality>=0)return found->second.nationality;
  return int(player->nationality);
}
Snapshot ReadPlayer(uint16_t id) {
  std::lock_guard lock(mutex);
  static uint16_t cached_id=0;
  static uint64_t cached_gen=0,cached_rev=0;
  static Snapshot cached_snapshot;
  const uint64_t cur_rev=revision.load(std::memory_order_relaxed);
  if(id&&id==cached_id&&catalog->generation==cached_gen&&cur_rev==cached_rev){
    Snapshot s=cached_snapshot;
    s.information_available=native_player.load()==id;
    return s;
  }
  Snapshot s;const Player* p=Find(id);if(!p)return s;
  s.player_id=id;s.generation=catalog->generation;s.player_name=p->name;s.team_name="Editor de equipacion";
  s.appearance_available=true;s.boot_index=p->boot;
  s.information_available=native_player.load()==id;
  Changes c;auto it=changes.find(id);if(it!=changes.end()&&it->second.name==p->name)c=it->second;
  if(c.boot>=0)s.boot_index=uint8_t(c.boot);
  for(unsigned i=0;i<boot_count;i++)if(!hidden_boots[i])s.boots.push_back({uint8_t(i),BootName(i),0});
  std::sort(s.boots.begin(),s.boots.end(),[](const auto& a,const auto& b){
    const int order=_stricmp(a.name.c_str(),b.name.c_str());
    return order==0?a.appearance_index<b.appearance_index:order<0;
  });
  auto selector=[&](std::string id,std::string label,std::vector<std::string> options,int value){s.accessories.push_back({std::move(id),std::move(label),std::move(options),value,true});};
  selector("shirt","Camiseta",{"Por dentro","Por fuera"},c.shirt);
  selector("sleeves","Mangas",{"Por defecto","Cortas","Largas"},c.sleeves+1);
  for(auto [field,label,value]:std::initializer_list<std::tuple<const char*,const char*,int>>{
    {"wrist_left","Muñequera izquierda",c.wrist_left},{"wrist_right","Muñequera derecha",c.wrist_right},
    {"bracelet_left","Brazalete izquierdo",c.bracelet_left},{"bracelet_right","Brazalete derecho",c.bracelet_right},
    {"undershorts","Malla bajo pantalón",c.undershorts},{"ankle_tape","Vendaje de tobillo",c.ankle_tape}})
    selector(field,label,{"Por defecto","No","Sí"},value+1);
  selector("necklace","Collar",{"Por defecto","No","Tipo 1","Tipo 2"},c.necklace+1);
  for(size_t k=0;k<animation_fields.size();k++){
    std::vector<std::string> choices{"Por defecto (actual: "+std::to_string(p->animations[k]+(k<4?1:0))+")"};
    for(int value=0;value<=animation_max[k];value++)
      choices.push_back(k==0?std::string(free_kick_names[value]):k==1?std::string(penalty_names[value]):k>=4&&value==0?"Automatica":"Tipo "+std::to_string(value+(k<4?1:0)));
    s.animations.push_back({animation_fields[k],animation_labels[k],std::move(choices),c.animations[k]+1,true});
  }
  s.appearance.push_back({"face_mode","Tipo de cara",{"Original / preset","Construir cara"},c.face_mode==0?1:0,true});
  for(const auto& [field,label,source]:std::initializer_list<std::tuple<const char*,const char*,int>>{{"face","Cara (preset)",c.face_source}}){
    std::vector<std::string> choices;choices.reserve(catalog->players.size()+1);choices.emplace_back("Original");int selected=0;
    for(const auto& donor:catalog->players){choices.push_back(donor.name);if(donor.id==source)selected=int(choices.size()-1);}
    s.appearance.push_back({field,label,std::move(choices),selected,true});
  }
  const bool build=c.face_mode==0||p->face_kind!=1;
  for(const auto& component:face_component_types){
    std::vector<std::string> options{"Actual / personalizado"};
    for(unsigned i=0;i<component.types.size();i++)options.push_back("Tipo "+std::to_string(i+1));
    s.appearance.push_back({component.id,component.label,std::move(options),build?FaceComponentSelection(c,p->face,component):0,true});
  }
  for(unsigned i=0;i<face_fields.size();i++){
    const auto& field=face_fields[i];std::vector<std::string> choices;
    const int current=c.face[i]==face_unset?p->face[i]:c.face[i];
    for(int value=field.minimum;value<=field.maximum;value++)choices.push_back(std::to_string(value));
    s.appearance.push_back({field.id,field.label,std::move(choices),current-field.minimum,build});
  }
  {
    const int group=HairGroup(c.hair_model);const auto count=HairGroupSize(group);
    s.appearance.push_back({"hair_type","Tipo de peinado",{"Por defecto","Muy corto","Corto","Mediano","Largo"},group,true});
    std::vector<std::string> choices;int selected=0;
    for(unsigned i=0;i<count;i++){
      choices.push_back("Peinado "+std::to_string(i+1)+"/"+std::to_string(count));
      if(HairGroupModel(group,i)==c.hair_model)selected=int(i);
    }
    if(choices.empty())choices.push_back("--");
    s.appearance.push_back({"hair","Peinado",std::move(choices),selected,group!=0});
  }
  for(unsigned channel=0;channel<3;channel++){
    std::vector<std::string> choices{"Por defecto"};
    for(int value=0;value<256;value++)choices.push_back(std::to_string(value));
    s.appearance.push_back({std::array<const char*,3>{"hair_red","hair_green","hair_blue"}[channel],
      std::array<const char*,3>{"Color de pelo: rojo","Color de pelo: verde","Color de pelo: azul"}[channel],
      std::move(choices),c.hair_color[channel]+1,true});
    s.appearance.back().slider_value=c.hair_color[channel]<0?HairColor8(p->hair_color[channel]):c.hair_color[channel];
  }
  for(unsigned i=0;i<3;i++){
    const int range=i==2?20:30;std::vector<std::string> choices;
    for(int v=-range;v<=range;v++)choices.push_back(v==0?"Por defecto":std::to_string(v)+(i==2?" %":" mm"));
    s.appearance.push_back({std::array<const char*,3>{"hair_height","hair_depth","hair_width"}[i],
      std::array<const char*,3>{"Pelo: arriba / abajo","Pelo: adelante / atrás","Pelo: ancho"}[i],
      std::move(choices),c.hair_fit[i]+range,true});
  }
  for(unsigned i=0;i<body_fields.size();i++){
    const auto& field=body_fields[i];std::vector<std::string> choices;
    const int current=c.body[i]==body_unset?p->body[i]:c.body[i];
    for(int value=field.minimum;value<=field.maximum;value++)choices.push_back(std::to_string(value));
    s.appearance.push_back({field.id,field.label,std::move(choices),std::clamp(current,field.minimum,field.maximum)-field.minimum,true});
  }
  s.error=status;
  cached_id=id;cached_gen=catalog->generation;cached_rev=cur_rev;cached_snapshot=s;
  return s;
}
bool BeginFieldEdit(uint16_t id){
  std::lock_guard lock(mutex);if(!Find(id))return false;
  const auto found=changes.find(id);
  field_edit_backup=FieldEditBackup{id,found!=changes.end(),found!=changes.end()?found->second:Changes{}};
  return true;
}
void EndFieldEdit(){std::lock_guard lock(mutex);field_edit_backup.reset();}
bool CancelFieldEdit(uint16_t id){
  std::lock_guard lock(mutex);
  if(!field_edit_backup||field_edit_backup->id!=id)return false;
  const auto previous=changes;
  if(field_edit_backup->existed)changes[id]=field_edit_backup->value;
  else changes.erase(id);
  if(!Save()){changes=previous;return false;}
  const auto found=changes.find(id);
  if(found==changes.end())RegisterPlayerSetPieces(id,-1,-1);
  else RegisterPlayerSetPieces(id,found->second.animations[0],found->second.animations[1]);
  field_edit_backup.reset();++revision;return true;
}
bool Apply(const Request& r) {
  std::lock_guard lock(mutex);const Player* p=Find(r.player_id);
  if(!p||r.generation!=catalog->generation)return false;
  const auto saved=changes;auto& c=changes[r.player_id];c.name=p->name;
  if(r.action==Action::Boot&&r.value>=0&&r.value<int(boot_count))c.boot=r.value;
  else if(r.action==Action::Information&&r.field=="nationality"){
    if(r.value<0||r.value>int(nationality_names.size())){changes=saved;return false;}
    c.nationality=r.value-1;
  }
  else if(r.action==Action::Animation){
    const auto it=std::find(animation_fields.begin(),animation_fields.end(),r.field);
    if(it==animation_fields.end()){changes=saved;return false;}
    const size_t field=it-animation_fields.begin();const int value=r.value-1;
    if(value<-1||value>animation_max[field]){changes=saved;return false;}
    c.animations[field]=value;RegisterPlayerSetPieces(r.player_id,c.animations[0],c.animations[1]);
  }
  else if(r.action==Action::Appearance&&r.field=="face_mode"){
    if(r.value<0||r.value>1){changes=saved;return false;}
    c.face_mode=r.value==1?0:-1;
  }
  else if(r.action==Action::Appearance&&FindFaceComponentType(r.field)!=face_component_types.end()){
    const auto& component=*FindFaceComponentType(r.field);
    if(r.value<0||r.value>int(component.types.size())){changes=saved;return false;}
    if(!r.value)ResetFaceComponent(c,component);
    else{c.face_mode=0;for(unsigned i=0;i<component.count;i++)c.face[component.fields[i]]=component.types[r.value-1][i];}
  }
  else if(r.action==Action::Appearance&&r.field.starts_with("face_")){
    const auto field=FindFaceField(r.field);
    if(field==face_fields.end()||r.value<0||r.value>field->maximum-field->minimum){changes=saved;return false;}
    c.face[field-face_fields.begin()]=field->minimum+r.value;
  }
  else if(r.action==Action::Appearance&&r.field=="hair_type"){
    if(r.value<0||r.value>4){changes=saved;return false;}
    const int model=r.value?HairGroupModel(r.value,0):-1;
    if(r.value&&model<0){changes=saved;return false;}
    c.hair_model=model;c.hair_source=-1;
  }
  else if(r.action==Action::Appearance&&r.field=="hair"){
    const int group=HairGroup(c.hair_model);
    if(r.value<0||r.value>=int(HairGroupSize(group))){changes=saved;return false;}
    c.hair_model=HairGroupModel(group,unsigned(r.value));c.hair_source=-1;
  }
  else if(r.action==Action::Appearance&&(r.field=="hair_red"||r.field=="hair_green"||r.field=="hair_blue")){
    if(r.value<0||r.value>256){changes=saved;return false;}
    c.hair_color[r.field=="hair_red"?0:r.field=="hair_green"?1:2]=r.value-1;
  }
  else if(r.action==Action::Appearance&&(r.field=="hair_height"||r.field=="hair_depth"||r.field=="hair_width")){
    const unsigned i=r.field=="hair_height"?0:r.field=="hair_depth"?1:2;const int range=i==2?20:30;
    if(r.value<0||r.value>range*2){changes=saved;return false;}c.hair_fit[i]=r.value-range;
  }
  else if(r.action==Action::Appearance&&r.field=="face"){
    if(r.value<0||r.value>int(catalog->players.size())){changes=saved;return false;}
    const int source=r.value?catalog->players[r.value-1].id:-1;
    c.face_source=source;
  }
  else if(r.action==Action::Appearance){
    const auto it=std::find_if(body_fields.begin(),body_fields.end(),[&](const BodyField& f){return r.field==f.id;});
    if(it==body_fields.end()||r.value<0||r.value>it->maximum-it->minimum){changes=saved;return false;}
    c.body[it-body_fields.begin()]=it->minimum+r.value;
  }
  else if(r.action==Action::Accessory) {
    int* field=nullptr;int value=r.value-1,max=1;
    if(r.field=="shirt"){field=&c.shirt;value=r.value;}
    else if(r.field=="sleeves")field=&c.sleeves;
    else if(r.field=="wrist_left")field=&c.wrist_left;
    else if(r.field=="wrist_right")field=&c.wrist_right;
    else if(r.field=="bracelet_left")field=&c.bracelet_left;
    else if(r.field=="bracelet_right")field=&c.bracelet_right;
    else if(r.field=="undershorts")field=&c.undershorts;
    else if(r.field=="ankle_tape")field=&c.ankle_tape;
    else if(r.field=="necklace"){field=&c.necklace;max=2;}
    if(!field||value< (r.field=="shirt"?0:-1)||value>max){changes=saved;return false;}*field=value;
  } else {changes=saved;return false;}
  if(!Save()){changes=saved;return false;}++revision;return true;
}
bool ResetField(const Request& r){
  std::lock_guard lock(mutex);const auto* p=Find(r.player_id);
  if(!p||r.generation!=catalog->generation)return false;
  const auto saved=changes;auto& c=changes[r.player_id];c.name=p->name;
  bool valid=false;
  if(r.action==Action::Information&&r.field=="nationality"){c.nationality=-1;valid=true;}
  else if(r.action==Action::Boot)valid=ResetEditorField(c,ResetKind::Boot,r.field);
  else if(r.action==Action::Animation)valid=ResetEditorField(c,ResetKind::Animation,r.field);
  else if(r.action==Action::Appearance)valid=ResetEditorField(c,ResetKind::Appearance,r.field);
  else if(r.action==Action::Accessory)valid=ResetEditorField(c,ResetKind::Accessory,r.field);
  if(!valid){changes=saved;return false;}
  if(!Save()){changes=saved;return false;}++revision;return true;
}
bool ResetPlayer(uint16_t id,ResetSection section) {
  std::lock_guard lock(mutex);const auto previous=changes;
  if(section==ResetSection::All)changes.erase(id);
  else if(auto it=changes.find(id);it!=changes.end()){
    auto& c=it->second;
    if(section==ResetSection::Appearance)c.body.fill(body_unset);
    if(section==ResetSection::Head||section==ResetSection::Face){c.face_source=-1;c.face_mode=-1;c.face.fill(face_unset);}
    if(section==ResetSection::Head||section==ResetSection::Hair){c.hair_source=c.hair_model=-1;c.hair_color.fill(-1);c.hair_fit.fill(0);}
    if(section==ResetSection::Boots)c.boot=-1;
    if(section==ResetSection::Animations)c.animations.fill(-1);
    if(section==ResetSection::Accessories){c.sleeves=-1;c.shirt=0;c.wrist_left=c.wrist_right=c.bracelet_left=c.bracelet_right=c.undershorts=c.ankle_tape=c.necklace=-1;}
  }
  if(!Save()){changes=previous;return false;}++revision;return true;
}
// Called only from a guest frame submission. UI never dereferences this memory.
extern "C" void pes6_team_management_frame(uint8_t*);
extern "C" void pes6_editor_frame(uint8_t* base) {
  pes6_team_management_frame(base);
  const auto raw_db=REX_LOAD_U32(0x83139440u);
  // Stable, closed editor: no guest-page query or mutex on every frame.
  // Atomics publish only completed edits; a UI revision or database change
  // takes the original validated/locked path below.
  if(raw_db && !open.load(std::memory_order_relaxed) &&
     published_db.load(std::memory_order_acquire)==raw_db &&
     published_revision.load(std::memory_order_acquire)==revision.load(std::memory_order_acquire))return;
  const uint32_t db=Database(base);if(!db)return;
  std::lock_guard lock(mutex);
  if(db==observed_db&&applied_revision==revision&&(!open.load()||!catalog->players.empty()))return;
  const bool new_db=db!=observed_db;
  // Retain originals for reset, per loaded database, never across guest loads.
  if(new_db){ClearExtendedPenalties();if(original_db!=db){original.clear();original_db=db;}appearance_copies.clear();head_identity_db=0;decoded_hair_tints.clear();observed_db=db;
    for(const auto& entry:licensed_southamerica_names){
      const uint32_t rec=db+152u*entry.id;
      std::memset(base+rec+4,0,46);std::memcpy(base+rec+4,entry.name,std::min<size_t>(45,std::strlen(entry.name)));
      std::memset(base+rec+50,0,16);std::memcpy(base+rec+50,entry.jersey,std::min<size_t>(15,std::strlen(entry.jersey)));
    }
  }
  for(const auto& [id,c]:changes){const uint32_t record=db+152u*id;
    if(Name(base,record)!=c.name)continue;
    CaptureOriginal(base,db,id);
  }
  for(const auto& [id,bytes]:original){const uint32_t record=db+152u*id;
    // Age belongs to the native Information form and may change after this
    // appearance baseline was captured. Preserve it when reapplying cosmetics.
    const uint8_t age=base[record+153]&0x7c;
    std::memcpy(base+record+108,bytes.appearance.data(),48);
    base[record+153]=uint8_t((base[record+153]&~0x7c)|age);
    RegisterPlayerSetPieces(id,-1,-1);auto* stats=base+record+72;
    stats[0]=(stats[0]&0x80)|(bytes.motions[0]&0x7f);
    stats[1]=(stats[1]&0x0f)|(bytes.motions[1]&0xf0);
    stats[31]=(stats[31]&1)|(bytes.motions[2]&254);
    stats[32]=(stats[32]&1)|(bytes.motions[3]&254);
    const auto it=changes.find(id);if(it!=changes.end()&&Name(base,record)==it->second.name){Patch(base+record+108,it->second,base);PatchAnimations(stats,it->second,id);}
    else base[record+109]&=254;
  }
  applied_revision=revision.load(std::memory_order_relaxed);
  if(open.load()||new_db) {
    auto next=std::make_shared<Catalog>();next->generation=catalog->generation+(new_db?1:0);
    for(uint16_t id=1;id<4210;id++){const uint32_t r=db+152u*id;auto name=Name(base,r);
      if(!name.empty())next->players.push_back({id,std::move(name),base[r+155],uint8_t(base[r+113]&3),ReadAnimations(base+r+72),ReadBody(base+r+108),ReadHairColor(base+r+108),ReadFace(base+r+108),uint8_t(base[r+117]&3),unsigned(base[r+152])*2u+(base[r+153]>>7)});
    }
    catalog=std::move(next);
    if(new_db){
      std::error_code ec;std::filesystem::create_directories(store_path.parent_path(),ec);
      std::ofstream report(store_path.parent_path()/"runtime-status.txt",std::ios::trunc);
      report<<"Live player database: "<<std::hex<<db<<std::dec<<"\nPlayers: "<<catalog->players.size()<<"\n";
      if(const auto* p=Find(446))report<<"Player 446: "<<p->name<<"\n";
    }
  }
  published_db.store(db,std::memory_order_release);
  published_revision.store(applied_revision,std::memory_order_release);
}
extern "C" void pes6_editor_decode(PPCContext& ctx,uint8_t* base) {
  decoding_head={};
  const uint32_t db=Database(base),source=ctx.r4.u32;
  if(!db||source<0x10000u||uint64_t(source)+48>=0xe0000000ull||!Valid(base+source,48))return;
  std::lock_guard lock(mutex);
  const auto id=AppearanceId(base,db,source);if(!id)return;
  // Keep identity through the leaf decoder even when applying a donor face
  // makes this head's resulting bytes identical to another player's head.
  decoding_head={ctx.r3.u32,source,db,id};
  auto it=changes.find(id);
  if(it!=changes.end()&&Name(base,db+id*152)==it->second.name)CaptureOriginal(base,db,uint16_t(id));
  base[source+1]&=254;
  if(it!=changes.end()&&Name(base,db+id*152)==it->second.name){Patch(base+source,it->second,base);PatchAnimations(base+db+id*152+72,it->second,id);}
}
extern "C" void pes6_editor_instance(PPCContext& ctx,uint8_t* base) {
  // Direct database appearance or an already-patched per-match copy. The kit
  // configuration at instance+1348 contains a TEAM id, never use it as player id.
  pes6_editor_decode(ctx,base);
}
extern "C" void pes6_editor_head(uint8_t* base,uint32_t target,uint32_t source){
  const auto db=Database(base);if(!db||!Valid(base+target,68)||source<0x10000u||uint64_t(source)+48>=0xe0000000ull||!Valid(base+source,48))return;
  std::lock_guard lock(mutex);
  const auto id=decoding_head.target==target&&decoding_head.source==source&&decoding_head.db==db
    ?decoding_head.id:AppearanceId(base,db,source);
  decoding_head={};
  decoded_hair_tints.erase(target);
  const auto change=changes.find(id);if(change==changes.end()||Name(base,db+152u*id)!=change->second.name)return;
  const auto& c=change->second;
  const bool tint=std::any_of(c.hair_color.begin(),c.hair_color.end(),[](int value){return value>=0;});
  if(tint||c.hair_fit!=std::array<int,3>{0,0,0}){
    if(decoded_hair_tints.size()>4096)decoded_hair_tints.clear();
    HeadEdit edit;edit.tint=tint;edit.fit=c.hair_fit;
    for(unsigned i=0;i<3;i++)edit.color[i]=uint8_t(c.hair_color[i]<0?HairColor8(base[target+61+i]):c.hair_color[i]);
    decoded_hair_tints[target]=edit;
  }
  if(c.hair_model>=0){
    // Generic hair bank; keep the scanned face kind and face model +14 intact.
    base[target+2]=0;REX_STORE_U16(target+4,c.hair_model);return;
  }
  if((c.face_mode==0||c.face_source>=1)&&c.hair_source<1){
    const auto saved=original.find(id);
    if(saved!=original.end()){
      const auto* appearance=saved->second.appearance.data();
      const bool scanned=(appearance[9]&3)==1;
      base[target+2]=scanned?1:0;
      const unsigned scanned_model=(unsigned(appearance[8])<<3)|(appearance[9]>>5);
      const unsigned generic_model=((unsigned(appearance[4])&31)<<6)|(appearance[5]>>2);
      REX_STORE_U16(target+4,scanned?scanned_model:generic_model);
    }
  }
  if(c.hair_source<1)return;
  const auto donor=base+db+152u*unsigned(change->second.hair_source)+108;
  if(!Valid(donor,48))return;
  const auto word=(uint32_t(donor[4])<<24)|(uint32_t(donor[5])<<16)|(uint32_t(donor[6])<<8)|donor[7];
  // 821A7EA8 aliases model ID +4 to scanned-face ID +14 for preset faces.
  // Keep face kind/ID +1/+14 intact and override only the hair model ID.
  REX_STORE_U16(target+4,(word>>18)&2047);
}
extern "C" void pes6_editor_hair_tint(PPCContext& ctx,uint8_t* base,uint32_t target,uint32_t head){
  HeadEdit edit;
  {std::lock_guard lock(mutex);const auto found=decoded_hair_tints.find(head);
    if(found==decoded_hair_tints.end())return;edit=found->second;}
  if(!Valid(base+target,20))return;
  if(edit.tint){base[target+4]=2;std::memcpy(base+target+5,edit.color.data(),3);}
  if(edit.fit==std::array<int,3>{0,0,0})return;
  const auto begin=REX_LOAD_U32(target+12),end=REX_LOAD_U32(target+16);
  // Native hair uses exactly three morph controls. Append a private identity
  // suffix, leaving those controls unchanged; cache equality compares all ints.
  if(!begin||end!=begin+12||!Valid(base+begin,12))return;
  for(int value:std::array<int,4>{0x50485336,edit.fit[0],edit.fit[1],edit.fit[2]}){
    PPCContext call=ctx;call.r1.u32-=128;
    const auto scratch=call.r1.u32+64;REX_STORE_U32(scratch,uint32_t(value));
    call.r3.u64=target+8;call.r4.u64=scratch;sub_8214B560(call,base);
  }
}
extern "C" void pes6_editor_fit_hair(uint8_t* base,uint32_t process){
  if(!Valid(base+process,432))return;
  const auto begin=REX_LOAD_U32(process+420),end=REX_LOAD_U32(process+424);
  if(!begin||end!=begin+28||!Valid(base+begin,28)||REX_LOAD_U32(begin+12)!=0x50485336)return;
  const std::array<int,3> fit{int32_t(REX_LOAD_U32(begin+16)),int32_t(REX_LOAD_U32(begin+20)),int32_t(REX_LOAD_U32(begin+24))};
  if(!ValidHairFit(fit))return;
  const auto data=REX_LOAD_U32(process+16),size=REX_LOAD_U32(process+20);
  if(size<32||size>8*1024*1024||!Valid(base+data,size))return;
  const auto count=REX_LOAD_U32(data);
  if(!count||count>16||REX_LOAD_U32(data+4)!=8||8+12*count>size)return;
  // Decoder result is owned by this hair-cache entry and freed by822FC8E0.
  // Only its geometry children change; archive bytes, faces and textures stay.
  for(unsigned i=0;i<count;i++){
    const auto at=REX_LOAD_U32(data+8+12*i),length=REX_LOAD_U32(data+12+12*i);
    if(at<8+12*count||uint64_t(at)+length>size)continue;
    if(length>=128&&std::memcmp(base+data+at,"KTMDL",5)==0)
      FitHairGeometry(std::span<uint8_t>(base+data+at,length),fit);
  }
}
extern "C" void pes6_editor_hair_color8(uint8_t* base,uint32_t effect){
  if(!Valid(base+effect,40)||base[effect+36]!=2)return;
  const auto model=REX_LOAD_U32(effect+8);if(!Valid(base+model,16))return;
  const auto data=REX_LOAD_U32(model+12),index=REX_LOAD_U32(effect+16);
  if(index>4096||!Valid(base+data,80))return;
  const uint64_t at=uint64_t(data)+REX_LOAD_U32(data+76)+index*160ull;
  if(at>0xffffffffu||!Valid(base+uint32_t(at),160))return;
  const auto material=uint32_t(at);
  if(REX_LOAD_U64(material)!=REX_LOAD_U64(effect+24))return;
  const auto* rgb=base+effect+32;const auto color=HairMaterialColor({rgb[0],rgb[1],rgb[2]});
  for(unsigned i=0;i<3;i++)REX_STORE_U32(material+48+4*i,std::bit_cast<uint32_t>(color[i]));
}
extern "C" void pes6_editor_copy(PPCContext& ctx,uint8_t* base) {
  const uint32_t size=ctx.r5.u32;
  if(size!=84&&size!=48&&size!=152)return;
  const uint32_t db=REX_LOAD_U32(0x83139440u),source=ctx.r4.u32;
  {
    std::lock_guard lock(mutex);uint16_t copyId=0;unsigned appearanceOffset=0;
    if(db>=0x10000&&source>=db){const auto rel=source-db,id=rel/152,offset=rel%152;
      if(id>0&&id<4210&&((offset==72&&size==84)||(offset==108&&size==48)||(offset==4&&size==152))){copyId=uint16_t(id);appearanceOffset=offset==72?36:offset==4?104:0;}
    }
    if(!copyId&&size==48)if(const auto it=appearance_copies.find(source);it!=appearance_copies.end())copyId=it->second;
    if(copyId){if(appearance_copies.size()>16384)appearance_copies.clear();appearance_copies[ctx.r3.u32+appearanceOffset]=copyId;}
  }
  if(db<0x10000u||db>=0xdff00000u||source<db)return;
  const uint32_t relative=source-db,id=relative/152,offset=relative%152;
  if(!id||id>=4210||!((offset==72&&size==84)||(offset==108&&size==48)||(offset==4&&size==152)))return;
  const uint32_t appearance=db+id*152+108;
  if(!Valid(base+appearance,48))return;
  std::lock_guard lock(mutex);
  const auto it=changes.find(uint16_t(id));
  if(it!=changes.end()&&Name(base,db+id*152)==it->second.name)CaptureOriginal(base,db,uint16_t(id));
  base[appearance+1]&=254;
  if(it!=changes.end()&&Name(base,db+id*152)==it->second.name){Patch(base+appearance,it->second,base);PatchAnimations(base+db+id*152+72,it->second,id);}
}
extern "C" void pes6_ktmdl_decal_bias(uint8_t* base,uint32_t obj){
  if(!Valid(base+obj,16))return;
  const uint32_t ktmdl=REX_LOAD_U32(obj+12);
  if(!Valid(base+ktmdl,128)||std::memcmp(base+ktmdl,"KTMDL",5)!=0)return;
  const uint32_t group_count=REX_LOAD_U32(ktmdl+64);
  if(group_count!=42)return;
  const uint32_t stream_end=REX_LOAD_U32(ktmdl+120);
  const uint32_t count=REX_LOAD_U32(ktmdl+40),meshes=REX_LOAD_U32(ktmdl+44);
  if(!count||count>256||meshes<128||uint64_t(meshes)+uint64_t(count)*96>stream_end)return;
  const auto f32=[&](uint32_t at){return std::bit_cast<float>(REX_LOAD_U32(at));};
  const auto sf32=[&](uint32_t at,float v){REX_STORE_U32(at,std::bit_cast<uint32_t>(v));};
  std::set<uint32_t> seen;
  for(uint32_t i=0;i<count;i++){
    const uint32_t mesh=ktmdl+meshes+i*96;
    if(!Valid(base+mesh,96))break;
    const uint32_t group=REX_LOAD_U32(mesh+8);
    if(group!=21&&group!=22)continue;
    for(uint32_t rel:{20u,36u}){
      const uint32_t streams=REX_LOAD_U32(mesh+rel-4);
      const int32_t table_off=int32_t(REX_LOAD_U32(mesh+rel));
      if(streams>16||table_off<0)continue;
      const uint32_t table=mesh+uint32_t(table_off);
      if(!Valid(base+table,streams*32))continue;
      for(uint32_t j=0;j<streams;j++){
        const uint32_t s=table+j*32;
        if(int32_t(REX_LOAD_U32(s+20))!=0)continue;
        const int32_t v_off=int32_t(REX_LOAD_U32(s)),d_off=int32_t(REX_LOAD_U32(s+12));
        const uint32_t n=REX_LOAD_U32(s+4);
        const unsigned stride=base[s+9],elements=base[s+10];
        if(!n||n>8192||!stride||elements>16)continue;
        const uint32_t vertices=s+uint32_t(v_off),decl=s+uint32_t(d_off);
        if(!Valid(base+vertices,n*stride)||!Valid(base+decl,elements*4))continue;
        int pos_off=-1,nrm_off=-1;
        for(unsigned e=0;e<elements;e++){
          const uint32_t d=decl+e*4;
          if(base[d+2]!=2)continue;
          if(base[d+3]==16&&base[d+1]+12<=stride)pos_off=base[d+1];
          else if(base[d+3]==18&&base[d+1]+12<=stride)nrm_off=base[d+1];
        }
        if(pos_off<0||nrm_off<0)continue;
        for(uint32_t v=0;v<n;v++){
          const uint32_t p=vertices+v*stride+uint32_t(pos_off);
          const uint32_t q=vertices+v*stride+uint32_t(nrm_off);
          if(!seen.insert(p).second)continue;
          const float px=f32(p),py=f32(p+4),pz=f32(p+8);
          const float nx=f32(q),ny=f32(q+4),nz=f32(q+8);
          if(!std::isfinite(px)||!std::isfinite(py)||!std::isfinite(pz)||
             !std::isfinite(nx)||!std::isfinite(ny)||!std::isfinite(nz))continue;
          const float len=std::sqrt(nx*nx+ny*ny+nz*nz);
          if(len<=1e-4f)continue;
          const float bias=0.004f/len;
          sf32(p,px+nx*bias);sf32(p+4,py+ny*bias);sf32(p+8,pz+nz*bias);
        }
      }
    }
  }
}
extern "C" void pes6_editor_collar_groups(PPCContext&,uint8_t*,uint32_t,uint32_t);
extern "C" void pes6_editor_groups(PPCContext& ctx,uint8_t* base) {
  const uint32_t object=ctx.r3.u32;
  if(!Valid(base+object,1360))return;
  const bool outside=base[object+1359]!=0;
  const PPCContext saved=ctx;
  // Model 22's unused group-7 packet is adapted to the model-0 native FRONT
  // crest in entry2775. Never enable unadapted back/waist badge packets.
  const uint32_t kit=REX_LOAD_U32(object+1348);
  if(Valid(base+kit,84)){
    pes6_editor_collar_groups(ctx,base,object,kit);
    const bool legacy=((base[kit]==22&&REX_LOAD_U32(kit+12)==69&&
       (REX_LOAD_U32(kit+20)==138||REX_LOAD_U32(kit+20)==139))||
      ((base[kit]==22||base[kit]==25)&&REX_LOAD_U32(kit+12)==72&&
       (REX_LOAD_U32(kit+20)==144||REX_LOAD_U32(kit+20)==145)));
    const int crestMode=pes6::crest::Mode(kit);
    // Front badges have been separated from torso groups into group 7.
    // Keep them enabled for original kits, disabled for crest_mode=2 kits.
    // Native isolated front: LOD0 only; adapted 22/25: LOD0 and LOD1.
    // Other LODs retain original hidden group 7; never force rear packets on.
    const bool show=pes6::crest::Visible(base[kit],crestMode,legacy);
    const bool adapted=(base[kit]==22||base[kit]==25);
    ctx=saved;ctx.r3.u64=object;ctx.r4.u64=7;ctx.r5.u64=(show&&adapted)?0x40000011u:0x40000000u;
    sub_821A6F78(ctx,base);
  }
  // Only switch the torso. Groups 8/9 are optional kit decals, NOT required
  // companions of the tucked/untucked body. Forcing them on renders unused
  // crest geometry around the waist with a kit-dependent texture/material.
  // Keep the native decal/name selection computed by sub_822F2C10 intact.
  // Setter updates each mesh instance (all LOD renderables), not shared KTMDL.
  for(const auto& [group,enabled]:std::initializer_list<std::pair<uint32_t,bool>>{
    {34,!outside},{35,outside}}){
    ctx=saved;ctx.r3.u64=object;ctx.r4.u64=group;
    ctx.r5.u64=enabled?0x40001111u:0x40000000u;
    sub_821A6F78(ctx,base);
  }
  ctx=saved;
}
extern "C" void pes6_editor_selected(uint8_t* base) {
  const uint32_t id=REX_LOAD_U32(0x831398A4u)&0xffffu;
  const uint32_t db=Database(base);
  if(!db||!id||id>=4210||Name(base,db+152*id).empty())return;
  native_player.store(uint16_t(id));
  std::function<void(uint16_t)> callback;
  {std::lock_guard lock(mutex);callback=open_callback;}
  if(callback)callback(uint16_t(id));
}
}  // namespace pes6::editor
