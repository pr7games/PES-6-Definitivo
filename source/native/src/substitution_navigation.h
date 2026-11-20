#pragma once
DECLARE_REX_FUNC(sub_8224E6D0);
DECLARE_REX_FUNC(sub_8224F628);
DECLARE_REX_FUNC(sub_8224E718);
DECLARE_REX_FUNC(sub_82241290);
DECLARE_REX_FUNC(sub_82728928);
DECLARE_REX_FUNC(sub_8272F650);
DECLARE_REX_FUNC(sub_8272EF98);

// Used only at the two player-list input calls in formation state 1
// (8225A928). Native input collection merges D-pad and left-stick directions.
// Native 8224F628 supplies the SAME position-compatibility colours as the UI;
// native 8224E718 updates cursor, scroll, callbacks and detail panels together.
inline bool pes6_substitution_jump(PPCContext& ctx,uint8_t* base,uint32_t team) {
  const uint32_t panel=ctx.r3.u32,pad=ctx.r4.u32;
  if(!team||!panel||pad>=4||REX_LOAD_U32(team+3712)!=panel||
     REX_LOAD_U8(team+22)>1)return false;
  const uint32_t manager=REX_LOAD_U32(0x83139808u);
  if(!manager)return false;
  const uint32_t keys=REX_LOAD_U32(manager+376+pad*4);
  // Preserve native priority for vertical moves, confirm, cancel and modifiers.
  if((keys&~12u)!=0||(keys&12u)==0||(keys&12u)==12u)return false;
  const uint32_t list=REX_LOAD_U32(panel+956);
  if(!list||!REX_LOAD_U32(list+68))return false;
  const unsigned count=REX_LOAD_U8(list+4),selected=REX_LOAD_U8(list+9);
  if(count<=11||count>32||ctx.r1.u32<0x11000)return false;
  PPCContext saved=ctx;
  ctx.r1.u64-=256;
  const uint32_t scratch=ctx.r1.u32+128;
  auto cursor=[&](){ctx.r3.u64=panel;sub_8224E6D0(ctx,base);return ctx.r3.u32;};
  const unsigned current=cursor();
  unsigned target=count;
  if((keys&8u)&&current<11){
    for(unsigned row=11;row<count;row++){
      // Same selectable-row gate used by native 8224E170 on confirmation.
      if(!REX_LOAD_U8(list+19+row))continue;
      const uint32_t squad=REX_LOAD_U32(panel+944);
      if(!squad)continue;
      const unsigned player=REX_LOAD_U8(squad+1+row);
      if(player>=32||REX_LOAD_U8(squad+496+player))continue;
      if(selected<11){
        REX_STORE_U32(scratch,0);
        ctx.r3.u64=panel;ctx.r4.u64=row;ctx.r5.u64=scratch;
        sub_8224F628(ctx,base);
        if(REX_LOAD_U8(scratch+3)){target=row;break;}
      }
    }
    // Navigation must always reach the bench, even when nobody matches or is
    // available for a swap. Native confirmation still enforces swap rules.
    if(target==count)target=11;
  }else if((keys&4u)&&current>=11&&current<count){
    target=selected<11?selected:0;
    if(selected>=11&&selected<count){
      for(unsigned row=0;row<11;row++){
        if(!REX_LOAD_U8(list+19+row))continue;
        REX_STORE_U32(scratch,0);
        ctx.r3.u64=panel;ctx.r4.u64=row;ctx.r5.u64=scratch;
        // With a bench player marked, the native resolver tests that player's
        // abilities against each starter's actual position (reverse lookup).
        sub_8224F628(ctx,base);
        if(REX_LOAD_U8(scratch+3)){target=row;break;}
      }
    }
  }
  if(target>=count||target==current){ctx=saved;return false;}
  // Anchor the viewport to the section, independently of the chosen player.
  ctx.r3.u64=REX_LOAD_U32(list+68);
  ctx.r4.u64=REX_LOAD_U32(ctx.r3.u32+448);
  sub_8272F650(ctx,base);
  const uint32_t widget=ctx.r3.u32;
  const unsigned top=(keys&8u)?11:0;
  const uint16_t rows=REX_LOAD_U16(widget+474);
  const unsigned visible=REX_LOAD_U8(list+5);
  // Permit an incomplete final page: native scrolling otherwise clamps the
  // bench back into the starters when fewer than a page of substitutes exist.
  if(rows<top+visible)REX_STORE_U16(widget+474,top+visible);
  ctx.r3.u64=widget;ctx.r4.u64=0;ctx.r5.u64=top;
  sub_8272EF98(ctx,base);
  REX_STORE_U16(widget+474,rows);
  ctx.r3.u64=panel;ctx.r4.u64=target;ctx.r5.u64=0;ctx.r6.u64=0;
  sub_8224E718(ctx,base);
  // Use the same menu movement sound as native list navigation.
  sub_82241290(ctx,base);
  ctx.r5.u64=REX_LOAD_U32(ctx.r3.u32+28);ctx.r4.u64=5;
  sub_82728928(ctx,base);
  ctx=saved;
  return true;
}
