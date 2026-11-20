#pragma once
#include "gameplay_tuning.h"

// Shares the approved A/Y clock and HUD format. Open-play lofted balls use
// native initialization and charge accumulation; kind2: keeper hands (ball538=5);
// kind3: native restarts44/45; kind4: native action9 centre preparation.
struct Pes6DistributionGauge {
 uint32_t match=0,actor=0,team=0,start=0,last=0,release=0,button=0,deliveryTick=0;
 unsigned power=16,kind=0,action=0; bool active=false,held=false,pending=false,delivered=false,consumed=false,latched=false;
};
inline Pes6DistributionGauge pes6DistributionGauges[4]{};
inline unsigned pes6_distribution_action(uint8_t* base,uint32_t actor){return REX_LOAD_U8(actor+144);}
inline bool pes6_distribution_hands(uint8_t* base,uint32_t match,uint32_t actor){
 const uint32_t ball=REX_LOAD_U32(match+2520);
 return ball&&REX_LOAD_U8(ball+538)==5&&REX_LOAD_U8(match+244)==REX_LOAD_U8(actor+128);
}
inline void pes6_distribution_update(uint8_t* base,Pes6DistributionGauge& s,unsigned pad){
 const uint32_t match=pes6_gameplay_match(base);if(!match||s.match!=match){s={};return;}
 const uint32_t tick=REX_LOAD_U32(match+2152),keys=pes6_gameplay_pad_held(base,pad);
 if(s.latched&&!(keys&s.button))s.latched=false;
 if(!s.active)return;
 if(tick-s.last>180u||REX_LOAD_U8(s.actor+160)!=0||REX_LOAD_U8(s.actor+195)!=pad){s={};return;}
 if(s.held){
  const unsigned action=pes6_distribution_action(base,s.actor);
  if((keys&(0xF000u^s.button))||((keys&0xA00u)==0xA00u)||
    ((s.kind==1||s.kind==4)&&REX_LOAD_U8(match+244)!=REX_LOAD_U8(s.actor+128))||
    (s.kind==2&&!pes6_distribution_hands(base,match,s.actor))||
    (s.kind==3&&action!=s.action)){s={};return;}
  s.power=pes6_gameplay_pass_power(tick-s.start);
  if(!(keys&s.button)||s.power==128){s.held=false;s.pending=true;s.release=tick;s.latched=(keys&s.button)!=0;}
 }
 if(s.kind==4){const unsigned a=pes6_distribution_action(base,s.actor);if(a!=9&&a!=10&&a!=65){s={};return;}}
 if(s.pending&&tick-s.release>180u){s={};return;}
 if(s.consumed&&tick-s.release>6u)s.active=false;
 s.last=tick;
}
inline void pes6_distribution_start(uint8_t* base,Pes6DistributionGauge& s,uint32_t actor,uint32_t button,unsigned kind,unsigned action){
 const uint32_t match=pes6_gameplay_match(base),tick=REX_LOAD_U32(match+2152);
 const unsigned team=(REX_LOAD_U32(actor+136)&0xC0000000u)==0x40000000u?1:0;
 s={};s.match=match;s.actor=actor;s.team=REX_LOAD_U32(match+2496+team*4);s.start=s.last=tick;
 s.button=button;s.kind=kind;s.action=action;s.active=s.held=true;
}
inline Pes6DistributionGauge* pes6_distribution_pending(uint8_t* base,uint32_t actor){
 if(!actor||REX_LOAD_U8(actor+160)!=0)return nullptr;
 const unsigned pad=REX_LOAD_U8(actor+195);if(pad>=4)return nullptr;
 auto& s=pes6DistributionGauges[pad];pes6_distribution_update(base,s,pad);
 return s.active&&s.actor==actor&&s.pending&&!s.consumed?&s:nullptr;
}
// Keep the generated selector hook compatible, without postponing native B actions.
inline uint32_t pes6_distribution_select(uint8_t*,uint32_t,uint32_t original){return original;}
// Wait at native action8 preparation, BEFORE target/type selection. Its ordinary
// footer accumulates actor201 at the configured60% rate and draws the native HUD.
inline bool pes6_distribution_wait_loft(uint8_t* base,uint32_t wrapper){
 const uint32_t actor=REX_LOAD_U32(REX_LOAD_U32(wrapper));
 if(!actor||REX_LOAD_U8(actor+160)!=0)return false;
 const uint32_t keys=REX_LOAD_U32(actor+280);
 return (keys&0x2000u)&&!(keys&0xD200u)&&REX_LOAD_U8(actor+201)<128;
}
// Called only inside native action9 state1, after initialization/cancellation and
// before its animation/target preparation. Action8 and its state2 fallback into9
// never start this gauge: long switches keep their existing native charge.
inline bool pes6_distribution_wait_cross(uint8_t* base,uint32_t wrapper){
 const uint32_t actor=REX_LOAD_U32(REX_LOAD_U32(wrapper));
 if(!actor||REX_LOAD_U8(actor+160)!=0)return false;
 const unsigned pad=REX_LOAD_U8(actor+195);if(pad>=4)return false;
 const uint32_t match=pes6_gameplay_match(base);if(!match)return false;
 auto& s=pes6DistributionGauges[pad];if(s.match&&s.match!=match)s={};
 pes6_distribution_update(base,s,pad);
 const uint32_t keys=pes6_gameplay_pad_held(base,pad);
 if(!s.active&&!s.latched&&REX_LOAD_U8(actor+144)==9&&REX_LOAD_U16(actor+146)==1&&
    REX_LOAD_U8(match+244)==REX_LOAD_U8(actor+128)&&(keys&0x2000u)&&!(keys&0xD200u))
  pes6_distribution_start(base,s,actor,0x2000u,4,9);
 if(!s.active||s.actor!=actor||s.kind!=4)return false;
 REX_STORE_U8(actor+201,s.power);
 return s.held;
}
// Apply the released centre charge to initial horizontal velocity, inside the
// centre's own contact function. Keep its native direction, loft/flight time,
// spin and animation. Full charge is exactly100% of the original launch speed.
inline void pes6_distribution_cross_launch(uint8_t* base,PPCContext& ctx){
 auto* s=pes6_distribution_pending(base,ctx.r3.u32);
 if(!s||s->kind!=4||ctx.r5.s32<=0)return;
 const int64_t percent112=40*112+int64_t(s->power-16)*60;
 ctx.r5.s64=int64_t(ctx.r5.s32)*percent112/(100*112);
 s->pending=false;s->consumed=true;s->release=REX_LOAD_U32(s->match+2152);
}
// Called after native human input. Only the distribution button is postponed;
// direction, camera, animation, clock and game state continue to run normally.
inline void pes6_distribution_input(uint8_t* base,uint32_t actor){
 if(!actor||REX_LOAD_U8(actor+160)!=0)return;
 const unsigned pad=REX_LOAD_U8(actor+195);if(pad>=4)return;
 const uint32_t match=pes6_gameplay_match(base);if(!match)return;
 auto& s=pes6DistributionGauges[pad];if(s.match&&s.match!=match)s={};
 pes6_distribution_update(base,s,pad);
 const unsigned action=pes6_distribution_action(base,actor),state=REX_LOAD_U16(actor+146);
 const bool hands=pes6_distribution_hands(base,match,actor);
 const bool restart=(action==44&&state==5)||(action==45&&state==1);
 if(!s.active&&!s.latched&&(hands||restart)){
  // Native pressed/held words, not a guessed physical trigger mapping.
  const uint32_t pressed=REX_LOAD_U32(actor+284)&REX_LOAD_U32(actor+280)&0xF000u;
  if(pressed&&!(pressed&(pressed-1u)))pes6_distribution_start(base,s,actor,pressed,hands?2:3,action);
 }
 if(s.actor!=actor||s.kind==1||s.kind==4||(!s.active&&!s.latched))return;
 if(s.held||s.latched||s.pending){
  REX_STORE_U32(actor+280,REX_LOAD_U32(actor+280)&~s.button);
  REX_STORE_U32(actor+284,REX_LOAD_U32(actor+284)&~s.button);
 }
 const uint32_t tick=REX_LOAD_U32(match+2152);
 if(s.pending&&(!s.delivered||s.deliveryTick==tick)){
  s.delivered=true;s.deliveryTick=tick;
  REX_STORE_U32(actor+280,REX_LOAD_U32(actor+280)|s.button);
  REX_STORE_U32(actor+284,REX_LOAD_U32(actor+284)|s.button);
 }
}
// Legacy generated entry hooks remain harmless; native action201 is authoritative.
inline void pes6_distribution_native_power(uint8_t*,uint32_t){}
inline void pes6_distribution_launch(uint8_t* base,PPCContext& ctx){
 auto* s=pes6_distribution_pending(base,ctx.r3.u32);if(!s||s->kind==4)return;
 // Goal-kick setup can launch a tiny positioning touch before the real delivery.
 if(ctx.lr==0x824AECE8u||ctx.r5.s32<=0)return;
 if(s->kind==1){
  const unsigned action=pes6_distribution_action(base,s->actor);
  if(action!=8&&action!=9&&action!=10&&action!=65&&REX_LOAD_U16(s->actor+148)!=s->action)return;
 }
 // Centres already use the released native201 charge; keeper/restart routes
 // without native power get a continuous40..100% speed profile from the same bar.
 const int64_t percent112=s->kind==1?100*112:40*112+int64_t(s->power-16)*60;
 ctx.r5.s64=int64_t(ctx.r5.s32)*percent112/(100*112);
 s->pending=false;s->consumed=true;s->release=REX_LOAD_U32(s->match+2152);
}
inline void pes6_distribution_hud(uint8_t* base,uint32_t team){
 for(unsigned pad=0;pad<4;++pad){auto& s=pes6DistributionGauges[pad];pes6_distribution_update(base,s,pad);
  if(!s.active||s.team!=team)continue;
  union {float value;uint32_t bits;} f{};f.value=float(s.power)/128.0f;REX_STORE_U32(team+13780,f.bits);
 }
}
