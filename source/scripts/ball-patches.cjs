'use strict';
function patchBalls(source){
 const patch=(address,change)=>{
  const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
  const end=source.indexOf('\n}\n',start),old=source.slice(start,end);
  if(old.includes('// PES6 Aerow variants v1'))return;
  const next=change(old);if(next===old)throw Error('Missing ball anchor '+address);
  source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 Aerow variants v1')+source.slice(end);
 };
 patch('827142F0',fn=>fn.replace('ctx.r4.s64 = ctx.r11.s64 + 25544;','ctx.r4.u64 = pes6_ball_table(ctx,base,2);').replace('ctx.r5.s64 = 16;','ctx.r5.u64 = pes6_ball_count();'));
 patch('82714348',fn=>fn.replace('ctx.r9.s64 = ctx.r11.s64 + 25340;','ctx.r9.u64 = pes6_ball_table(ctx,base,0);').replace('ctx.r10.s64 = 16;','ctx.r10.u64 = pes6_ball_count();'));
 patch('827143A8',fn=>{
  const at=fn.indexOf('loc_82714410:');if(at<0)throw Error('Ball Random branch missing');
  const a=fn.slice(0,at),b=fn.slice(at).replace('ctx.r3.u32, 15,','ctx.r3.u32, pes6_ball_count()-1,').replace('ctx.r4.s64 = 15;','ctx.r4.u64 = pes6_ball_count()-1;').replace('ctx.r11.s64 = ctx.r11.s64 + 25340;','ctx.r11.u64 = pes6_ball_table(ctx,base,0);');
  return a+b;
 });
 patch('82714450',fn=>fn.replace('ctx.r10.s64 = ctx.r10.s64 + 25544;','ctx.r10.u64 = pes6_ball_table(ctx,base,2);').replace('ctx.r11.u32, 16,','ctx.r11.u32, pes6_ball_count(),'));
 patch('82714488',fn=>{
  const at=fn.indexOf('loc_827144D0:');if(at<0)throw Error('Ball preview branch missing');
  return fn.slice(0,at)+fn.slice(at).replace('ctx.r30.s64 = 16;','ctx.r30.u64 = pes6_ball_count();').replace('ctx.r11.s64 = ctx.r11.s64 + 25416;','ctx.r11.u64 = pes6_ball_table(ctx,base,1);');
 });
 patch('827320A0',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tif(pes6_ball_name(ctx,base))return;'));
 patch('82643A48',fn=>fn.replace('ctx.r4.s64 = ctx.r10.s64 + 5720;','ctx.r4.u64 = pes6_ball_resource(ctx.r10.u32);'));
 patch('82745430',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tctx.r3.u64=pes6_ball_save_choice(ctx.r3.u32);'));
 patch('82745508',fn=>fn.replace('ctx.r3.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 22) & 0xF;','ctx.r3.u64 = pes6_ball_load_choice(__builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 22) & 0xF);'));
 if((source.includes('pes6_ball_save_choice(ctx')||source.includes('pes6_ball_load_choice(__builtin'))&&!source.includes('extern "C" uint32_t pes6_ball_save_choice'))source='extern "C" uint32_t pes6_ball_save_choice(uint32_t);\nextern "C" uint32_t pes6_ball_load_choice(uint32_t);\n'+source;
 if(source.includes('// PES6 Aerow variants v1')&&!source.includes('extern "C" uint32_t pes6_ball_table'))source='extern "C" uint32_t pes6_ball_table(PPCContext&,uint8_t*,unsigned);\nextern "C" uint32_t pes6_ball_count();\nextern "C" bool pes6_ball_name(PPCContext&,uint8_t*);\nextern "C" uint32_t pes6_ball_resource(uint32_t);\n'+source;
 // V2: expanded tables no longer have the original 204-byte displacement.
 const nameStart=source.indexOf('DEFINE_REX_FUNC(sub_82714450) {');
 if(nameStart>=0){const end=source.indexOf('\n}\n',nameStart),fn=source.slice(nameStart,end);source=source.slice(0,nameStart)+fn.replace('ctx.r8.s64 = ctx.r10.s64 + -204;','ctx.r8.u64 = pes6_ball_table(ctx,base,0);')+source.slice(end);}
 return source;
}
module.exports={patchBalls};
