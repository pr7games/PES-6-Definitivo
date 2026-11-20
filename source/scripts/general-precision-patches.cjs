// Local error terms only: no global RNG, attribute, charge or receiver changes.
const marker='// PES6 general precision60 v1';
const signedSites={
 '823C38A8':['823C405C','823C686C','823C5600','823C5610','823C5628','823C7C80','823C8028','823C8034','823C804C'],
 '823E4F68':['823E5310','823E5EF0'],
 '8235C8C0':['82362638','82363674','82363FE8','82365024','8236532C'],
 '82408498':['82414208','8241479C'],
 '823660B0':['82366138']
};
const maskedSites=['8235CFBC','8235D9D4','8235EC98','8235FD50','8235FDDC','82360628','823606E8','82360F70','82360F8C','82364C80'];
module.exports=function patchGeneralPrecision(text){let touched=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(fn.includes(marker))return fn;
  let changed=false;
  function insert(anchor,value){if(!fn.includes(anchor))throw Error('Precision anchor changed '+id+': '+anchor);fn=fn.replace(anchor,anchor+value);changed=true;}
  const percent=id==='823C38A8'?'pes6_pass_error_percent':id==='823E4F68'?'pes6_centre_error_percent':'pes6_touch_error_percent';
  for(const lr of signedSites[id]||[]){
   const target=['823C8028','823C8034'].includes(lr)?'82500C00':'82500C48';
   insert('ctx.lr = 0x'+lr+';\n\tsub_'+target+'(ctx, base);','\n\tctx.r3.s64=pes6_precision_error(ctx.r3.s32,'+percent+');');
  }
  if(id==='8235C8C0')for(const lr of maskedSites){
   const begin=fn.indexOf('ctx.lr = 0x'+lr+';');if(begin<0)throw Error('Touch site missing '+lr);
   const rest=fn.slice(begin),m=rest.match(/ctx\.r(10|11)\.u64 = ctx\.r3\.u32 & 0xFF;/);
   if(!m||m.index>1400)throw Error('Touch conversion changed '+lr);
   const end=begin+m.index+m[0].length;
   fn=fn.slice(0,end)+'\n\tctx.r'+m[1]+'.u64=uint32_t(pes6_precision_error(ctx.r'+m[1]+'.s32,pes6_touch_error_percent));'+fn.slice(end);changed=true;
  }
  if(id==='823FDC28')insert('loc_823FDF30:\n','\tctx.r11.u64=pes6_precision_heading(ctx.r28.u32,ctx.r11.u32,pes6_centre_error_percent);\n');
  if(id==='823EB1D8')insert('loc_823EB450:\n','\tctx.r11.u64=pes6_precision_heading(ctx.r31.u32,ctx.r11.u32,pes6_centre_error_percent);\n');
  if(!changed)return fn;touched=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t'+marker);
 });
 if(touched&&!text.includes('#include "../../src/general_precision.h"'))text='#include "../../src/general_precision.h"\n'+text;
 return text;
};
