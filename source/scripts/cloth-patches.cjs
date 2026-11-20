function patchCloth(source){
 source=source.replaceAll('ctx.r31.u64 = pes6_cloth_projected_pair(ctx,base,ctx.r3.u32,ctx.r4.u32);','ctx.r31.u64 = ctx.r4.u64;');
 source=source.replaceAll('uint32_t pes6_cloth_palette_matrix(uint8_t*,uint32_t,uint32_t,uint32_t);','uint32_t pes6_cloth_palette_matrix(uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);')
  .replaceAll('ctx.r5.u64 = pes6_cloth_palette_matrix(base,ctx.r10.u32,ctx.r11.u32>>6,ctx.r1.u32+4208);','ctx.r5.u64 = pes6_cloth_palette_matrix(base,ctx.r10.u32,ctx.r11.u32>>6,ctx.r1.u32+4208,ctx.r30.u32,ctx.r28.u32/2);')
  .replaceAll('ctx.r11.u64 = pes6_cloth_palette_matrix(base,ctx.r10.u32,ctx.r11.u32>>6,ctx.r1.u32+4208);','ctx.r11.u64 = pes6_cloth_palette_matrix(base,ctx.r10.u32,ctx.r11.u32>>6,ctx.r1.u32+4208,ctx.r30.u32,(ctx.r8.u32-2)/2);');
 // Upgrade the old pre-skin integration in-place. The native function must
 // finish its own matrix conversion before any cloth correction is applied.
 const key='DEFINE_REX_FUNC(sub_821A6500) {',begin=source.indexOf(key);
 if(begin>=0){const end=source.indexOf('\n}\n',begin);let fn=source.slice(begin,end);
  if(fn.includes('pes6_cloth_pose(base,ctx.r31.u32,pes6ClothBoneCount);')&&!fn.includes('PES6 cloth post-native skinning v6')){
   fn=fn.replace('\n\tpes6_cloth_pose(base,ctx.r31.u32,pes6ClothBoneCount);','');
   if(!fn.includes('loc_821A6650:\n'))throw Error('Cloth final skin anchor missing');
   fn=fn.replace('loc_821A6650:\n','loc_821A6650:\n\t// PES6 cloth post-native skinning v6\n\tpes6_cloth_pose(base,ctx.r31.u32,pes6ClothBoneCount);\n');
  }
  fn=fn.replace('pes6ClothBoneCount=ctx.r5.u32;','pes6ClothBoneCount=ctx.r5.u32&0xffffu;');
  source=source.slice(0,begin)+fn+source.slice(end);
 }
 const declarations=[];
 function edit(address,modify){const entry=`DEFINE_REX_FUNC(sub_${address}) {\n\tREX_FUNC_PROLOGUE();`,start=source.indexOf(entry);if(start<0)return;const end=source.indexOf('\n}\n',start);if(end<0)throw Error('Cloth function end');let fn=source.slice(start,end);if(fn.includes('// PES6 reactive shirt v1'))return;fn=modify(fn,entry);fn=fn.replace(entry,entry+'\n\t// PES6 reactive shirt v1');source=source.slice(0,start)+fn+source.slice(end);}
 edit('821A6500',(fn,entry)=>{const at='\tsub_821EEC98(ctx, base);';if(!fn.includes(at)||!fn.includes('loc_821A6650:\n'))throw Error('Cloth skin pipeline anchor');declarations.push('void pes6_cloth_pose(uint8_t*,uint32_t,uint32_t);');return fn.replace(entry,entry+'\n\tuint32_t pes6ClothBoneCount=0;').replace(at,'\tpes6ClothBoneCount=ctx.r5.u32&0xffffu;\n'+at).replace('loc_821A6650:\n','loc_821A6650:\n\t// PES6 cloth post-native skinning v6\n\tpes6_cloth_pose(base,ctx.r31.u32,pes6ClothBoneCount);\n');});
 edit('825DFAA0',fn=>{const at='\t// addi r1,r1,208';if(!fn.includes(at))throw Error('Cloth clock anchor');declarations.push('void pes6_cloth_tick(uint8_t*);');return fn.replace(at,'\tpes6_cloth_tick(base);\n'+at);});
 edit('8247A3B0',(fn,entry)=>{declarations.push('void pes6_cloth_replay(uint8_t*,uint32_t);');return fn.replace(entry,entry+'\n\tpes6_cloth_replay(base,ctx.r3.u32);');});
 edit('822F2010',(fn,entry)=>{declarations.push('void pes6_cloth_forget(uint8_t*,uint32_t);');return fn.replace(entry,entry+'\n\tpes6_cloth_forget(base,ctx.r3.u32);');});
 edit('82202890',(fn,entry)=>{
  const first='ctx.r5.u64 = ctx.r11.u64 + ctx.r10.u64;',second='ctx.r11.u64 = ctx.r11.u64 + ctx.r10.u64;';
  if(!fn.includes(first)||!fn.includes(second)||fn.includes('ctx.r1.u32 + 4208'))throw Error('Native palette anchors changed');
  declarations.push('uint32_t pes6_cloth_palette_matrix(uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);');
  return fn.replace(first,'ctx.r5.u64 = pes6_cloth_palette_matrix(base,ctx.r10.u32,ctx.r11.u32>>6,ctx.r1.u32+4208,ctx.r30.u32,ctx.r28.u32/2);')
   .replace(second,'ctx.r11.u64 = pes6_cloth_palette_matrix(base,ctx.r10.u32,ctx.r11.u32>>6,ctx.r1.u32+4208,ctx.r30.u32,(ctx.r8.u32-2)/2);');
 });
 return declarations.length?declarations.map(d=>'extern "C" '+d).join('\n')+'\n'+source:source;
}
module.exports={patchCloth};
