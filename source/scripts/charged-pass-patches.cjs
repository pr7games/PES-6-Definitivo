// Charged A/Y input, native candidate scoring and pre-launch physical speed.
module.exports=function patchChargedPasses(text){let touched=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(fn.includes('// PES6 charged A Y v1'))return fn;
  if(id==='82344C80'){
   fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6PassActor=REX_LOAD_U32(REX_LOAD_U32(ctx.r3.u32));');
   const anchor='loc_82344CB4:\n';if(!fn.includes(anchor))throw Error('Pass selector exit changed');
   fn=fn.replace(anchor,anchor+'\tctx.r3.u64=pes6_gameplay_pass_decide(base,pes6PassActor,ctx.r3.u32);\n');
  }else if(id==='824441F0'){
   const anchor='loc_82444E70:\n';if(!fn.includes(anchor))throw Error('Pass candidate score changed');
   fn=fn.replace(anchor,anchor+'\tctx.r11.s64=pes6_gameplay_pass_score(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r26.u32)),ctx.r11.s32,REX_LOAD_U32(ctx.r1.u32+80),ctx.r27.u32&65535u);\n');
  }else if(id==='823C38A8'){
   const anchor='loc_823C87A8:\n';if(!fn.includes(anchor))throw Error('Pass physical launch changed');
   fn=fn.replace(anchor,anchor+'\tctx.r27.s64=pes6_gameplay_pass_speed(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r15.u32)),ctx.r27.s32);\n');
  }else return fn;
  touched=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 charged A Y v1');
 });
 if(touched&&!text.includes('#include "../../src/gameplay_tuning.h"'))text='#include "../../src/gameplay_tuning.h"\n'+text;
 return text;
};