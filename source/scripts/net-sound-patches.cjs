function patchNetSound(source){
 const decl=new Set();
 function edit(address,modify){const entry=`DEFINE_REX_FUNC(sub_${address}) {\n\tREX_FUNC_PROLOGUE();`,start=source.indexOf(entry);if(start<0)return;const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);if(fn.includes('// PES6 original net sound'))return;fn=modify(fn,entry);source=source.slice(0,start)+fn+source.slice(end);}
 edit('825DFAA0',(fn)=>{const at='\t// addi r1,r1,208';if(!fn.includes(at))throw Error('Net sound live tick anchor');decl.add('void pes6_net_sound_tick(uint8_t*);');return fn.replace(at,'\t// PES6 original net sound live\n\tpes6_net_sound_tick(base);\n'+at);});
 edit('8247A3B0',(fn,entry)=>{const at='\t// addi r1,r1,144';if(!fn.includes(at))throw Error('Net sound replay anchor');decl.add('void pes6_net_sound_replay(uint8_t*,uint32_t);');return fn.replace(entry,entry+'\n\tconst uint32_t netSoundReplayManager=ctx.r3.u32;').replace(at,'\t// PES6 original net sound replay\n\tpes6_net_sound_replay(base,netSoundReplayManager);\n'+at);});
 edit('82188408',(fn,entry)=>{decl.add('void pes6_net_sound_cue(uint32_t);');return fn.replace(entry,entry+'\n\t// PES6 original net sound post guard\n\tpes6_net_sound_cue(ctx.r3.u32);');});
 if(decl.size)source=[...decl].map(d=>'extern "C" '+d).join('\n')+'\n'+source;return source;
}
module.exports={patchNetSound};
