#pragma once
// Included inside game_weather's namespace after checked guest access helpers.
struct PlayerSurfaceVertex {Vec p{},n{};std::array<unsigned,4> bone{};std::array<float,4> weight{};Vec uv{};bool has_uv=false,sleeve_motion=false;};
struct PlayerSurfaceSample {std::array<PlayerSurfaceVertex,3> triangle;Vec bary{};float size=0;unsigned region=1,group=0,selector=0;bool cloth_hem=false,print_overlay=false;};
using FaceGrid=std::array<std::vector<unsigned>,256>;
struct PlayerSurfaceModel {bool face=false,pants=false,hair=false;unsigned bones=0,resident=0,size=0,rejection=1,groups=0,selectors=0;uint64_t revision=0;std::vector<PlayerSurfaceSample> samples,skin;std::vector<unsigned> dirt_groups,dirt_selectors;std::shared_ptr<FaceGrid> face_grid;std::array<float,13> hairline{},forehead{};float eye_top=0;};
bool GarmentCompatible(unsigned sample,unsigned patch){
  return sample==patch||(sample<2&&patch<2)||
    (sample<2&&patch>=4&&patch<6)||(patch<2&&sample>=4&&sample<6);
}
struct DirtPatch {Vec center{},normal{};float strength=0;unsigned region=1;uint32_t event=0;float contact_score=1e9f;uint32_t birth_tick=0;Vec uv{};uint64_t source_model=0;bool has_uv=false,sleeve_motion=false;};
constexpr unsigned dirt_region_count=10,dirt_patch_limit=30;
constexpr std::array<unsigned,dirt_region_count> dirt_region_limits{5,6,2,2,1,1,2,2,1,1};
struct DirtClimate {float shirt_width,pants_width,shirt_height,pants_height,shirt_low,shirt_high,pants_low,pants_high;};
constexpr std::array<DirtClimate,3> dirt_climates{{
  {.15f,.12f,.20f,.14f,.25f,.30f,.25f,.30f},
  {.15f,.16f,.20f,.20f,.30f,.36f,.30f,.36f},
  {.20f,.20f,.34f,.26f,.34f,.42f,.34f,.42f}
}};
Vec DirtRadii(unsigned climate,unsigned region){
  const auto& c=dirt_climates[std::min(climate,2u)];
  const bool shirt=region==0||region==4||region==5;
  const float smaller=region>=2?.65f:1.f;
  const float width=shirt?c.shirt_width:c.pants_width;
  return {width*.5f*smaller,(shirt?c.shirt_height:c.pants_height)*.5f*smaller,width*.5f*smaller};
}
struct DirtPatches {
  std::shared_ptr<std::vector<DirtPatch>> data;
  void Mutate(){if(!data)data=std::make_shared<std::vector<DirtPatch>>();else if(data.use_count()!=1)data=std::make_shared<std::vector<DirtPatch>>(*data);}
  size_t size()const{return data?data->size():0;}
  const DirtPatch& operator[](size_t i)const{return (*data)[i];}
  DirtPatch& operator[](size_t i){Mutate();return (*data)[i];}
  void push_back(DirtPatch patch){Mutate();data->push_back(patch);}
  void resize(size_t n){Mutate();data->resize(n);}
};
struct PlayerDirt {Vec position{};float amount=0;uint32_t input=0;DirtPatches patches{};unsigned patch_count=0;bool pending=false;uint32_t contact_tick=0,actor=0,identity=0,last_contact_scan=~0u;};
bool OuterGarmentContact(Vec center,Vec normal,unsigned region){
  if(region!=1)return true;
  // Never seed a smear on the crotch or the inward side of a trouser leg.
  const bool seat=normal.z<-.35f&&center.z<-.025f&&center.y>.82f;
  const bool outer_thigh=std::abs(center.x)>.065f&&center.x*normal.x>.018f;
  const bool front_thigh=normal.z>.35f&&std::abs(center.x)>.065f;
  return seat||outer_thigh||front_thigh;
}
float DirtFieldPrepared(const DirtPatch& patch,Vec p,Vec normal,unsigned region,unsigned climate,float grain_a,float grain_b){
  const float facing=normal.x*patch.normal.x+normal.y*patch.normal.y+normal.z*patch.normal.z;
  if(!GarmentCompatible(region,patch.region)||facing<=.05f)return 0;
  const float wrap=std::clamp((facing-.05f)/.4f,0.f,1.f);
  const auto radii=DirtRadii(climate,patch.region);
  const float x=(p.x-patch.center.x)/radii.x,y=(p.y-patch.center.y)/radii.y,z=(p.z-patch.center.z)/radii.z;
  const float raw_r=std::sqrt(x*x+y*y+z*z);if(raw_r>=1.f)return 0;
  // Move the contour itself, not just its opacity. A smear ending across the
  // back then has a feathered, uneven outline instead of a horizontal cut.
  const float r=raw_r+(patch.region==0?.07f:.04f)*grain_a+.025f*grain_b;
  if(r>=1)return 0;
  const float fade=std::clamp((1-r)/.82f,0.f,1.f);
  const float edge=fade*fade*(3-2*fade);
  const auto smooth=[](float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);};
  const float garment=(patch.region==2||patch.region==3)?smooth((.455f-p.y)/.055f)*smooth((p.y-.16f)/.04f):
    patch.region>=8?smooth((p.y-.455f)/.025f)*smooth((.80f-p.y)/.04f):1.f;
  const auto& c=dirt_climates[std::min(climate,2u)];
  const bool shirt=patch.region==0||patch.region==4||patch.region==5;
  const float texture=std::clamp(.5f+.33f*grain_a+.17f*grain_b,0.f,1.f);
  const float opacity=shirt?c.shirt_low+(c.shirt_high-c.shirt_low)*texture:c.pants_low+(c.pants_high-c.pants_low)*texture;
  return std::min(1.f,patch.strength/.78f)*edge*opacity*garment*wrap*wrap*(3-2*wrap);
}
float DirtField(const DirtPatch& patch,Vec p,Vec normal,unsigned region,unsigned climate=1){
  return DirtFieldPrepared(patch,p,normal,region,climate,std::sin(p.x*137+p.y*83+p.z*61),std::sin(p.y*293-p.x*181));
}
void AddDirtPatch(PlayerDirt& dirt,DirtPatch patch){
  if(patch.region>=dirt_region_count)return;
  unsigned regional=0;
  for(unsigned i=0;i<dirt.patch_count;i++){
    const auto& old=std::as_const(dirt.patches)[i];if(old.region!=patch.region)continue;
    ++regional;
    // One contact seeds one mark per body/garment region, not one per pose
    // during the twelve ticks spent touching the ground.
    if(patch.event&&old.event==patch.event)return;
    const auto delta=Vec{old.center.x-patch.center.x,old.center.y-patch.center.y,old.center.z-patch.center.z};
    if(std::hypot(delta.x,delta.y,delta.z)<.045f&&old.normal.x*patch.normal.x+old.normal.y*patch.normal.y+old.normal.z*patch.normal.z>.8f){
      if(patch.event&&patch.event==old.event)return;
      auto& writable=dirt.patches[i];
      if(!patch.event||patch.event!=writable.event)writable.strength=std::min(1.f,writable.strength+.22f);
      writable.event=patch.event;return;
    }
  }
  // A completely saturated kit keeps every existing mark; no older patch is
  // evicted just to make room for a later fall.
  if(dirt.patch_count>=dirt_patch_limit||regional>=dirt_region_limits[patch.region])return;
  if(dirt.patch_count<dirt.patches.size())dirt.patches[dirt.patch_count++]=patch;
  else {dirt.patches.push_back(patch);++dirt.patch_count;}
}
void MergePlayerDirt(PlayerDirt& target,const PlayerDirt& source){
  target.amount=std::max(target.amount,source.amount);
  if(target.patches.data==source.patches.data)return;
  for(unsigned i=0;i<source.patch_count;i++){
    const auto& mark=source.patches[i];unsigned matching=target.patch_count;
    for(unsigned j=0;j<target.patch_count;j++){
      const auto& old=std::as_const(target.patches)[j];
      if(old.region==mark.region&&std::hypot(old.center.x-mark.center.x,old.center.y-mark.center.y,old.center.z-mark.center.z)<.015f&&
         old.normal.x*mark.normal.x+old.normal.y*mark.normal.y+old.normal.z*mark.normal.z>.95f){matching=j;break;}
    }
    if(matching==target.patch_count)AddDirtPatch(target,mark);
    else if(mark.strength>std::as_const(target.patches)[matching].strength)target.patches[matching].strength=mark.strength;
  }
}
struct AppearanceFrame {float minute=0,water_time=0;unsigned period=0;uint32_t tick=0;std::array<PlayerDirt,64> players{};};
struct PlayerSurfaceInstance {uint32_t address=0,input=0,data=0,view=0;bool face=false;float time=-1000;std::vector<ParticleMesh> meshes;};
std::unordered_map<uint32_t,PlayerSurfaceModel> player_surface_models;
// Resource ownership survives renderer rebuilds. Capture samples before native
// GPU preparation changes/releases the original CPU streams.
std::unordered_map<uint32_t,PlayerSurfaceModel> player_surface_sources;
std::atomic<uint32_t> player_surface_source_epoch{1};
std::array<PlayerSurfaceInstance,256> player_surface_instances;
struct HairlineBinding {uint32_t input=0,epoch=0;float time=-1000;Vec root{};std::array<float,13> line{};};
std::array<HairlineBinding,64> player_hairlines;
std::array<PlayerDirt,64> player_dirt{};
std::unordered_map<uint32_t,PlayerDirt> player_dirt_history;
std::unordered_map<uint32_t,PlayerDirt> player_dirt_identities;
std::unordered_map<uint32_t,uint32_t> player_input_identities;
uint64_t surface_capture_us=0,surface_capture_max_us=0,surface_capture_calls=0;uint32_t surface_capture_max_input=0;
uint64_t dirt_bake_hits=0,dirt_bake_misses=0,dirt_bake_us=0;
extern float appearance_minute;extern unsigned appearance_period;
bool PlayerSurfaceDiagnosticsEnabled(){
#ifdef PES6_PLAYER_SURFACE_FIXTURE
  return true;
#else
  static const bool enabled=[](){const auto* v=std::getenv("PES6_PLAYER_SURFACE_DIAGNOSTICS");return v&&v[0]=='1'&&!v[1];}();
  return enabled;
#endif
}
std::filesystem::path SurfaceTracePath(){
  static const auto path=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);
    auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<7&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();
    return root/"data/diagnostics/player-surface-events.csv";}();return path;
}
void SurfaceTrace(const char* event,uint32_t slot,const PlayerDirt& before,const PlayerDirt& after){
#ifndef PES6_PLAYER_SURFACE_FIXTURE
  if(!PlayerSurfaceDiagnosticsEnabled())return;
  static bool ready=[](){std::error_code ec;std::filesystem::create_directories(SurfaceTracePath().parent_path(),ec);return !ec;}();
  if(!ready)return;std::ofstream out(SurfaceTracePath(),std::ios::app);if(!out)return;
  out<<event<<','<<motion_clock.load(std::memory_order_relaxed)<<','<<appearance_period<<','<<appearance_minute<<','<<slot<<','
     <<std::hex<<before.actor<<','<<after.actor<<std::dec<<','<<before.identity<<','<<after.identity<<','<<before.patch_count<<','<<after.patch_count<<'\n';
#endif
}
uint32_t PlayerRecordIdentity(uint8_t* base,uint32_t actor){
  if(!Valid(base,actor,1358))return 0;
  const uint32_t record=REX_LOAD_U32(actor+1352),database=Pointer(base,0x83139440u);
  if(database&&record>=database+50){const auto offset=record-database-50;
    if(offset%152==0&&offset/152<10000)return offset/152+1;}
  // Match actors usually point to a copied appearance, so their record is
  // outside the database. Kit team + shirt number survive actor recreation.
  const auto kit=Pointer(base,actor+1348);
  if(!Valid(base,kit,16))return 0;
  const uint32_t team=REX_LOAD_U32(kit+12),number=base[actor+1357];
  return team<256&&number<100?0x10000000u|(team<<8)|number:0;
}
void RememberPlayerDirt(const PlayerDirt& dirt){
  if(!dirt.actor)return;
  const auto keep=[&](auto& history,uint32_t key){
    const auto found=history.find(key);
    if(found==history.end())history.emplace(key,dirt);
    else if(found->second.identity&&dirt.identity&&found->second.identity!=dirt.identity)found->second=dirt;
    else MergePlayerDirt(found->second,dirt);
  };
  keep(player_dirt_history,dirt.actor);
  if(dirt.identity)keep(player_dirt_identities,dirt.identity);
}
void RememberLivePlayerDirt(){for(const auto& dirt:player_dirt)RememberPlayerDirt(dirt);}
void MergeVisibleReplayDirt(PlayerDirt& target,const PlayerDirt& source,uint32_t replay_tick){
  if(target.patches.data==source.patches.data&&target.patch_count==source.patch_count)return;
  for(unsigned i=0;i<source.patch_count;i++){
    const auto& mark=source.patches[i];
    if(!mark.birth_tick||mark.birth_tick>replay_tick)continue;
    bool present=false;
    for(unsigned j=0;j<target.patch_count;j++)if(std::as_const(target.patches)[j].event==mark.event&&mark.event&&
      std::as_const(target.patches)[j].region==mark.region){present=true;break;}
    if(!present)AddDirtPatch(target,mark);
  }
}
PlayerDirt& PlayerDirtForActor(unsigned slot,uint32_t actor,uint32_t identity=0){
  auto& dirt=player_dirt[slot];if(dirt.actor!=actor){
    const auto before=dirt;
    RememberPlayerDirt(dirt);
    for(unsigned i=0;i<player_dirt.size();++i)if(i!=slot&&
      ((player_dirt[i].actor==actor&&(!identity||!player_dirt[i].identity||player_dirt[i].identity==identity))||
       (identity&&player_dirt[i].identity==identity))){
      dirt=std::move(player_dirt[i]);player_dirt[i]={};
      if(identity)if(const auto found=player_dirt_identities.find(identity);found!=player_dirt_identities.end())MergePlayerDirt(dirt,found->second);
      dirt.actor=actor;dirt.identity=identity;SurfaceTrace("move",slot,before,dirt);return dirt;
    }
    const auto by_id=identity?player_dirt_identities.find(identity):player_dirt_identities.end();
    if(by_id!=player_dirt_identities.end())dirt=by_id->second;
    else dirt={};
    if(const auto found=player_dirt_history.find(actor);found!=player_dirt_history.end()&&
       (!identity||!found->second.identity||found->second.identity==identity))MergePlayerDirt(dirt,found->second);
    dirt.actor=actor;dirt.identity=identity;SurfaceTrace("replace",slot,before,dirt);
  }else if(identity&&dirt.identity!=identity){
    const auto before=dirt;
    RememberPlayerDirt(dirt);
    const auto found=player_dirt_identities.find(identity);dirt=found==player_dirt_identities.end()?PlayerDirt{}:found->second;
    dirt.actor=actor;dirt.identity=identity;SurfaceTrace("identity",slot,before,dirt);
  }return dirt;
}
std::shared_ptr<AppearanceFrame> replay_appearance;
float appearance_minute=0;unsigned appearance_period=0;uint32_t appearance_game=0;
bool appearance_finished=false;unsigned appearance_new_match_ticks=0;
uint32_t player_surface_capture_epoch=0;
uint32_t player_surface_view_epoch=0;
Vec player_surface_camera{};bool player_surface_camera_ready=false;
struct ReplayDirtBinding {uint32_t input=0,actor=0;unsigned index=64;};
std::array<ReplayDirtBinding,64> replay_dirt_bindings{};
void ResetReplayDirtBindings(){replay_dirt_bindings={};}
unsigned ResolvePlayerDirt(const std::array<PlayerDirt,64>& dirt,uint32_t input,Vec root,bool replay){
  // Close-up/replay wrappers can have a different input pointer. Prefer the
  // stable player identity over a previous spatial binding when it is known.
  if(const auto known=player_input_identities.find(input);known!=player_input_identities.end())
    for(unsigned i=0;i<dirt.size();i++)if(dirt[i].identity==known->second)return i;
  if(!replay)for(unsigned i=0;i<dirt.size();i++)if(input&&dirt[i].input==input)return i;
  if(replay)for(const auto& b:replay_dirt_bindings)if(b.input==input){
    if(b.actor){for(unsigned i=0;i<dirt.size();i++)if(dirt[i].actor==b.actor)return i;}
    else if(b.index<dirt.size())return b.index;
  }
  unsigned selected=64;float best=35.f;
  for(unsigned i=0;i<dirt.size();i++){const auto& d=dirt[i];if(!d.input&&!d.actor&&d.amount<=0)continue;
    const float distance=std::hypot(root.x-d.position.x,root.z-d.position.z);if(distance<best){best=distance;selected=i;}
  }
  if(replay&&selected<64){ReplayDirtBinding* free=&replay_dirt_bindings[(input>>4)%64];for(auto& b:replay_dirt_bindings)if(!b.input){free=&b;break;}*free={input,dirt[selected].actor,selected};}
  return selected;
}
float WaterFresnel(float cosine){return .0204f+.9796f*std::pow(1-std::clamp(cosine,0.f,1.f),5.f);}
ParticleMesh WaterBead(Vec center,Vec normal,float radius,Vec eye){
  ParticleMesh mesh{};mesh.count=54;normal=Unit(normal);
  const auto side=Unit(Cross(normal,std::abs(normal.y)<.9f?Vec{0,1,0}:Vec{1,0,0})),up=Cross(normal,side);
  const Vec view=Unit({eye.x-center.x,eye.y-center.y,eye.z-center.z});
  static const Vec light=Unit({-.35f,.8f,.45f});
  const Vec halfway=Unit(Add(view,light));
  const auto shade=[&](Vec n){
    const float facing=std::max(0.f,n.x*view.x+n.y*view.y+n.z*view.z),fresnel=WaterFresnel(facing);
    const float spec=std::pow(std::max(0.f,n.x*halfway.x+n.y*halfway.y+n.z*halfway.z),24.f);
    const float sky=.5f+.5f*n.y;
    const auto channel=[&](float base,float reflection){return uint32_t(std::clamp(base+reflection*fresnel*sky+spec*190,0.f,245.f));};
    const auto alpha=uint32_t(std::clamp(4+fresnel*35+spec*190,0.f,220.f));
    return(alpha<<24)|(channel(110,75)<<16)|(channel(130,80)<<8)|channel(142,85);
  };
  const MeshVertex tip{Add(center,Mul(normal,radius*.72f)),shade(normal)};
  std::array<MeshVertex,6> inner,outer;
  static const auto ring=[](){std::array<std::array<float,2>,6> result{};
    for(unsigned i=0;i<6;i++){const float angle=float(i)*1.047197551f;result[i]={std::cos(angle),std::sin(angle)*1.25f};}
    return result;
  }();
  for(unsigned i=0;i<6;i++){
    const auto radial=Add(Mul(side,ring[i][0]),Mul(up,ring[i][1])),radial_unit=Unit(radial);
    inner[i]={Add(center,Add(Mul(radial,radius*.55f),Mul(normal,radius*.5f))),shade(Unit(Add(Mul(radial_unit,.6f),Mul(normal,.8f))))};
    outer[i]={Add(center,Mul(radial,radius)),shade(Unit(Add(radial_unit,Mul(normal,.16f))))};
  }
  for(unsigned i=0;i<6;i++){const auto next=(i+1)%6;const unsigned at=i*9;
    mesh.vertices[at]=tip;mesh.vertices[at+1]=inner[i];mesh.vertices[at+2]=inner[next];
    mesh.vertices[at+3]=inner[i];mesh.vertices[at+4]=outer[i];mesh.vertices[at+5]=outer[next];
    mesh.vertices[at+6]=inner[i];mesh.vertices[at+7]=outer[next];mesh.vertices[at+8]=inner[next];
  }return mesh;
}
struct PlayerSurfaceDiagnostics {uint64_t pose=0,draw=0,models=0,empty=0,outside=0,faces=0,pants=0,slides=0,vertices=0,loads=0,loaded_models=0,material_updates=0;Vec last_root{};uint32_t last_data=0,last_magic=0,last_bones=0;} player_surface_diagnostics;
void WritePlayerSurfaceDiagnostics(){
#ifdef PES6_PLAYER_SURFACE_FIXTURE
  return;
#endif
  if(!PlayerSurfaceDiagnosticsEnabled())return;
  static uint32_t trace_previous=~0u;
  const auto trace_tick=motion_clock.load(std::memory_order_relaxed);
  if(trace_previous==~0u||trace_tick-trace_previous>=300){
    trace_previous=trace_tick;
    unsigned dirty=0,marks=0;for(const auto& p:player_dirt)if(p.patch_count){++dirty;marks+=p.patch_count;}
    std::error_code ec;std::filesystem::create_directories(SurfaceTracePath().parent_path(),ec);
    if(!ec){std::ofstream trace(SurfaceTracePath(),std::ios::app);
      if(trace)trace<<"perf,"<<trace_tick<<','<<appearance_period<<','<<appearance_minute<<",dirty,"<<dirty<<",marks,"<<marks
                    <<",captures,"<<surface_capture_calls<<",capture_us,"<<surface_capture_us<<",max_capture_us,"<<surface_capture_max_us
                    <<",max_input,"<<std::hex<<surface_capture_max_input<<std::dec<<",bake_hits,"<<dirt_bake_hits<<",bake_misses,"<<dirt_bake_misses<<",bake_us,"<<dirt_bake_us<<'\n';}
    surface_capture_calls=surface_capture_us=surface_capture_max_us=0;surface_capture_max_input=0;
    dirt_bake_hits=dirt_bake_misses=dirt_bake_us=0;
  }
  static const bool enabled=[](){const auto* value=std::getenv("PES6_PLAYER_SURFACE_DIAGNOSTICS");return value&&value[0]=='1'&&!value[1];}();
  if(!enabled)return;
  static const auto path=[](){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<7&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root/"data/diagnostics/player-surfaces.txt";}();
  static uint32_t previous=~0u;const auto tick=motion_clock.load(std::memory_order_relaxed);
  if(previous!=~0u&&tick-previous<300)return;previous=tick;
  std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);std::ofstream out(path,std::ios::trunc);if(!out)return;
  const auto& d=player_surface_diagnostics;
  out<<"version=13\nticks="<<tick<<"\npose_callbacks="<<d.pose<<"\ndraw_callbacks="<<d.draw<<"\nmodels="<<d.models<<"\nempty_models="<<d.empty<<"\nworld_rejected="<<d.outside<<"\nface_meshes="<<d.faces<<"\ndirt_meshes="<<d.pants<<"\nslide_contacts="<<d.slides<<"\nsubmitted_vertices="<<d.vertices<<"\nlast_root="<<d.last_root.x<<','<<d.last_root.y<<','<<d.last_root.z<<"\nlast_data="<<std::hex<<d.last_data<<"\nlast_magic="<<d.last_magic<<std::dec<<"\nlast_bones="<<d.last_bones<<'\n';
  for(const auto& [address,m]:player_surface_models)out<<"model="<<std::hex<<address<<std::dec<<",bones="<<m.bones<<",resident="<<m.resident<<",size="<<m.size<<",face="<<m.face<<",pants="<<m.pants<<",samples="<<m.samples.size()<<",rejection="<<m.rejection<<'\n';
  out<<"source_loads="<<d.loads<<"\nsource_models="<<d.loaded_models<<"\nsource_owners="<<player_surface_sources.size()<<'\n';
  out<<"native_skin_material_updates="<<d.material_updates<<'\n';
}
float PlayerSweat(float minute,unsigned period){
  // Sweat accumulated during the match survives half-time, stoppage time
  // and the walk-off cinematics. Period changes are not a shower/reset.
  const float m=std::max(0.f,minute);
  if(m<5)return 0;
  if(m<10)return .20f+(m-5)*.02f;
  if(m<20)return .30f+(m-10)*.02f;
  if(m<30)return .50f+(m-20)*.03f;
  return std::min(1.f,.80f+(m-30)*.04f);
}
void PlayerAppearanceClock(uint8_t* base){
  const auto game=Pointer(base,0x8313A948u);if(!Valid(base,game+2064,44))return;
  const auto period=base[game+2072];if(period>3)return;
  // Exact native 82500630 conversion: match ticks * rate / configured duration.
  const unsigned duration=base[game+2104];if(!duration)return;
  float minute=base[game+2083];
  const auto start=REX_LOAD_U32(game+2076),now=REX_LOAD_U32(game+2084);
  const float rate=Float(base,game+2088);
  const bool running_clock=!base[game+2082]&&start&&now>=start&&std::isfinite(rate)&&rate>=0;
  if(running_clock)
    minute+=float(now-start)*rate/(duration*60.f);
  else {constexpr float end[]{45,90,105,120},begin[]{0,45,90,105};minute=start?end[period]:begin[period];}
  if(!std::isfinite(minute)||minute<0||minute>150)return;
  // Session lifetime is controlled by the frontend, never by a reset clock.
  if(period!=appearance_period){
    for(unsigned i=0;i<player_dirt.size();i++)if(player_dirt[i].patch_count)SurfaceTrace("period",i,player_dirt[i],player_dirt[i]);
  }
  // Native walk-off/half-time transitions can also change the period or
  // recreate the clock. Only the explicit new-match reset clears this peak.
  if(appearance_game)minute=std::max(minute,appearance_minute);
  appearance_game=game;
  appearance_period=period;appearance_minute=minute;
}
Vec PlayerMatrixPoint(uint8_t* base,uint32_t matrices,unsigned bone,Vec p){
  const auto at=matrices+bone*64;
  return {Float(base,at)*p.x+Float(base,at+16)*p.y+Float(base,at+32)*p.z+Float(base,at+48),
    Float(base,at+4)*p.x+Float(base,at+20)*p.y+Float(base,at+36)*p.z+Float(base,at+52),
    Float(base,at+8)*p.x+Float(base,at+24)*p.y+Float(base,at+40)*p.z+Float(base,at+56)};
}
Vec PlayerSkin(uint8_t* base,uint32_t matrices,const PlayerSurfaceVertex& v){
  Vec out{};for(unsigned i=0;i<4;i++)if(v.weight[i]>0)out=Add(out,Mul(PlayerMatrixPoint(base,matrices,v.bone[i],v.p),v.weight[i]));return out;
}
uint64_t PlayerGroupName(uint8_t* base,uint32_t data,uint32_t packet){
  const auto group=REX_LOAD_U32(packet+8),offset=REX_LOAD_U32(data+68);
  if(group>128||!Valid(base,uint32_t(uint64_t(data)+offset+group*48),8))return 0;
  const auto at=data+offset+group*48;
  return (uint64_t(REX_LOAD_U32(at))<<32)|REX_LOAD_U32(at+4);
}
PlayerSurfaceModel BuildPlayerSurfaceModel(uint8_t* base,uint32_t data){
  PlayerSurfaceModel result;
  static uint64_t revision=0;result.revision=++revision;
  if(!Valid(base,data,148)||REX_LOAD_U32(data)!=0x4b544d44u)return result;
  const unsigned bones=REX_LOAD_U32(data+24),size=REX_LOAD_U32(data+144),resident=REX_LOAD_U32(data+120);
  result.bones=bones;result.size=size;result.resident=resident;result.rejection=2;
  // Native82201370 keeps only the resident prefix here and relocates stream
  // payloads into a separate allocation. Never require the file-size span.
  if(!bones||bones>50||size<192||size>4*1024*1024||resident<148||resident>size||!Valid(base,data,resident))return result;
  result.pants=bones==50;result.rejection=3;
  result.groups=REX_LOAD_U32(data+64);
  const auto strings=REX_LOAD_U32(data+96),vertices=REX_LOAD_U32(data+120);
  if(bones<=24&&strings<vertices&&vertices<=size&&vertices-strings<16384){
    const std::string_view names(reinterpret_cast<char*>(base+data+strings),vertices-strings);
    // head_base alone is the BACK of the head. The separate animated facial
    // model has19 bones and f_<player> / face materials.
    result.hair=bones<=8&&(names.find("h_")!=names.npos||names.find("hair")!=names.npos||names.find("head_base")!=names.npos);
    result.face=!result.hair&&(names.find("f_")!=names.npos||names.find("face")!=names.npos);
  }
  if(!result.pants&&!result.face&&!result.hair)return result;
  const auto packets=REX_LOAD_U32(data+40),offset=REX_LOAD_U32(data+44);
  result.rejection=4;if(packets>256||uint64_t(offset)+packets*96>resident)return result;
  bool hair_overlay=false;
  if(result.hair)for(unsigned i=0;i<packets;i++)hair_overlay|=(REX_LOAD_U32(data+offset+i*96+4)&0xffffu)!=0;
  const auto inside=[&](uint32_t at,size_t bytes){return Valid(base,at,bytes);};
  for(unsigned packet_index=0;packet_index<packets;packet_index++){
    const auto packet=data+offset+packet_index*96;
    const auto name=PlayerGroupName(base,data,packet);
    // Six-bit native names, compare decoded group strings to avoid skin/printing.
    char group[11]{};for(unsigned i=0;i<10;i++){const auto c=unsigned(name>>(54-i*6))&63;group[i]=c?char(c<27?c+96:c+20):0;}
    const bool shirt_group=std::string_view(group)=="bshirtin"||std::string_view(group)=="bshirtout";
    const bool sleeve_group=std::string_view(group)=="as1"||std::string_view(group)=="as1c"||std::string_view(group)=="as2"||std::string_view(group)=="as2c";
    const std::string_view group_name(group);
    const bool number_group=group_name.starts_with("aname")||group_name.starts_with("asebango")||
      group_name.starts_with("amark")||group_name.starts_with("amunenum")||group_name.starts_with("amomo");
    const bool collar_group=group_name=="deri1"||group_name=="deri2";
    const bool sock_group=std::string_view(group).starts_with("afoot");
    const bool arm_group=group_name=="darm2"||group_name=="darm3";
    if(result.pants&&!shirt_group&&!sock_group&&!sleeve_group&&!number_group&&!collar_group&&!arm_group)continue;
    if(result.face&&std::string_view(group)!="model")continue;
    if(result.hair&&(std::string_view(group)!="model"||(hair_overlay&&(REX_LOAD_U32(packet+4)&0xffffu)==0)))continue;
    const auto palette=packet+REX_LOAD_U32(packet+28),palette_count=REX_LOAD_U32(packet+24);
    if(!palette_count||palette_count>50||!inside(palette,palette_count*2))continue;
    uint32_t stream=0,index_stream=0;
    for(unsigned rel:{20u,36u}){
      const unsigned count=REX_LOAD_U32(packet+rel-4);const auto first=packet+REX_LOAD_U32(packet+rel);
      if(count>8||!inside(first,count*32))continue;
      for(unsigned k=0;k<count;k++){const auto s=first+k*32,type=REX_LOAD_U32(s+20);if(type==0)stream=s;else if(type==1)index_stream=s;}
    }
    if(!stream||!index_stream)continue;
    const auto vb=stream+REX_LOAD_U32(stream),ib=index_stream+REX_LOAD_U32(index_stream);
    const unsigned stride=base[stream+9],count=REX_LOAD_U32(stream+4),indices=REX_LOAD_U32(index_stream+4);
    if(!stride||count>8192||indices>32768||!inside(vb,size_t(count)*stride)||!inside(ib,size_t(indices)*2))continue;
    const auto declaration=stream+REX_LOAD_U32(stream+12);const unsigned elements=base[stream+10];
    if(elements>16||!inside(declaration,elements*4))continue;
    int pos=-1,normal=-1,weights=-1,bone_indices=-1,uv=-1;unsigned weight_count=0;
    for(unsigned i=0;i<elements;i++){
      const auto e=declaration+i*4;const unsigned semantic=base[e+3],type=base[e+2],at=base[e+1];
      if(semantic==16&&type==2&&at+12<=stride)pos=at;
      if(semantic==18&&type==2&&at+12<=stride)normal=at;
      if(semantic==32&&type<=2&&at+(type+1)*4<=stride){weights=at;weight_count=type+1;}
      if(semantic==33&&type==11&&at+4<=stride)bone_indices=at;
      if(semantic==22&&type==1&&at+8<=stride)uv=at;
    }
    if(pos<0||normal<0||bone_indices<0)continue;
    const auto vertex=[&](unsigned index){
      PlayerSurfaceVertex v;v.sleeve_motion=((REX_LOAD_U32(packet+8)==24||REX_LOAD_U32(packet+8)==25)&&palette_count==6)||((REX_LOAD_U32(packet+8)==36||REX_LOAD_U32(packet+8)==37)&&palette_count==13);const auto at=vb+index*stride;
      v.p=Position(base,at+pos);v.n=Position(base,at+normal);
      if(uv>=0){v.uv={Float(base,at+uv),Float(base,at+uv+4),0};v.has_uv=std::isfinite(v.uv.x)&&std::isfinite(v.uv.y);}
      // Low-detail rigid packets omit explicit weights: their sole palette
      // index carries weight 1, just as in the native skinning path.
      float total=0;for(unsigned i=0;weights>=0&&i<weight_count;i++){v.weight[i+1]=Float(base,at+weights+i*4);total+=v.weight[i+1];}v.weight[0]=1-total;
      for(unsigned i=0;i<=weight_count;i++){const unsigned b=base[at+bone_indices+3-i];if(b>=palette_count){v.weight={};break;}v.bone[i]=REX_LOAD_U16(palette+b*2);if(v.bone[i]>=bones||!std::isfinite(v.weight[i])||v.weight[i]<-.001f){v.weight={};break;}}
      return v;
    };
    bool torso_stream=false;
    if(shirt_group)for(unsigned i=0;i<count&&!torso_stream;++i)torso_stream=vertex(i).p.y>1.16f;
    std::array<unsigned,3> last{};unsigned run=0,eligible=0;
    for(unsigned i=0;i<indices;i++){
      const unsigned ix=REX_LOAD_U16(ib+i*2);if(ix==65535||ix>=count){run=0;continue;}
      last[run%3]=ix;run++;const unsigned primitive=base[packet+2];
      if(run<3||(primitive==1&&run%3))continue;
      const std::array<unsigned,3> ids{last[(run-3)%3],last[(run-2)%3],last[(run-1)%3]};
      if(ids[0]==ids[1]||ids[0]==ids[2]||ids[1]==ids[2])continue;
      std::array<PlayerSurfaceVertex,3> tri{vertex(ids[0]),vertex(ids[1]),vertex(ids[2])};
      bool good=true;for(const auto& v:tri){float weight=0;for(float w:v.weight)weight+=w;
        good&=std::isfinite(v.p.x)&&std::isfinite(v.p.y)&&std::isfinite(v.p.z)&&std::abs(v.p.x)<5&&std::abs(v.p.y)<5&&std::abs(v.p.z)<5&&std::isfinite(v.n.x)&&std::isfinite(v.n.y)&&std::isfinite(v.n.z)&&std::abs(weight-1)<.01f;}
      if(!good)continue;
      const Vec center=Mul(Add(Add(tri[0].p,tri[1].p),tri[2].p),1.f/3);
      const Vec n=Unit(Add(Add(tri[0].n,tri[1].n),tri[2].n));
      if(result.face&&(REX_LOAD_U32(packet+4)&0xffffu)!=0){
        if((REX_LOAD_U32(packet+4)&0xffffu)==1)
          for(const auto& v:tri)if(std::abs(v.p.x)>.012f&&std::abs(v.p.x)<.060f&&v.p.y>.065f&&v.p.y<.13f)
            result.eye_top=std::max(result.eye_top,v.p.y);
        continue; // Eye geometry supplies the brow limit, never receives droplets.
      }
      if(result.face&&n.z>.25f){
        for(unsigned lane=0;lane<result.forehead.size();lane++){
          const float x=(int(lane)-6)*.01f;
          for(unsigned e=0;e<3;e++){const auto a=tri[e].p,b=tri[(e+1)%3].p;
            if(x<std::min(a.x,b.x)||x>std::max(a.x,b.x)||std::abs(b.x-a.x)<1e-7f)continue;
            const float t=(x-a.x)/(b.x-a.x),y=a.y+(b.y-a.y)*t,z=a.z+(b.z-a.z)*t;
            if(y>.105f&&y<.225f&&z>.025f)result.forehead[lane]=std::max(result.forehead[lane],y);
          }
        }
      }
      if(result.hair){
        for(unsigned lane=0;lane<result.hairline.size();lane++){
          const float x=(int(lane)-6)*.01f;
          for(unsigned e=0;e<3;e++){const auto a=tri[e].p,b=tri[(e+1)%3].p;
            if(x<std::min(a.x,b.x)||x>std::max(a.x,b.x)||std::abs(b.x-a.x)<1e-7f)continue;
            const float t=(x-a.x)/(b.x-a.x),y=a.y+(b.y-a.y)*t,z=a.z+(b.z-a.z)*t;
            if(y<.095f||y>.205f||z<.025f)continue;
            auto& line=result.hairline[lane];if(!line||y<line)line=y;
          }
        }
      }
      unsigned region=1;
      if(result.pants){
        if(arm_group){if(std::abs(center.x)<.15f||center.y<1.05f||center.y>1.58f)continue;region=center.x<0?6:7;}
        else if(sleeve_group){region=center.x<0?4:5;}
        else if(shirt_group){if(center.y<.64f||center.y>1.56f)continue;region=torso_stream?0:1;}
        else if(number_group){if(center.y<.62f||center.y>1.56f)continue;region=center.y>1.06f?0:1;}
        else if(collar_group){if(center.y<1.3f||center.y>1.59f)continue;region=0;}
        else {if(center.y<.16f||center.y>.80f)continue;region=center.y>.455f?(center.x<0?8:9):(center.x<0?2:3);}
      }
      // Positive local Z is the facial side of these native heads; negative Z
      // is the back of the skull (the old selection put white beads in hair).
      if(result.face&&(center.y<.035f||center.y>.225f||n.z<.25f))continue;
      const unsigned limit=result.face?64:result.hair?1:8192;
      // Deterministic reservoir avoids selecting only the first side of a model.
      const unsigned number=eligible++;
      const uint32_t hash=(number+1)*2654435761u;
      const unsigned group_id=REX_LOAD_U32(packet+8),selector=REX_LOAD_U32(packet+12);
      if(selector<256)result.selectors=std::max(result.selectors,selector+1);
      PlayerSurfaceSample sample{tri,{.24f+float(hash&255)/255*.18f,.24f,0},result.face?.0014f:.028f,region,group_id,selector};
      sample.cloth_hem=group_id==35&&palette_count==26;
      sample.print_overlay=number_group;
      sample.bary.z=1-sample.bary.x-sample.bary.y;
      if(result.face&&result.skin.size()<1024)result.skin.push_back(sample);
      if(result.samples.size()<limit)result.samples.push_back(sample);
      else if(result.face){const auto slot=hash%(number+1);if(slot<limit)result.samples[slot]=sample;}
    }
  }
  // Identical source models/LODs share immutable rest-space work across actors.
  // Per-instance matrices remain live and are never shared by this key.
  if(result.face&&!result.skin.empty()){
    result.face_grid=std::make_shared<FaceGrid>();
    const auto column=[](float x){return std::clamp(int((x+.12f)*(16.f/.24f)),0,15);};
    const auto row=[](float y){return std::clamp(int((y-.035f)*(16.f/.145f)),0,15);};
    for(unsigned i=0;i<result.skin.size();i++){const auto& t=result.skin[i].triangle;
      const int x0=column(std::min({t[0].p.x,t[1].p.x,t[2].p.x})),x1=column(std::max({t[0].p.x,t[1].p.x,t[2].p.x}));
      const int y0=row(std::min({t[0].p.y,t[1].p.y,t[2].p.y})),y1=row(std::max({t[0].p.y,t[1].p.y,t[2].p.y}));
      for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)(*result.face_grid)[y*16+x].push_back(i);
    }
  }
  if(!result.samples.empty()){
    std::array<bool,256> groups{},selectors{};
    for(const auto& sample:result.samples){if(sample.group<256)groups[sample.group]=true;if(sample.selector<256)selectors[sample.selector]=true;}
    for(unsigned i=0;i<256;i++){if(groups[i])result.dirt_groups.push_back(i);if(selectors[i])result.dirt_selectors.push_back(i);}
    uint64_t hash=1469598103934665603ull;const auto mix=[&](uint32_t value){hash=(hash^value)*1099511628211ull;};
    mix(result.bones);mix(result.groups);mix(result.selectors);mix(result.face);mix(result.hair);
    if(result.hair)for(float y:result.hairline)mix(std::bit_cast<uint32_t>(y));
    if(result.face){mix(std::bit_cast<uint32_t>(result.eye_top));for(float y:result.forehead)mix(std::bit_cast<uint32_t>(y));}
    for(const auto& sample:result.samples){mix(sample.region);mix(sample.group);mix(sample.selector);mix(sample.cloth_hem);mix(sample.print_overlay);
      for(const auto& v:sample.triangle){mix(v.has_uv);for(float f:{v.p.x,v.p.y,v.p.z,v.n.x,v.n.y,v.n.z,v.uv.x,v.uv.y})mix(std::bit_cast<uint32_t>(f));
        for(unsigned i=0;i<4;i++){mix(v.bone[i]);mix(std::bit_cast<uint32_t>(v.weight[i]));}
      }
    }result.revision=hash;
  }
  result.rejection=result.samples.empty()?5:0;return result;
}
#include "player_dirt_bake.h"
void CapturePlayerSurface(uint8_t* base,uint32_t instance){
  struct Timer {bool enabled=PlayerSurfaceDiagnosticsEnabled();std::chrono::steady_clock::time_point start=enabled?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{};uint32_t input=0;
    ~Timer(){if(!enabled)return;const auto us=uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count());
      surface_capture_us+=us;if(us>surface_capture_max_us){surface_capture_max_us=us;surface_capture_max_input=input;}++surface_capture_calls;}} timer;
  if(!Valid(base,instance,296))return;
  const auto object=Pointer(base,instance+248),data=Pointer(base,object+12),input=Pointer(base,instance+240),matrices=Pointer(base,instance+244);
  timer.input=input;
  if(!data||!input||!matrices)return;
  auto found=player_surface_models.find(data);
  const auto original=player_surface_sources.find(object);
  if(original!=player_surface_sources.end()&&(found==player_surface_models.end()||found->second.samples.empty()))
    found=player_surface_models.insert_or_assign(data,original->second).first;
  if(found==player_surface_models.end()){
    if(player_surface_models.size()>=256)player_surface_models.clear();
    found=player_surface_models.emplace(data,BuildPlayerSurfaceModel(base,data)).first;
    ++player_surface_diagnostics.models;
    if(found->second.samples.empty())++player_surface_diagnostics.empty;
    if(Valid(base,data,28)){player_surface_diagnostics.last_data=data;player_surface_diagnostics.last_magic=REX_LOAD_U32(data);player_surface_diagnostics.last_bones=REX_LOAD_U32(data+24);}
  }
  const auto& model=found->second;if(model.samples.empty()||!Valid(base,matrices,model.bones*64))return;
  if(model.hair){
    Vec head=PlayerMatrixPoint(base,matrices,0,{0,0,0});
    if(base[instance+292]){if(!Valid(base,instance+176,64))return;head=PlayerMatrixPoint(base,instance+176,0,head);}
    auto* binding=&player_hairlines[0];
    for(auto& h:player_hairlines){if(h.input==input){binding=&h;break;}if(h.time<binding->time)binding=&h;}
    *binding={input,player_surface_capture_epoch,effect_time,head,model.hairline};
    return;
  }
  if(model.pants&&!model.face&&!pes6::graphics::EffectOptions().dirt)return;
  const auto group_flags=Pointer(base,instance+252),selector_flags=Pointer(base,instance+256);
  const bool groups_ready=group_flags&&model.groups&&model.groups<=256&&Valid(base,group_flags,model.groups);
  const bool selectors_ready=selector_flags&&model.selectors&&model.selectors<=256&&Valid(base,selector_flags,model.selectors);
  // Use the very same visibility masks read by822000A8: inactive shirt/shorts
  // variants must neither seed contact nor receive decals over the active mesh.
  const auto visible=[&](const PlayerSurfaceSample& sample){return
    (!groups_ready||(sample.group<model.groups&&base[group_flags+sample.group]))&&
    (!selectors_ready||(sample.selector<model.selectors&&base[selector_flags+sample.selector]));};
  auto* cached=&player_surface_instances[0];
  for(auto& slot:player_surface_instances)if(slot.view<cached->view||(slot.view==cached->view&&slot.time<cached->time))cached=&slot;
  for(auto& slot:player_surface_instances)if(slot.address==instance||!slot.address){cached=&slot;break;}
  cached->address=instance;cached->input=input;cached->data=data;cached->view=player_surface_view_epoch;cached->face=model.face;cached->time=effect_time;cached->meshes.clear();
  const auto state=replay_mode&&replay_appearance?replay_appearance.get():nullptr;
  const float sweat=PlayerSweat(state?state->minute:appearance_minute,state?state->period:appearance_period);
  const bool wet=Wet(base);const float moisture=pes6::graphics::EffectOptions().Moisture(wet,sweat);
  const auto& dirt=state?state->players:player_dirt;
  const PlayerDirt* selected_dirt=nullptr;unsigned dirt_index=64;
  // 82200518's intermediate244 array is model-local skinning. Native822000A8
  // applies instance+176 before the camera when flag292 is set. Surface
  // particles consume WORLD coordinates, so retain that same instance transform.
  const bool transformed=base[instance+292]!=0;
  if(transformed&&!Valid(base,instance+176,64))return;
  const auto read_matrix=[&](uint32_t at){std::array<float,12> out{};
    for(unsigned col=0;col<4;col++)for(unsigned row=0;row<3;row++)out[col*3+row]=Float(base,at+col*16+row*4);return out;};
  const auto point=[](const auto& m,Vec p){return Vec{m[0]*p.x+m[3]*p.y+m[6]*p.z+m[9],
    m[1]*p.x+m[4]*p.y+m[7]*p.z+m[10],m[2]*p.x+m[5]*p.y+m[8]*p.z+m[11]};};
  const auto world_matrix=transformed?read_matrix(instance+176):std::array<float,12>{};
  const auto world=[&](Vec p){return transformed?point(world_matrix,p):p;};
  std::array<std::array<float,12>,50> native_matrices;std::array<bool,50> matrix_ready{};
  const auto native_point=[&](unsigned bone,Vec p){
    if(!matrix_ready[bone]){native_matrices[bone]=read_matrix(matrices+bone*64);matrix_ready[bone]=true;}
    return point(native_matrices[bone],p);
  };
  std::array<float,6> sleeve_offsets{};bool sleeve_ready=false,sleeve_active=false;
  const auto sleeve_point=[&](const PlayerSurfaceVertex& v,unsigned bone,Vec p){
    if(!v.sleeve_motion||bone<18||bone>25)return p;
    if(!sleeve_ready){sleeve_active=pes6_cloth_sleeve_offsets(base,instance,matrices,sleeve_offsets.data());sleeve_ready=true;}
    if(sleeve_active){const unsigned side=bone==19||bone==22||bone==23||bone==25?1:0;const float factor=bone==24||bone==25?1.f:bone==21||bone==23?.85f:bone==20||bone==22?.35f:.20f;const auto* d=sleeve_offsets.data()+side*3;p=Add(p,Mul(Vec{d[0],d[1],d[2]},factor));}return p;
  };
  const auto native_skin=[&](const PlayerSurfaceVertex& v){Vec out{};
    for(unsigned i=0;i<4;i++)if(v.weight[i]>0)out=Add(out,Mul(sleeve_point(v,v.bone[i],native_point(v.bone[i],v.p)),v.weight[i]));return out;};
  std::array<float,72> cloth_palette{};
  bool cloth_palette_ready=false,cloth_override=false;
  const auto skin=[&](const PlayerSurfaceSample& sample,const PlayerSurfaceVertex& v){
    // Clean garments still warm their topology cache, but need no cloth
    // palette lookup/lock until a contact or an actual dirt mesh is skinned.
    if(!cloth_palette_ready){
      cloth_override=model.pants&&pes6_cloth_surface_palette(base,instance,matrices,cloth_palette.data());
      cloth_palette_ready=true;
    }
    if(!cloth_override||sample.cloth_hem)return native_skin(v);
    Vec out{};
    for(unsigned i=0;i<4;i++)if(v.weight[i]>0){
      const auto bone=v.bone[i];Vec point;
      if(bone>=28&&bone<=33){const auto* p=cloth_palette.data()+(bone-28)*12;
        point={p[0]*v.p.x+p[3]*v.p.y+p[6]*v.p.z+p[9],p[1]*v.p.x+p[4]*v.p.y+p[7]*v.p.z+p[10],p[2]*v.p.x+p[5]*v.p.y+p[8]*v.p.z+p[11]};
      }else point=sleeve_point(v,bone,native_point(bone,v.p));
      out=Add(out,Mul(point,v.weight[i]));
    }return out;
  };
  const Vec root=model.pants?world(native_point(0,{0,.95f,0})):world(native_skin(model.samples[0].triangle[0]));
  player_surface_diagnostics.last_root=root;
  if(!WaterWorld(root)){++player_surface_diagnostics.outside;return;}
  dirt_index=model.pants?ResolvePlayerDirt(dirt,input,root,replay_mode):64;
  if(dirt_index<64)selected_dirt=&dirt[dirt_index];
  PlayerDirt visible_dirt;
  if(model.pants&&pes6::graphics::EffectOptions().dirt){
    uint32_t identity=0;
    if(const auto known=player_input_identities.find(input);known!=player_input_identities.end())identity=known->second;
    if(!identity&&selected_dirt)identity=selected_dirt->identity;
    if(identity&&selected_dirt&&selected_dirt->identity&&selected_dirt->identity!=identity)selected_dirt=nullptr;
    const PlayerDirt* permanent=nullptr;
    if(identity){
      for(const auto& live:player_dirt)if(live.identity==identity&&live.patch_count){permanent=&live;break;}
      if(const auto saved=player_dirt_identities.find(identity);saved!=player_dirt_identities.end()&&
         (!permanent||saved->second.patch_count>permanent->patch_count))permanent=&saved->second;
    }
    if(!permanent&&selected_dirt&&selected_dirt->actor){
      if(const auto saved=player_dirt_history.find(selected_dirt->actor);saved!=player_dirt_history.end())permanent=&saved->second;
    }
    if(!selected_dirt&&permanent)selected_dirt=permanent;
    else if(state&&selected_dirt&&permanent&&selected_dirt!=permanent){
      visible_dirt=*selected_dirt;
      MergeVisibleReplayDirt(visible_dirt,*permanent,state->tick);
      selected_dirt=&visible_dirt;
    }
    if(state&&selected_dirt==permanent&&permanent){
      visible_dirt={};MergeVisibleReplayDirt(visible_dirt,*permanent,state->tick);selected_dirt=&visible_dirt;
    }
  }
  if(model.pants&&pes6::graphics::EffectOptions().dirt&&!replay_mode&&!state&&dirt_index<64)player_dirt[dirt_index].position=root;
  if(model.pants&&pes6::graphics::EffectOptions().dirt&&!replay_mode&&!state&&dirt_index<64&&player_dirt[dirt_index].pending&&player_dirt[dirt_index].last_contact_scan!=motion_clock.load(std::memory_order_relaxed)){
    auto& live=player_dirt[dirt_index];live.last_contact_scan=motion_clock.load(std::memory_order_relaxed);std::array<float,dirt_region_count> lowest;lowest.fill(.19f);std::array<const PlayerSurfaceSample*,dirt_region_count> contact{};
    for(const auto& sample:model.samples){
      if(!visible(sample)||sample.print_overlay)continue;
      const auto& tri=sample.triangle;
      const Vec rest_center=Mul(Add(Add(tri[0].p,tri[1].p),tri[2].p),1.f/3),rest_normal=Unit(Add(Add(tri[0].n,tri[1].n),tri[2].n));
      if(!OuterGarmentContact(rest_center,rest_normal,sample.region))continue;
      Vec center{},first_point{};
      for(unsigned j=0;j<3;j++){
        const auto p=world(skin(sample,tri[j]));if(!j)first_point=p;
        center=Add(center,Mul(p,1.f/3));
      }
      auto shifted=tri[0];shifted.p=Add(shifted.p,Mul(shifted.n,.01f));
      const auto a=first_point,b=world(skin(sample,shifted));
      const Vec normal=Unit({b.x-a.x,b.y-a.y,b.z-a.z});const float down=-normal.y;
      if(down<.25f||center.y<-.08f||center.y>.14f)continue;
      const float score=center.y+.05f*(1-down);
      if(score<lowest[sample.region]){lowest[sample.region]=score;contact[sample.region]=&sample;}
    }
    bool touched=false;
    for(unsigned region=0;region<dirt_region_count;region++)if(contact[region]){
      const auto& tri=contact[region]->triangle;
      const Vec center=Mul(Add(Add(tri[0].p,tri[1].p),tri[2].p),1.f/3),normal=Unit(Add(Add(tri[0].n,tri[1].n),tri[2].n));
      DirtPatch patch{center,normal,.78f,region,live.contact_tick,lowest[region],motion_clock.load(std::memory_order_relaxed)};
      patch.has_uv=tri[0].has_uv&&tri[1].has_uv&&tri[2].has_uv;
      patch.uv=Mul(Add(Add(tri[0].uv,tri[1].uv),tri[2].uv),1.f/3);
      patch.source_model=model.revision;
      AddDirtPatch(live,patch);touched=true;
    }
    if(motion_clock.load(std::memory_order_relaxed)-live.contact_tick>12)live.pending=false;
    if(touched&&recording_ready){auto& frame=recorded_effects[recording_slot];
      auto snapshot=frame.appearance?std::make_shared<AppearanceFrame>(*frame.appearance):std::make_shared<AppearanceFrame>();snapshot->tick=motion_clock.load(std::memory_order_relaxed);snapshot->players=player_dirt;frame.appearance=std::move(snapshot);PublishEffects(frame);
    }
  }
  if(model.pants&&pes6::graphics::EffectOptions().dirt){
    uint64_t visibility=1469598103934665603ull;
    const auto mix=[&](uint32_t v){visibility=(visibility^v)*1099511628211ull;};
    mix(groups_ready);mix(selectors_ready);
    if(groups_ready)for(unsigned i:model.dirt_groups)mix(base[group_flags+i]);
    if(selectors_ready)for(unsigned i:model.dirt_selectors)mix(base[selector_flags+i]);
    // Prepare immutable topology while the kit is still clean, during initial
    // scene/LOD draws. A player's first fall should not trigger that cold work.
    const auto& topology=GetDirtTopology(model,visibility,visible);
    if(!selected_dirt||!selected_dirt->patch_count)return;
    const auto& baked=BakePlayerDirt(model,*selected_dirt,wet?(pes6_torrential_selected()?2u:1u):0u,topology);
    ParticleMesh batch{};batch.count=0;
    static thread_local std::vector<MeshVertex> posed;
    static thread_local std::vector<Vec> posed_corners;posed_corners.clear();posed_corners.reserve(baked.skin.size());
    for(const auto& v:baked.skin)posed_corners.push_back(world(skin(model.samples[v.sample],v.vertex)));
    const auto emit=[&](MeshVertex a,MeshVertex b,MeshVertex c){
      if(batch.count+6>batch.vertices.size()){cached->meshes.push_back(batch);batch.count=0;}
      for(const auto v:{a,b,c,c,b,a})batch.vertices[batch.count++]=v;
    };
    for(const auto& piece:baked.pieces){
      const auto& sample=model.samples[piece.sample];
      const std::array<Vec,3> lifted{posed_corners[piece.corners[0]],posed_corners[piece.corners[1]],posed_corners[piece.corners[2]]};
      bool ready=true;for(const auto& p:lifted)ready&=std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&
        std::abs(p.x-root.x)<3.f&&std::abs(p.y-root.y)<3.f&&std::abs(p.z-root.z)<3.f;
      if(!ready)continue; // Native cinematic wrappers can expose an unready palette for one draw.
      posed.clear();for(const auto& v:piece.vertices)posed.push_back({Add(Add(Mul(lifted[0],v.bary.x),Mul(lifted[1],v.bary.y)),Mul(lifted[2],v.bary.z)),v.color});
      for(const auto& triangle:piece.triangles)emit(posed[triangle[0]],posed[triangle[1]],posed[triangle[2]]);
      ++player_surface_diagnostics.pants;
    }
    if(batch.count)cached->meshes.push_back(batch);
  }
  if(model.face){
    const Vec eye=player_surface_camera_ready?player_surface_camera:Add(root,Add(world({0,0,3}),Mul(world({0,0,0}),-1)));
    if(moisture<=0||std::hypot(eye.x-root.x,eye.y-root.y,eye.z-root.z)>=10.f)return;
    const auto head=world(native_point(0,{0,0,0}));
    const HairlineBinding* hair=nullptr;float nearest=.15f;
    for(const auto& h:player_hairlines){
      if(!h.input||h.epoch!=player_surface_capture_epoch||std::abs(h.time-effect_time)>.20f)continue;
      const float distance=std::hypot(h.root.x-head.x,h.root.y-head.y,h.root.z-head.z);
      if(h.input==input&&distance<.15f){hair=&h;break;}
      if(distance<nearest){nearest=distance;hair=&h;}
    }
    // Hair can be omitted from an individual draw/pass. Do not substitute the
    // top of the skull for a hairline already resolved for this same face.
    struct FaceHairCache {uint32_t input=0,epoch=0;uint64_t model=0;std::array<float,13> line{};};
    static thread_local std::array<FaceHairCache,64> face_hair_cache;
    FaceHairCache* remembered=nullptr;
    for(auto& entry:face_hair_cache)if(entry.input==input&&entry.epoch==player_surface_capture_epoch&&entry.model==model.revision){remembered=&entry;break;}
    if(hair&&std::any_of(hair->line.begin(),hair->line.end(),[](float y){return y>0;})){
      if(!remembered){for(auto& entry:face_hair_cache)if(!entry.input||entry.epoch!=player_surface_capture_epoch){remembered=&entry;break;}}
      if(!remembered)remembered=&face_hair_cache[(input>>4)%face_hair_cache.size()];
      *remembered={input,player_surface_capture_epoch,model.revision,hair->line};
    }
    const float brow=model.eye_top?std::clamp(model.eye_top+.012f,.105f,.130f):.112f;
    const auto forehead_top=[&](float x){
      const float lane=std::clamp(x/.01f+6.f,0.f,12.f);const unsigned i=unsigned(lane);
      const auto sample_line=[&](const auto& line){const float a=line[i],b=line[std::min(i+1,12u)];
        return a&&b?a+(b-a)*(lane-i):a?a:b;};
      const float skin_top=sample_line(model.forehead);
      // A sparse hair strip or curl can extend below a linearly interpolated
      // edge. Use the lower neighbouring envelope, then allow a full bead's
      // diameter of clearance below it.
      float hair_top=0;
      if(remembered)for(int j=std::max(0,int(i)-1);j<=std::min(12,int(i)+2);j++){
        const float y=remembered->line[j];
        if(y>brow+.004f&&(!hair_top||y<hair_top))hair_top=y;
      }
      const float border=hair_top?hair_top:brow+.028f;
      return std::min(border,skin_top?skin_top:border)-.012f;
    };
    struct FaceTrianglePose {std::array<Vec,3> points;Vec normal;};
    static thread_local std::vector<FaceTrianglePose> face_poses;
    static thread_local std::vector<uint8_t> face_pose_ready;
    face_poses.resize(model.skin.size());face_pose_ready.assign(model.skin.size(),0);
    const auto skin_point=[&](float x,float y,Vec& point,Vec& normal){
      float foremost=-1e9f;const PlayerSurfaceSample* best=nullptr;Vec bary{};
      const std::vector<unsigned>* candidates=nullptr;
      if(model.face_grid&&x>=-.12f&&x<=.12f&&y>=.035f&&y<=.18f){
        const auto col=std::clamp(int((x+.12f)*(16.f/.24f)),0,15),row=std::clamp(int((y-.035f)*(16.f/.145f)),0,15);
        candidates=&(*model.face_grid)[row*16+col];
      }
      const auto count=candidates?candidates->size():model.skin.size();
      for(unsigned index=0;index<count;index++){const auto& s=model.skin[candidates?(*candidates)[index]:index];const auto& a=s.triangle[0].p;const auto& b=s.triangle[1].p;const auto& c=s.triangle[2].p;
        if(!visible(s))continue;
        const float denominator=(b.y-c.y)*(a.x-c.x)+(c.x-b.x)*(a.y-c.y);if(std::abs(denominator)<1e-9f)continue;
        const float u=((b.y-c.y)*(x-c.x)+(c.x-b.x)*(y-c.y))/denominator,v=((c.y-a.y)*(x-c.x)+(a.x-c.x)*(y-c.y))/denominator,w=1-u-v;
        if(std::min({u,v,w})<0)continue;const float z=u*a.z+v*b.z+w*c.z;if(z<=foremost)continue;foremost=z;
        best=&s;bary={u,v,w};
      }
      if(!best)return false;
      const auto triangle=unsigned(best-model.skin.data());auto& pose=face_poses[triangle];
      if(!face_pose_ready[triangle]){
        for(unsigned j=0;j<3;j++)pose.points[j]=world(native_skin(best->triangle[j]));
        auto shifted=best->triangle[0];shifted.p=Add(shifted.p,Mul(shifted.n,.01f));const auto q=world(native_skin(shifted));
        pose.normal=Unit({q.x-pose.points[0].x,q.y-pose.points[0].y,q.z-pose.points[0].z});
        face_pose_ready[triangle]=1;
      }
      const auto& p=pose.points;
      point=Add(Add(Mul(p[0],bary.x),Mul(p[1],bary.y)),Mul(p[2],bary.z));
      normal=pose.normal;return true;
    };
    const float water_time=state?state->water_time:effect_time;
    // Five stable beads within the central forehead ellipse marked by the user.
    constexpr std::array<float,5> bead_x{-.014f,0,.014f,-.007f,.007f};
    constexpr std::array<float,5> bead_y{.0035f,.0045f,.0035f,-.0045f,-.0045f};
    for(unsigned bead=0;bead<5;bead++){
      const uint32_t seed=uint32_t(model.revision)^(0x9e3779b9u*(bead+1));
      const float x=bead_x[bead]+(float(seed&255)/255.f-.5f)*.002f;
      const float top=forehead_top(x),bottom=brow+.006f,span=top-bottom;
      if(span<.004f)continue;
      const float y=std::clamp(bottom+span*.62f+bead_y[bead]+(float((seed>>8)&255)/255.f-.5f)*.0015f,bottom,top-.003f);
      Vec p,n;if(!skin_point(x,y,p,n))continue;
      const auto view=Unit({eye.x-p.x,eye.y-p.y,eye.z-p.z});if(view.x*n.x+view.y*n.y+view.z*n.z<=.05f)continue;
      cached->meshes.push_back(WaterBead(Add(p,Mul(n,.0005f)),n,std::min(.0032f,span*.32f),eye));
      ++player_surface_diagnostics.faces;
    }
    const unsigned moving_drops=unsigned(std::ceil(moisture*10));
    struct PathJitter {float cycle=0,jitter=0;bool ready=false;};
    static thread_local std::array<PathJitter,10> path_jitter;
      for(unsigned path=0;path<10;path++){
        if(path>=moving_drops)break;
        const bool nose=path==0,forehead=path>=1&&path<=6;
        const float side=(path&1)?-1.f:1.f;
        const float lane_x=nose?0:forehead?side*(path<=2?.010f:path<=4?.022f:.033f):side*.060f;
        const float end_y=nose?.070f:forehead?brow+.008f:.042f;
        const float longest=std::max(forehead_top(lane_x-.0005f),forehead_top(lane_x+.0005f))-end_y;
        const float reference=nose?.060f:forehead?.017f:path==9?.075f:.090f;
        const float speed_scale=reference/std::max(reference,longest);
        const float phase=water_time*((1.15f+float(path)*.019f)*.60f*speed_scale)+float(path)*.173f;
        const float cycle=std::floor(phase),t=phase-cycle;
        // Re-seed each path per run, deterministically so replay frames cannot jitter.
        auto& random=path_jitter[path];
        if(!random.ready||random.cycle!=cycle){
          const float seed=std::sin((cycle+1.f)*127.1f+float(path)*311.7f)*43758.5453f;
          random={cycle,(seed-std::floor(seed))-.5f,true};
        }
        const float jitter=random.jitter;
        // One nose lane, six forehead lanes stopping above the eyebrows,
        // and three outer-side lanes. No cheek/under-eye routes.
        const auto path_x=[&](float progress){return nose?jitter*.001f:forehead?
          side*((path<=2?.010f:path<=4?.022f:.033f)+jitter*.001f):
          side*(.060f+progress*.003f+jitter*.001f);};
        const float start_y=forehead_top(path_x(0));
        if(start_y<=brow+.008f)continue;
        const float travel=std::max(0.f,start_y-end_y);
        const float y=start_y-t*travel,x=path_x(t);
        // Exclude the complete under-eye band, including the bead radius.
        if(y<brow+.008f&&std::abs(x)>.008f&&std::abs(x)<.055f)continue;
        Vec p,n;if(!skin_point(x,y,p,n))continue;const Vec view=Unit({eye.x-p.x,eye.y-p.y,eye.z-p.z});
        if(view.x*n.x+view.y*n.y+view.z*n.z<=.05f)continue;
        const float visibility=std::max(0.f,std::min(1.f,std::min(t/.09f,(1.f-t)/.13f)));
        cached->meshes.push_back(WaterBead(Add(p,Mul(n,.0005f)),n,.0052f*visibility*(.8f+.2f*moisture),eye));
        ++player_surface_diagnostics.faces;
        if(t>.12f){
          Vec previous,prior_normal;
          const float prior_t=std::max(0.f,t-.25f);
          if(skin_point(path_x(prior_t),start_y-prior_t*travel,previous,prior_normal)){
            const auto axis=Unit(Add(p,Mul(previous,-1.f))),across=Unit(Cross(n,axis));
            const auto top=Add(previous,Mul(prior_normal,.00055f)),bottom=Add(p,Mul(n,.00055f));
            const auto width=Mul(across,.00155f*visibility);
            const uint32_t clear=0x00C6D9E0u,water=(uint32_t(88.f*visibility*moisture)<<24)|0x00DCEEF4u;
            ParticleMesh trail{};trail.count=6;
            trail.vertices[0]={Add(top,width),clear};trail.vertices[1]={Add(top,Mul(width,-1)),clear};trail.vertices[2]={Add(bottom,width),water};
            trail.vertices[3]={Add(bottom,width),water};trail.vertices[4]={Add(top,Mul(width,-1)),clear};trail.vertices[5]={Add(bottom,Mul(width,-1)),water};
            cached->meshes.push_back(trail);
          }
        }
      }

  }
}
void CapturePlayerSurfaceOnce(uint8_t* base,uint32_t instance){
  // The common draw entry runs BEFORE 82200518 refreshes the skin matrices.
  // Do not mark that stale pose as already captured for this frame.
  if(!Valid(base,instance,296)||base[instance+293])return;
  const auto object=Pointer(base,instance+248),data=Pointer(base,object+12),input=Pointer(base,instance+240),matrices=Pointer(base,instance+244);
  uint64_t signature=1469598103934665603ull;
  const auto mix=[&](uint32_t value){signature=(signature^value)*1099511628211ull;};
  mix(data);mix(input);mix(matrices);
  if(const auto model=player_surface_models.find(data);model!=player_surface_models.end()){
    for(const auto offset:{252u,256u}){
      const auto& indices=offset==252?model->second.dirt_groups:model->second.dirt_selectors;
      const auto count=offset==252?model->second.groups:model->second.selectors;
      const auto flags=Pointer(base,instance+offset);
      if(flags&&count<=256&&Valid(base,flags,count)){mix(flags);for(unsigned i:indices)mix(base[flags+i]);}
    }
  }
  struct Seen {uint32_t instance=0,tick=~0u,epoch=~0u,view=~0u;float cursor=-1;bool replay=false;uint64_t signature=0;};
  static std::array<Seen,256> seen;
  Seen* selected=nullptr;for(auto& s:seen)if(s.instance==instance){selected=&s;break;}
  if(!selected)for(auto& s:seen)if(!s.instance){selected=&s;break;}
  if(!selected)selected=&seen[((instance>>4)^(instance>>12))%seen.size()];
  const auto tick=motion_clock.load(std::memory_order_relaxed);
  if(selected->instance==instance&&selected->tick==tick&&selected->epoch==player_surface_capture_epoch&&selected->view==player_surface_view_epoch&&selected->signature==signature&&selected->replay==replay_mode&&(!replay_mode||selected->cursor==last_cursor))return;
  // Render matrices and active LOD/garment masks belong to this view. Never
  // reuse a posed mesh from another camera merely because simulation paused.
  *selected={instance,tick,player_surface_capture_epoch,player_surface_view_epoch,last_cursor,replay_mode,signature};CapturePlayerSurface(base,instance);
}

