// Shot error v1: reference arithmetic is generated beside native arithmetic.
// Native control flow/calls stay authoritative; reference writes are private.
const shotIds=new Set(['823D1CF0','8242E6A0','8241CD70','8242B7F0','82420628','8241B130','8241A288','8241E810']);
const marker='// PES6 shot dispersion60 reference v1';
function mirror(body){
 const addresses=[];
 body=body.replace(/REX_(LOAD|STORE)_U(8|16|32|64)\(/g,(head,kind,bits,offset,all)=>head); // parsed below
 let out='',cursor=0,re=/REX_(LOAD|STORE)_U(8|16|32|64)\(/g,m;
 while((m=re.exec(body))){let i=re.lastIndex,depth=1,comma=-1;for(;i<body.length&&depth;i++){const c=body[i];if(c==='(')depth++;else if(c===')')depth--;else if(c===','&&depth===1&&comma<0)comma=i;}
  if(depth)throw Error('Unbalanced memory expression');
  const end=i-1,addr=body.slice(re.lastIndex,comma<0?end:comma).replace(/\bea\b/g,'pes6Reference.ea');
  const key='PES6PINNEDADDRESS'+addresses.length;addresses.push(addr);
  out+=body.slice(cursor,m.index)+'pes6Reference.'+(m[1]==='LOAD'?'load('+key+','+Number(m[2])/8+')':'store('+key+','+body.slice(comma+1,end)+','+Number(m[2])/8+')');
  cursor=i;re.lastIndex=i;
 }
 out+=body.slice(cursor);
 out=out.replace(/\bctx\b/g,'pes6Reference.n').replace(/\btemp\b/g,'pes6Reference.temp').replace(/(?<!\.)\bea\b/g,'pes6Reference.ea');
 out=out.replace(/PES6PINNEDADDRESS(\d+)/g,(_,i)=>addresses[Number(i)]);
 return out;
}
function referenceFunction(fn,id){
 let launches=0;
 // Recognise native min/max clamps independently of actual sampled branch.
 const clamps=[];
 const clampRe=/\t\/\/ (cmpwi|cmplwi) (cr\d+,)?r(\d+),(-?\d+)\n[^\n]*\n\t\/\/ (ble|bge)(?: cr\d+,)?[^\n]*\n[^\n]*goto (loc_[A-F0-9]+);\n\t\/\/ li r\3,\4\n[^\n]*\n\6:/g;
 for(const m of fn.matchAll(clampRe))clamps.push({reg:m[3],value:m[4],label:m[6],max:m[5]==='ble',unsigned:m[1]==='cmplwi',li:'\t// li r'+m[3]+','+m[4]+'\n'});
 const liOffsets=new Set();
 for(const m of fn.matchAll(clampRe)){const start=m.index+ m[0].indexOf('\t// li ');liOffsets.add(start);}
 fn=fn.replace(/\t\/\/ [^\n]*\n[\s\S]*?(?=\t\/\/ |^loc_|^\}$)/gm,(block,offset)=>{
  const firstEnd=block.indexOf('\n')+1,comment=block.slice(0,firstEnd),body=block.slice(firstEnd);
  const direct=body.match(/\b(sub_([A-F0-9]+)|__savegprlr_\d+|__restgprlr_\d+)\(ctx, base\);/);
  if(direct){
   if(direct[1].startsWith('__rest'))return block;
   const target=direct[2]?'0x'+direct[2]+'u':'0u';
   let before='\tpes6Reference.before();\n';
   if(direct[2]==='825E1500'){before='\tpes6Reference.launch(ctx);\n'+before;launches++;}
   return comment+before+body+'\tpes6Reference.after(ctx,'+target+');\n';
  }
  if(/REX_CALL_INDIRECT_FUNC/.test(body))return comment+'\tpes6Reference.before();\n'+body+'\tpes6Reference.after(ctx,0);\n';
  if(/\b(goto|return|switch)\b/.test(body)||!body.trim()||liOffsets.has(offset))return block;
  if(/__builtin_trap|REX_TRAP/.test(body))return block;
  const duplicate=mirror(body);
  return comment+duplicate+body+(/REX_STORE_/.test(body)?'\tpes6Reference.commit();\n':'');
 });
 if(launches!==1)throw Error('Shot launch count '+id+': '+launches);
 for(const c of clamps){const field=c.unsigned?'u32':'s32';fn=fn.replace(c.label+':\n',c.label+':\n\tpes6Reference.n.r'+c.reg+'.'+field+'=std::'+(c.max?'min':'max')+'<'+(c.unsigned?'uint32_t':'int32_t')+'>(pes6Reference.n.r'+c.reg+'.'+field+','+c.value+');\n');}
 if(id==='823D1CF0')fn=fn.replace('loc_823D2754:\n','loc_823D2754:\n\tpes6Reference.n.r31.s64=pes6_shot_reference_denominator(base,ctx);\n');
 fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t'+marker+'\n\tPes6ShotReference pes6Reference(ctx,base);');
 return fn;
}
module.exports=function patchShotDispersion(text){let touched=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(fn.includes(marker))return fn;
  if(shotIds.has(id)){touched=true;return referenceFunction(fn,id);}
  if(id==='8249FC10'){
   const anchor='ctx.lr = 0x824A3360;\n\tsub_82500C48(ctx, base);';
   if(!fn.includes(anchor))throw Error('Free kick error anchor changed');
   fn=fn.replace(anchor,anchor+'\n\tctx.r3.s64=pes6_gameplay_scale_error(ctx.r3.s32);');
  }else if(id==='826AA568'){
   fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tint32_t pes6PenaltySide=1;');
   if(!fn.includes('loc_826AA810:\n'))throw Error('Penalty side anchor changed');
   fn=fn.replace('loc_826AA810:\n','loc_826AA810:\n\tpes6PenaltySide=ctx.r11.s16;\n');
   const lateral='ctx.lr = 0x826AADE8;\n\tsub_82302AF0(ctx, base);';
   if(!fn.includes(lateral))throw Error('Penalty lateral anchor changed');
   fn=fn.replace(lateral,'pes6_penalty_lateral(base,ctx,REX_LOAD_U32(REX_LOAD_U32(ctx.r30.u32)),pes6PenaltySide);\n\t'+lateral);
   const vertical='ctx.lr = 0x826AB608;\n\tsub_825E1500(ctx, base);';
   if(!fn.includes(vertical))throw Error('Penalty launch anchor changed');
   fn=fn.replace(vertical,'pes6_penalty_vertical(base,ctx,REX_LOAD_U32(REX_LOAD_U32(ctx.r30.u32)));\n\t'+vertical);
  }else return fn;
  touched=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t'+marker);
 });
 if(touched&&!text.includes('#include "../../src/shot_dispersion.h"'))text='#include "../../src/shot_dispersion.h"\n'+text;
 return text;
};
