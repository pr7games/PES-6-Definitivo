#pragma once
#include "elastica_input.h"
#include "setpiece_gameplay.h"

struct Pes6CR7Combo {uint32_t match=0,actor=0,tick=0,started=0;bool initialized=false,x=false,a=false,armed=false,latched=false;};
inline Pes6CR7Combo pes6CR7Combos[4]{};
// Read physical face keys through the actual native XInput device table, before remapping.
inline unsigned pes6_cr7_faces(uint8_t* base,unsigned pad){
 const unsigned keys=pes6_skills_buttons(base,pad);return ((keys&0x4000u)?1u:0u)|((keys&0x1000u)?2u:0u);
}
// Returns1 while reserving X,2 for the one completed LT+X->A chord,0 otherwise.
inline unsigned pes6_cr7_combo(uint8_t* base,uint32_t actor){
 // Temporarily disabled on CR7 (ID 446) while keeping the animation asset intact.
 return 0;
 if(!pes6_elastica_human(base,actor)||REX_LOAD_U16(actor+132)!=446)return 0;
 const unsigned pad=REX_LOAD_U8(actor+195);auto& s=pes6CR7Combos[pad];const auto match=pes6_gameplay_match(base);if(!match){s={};return 0;}
 const unsigned tick=REX_LOAD_U32(match+2152),faces=pes6_cr7_faces(base,pad);const bool x=faces&1,a=faces&2,lt=pes6_elastica_pad_lt(base,pad)>30;
 if(!s.initialized||s.actor!=actor||s.match!=match||tick-s.tick>64){s={match,actor,tick,0,true,x,a,false,false};return 0;}
 if(s.tick==tick)return lt?1:0;s.tick=tick;const bool riseX=x&&!s.x,riseA=a&&!s.a;s.x=x;s.a=a;
 const unsigned action=REX_LOAD_U8(actor+144);
 const bool eligible=REX_LOAD_U8(match+244)==REX_LOAD_U8(actor+128)&&REX_LOAD_U8(match+439)==0&&REX_LOAD_U8(match+456)==0&&(action==0||action==1||action==2||action==7||action==4||action==10);
 if(!lt||!eligible){s.armed=false;if(!lt&&!x&&!a)s.latched=false;return 0;}
 if(s.latched){if(!x&&!a)s.latched=false;return 1;}
 if(s.armed&&tick-s.started>18){s.armed=false;s.latched=true;return 1;}
 if(riseX&&!a){s.armed=true;s.started=tick;}
 if(s.armed&&riseA&&tick>s.started){s.armed=false;s.latched=true;pes6ElasticaTaps[pad].armed=false;return 2;}
 return 1;
}

// Apply once per native clip start, including LB's repeated loop starts.
inline bool pes6_cr7_stepover(uint8_t* base,uint32_t wrapper,unsigned clip){
 if(clip!=100&&clip!=101&&clip!=102&&clip!=617&&clip!=567)return false;
 const auto match=pes6_gameplay_match(base);if(!match)return false;
 for(unsigned i=0;i<23;i++){const auto actor=REX_LOAD_U32(match+2560+i*12);if(actor&&REX_LOAD_U32(actor+624)==wrapper&&REX_LOAD_U16(actor+132)==446)return true;}return false;
}
