// Persistent distribution charging; existing A/Y hooks and native actions retained.
module.exports=function patchDistribution(text){let touched=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(id==='823F6E80'){
   // Restore native running centres, including already-patched installations.
   // Long-ball action8 charging below is independent and remains enabled.
   return fn.replace(/\t\/\/ PES6 native centre preparation v3\r?\n\tif\(pes6_distribution_wait_cross\(base,ctx\.r29\.u32\)\)goto loc_823F7EF0;\r?\n/,'');
  }
  if(id==='823FC4A0'||id==='823EB1D8'){
   fn=fn.replace(/\t\/\/ PES6 native centre contact v3\r?\n\tpes6_distribution_cross_launch\(base,ctx\);\r?\n/,'');
  }
  if(id==='823E2070'||id==='823E2168'){
   if(fn.includes('// PES6 native loft preparation v2'))return fn;
   const anchor='loc_823E25E4:\n';
   if(!fn.includes(anchor))throw Error('Native loft preparation anchor changed');
   fn=fn.replace(anchor,anchor+'\t// PES6 native loft preparation v2\n\tif(pes6_distribution_wait_loft(base,ctx.r28.u32))goto loc_823E2E30;\n');
   touched=true;return fn;
  }
  if(fn.includes('// PES6 charged distributions80 v1'))return fn;
  function insert(anchor,value){if(!fn.includes(anchor))throw Error('Distribution anchor changed '+id+': '+anchor);fn=fn.replace(anchor,anchor+value);}
  if(id==='822C1D60'){
   insert('REX_FUNC_PROLOGUE();','\n\tconst uint32_t pes6DistributionActor=REX_LOAD_U32(REX_LOAD_U32(ctx.r3.u32));');
   insert('loc_822C2058:\n','\tpes6_distribution_input(base,pes6DistributionActor);\n');
  }else if(id==='82344C80'){
   insert('ctx.r3.u64=pes6_gameplay_pass_decide(base,pes6PassActor,ctx.r3.u32);','\n\tctx.r3.u64=pes6_distribution_select(base,pes6PassActor,ctx.r3.u32);');
  }else if(id==='825E1500'){
   insert('REX_FUNC_PROLOGUE();','\n\tpes6_distribution_launch(base,ctx);');
  }else if(id==='824FF2A0'){
   insert('pes6_gameplay_pass_hud(base,ctx.r3.u32);','\n\tpes6_distribution_hud(base,ctx.r3.u32);');
  }else if(['823E4F68','823EB1D8','823FC4A0'].includes(id)){
   insert('REX_FUNC_PROLOGUE();','\n\tpes6_distribution_native_power(base,ctx.r3.u32);');
  }else return fn;
  touched=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 charged distributions80 v1');
 });
 if(touched&&!text.includes('#include "../../src/distribution_charge.h"'))text='#include "../../src/distribution_charge.h"\n'+text;
 return text;
};
