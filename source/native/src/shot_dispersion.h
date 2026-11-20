#pragma once
#include "gameplay_tuning.h"
#include <unordered_map>
#include <algorithm>

// A reference calculation follows the selected native shot branch. It substitutes
// the centre of numerical RNG samples, never calls guest functions, never writes
// guest memory. Native calls, RNG sequence, speed r5 and effects stay authoritative.
// This is a branch-conditioned gameplay reference, not a guaranteed goal target.
struct Pes6ShotReference {
 PPCContext n; PPCRegister temp{}; uint32_t ea=0; uint8_t* base;
 struct Byte {uint8_t value, expected;};
 std::unordered_map<uint32_t,Byte> memory;
 uint32_t pending[16]{}; unsigned pendingCount=0;
 int32_t rng3=0; uint32_t rng4=0;
 Pes6ShotReference(const PPCContext& c,uint8_t* b):n(c),base(b){}
 uint8_t raw(uint32_t a) {return REX_LOAD_U8(a);}
 uint64_t load(uint32_t a,unsigned bytes) {
  uint64_t v=0;for(unsigned i=0;i<bytes;++i){auto it=memory.find(a+i);const auto real=raw(a+i);
   if(it!=memory.end()&&it->second.expected!=real){memory.erase(it);it=memory.end();}
   v=(v<<8)|(it==memory.end()?real:it->second.value);
  }return v;
 }
 void store(uint32_t a,uint64_t value,unsigned bytes) {
  for(unsigned i=0;i<bytes;++i)memory[a+i]={uint8_t(value>>((bytes-1-i)*8)),raw(a+i)};
  pending[pendingCount++]=a;pending[pendingCount++]=bytes;
 }
 void commit(){for(unsigned j=0;j<pendingCount;j+=2)for(unsigned i=0;i<pending[j+1];++i)memory[pending[j]+i].expected=raw(pending[j]+i);pendingCount=0;}
 void before(){rng3=n.r3.s32;rng4=n.r4.u32;}
 void after(const PPCContext& c,uint32_t target){
  n.r0=c.r0;
  n.r1=c.r1;
  n.r2=c.r2;
  n.r3=c.r3;
  n.r4=c.r4;
  n.r5=c.r5;
  n.r6=c.r6;
  n.r7=c.r7;
  n.r8=c.r8;
  n.r9=c.r9;
  n.r10=c.r10;
  n.r11=c.r11;
  n.r12=c.r12;
  n.f0=c.f0;
  n.f1=c.f1;
  n.f2=c.f2;
  n.f3=c.f3;
  n.f4=c.f4;
  n.f5=c.f5;
  n.f6=c.f6;
  n.f7=c.f7;
  n.f8=c.f8;
  n.f9=c.f9;
  n.f10=c.f10;
  n.f11=c.f11;
  n.f12=c.f12;
  n.f13=c.f13;
  n.lr=c.lr;n.ctr=c.ctr;n.xer=c.xer;
  n.cr0=c.cr0;n.cr1=c.cr1;n.cr2=c.cr2;n.cr3=c.cr3;n.cr4=c.cr4;n.cr5=c.cr5;n.cr6=c.cr6;n.cr7=c.cr7;
  if(target==0x82500C48u)n.r3.s64=0;
  else if(target==0x82500928u)n.r3.u64=rng4?((rng4-1u)/2u):0;
  else if(target==0x82500C00u||target==0x82271E60u){const int64_t v=rng3;const int64_t m=v<0?-v:v;n.r3.s64=(v<0?-1:1)*(m?((m-1)/2):0);}
 }
 void launch(PPCContext& c){
  const int64_t reference=n.r6.s32, error=int64_t(c.r6.s32)-reference;
  c.r6.s64=reference+error*int64_t(pes6_gameplay_error_percent)/100;
 }
};

// Penalty reference points come from the native successful branches: centre16,
// low6, high19; left/right lateral coordinate640, centre0. They preserve the
// selected corner. Failure excursions are contracted rather than disabling RNG.
inline unsigned pes6_penalty_style(uint8_t* base,uint32_t actor){return REX_LOAD_U32(actor+280)&0xE0u;}
inline void pes6_penalty_vertical(uint8_t* base,PPCContext& ctx,uint32_t actor){
 const unsigned style=pes6_penalty_style(base,actor);
 const int32_t nominal=(style==32||style==128)?38:((style==64||style==96||style==192)?12:32);
 ctx.r6.s64=nominal+(int64_t(ctx.r6.s32)-nominal)*int64_t(pes6_gameplay_error_percent)/100;
}
inline void pes6_penalty_lateral(uint8_t* base,PPCContext& ctx,uint32_t actor,int32_t side){
 const unsigned style=pes6_penalty_style(base,actor);
 const double nominal=(style==192||style==128)?-640.0*side:((style==96||style==32)?640.0*side:0.0);
 ctx.f4.f64=nominal+(ctx.f4.f64-nominal)*(double(pes6_gameplay_error_percent)/100.0);
}

// The normal vertical divisor has two probability-selected increments. Centre
// those too, rather than preserving their random9/11 branch as nominal.
inline int32_t pes6_shot_reference_denominator(uint8_t* base,const PPCContext& ctx){
 const uint32_t actor=REX_LOAD_U32(REX_LOAD_U32(ctx.r18.u32));
 const uint32_t packed=REX_LOAD_U32(REX_LOAD_U32(actor+628)+16);
 const unsigned skill=((packed<<7)|(packed>>25))&127u;
 const int n=skill<=30?0:int((skill-30)*100/70);
 const double p1=std::clamp(n-40,0,100)/100.0,p2=std::clamp(n-20,0,100)/100.0;
 const int range=n/25;
 const double extra=range>0?(range-1)/2.0:0.0;
 return int(std::lround(9.0+2.0*p1+p2*extra))-(ctx.r29.u32<=256?1:0);
}
