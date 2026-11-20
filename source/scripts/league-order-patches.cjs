'use strict';
module.exports=function patchLeagueOrder(source){
 const transforms={
  '82704F38':fn=>fn.replace('ctx.r10.s64 = ctx.r4.s16;','ctx.r10.u64 = pes6_league_index(base,ctx.r3.u32,ctx.r4.u32 & 0xffffu);'),
  '82704AE0':fn=>fn.replace('ctx.r11.s64 = ctx.r4.s16;','ctx.r11.u64 = pes6_league_index(base,ctx.r3.u32,ctx.r4.u32 & 0xffffu);'),
  '82705C08':fn=>fn.replace('ctx.r11.s64 = int16_t(REX_LOAD_U16(ctx.r31.u32 + 176));','ctx.r11.u64 = pes6_league_index(base,ctx.r31.u32,REX_LOAD_U16(ctx.r31.u32 + 176));'),
  '82706730':fn=>fn.replaceAll('ctx.r9.u64 = REX_LOAD_U32(ctx.r11.u32 + ctx.r29.u32);','ctx.r9.u64 = REX_LOAD_U32(ctx.r29.u32 + 4u*pes6_league_index(base,ctx.r31.u32,ctx.r30.u32));')
 };
 let changed=false;
 for(const [address,transform]of Object.entries(transforms)){
  const start=source.indexOf('DEFINE_REX_FUNC(sub_'+address+') {');if(start<0)continue;
  const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);if(fn.includes('pes6_league_index'))continue;
  const patched=transform(fn);if(patched===fn)throw Error('Missing league order anchor '+address);
  source=source.slice(0,start)+patched+source.slice(end);changed=true;
 }
 if(changed&&!source.includes('extern "C" uint32_t pes6_league_index'))source='extern "C" uint32_t pes6_league_index(uint8_t*,uint32_t,uint32_t);\n'+source;
 return source;
};
