#pragma once
#include <cstdint>
#include <cmath>
#include <climits>
#include "../generated/default/pes6_pch.h"

// Native angles wrap at65536. Error100% restores the original completed error, including penalties,
// rather than changing attributes or changing the random generator's sequence.
inline constexpr unsigned pes6_gameplay_error_percent=40;
inline constexpr unsigned pes6_gameplay_charge_percent=60;
inline int32_t pes6_gameplay_scale_error(int32_t error){return error*int32_t(pes6_gameplay_error_percent)/100;}
inline uint32_t pes6_gameplay_half_heading(uint32_t aim,uint32_t result){
 if constexpr(pes6_gameplay_error_percent==100)return result;
 const uint32_t wrapped=(result-aim)&65535u;
 const int32_t error=wrapped>=32768u?int32_t(wrapped)-65536:int32_t(wrapped);
 return uint32_t(int32_t(aim&65535u)+pes6_gameplay_scale_error(error))&65535u;
}

// Standard held-shot path823CE328/823CE444: initial16, original step8.
// Both real shot increments and cosmetic A progress use this shared percentage.
inline constexpr unsigned pes6_gameplay_charge_total(unsigned original,unsigned ticks){return original*ticks*pes6_gameplay_charge_percent/100;}
struct Pes6ChargeFraction{uint32_t actor=0,match=0,tick=0;unsigned expected=0,remainder=0;};
inline Pes6ChargeFraction pes6ChargeFractions[32]{};
inline void pes6_gameplay_charge_reset(uint8_t* base,uint32_t actor){
 if(!actor)return;const unsigned index=REX_LOAD_U8(actor+128);if(index<32)pes6ChargeFractions[index]={};
}
inline unsigned pes6_gameplay_charge_step(uint8_t* base,uint32_t actor,unsigned original){
 const unsigned index=REX_LOAD_U8(actor+128),current=REX_LOAD_U8(actor+201);
 if(index>=32)return original*pes6_gameplay_charge_percent/100;
 const uint32_t match=REX_LOAD_U32(0x8313a948u),tick=match?REX_LOAD_U32(match+2152):0;
 auto& state=pes6ChargeFractions[index];
 if(state.actor!=actor||state.match!=match||state.expected!=current||tick-state.tick>2u)state={actor,match,tick,current,0};
 const unsigned scaled=original*pes6_gameplay_charge_percent+state.remainder;
 const unsigned step=scaled/100;state.remainder=scaled%100;state.tick=tick;
 state.expected=current+step>128?128:current+step;return step;
}
// Restrict shared target helper to shot-aim callers, preserving other uses.
inline thread_local unsigned pes6ShotAimDepth=0;
struct Pes6ShotAimScope{Pes6ShotAimScope(){++pes6ShotAimDepth;}~Pes6ShotAimScope(){--pes6ShotAimDepth;}};

// Charged human A/Y passes: input decision -> charge -> native receiver/launch.
struct Pes6PassGauge {
 uint32_t match=0,team=0,actor=0,start=0,last=0,release=0,button=0;
 unsigned power=0,action=0;bool active=false,held=false,pending=false,consumed=false,latched=false;
};
inline Pes6PassGauge pes6PassGauges[4]{};
inline uint32_t pes6_gameplay_pad_held(uint8_t* base,unsigned pad){return REX_LOAD_U32(0x83135440u+24u*pad);}
inline uint32_t pes6_gameplay_match(uint8_t* base){return REX_LOAD_U32(0x8313a948u);}
inline unsigned pes6_gameplay_pass_power(unsigned ticks){constexpr unsigned fullTicks=(11200u+8u*pes6_gameplay_charge_percent-1u)/(8u*pes6_gameplay_charge_percent);const unsigned p=ticks>=fullTicks?128:16+pes6_gameplay_charge_total(8,ticks);return p>128?128:p;}
inline void pes6_gameplay_pass_update(uint8_t* base,Pes6PassGauge& s,unsigned pad){
 const uint32_t match=pes6_gameplay_match(base);if(!match||match!=s.match){s={};return;}
 const uint32_t tick=REX_LOAD_U32(match+2152),keys=pes6_gameplay_pad_held(base,pad);
 if(tick-s.last>64u){s={};return;}
 if(s.latched&&(keys&s.button)==0)s.latched=false;
 if(!s.active)return;
 if(s.held){
  // Owner244 is the native possession actor. Cancel charge on a turnover/control change.
  if(REX_LOAD_U8(match+244)!=REX_LOAD_U8(s.actor+128)||REX_LOAD_U8(s.actor+160)!=0||REX_LOAD_U8(s.actor+195)!=pad||
     (keys&(0xF000u^s.button))!=0||(keys&0xA00u)==0xA00u){s={};return;}
  s.power=pes6_gameplay_pass_power(tick-s.start);
  if((keys&s.button)==0||s.power==128){s.held=false;s.release=tick;s.pending=true;s.latched=(keys&s.button)!=0;}
 }
 if(s.pending){const unsigned action=REX_LOAD_U8(s.actor+144);
  if(tick-s.release>60u||(REX_LOAD_U8(match+244)!=REX_LOAD_U8(s.actor+128)&&action!=5&&action!=6)){s={};return;}
 }
 if(s.consumed&&tick-s.release>6u)s.active=false;
 s.last=tick;
}
inline Pes6PassGauge* pes6_gameplay_pass_pending(uint8_t* base,uint32_t actor){
 if(!actor||REX_LOAD_U8(actor+160)!=0)return nullptr;
 const uint32_t match=pes6_gameplay_match(base);if(!match)return nullptr;
 const uint32_t tick=REX_LOAD_U32(match+2152);
 for(auto& s:pes6PassGauges)if(s.actor==actor&&s.match==match&&s.pending&&!s.consumed&&tick-s.release<=60u)return &s;
 return nullptr;
}
inline uint32_t pes6_gameplay_pass_select(uint8_t* base,uint32_t actor,uint32_t original){
 if(!actor||REX_LOAD_U8(actor+160)!=0)return original;
 const unsigned pad=REX_LOAD_U8(actor+195);if(pad>=4)return original;
 const uint32_t match=pes6_gameplay_match(base);if(!match)return original;
 const uint32_t tick=REX_LOAD_U32(match+2152),keys=pes6_gameplay_pad_held(base,pad);
 auto& s=pes6PassGauges[pad];
 if(s.match&&s.match!=match)s={};
 pes6_gameplay_pass_update(base,s,pad);
 if(s.actor&&s.actor!=actor){if(s.active||s.pending)s={};}
 if(s.pending&&s.actor==actor&&!s.consumed){
  // Keep one queued pass; repeated native selector calls cannot create another.
  return 255;
 }
 if(s.active&&s.actor==actor){
  if(s.held)return 255;
  if(s.consumed)return (s.latched&&(keys&s.button))?255:original;
 }
 if(s.latched&&(keys&s.button))return 255;
 // Do not turn defensive A/Y, lob/one-two combinations, AI, or native manual8 into charges.
 if((original!=5&&original!=6)||REX_LOAD_U8(match+244)!=REX_LOAD_U8(actor+128)||
    (keys&0xA00u)!=0||REX_LOAD_U8(actor+200)==8)return original;
 const uint32_t button=original==5?0x4000u:0x1000u;
 if((keys&button)==0||(keys&(0xF000u^button))!=0)return original;
 if(s.latched&&(keys&s.button))return 255;
 const unsigned team=(REX_LOAD_U32(actor+136)&0xC0000000u)==0x40000000u?1:0;
 s={};s.match=match;s.team=REX_LOAD_U32(match+2496+team*4);s.actor=actor;s.start=s.last=tick;
 s.button=button;s.action=original;s.power=16;s.active=s.held=true;
 return 255;
}
// Release delivery is separate from selection: update() may observe release from the HUD first.
inline uint32_t pes6_gameplay_pass_decide(uint8_t* base,uint32_t actor,uint32_t original){
 if(!actor||REX_LOAD_U8(actor+160)!=0)return original;
 const unsigned pad=REX_LOAD_U8(actor+195);if(pad>=4)return original;
 auto& s=pes6PassGauges[pad];pes6_gameplay_pass_update(base,s,pad);
 if(s.actor==actor&&s.pending&&!s.consumed){
  if(s.action&0x80000000u)return 255;
  const unsigned action=s.action;s.action|=0x80000000u;
  // Preserve the native action's button-dependent setup on this release tick only.
  REX_STORE_U32(actor+280,REX_LOAD_U32(actor+280)|s.button);
  return action;
 }
 return pes6_gameplay_pass_select(base,actor,original);
}
inline void pes6_gameplay_pass_bar(uint8_t*,uint32_t){} // legacy call sites: HUD owns real state now
inline void pes6_gameplay_pass_hud(uint8_t* base,uint32_t team){
 const uint32_t match=pes6_gameplay_match(base);if(!match)return;
 for(unsigned pad=0;pad<4;pad++){
  auto& s=pes6PassGauges[pad];pes6_gameplay_pass_update(base,s,pad);
  if(!s.active||s.team!=team)continue;
  union{float value;uint32_t bits;}f{};f.value=float(s.power)/128.0f;
  REX_STORE_U32(team+13780,f.bits);
 }
}
// Tuneable design values: low charge favours short receivers, full charge distant receivers.
inline unsigned pes6_gameplay_pass_range(unsigned charge){return 512u+(charge>16?charge-16:0)*48u;}
inline int32_t pes6_gameplay_pass_score(uint8_t* base,uint32_t actor,int32_t original,uint32_t distanceSquared,unsigned angle){
 auto* s=pes6_gameplay_pass_pending(base,actor);if(!s)return original;
 const int64_t distance=int64_t(std::sqrt(double(distanceSquared)));
 const int64_t delta=distance-int64_t(pes6_gameplay_pass_range(s->power));
 int64_t score=delta*delta+int64_t(angle)*angle*256+(original>0?original/16:0);
 return int32_t(score>INT32_MAX?INT32_MAX:score);
}
inline int32_t pes6_gameplay_pass_speed(uint8_t* base,uint32_t actor,int32_t original){
 auto* s=pes6_gameplay_pass_pending(base,actor);if(!s)return original;
 const unsigned action=REX_LOAD_U8(actor+144),mode=REX_LOAD_U8(actor+200);
 if((action!=5&&action!=6)||mode==8)return original;
 // Match HUD value at release. Tap65%; full145%: restored100% of the approved A/Y maximum.
 const int64_t numerator=65*112+int64_t(s->power-16)*80;
 int64_t speed=int64_t(original)*numerator/(100*112);
 if(speed<1024)speed=1024;if(speed>45056)speed=45056;
 s->pending=false;s->consumed=true;s->release=REX_LOAD_U32(s->match+2152);
 return int32_t(speed);
}
