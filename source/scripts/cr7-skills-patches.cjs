module.exports=function(text){let changed=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(id==='82270390'||id==='82422A30'){const updated=fn.replace(/\*1\.30f/g,'*1.50f');if(updated!==fn){changed=true;fn=updated;}}
  if(id==='82270390'){
   if(fn.includes('// PES6 CR7 RB loop playback rate'))return fn;
   fn=fn.replace(/\n\t\/\/ PES6 CR7 LB stepover rate:[^\n]*\n\tif\(pes6_cr7_stepover[^\n]*\n/,'\n');
   const anchor='// stfs f0,8(r31)';if(!fn.includes(anchor))throw Error('Missing wrapper clock reset');changed=true;
   return fn.replace(anchor,'// PES6 CR7 RB loop playback rate: scale fresh native clock, not transition blend.\n\tif(pes6_cr7_stepover(base,ctx.r31.u32,ctx.r30.u32))ctx.f0.f64=double(float(ctx.f0.f64*1.50f));\n\t'+anchor);
  }
  if(id==='82422A30'){
   const old=fn;fn=fn.replace(/\n\t\/\/ PES6 CR7 test stepover playback130:[^\n]*\n\t\{const auto ref=[^\n]*\n/,'\n');
   if(!fn.includes('// PES6 commanded elastica immediate')){
    const anchor='loc_824253D0:\n';if(!fn.includes(anchor))throw Error('Missing elastica init');
    fn=fn.replace(anchor,anchor+'\t// PES6 commanded elastica immediate: skip native approach/direction admission.\n\t{const auto ref=REX_LOAD_U32(ctx.r31.u32);const auto actor=ref?REX_LOAD_U32(ref):0;if(pes6_elastica_no_error(base,actor)){ctx.r23.u64=0;ctx.r9.u64=actor;goto loc_82425550;}}\n');
    const admission='ctx.lr = 0x82425AC8;\n\tsub_8230D520(ctx, base);';if(!fn.includes(admission))throw Error('Missing elastica admission');
    fn=fn.replace(admission,admission+'\n\tif(pes6_elastica_no_error(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r31.u32))))ctx.r3.u64=1;');
   }
   if(!fn.includes('// PES6 CR7 individual playback rate')){
    const rate='ctx.lr = 0x824258F8;\n\tsub_82382A08(ctx, base);';if(!fn.includes(rate))throw Error('Missing individual stepover clock');
    fn=fn.replace(rate,rate+'\n\t// PES6 CR7 individual playback rate: replace computed native clock once.\n\t{const auto ref=REX_LOAD_U32(ctx.r31.u32);const auto actor=ref?REX_LOAD_U32(ref):0;if(actor&&REX_LOAD_U16(actor+132)==446)ctx.f1.f64=double(float(ctx.f1.f64*1.50f));}');
   }if(fn!==old)changed=true;return fn;
  }
  if(id!=='823015E8')return fn;
  if(fn.includes('// PES6 CR7 Chop chord')){const old='const PPCContext saved=ctx;ctx.r4.u64=combo==2?64:1;ctx.r5.u64=0;sub_82301578(ctx,base);ctx=saved;',next='const PPCContext saved=ctx;if(combo==2||REX_LOAD_U8(actor+144)==4||REX_LOAD_U8(actor+144)==10){ctx.r4.u64=combo==2?64:1;ctx.r5.u64=0;sub_82301578(ctx,base);}ctx=saved;';if(fn.includes(old)){changed=true;return fn.replace(old,next);}return fn;}
  changed=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 CR7 Chop chord: reserve physical LT+X, then deliver one native recut on A.\n\t{const auto actor=ctx.r3.u32;const unsigned combo=pes6_cr7_combo(base,actor);if(combo){\n\t const PPCContext saved=ctx;if(combo==2||REX_LOAD_U8(actor+144)==4||REX_LOAD_U8(actor+144)==10){ctx.r4.u64=combo==2?64:1;ctx.r5.u64=0;sub_82301578(ctx,base);}ctx=saved;\n\t REX_STORE_U32(actor+280,REX_LOAD_U32(actor+280)&~0xF000u);REX_STORE_U32(actor+284,REX_LOAD_U32(actor+284)&~0xF000u);\n\t for(auto& pass:pes6PassGauges)if(pass.actor==actor)pass={};pes6_gameplay_charge_reset(base,actor);\n\t if(combo==2){pes6_cr7_chop_arm(base,actor);REX_STORE_U8(actor+198,1);REX_STORE_U16(actor+148,255);}\n\t }}');
 });if(changed&&!text.includes('#include "../../src/cr7_skills_input.h"'))text='#include "../../src/cr7_skills_input.h"\n'+text;return text;
};
