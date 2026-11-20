// Persistent gameplay changes; patch only verified native power accumulators.
module.exports=function patchGameplay(text){
 const charge=new Map([['8234CE20',2],['824CA0C0',1],['823CE328',18],['823CE444',1],['8249F598',2]]);
 function upgradeCharge(fn,id){
  let count=0;
  fn=fn.replace(/pes6_gameplay_charge_step\((4|8|16)\); \/\/ PES6 real charge x0\.75/g,(all,amount,offset)=>{
   const prefix=fn.slice(Math.max(0,offset-550),offset);
   const loads=[...prefix.matchAll(/REX_LOAD_U8\(ctx\.r(\d+)\.u32 \+ 201\)/g)];
   const actor=id==='8249F598'?'11':loads.at(-1)?.[1];
   if(!actor)throw Error('Charge actor anchor changed '+id);count++;
   return 'pes6_gameplay_charge_step(base,ctx.r'+actor+'.u32,'+amount+'); // PES6 real charge x0.70';
  });
  if(count!==charge.get(id))throw Error('Gameplay70 count '+id+': '+count);return fn;
 }
 let touched=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(fn.includes('// PES6 gameplay error1 charge70 v3'))return fn;
  if(fn.includes('// PES6 gameplay75 pass v2')||fn.includes('// PES6 gameplay half v1')){
   fn=fn.replace(/(ctx\.r\d+\.s64 = ctx\.r\d+\.s64 \+ )(\d+); \/\/ PES6 real charge x0\.5/g,(_,a,n)=>a+'pes6_gameplay_charge_step('+Number(n)*2+'); // PES6 real charge x0.75');
   if(charge.has(id))fn=upgradeCharge(fn,id);
   if(id==='8234B248')fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_gameplay_charge_reset(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r3.u32)));');
   touched=true;return fn.replace(/PES6 gameplay(?:75 pass v2| half v1)/,'PES6 gameplay error1 charge70 v3');
  }
  if(charge.has(id)){
   let count=0;
   if(id==='8249F598'){
    fn=fn.replace(/(\/\/ addi r10,r10,)(8|4)(\n\tctx\.r10\.s64 = ctx\.r10\.s64 \+ )(8|4);/g,(all,a,n,b,m)=>{if(n!==m)throw Error('Charge operand mismatch');count++;return a+n+b+'pes6_gameplay_charge_step('+n+'); // PES6 real charge x0.75';});
   }else{
    fn=fn.replace(/(ctx\.r(\d+)\.u64 = REX_LOAD_U8\([^\n]+ \+ 201\);\n\t\/\/ addi r\2,r\2,)(8|16)(\n\tctx\.r\2\.s64 = ctx\.r\2\.s64 \+ )\3;/g,(all,a,r,n,b)=>{count++;return a+n+b+'pes6_gameplay_charge_step('+n+'); // PES6 real charge x0.75';});
   }
   if(count!==charge.get(id))throw Error('Gameplay charge anchors changed '+id+': '+count);
   fn=upgradeCharge(fn,id);
   }else if(id==='8234B248'){
   const entry='REX_FUNC_PROLOGUE();';
   fn=fn.replace(entry,entry+'\n\tpes6_gameplay_charge_reset(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r3.u32)));\n\tpes6_gameplay_pass_bar(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r3.u32)));');
  }else if(id==='824FF2A0'){
   fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_gameplay_pass_hud(base,ctx.r3.u32);');
  }else if(id==='8244F710'){
   const anchor='loc_824500B0:\n';
   if(!fn.includes(anchor))throw Error('Normal shot error anchor changed');
   fn=fn.replace(anchor,anchor+'\tctx.r11.u64=pes6_gameplay_half_heading(ctx.r31.u32,ctx.r11.u32);\n');
  }else if(id==='8244E880'||id==='8244F538'){
   fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tPes6ShotAimScope pes6ShotAimScope;');
   if(id==='8244F538'){
    let count=0;
    fn=fn.replace(/(ctx\.r3\.u64 = uint32_t\([^\n]+\);\n\t\/\/ bl 0x82500c48)/g,(all)=>{count++;return all.replace('\n\t// bl','\n\tctx.r3.u64=uint32_t(pes6_gameplay_scale_error(ctx.r3.s32));\n\t// bl');});
    if(count!==2)throw Error('Central shot target anchors changed '+count);
   }
  }else if(id==='82451ED0'){
   const anchor='\t// addi r1,r1,128';
   if(!fn.includes(anchor))throw Error('Shot target return anchor changed');
   fn=fn.replace(anchor,'\tif(pes6ShotAimDepth){const int32_t delta=int32_t(ctx.r3.u32&65535u)-688;ctx.r3.u64=uint32_t(688+pes6_gameplay_scale_error(delta))&65535u;}\n'+anchor);
  }else if(id==='8245E2F8'){
   const anchor='loc_8245E750:\n\t// mr r3,r26';
   if(!fn.includes(anchor))throw Error('Dispersion anchor changed');
   fn=fn.replace(anchor,'loc_8245E750:\n\tctx.r11.u64 = pes6_gameplay_half_heading(ctx.r27.u32,ctx.r11.u32);\n\t// mr r3,r26');
  }else if(id==='82301670'){
   // Internal dispatcher block must be compiled with its patched parent.
  }else if(id==='823015E8'){
   fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6GameplayActor=ctx.r3.u32;');
   const anchor='loc_82301EEC:\n';
   if(!fn.includes(anchor))throw Error('Pass HUD anchor changed');
   fn=fn.replace(anchor,anchor+'\tpes6_gameplay_pass_bar(base,pes6GameplayActor);\n');
  }else return fn;
  touched=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 gameplay error1 charge70 v3');
 });
 if(touched&&!text.includes('#include "../../src/gameplay_tuning.h"'))text='#include "../../src/gameplay_tuning.h"\n'+text;
 return text;
};
