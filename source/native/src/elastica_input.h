#pragma once
#include "gameplay_tuning.h"
#include <cstring>

// Capture physical bytes at the XInput poll, indexed by the actual native device.
struct Pes6RawSkillPad {uint32_t device=0;unsigned lt=0,buttons=0;bool valid=false;};
inline Pes6RawSkillPad pes6RawSkillPads[4]{};
inline uint8_t pes6ElasticaLT[4]{};
inline void pes6_elastica_raw(unsigned pad,unsigned value){if(pad<4)pes6ElasticaLT[pad]=uint8_t(value);}
inline void pes6_skills_raw(uint8_t* base,uint32_t device,uint32_t packet,bool connected){
 const unsigned port=REX_LOAD_U32(device+16);if(port>=4)return;
 pes6RawSkillPads[port]={device,connected?unsigned(REX_LOAD_U8(packet+6)):0u,connected?unsigned(REX_LOAD_U16(packet+4)):0u,connected};
}
inline uint32_t pes6_elastica_adapter(uint8_t* base,unsigned pad){
 if(pad>=4)return 0;const uint32_t table=REX_LOAD_U32(0x831395bcu);if(!table)return 0;
 const uint32_t controller=REX_LOAD_U32(table+pad*4);return controller?REX_LOAD_U32(controller+24):0;
}
inline uint32_t pes6_skills_device(uint8_t* base,unsigned pad){const auto adapter=pes6_elastica_adapter(base,pad);return adapter?REX_LOAD_U32(adapter+88):0;}
inline const Pes6RawSkillPad* pes6_skills_raw_pad(uint8_t* base,unsigned pad){
 const auto device=pes6_skills_device(base,pad);if(!device||!REX_LOAD_U8(device+32))return nullptr;
 for(const auto& sample:pes6RawSkillPads)if(sample.device==device&&sample.valid)return &sample;return nullptr;
}
inline unsigned pes6_elastica_pad_lt(uint8_t* base,unsigned pad){
 if(const auto sample=pes6_skills_raw_pad(base,pad))return sample->lt;
 return 0;
}
inline unsigned pes6_skills_buttons(uint8_t* base,unsigned pad){if(const auto sample=pes6_skills_raw_pad(base,pad))return sample->buttons;return 0;}
struct Pes6TacticKey {bool down=false,pressed=false,released=false;};
inline Pes6TacticKey pes6TacticKeys[4]{};
inline unsigned pes6SkillBlockedFaces[4]{};
// phase0/1/2 are pressed/held/released. Transfer LT's native mapped destination to R3.
inline uint32_t pes6_elastica_strip(uint8_t* base,unsigned pad,uint32_t keys,unsigned phase=1){
 const uint32_t adapter=pes6_elastica_adapter(base,pad);if(!adapter||pad>=4)return keys;
 const uint32_t match=pes6_gameplay_match(base);if(!match||REX_LOAD_U8(match+456)!=0)return keys;
 const uint32_t map=REX_LOAD_U32(adapter+64),count=REX_LOAD_U32(adapter+68);if(!map||count>64)return keys;
 auto& state=pes6TacticKeys[pad];if(phase==0){const bool down=(pes6_skills_buttons(base,pad)&0x80u)!=0;state.pressed=down&&!state.down;state.released=!down&&state.down;state.down=down;}
 // Reserve physical X/A before the native shot/pass selector, not only at the actor dispatcher.
 const unsigned physical=pes6_skills_buttons(base,pad);auto& blocked=pes6SkillBlockedFaces[pad];
 if(phase==0&&pes6_elastica_pad_lt(base,pad)>30)blocked|=physical&0x5000u;
 uint32_t tactic=0;const auto device=pes6_skills_device(base,pad);const unsigned buttons=device?REX_LOAD_U32(device+40):0;
 for(unsigned i=0;i<count;++i){const uint32_t row=map+i*4;const unsigned type=REX_LOAD_U8(row),source=REX_LOAD_U8(row+1),targetType=REX_LOAD_U8(row+2),destination=REX_LOAD_U8(row+3);if(targetType!=0||destination>=32)continue;
  if(type==1&&source==0){tactic|=1u<<destination;keys&=~(1u<<destination);}
  if(type==0&&source<buttons&&source<16){const unsigned physicalMask=REX_LOAD_U16(device+44+2*source);
   if(physicalMask==0x80u||(physicalMask&blocked)||(pes6_elastica_pad_lt(base,pad)>30&&(physicalMask&0x5000u)))keys&=~(1u<<destination);
  }
 }
 const bool active=phase==0?state.pressed:phase==2?state.released:state.down;if(active)keys|=tactic;
 if(phase==2)blocked&=physical;
 return keys;
}
struct Pes6ElasticaTap {uint32_t match=0,actor=0,tick=0,first=0;bool initialized=false,down=false,armed=false;};
inline Pes6ElasticaTap pes6ElasticaTaps[4]{};
inline bool pes6_elastica_human(uint8_t* base,uint32_t actor){return actor&&REX_LOAD_U8(actor+160)==0&&REX_LOAD_U8(actor+195)<4;}
inline bool pes6_elastica_press(uint8_t* base,uint32_t actor){
 if(!pes6_elastica_human(base,actor))return false;
 const unsigned pad=REX_LOAD_U8(actor+195);auto& s=pes6ElasticaTaps[pad];
 const uint32_t match=pes6_gameplay_match(base);const bool down=pes6_elastica_pad_lt(base,pad)>30;
 if(!match){s={};return false;}const uint32_t tick=REX_LOAD_U32(match+2152);
 const unsigned action=REX_LOAD_U8(actor+144);
 const uint32_t stats=REX_LOAD_U32(actor+628);
 const bool eligible=stats&&(REX_LOAD_U16(stats+32)&3)!=0&&(pes6_gameplay_pad_held(base,pad)&0x800u)==0&&action!=19&&action!=25&&REX_LOAD_U8(match+244)==REX_LOAD_U8(actor+128)&&REX_LOAD_U8(match+439)==0&&REX_LOAD_U8(match+456)==0;
 if(!eligible){if(s.actor==actor)s.armed=false;return false;}
 if(!s.initialized||s.match!=match||s.actor!=actor||tick-s.tick>64u){s={match,actor,tick,0,true,down,false};return false;}
 if(s.tick==tick)return false;s.tick=tick;
 const bool rising=down&&!s.down;s.down=down;
 if(s.armed&&tick-s.first>18u)s.armed=false;
 if(!rising)return false;
 if(s.armed){s.armed=false;return true;}
 s.first=tick;s.armed=true;return false;
}
inline bool pes6_elastica_no_error(uint8_t* base,uint32_t actor){return pes6_elastica_human(base,actor)&&REX_LOAD_U8(actor+144)==64&&REX_LOAD_U8(actor+198)==6;}
