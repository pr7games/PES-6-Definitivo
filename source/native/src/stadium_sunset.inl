// Extended UI choice3=Atardecer; native2 remains Random. Keep the engine's
// binary day/night enum unchanged and dispatch imported variant4/5 separately.
namespace {
std::filesystem::path SunsetRoot(){wchar_t exe[MAX_PATH]{};GetModuleFileNameW(nullptr,exe,MAX_PATH);auto root=std::filesystem::path(exe).parent_path();for(unsigned i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root;}
// Saved UI preferences must never activate rendering effects during startup
// or in editors. Only decoding a match environment enables stadium lighting.
bool konamiSunset=false;
bool sunsetOvercast=false;
// Logical night is independent of the legacy renderer's binary scene path.
// Imported Konami uses the working daytime geometry/background path while
// keeping its own night resources, lights and saved match hour.
bool konamiNight=false;
bool stadiumNight=false;
bool stadiumLightingContext=false;
int stadiumLightGainPercent=-1;
unsigned StadiumLightGain(){
 if(stadiumLightGainPercent<0){int value=115;std::ifstream in(SunsetRoot()/"data/stadiums/lighting-gain-percent.txt");int requested=0;if(in>>requested)value=std::clamp(requested,100,140);stadiumLightGainPercent=value;}
 return unsigned(stadiumLightGainPercent);
}
void StadiumAdjustLight(uint8_t* bytes,size_t size){
 const auto be=[&](size_t at){return uint32_t(bytes[at])<<24|uint32_t(bytes[at+1])<<16|uint32_t(bytes[at+2])<<8|bytes[at+3];};
 if(size<32||be(0)!=2||be(4)!=8||be(12)!=204)return;
 const uint64_t offset=be(8);if(offset+204>size)return;
 const auto gain=StadiumLightGain();if(gain==100)return;
 for(unsigned light=0;light<3;light++)for(unsigned state=0;state<2;state++)for(unsigned term=0;term<2;term++)for(unsigned c=0;c<3;c++){
  const auto at=size_t(offset)+light*40+4+state*18+term*6+c*2;
  const auto value=std::min(65535u,((unsigned(bytes[at])<<8|bytes[at+1])*gain+50)/100);
  bytes[at]=uint8_t(value>>8);bytes[at+1]=uint8_t(value);
 }
}
uint32_t sunsetLabels=0;
int savedLogicalHour=-2;
uint32_t SavedLogicalHour(uint32_t fallback){
 if(savedLogicalHour==-2){std::ifstream in(SunsetRoot()/"data/stadiums/konami/preferred-hour.txt");int value=-1;in>>value;savedLogicalHour=value>=0&&value<=3?value:-1;}
 return savedLogicalHour>=0?uint32_t(savedLogicalHour):fallback;
}
uint32_t SunsetNative(PPCContext& original,uint8_t* base,PPCFunc* f,uint32_t a,uint32_t b=0,uint32_t c=0,uint32_t d=0){PPCContext ctx=original;ctx.r3.u64=a;ctx.r4.u64=b;ctx.r5.u64=c;ctx.r6.u64=d;f(ctx,base);return ctx.r3.u32;}
bool SunsetLabels(PPCContext& ctx,uint8_t* base){
 if(sunsetLabels)return true;sunsetLabels=SunsetNative(ctx,base,sub_826D61B0,128);if(!sunsetLabels)return false;std::memset(base+sunsetLabels,0,128);
 const char* lang=std::getenv("PES6_TEXT_LANG");const char* name="Atardecer";if(lang&&(!_stricmp(lang,"en")||!_stricmp(lang,"e")))name="Dusk";else if(lang&&(!_stricmp(lang,"fr")||!_stricmp(lang,"f")))name="Crepuscule";else if(lang&&(!_stricmp(lang,"de")||!_stricmp(lang,"g")))name="Abend";else if(lang&&(!_stricmp(lang,"it")||!_stricmp(lang,"i")))name="Tramonto";else if(lang&&(!_stricmp(lang,"pt")||!_stricmp(lang,"p")))name="Entardecer";
 std::memcpy(base+sunsetLabels+32,name,std::strlen(name)+1);return true;
}
}
// Wet dusk keeps the logical hour, but uses the native overcast day sky/lights.
extern "C" bool pes6_stadium_sunset(){return konamiSunset&&!sunsetOvercast;}
extern "C" uint32_t pes6_stadium_lighting_hour(uint8_t* base,uint32_t original){
 const auto config=REX_LOAD_U32(0x83139360u);
 return stadiumLightingContext&&stadiumNight&&config&&(base[config+130]<=8||base[config+130]==10)?1:original;
}
extern "C" double pes6_stadium_shadow_weight(uint8_t* base,double weight){
 const auto config=REX_LOAD_U32(0x83139360u);
 if(!stadiumLightingContext||!config||(base[config+130]>8&&base[config+130]!=10)||stadiumNight||base[config+132]==1)return weight;
 // The interpolation's zero endpoint is the original fully lit profile.
 // Reduce only entry into the shade endpoint; never raise sunlit/global RGB.
 return weight>=0.0&&weight<=1.0?weight*0.70:weight;
}
extern "C" uint32_t pes6_stadium_time_saved(uint32_t fallback){return SavedLogicalHour(fallback);}
extern "C" void pes6_stadium_time_commit(uint32_t choice){
 if(choice>3||SavedLogicalHour(4)==choice)return;
 savedLogicalHour=int(choice);std::ofstream out(SunsetRoot()/"data/stadiums/konami/preferred-hour.txt",std::ios::trunc);out<<choice<<"\n";
}
extern "C" uint32_t pes6_stadium_time_begin(uint8_t* base,uint32_t venue,uint32_t time){
 const auto choice=base[time];const bool selected=choice==3;konamiSunset=selected;sunsetOvercast=false;
 if(choice==3)base[time]=0;return choice;
}
extern "C" void pes6_stadium_time_end(uint8_t* base,uint32_t time,uint32_t choice){
 if(choice!=3)return;base[time]=3;
 // Match record+40 is the logical environment packet. Keep the extended hour
 // there (and in copies/saved replay headers), never in engine config+132.
 const auto match=REX_LOAD_U32(0x83139438u);if(match)base[match+325]=3;
}
extern "C" uint32_t pes6_stadium_time_decode(uint8_t* base,uint32_t packet){
 stadiumLightingContext=true;
 stadiumLightGainPercent=-1;
 if(base[packet+4]==9)base[packet+4]=8; // Retired custom venue in old match/replay packets.
 const auto choice=base[packet+5];konamiSunset=choice==3;sunsetOvercast=konamiSunset&&base[packet+6]!=0;stadiumNight=(base[packet+4]<=8||base[packet+4]==10)&&choice==1;konamiNight=(base[packet+4]==8||base[packet+4]==10)&&stadiumNight;
 if(choice==3)base[packet+5]=0;return choice;
}
extern "C" void pes6_stadium_time_decode_end(uint8_t* base,uint32_t packet,uint32_t choice){
 if(choice==3)base[packet+5]=3;
 // Do not select the original venue's night-specific background/effect route.
 // The logical packet and profile handles still select actual Konami night.
 const auto config=REX_LOAD_U32(0x83139360u);
 if(config)sunsetOvercast=choice==3&&base[config+131]!=0;
 if(stadiumNight&&config&&(base[config+130]<=8||base[config+130]==10))base[config+132]=0;
}
extern "C" uint32_t pes6_stadium_time_choice(uint32_t row){constexpr uint32_t order[]{0,3,1,2};return row<4?order[row]:2;}
extern "C" uint32_t pes6_stadium_time_row(uint32_t choice){constexpr uint32_t order[]{0,2,3,1};return choice<4?order[choice]:3;}
extern "C" uint32_t pes6_stadium_time_key(uint32_t row){const auto choice=pes6_stadium_time_choice(row);return choice==3?0:choice;}
extern "C" bool pes6_stadium_time_caption(PPCContext& ctx,uint8_t* base,uint32_t row){if(row!=1||!SunsetLabels(ctx,base))return false;SunsetNative(ctx,base,sub_827378E8,ctx.r3.u32,sunsetLabels+32);return true;}
extern "C" void pes6_stadium_time_ready(PPCContext& ctx,uint8_t* base,uint32_t owner){
 if(!SunsetLabels(ctx,base))return;const auto root=REX_LOAD_U32(owner+148);if(!root)return;
 const auto panel=SunsetNative(ctx,base,sub_8272F650,root,1);if(!panel)return;
 const auto row=SunsetNative(ctx,base,sub_8272E410,panel,0);if(!row)return;
 const auto wrappers=REX_LOAD_U32(row+344),combo=wrappers?REX_LOAD_U32(wrappers):0;if(!combo||REX_LOAD_U32(combo)!=0x8200BD10u)return;
 const unsigned count=4;if(REX_LOAD_U32(combo+240)==count)return;
 auto selected=SunsetNative(ctx,base,sub_8273FB30,combo);selected=pes6_stadium_time_row(konamiSunset&&selected==0?3:selected);const auto original=REX_LOAD_U32(0x82B55DDCu);if(!original)return;
 REX_STORE_U32(sunsetLabels,REX_LOAD_U32(original));REX_STORE_U32(sunsetLabels+4,REX_LOAD_U32(original));REX_STORE_U32(sunsetLabels+8,REX_LOAD_U32(original+4));REX_STORE_U32(sunsetLabels+12,REX_LOAD_U32(original+8));
 SunsetNative(ctx,base,sub_8273FC78,combo,sunsetLabels,count);const auto list=REX_LOAD_U32(combo+276);if(!list)return;
 const auto item=SunsetNative(ctx,base,sub_8272E4B0,list,0,1),texts=item?REX_LOAD_U32(item+312):0,text=texts?REX_LOAD_U32(texts):0;
 if(text)SunsetNative(ctx,base,sub_827378E8,text+96,sunsetLabels+32);SunsetNative(ctx,base,sub_82740098,combo,selected<count?selected:2);
}
