// Inject a converted donor record before the native environment decoder.
// Konami never indexes or copies a reference venue's environment record.
#include "konami_environment_data.h"
#include "wembley_environment_data.h"
extern "C" uint32_t pes6_stadium_environment_record(PPCContext& ctx,uint8_t* base,uint32_t nativeRecord){
 stadiumLightingContext=true;
 const auto config=REX_LOAD_U32(0x83139360u);if(!config)return nativeRecord;
 // Native environment: venue*4 + hour*2 + weather, records of100 bytes.
 if(base[config+130]<8)return nativeRecord&&stadiumNight&&base[config+132]==0?nativeRecord+200:nativeRecord;
 const bool wembley=base[config+130]==10;
 if(base[config+130]!=8&&!wembley)return nativeRecord;
 static uint32_t records=0,wembleyRecords=0;
 if(wembley){
  if(!wembleyRecords){wembleyRecords=SunsetNative(ctx,base,sub_826D61B0,sizeof(wembleyEnvironmentData));if(!wembleyRecords){REXLOG_ERROR("Wembley: environment allocation failed");return 0;}std::memcpy(base+wembleyRecords,wembleyEnvironmentData,sizeof(wembleyEnvironmentData));}
 }else{
  if(!records){records=SunsetNative(ctx,base,sub_826D61B0,sizeof(konamiEnvironmentData));if(!records){REXLOG_ERROR("Konami: environment allocation failed");return 0;}std::memcpy(base+records,konamiEnvironmentData,sizeof(konamiEnvironmentData));}
 }
 const unsigned weather=base[config+131]==1?1:0,time=stadiumNight||base[config+132]==1?1:0;
 const unsigned variant=konamiSunset&&!weather?4:weather*2+time;
 return (wembley?wembleyRecords:records)+variant*100;
}
