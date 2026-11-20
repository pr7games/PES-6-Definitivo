'use strict';
function patchCompetitionPresentation(source){
 const patch=(address,marker,change)=>{const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;const end=source.indexOf('\n}\n',start),old=source.slice(start,end);if(old.includes(marker))return;const next=change(old);if(next===old)throw Error('Competition presentation anchor missing '+address);source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// '+marker)+source.slice(end);};
 patch('82714328','PES6 unrestricted competition ball catalog',fn=>fn.replace(/\n\tctx.r10.u64 = REX_LOAD_U32[\s\S]*$/,'\n\tctx.r4.u64=1;\n\tsub_827142F0(ctx,base);\n\treturn;'));
 patch('82714548','PES6 unrestricted competition ball preview',fn=>fn.replace(/\n\tPPCRegister temp\{\};[\s\S]*$/,'\n\tctx.r6.u64=1;\n\tsub_82714488(ctx,base);\n\treturn;'));
 patch('82714348','PES6 competition initial ball',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tctx.r4.u64=pes6_competition_default_ball(base,ctx.r4.u32);'));
 patch('82706E50','PES6 editable competition ball',fn=>{const at=fn.indexOf('loc_82707174:');if(at<0)throw Error('Missing competition ball section');const head=fn.slice(0,at),tail=fn.slice(at);const anchor='ctx.r10.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x2;';if(!tail.includes(anchor))throw Error('Competition ball lock changed');return head+tail.replace(anchor,'ctx.r10.u64 = 0;');});
 patch('82160808','PES6 competition commercial panels',fn=>{const anchor='ctx.lr = 0x821608E8;\n\tsub_8214D5D0(ctx, base);';if(!fn.includes(anchor))throw Error('Native adboard panel reader changed');return fn.replace(anchor,'ctx.r4.u64=pes6_competition_image(ctx,base,ctx.r4.u32,true);\n\t'+anchor);});
 patch('8218E880','PES6 competition imported venue adverts',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tctx.r3.u64=pes6_competition_image(ctx,base,ctx.r3.u32,false);'));
 if(source.includes('pes6_competition_default_ball(base,')&&!source.includes('extern "C" uint32_t pes6_competition_default_ball'))source='extern "C" uint32_t pes6_competition_default_ball(uint8_t*,uint32_t);\n'+source;
 if(source.includes('pes6_competition_image(ctx,')&&!source.includes('extern "C" uint32_t pes6_competition_image'))source='extern "C" uint32_t pes6_competition_image(PPCContext&,uint8_t*,uint32_t,bool);\n'+source;
 for(const [marker,callee] of [['PES6 unrestricted competition ball catalog','827142F0'],['PES6 unrestricted competition ball preview','82714488']])if(source.includes(marker)&&!source.includes('DECLARE_REX_FUNC(sub_'+callee+');'))source='DECLARE_REX_FUNC(sub_'+callee+');\n'+source;
 for(const [address,kind] of [['8218E880',1],['8218F4A8',2],['8272A848',3],['8272CFB8',4],['82735B20',5],['821A5188',6],['82463438',7],['8272E980',8],['8219E398',10],['8212D2C8',11],['821A6360',12],['821A6A28',13],['8214D5D0',14]])patch(address,'PES6 replay transition trace '+kind,fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_replay_transition_trace(ctx,base,'+kind+');'));
 if(source.includes('pes6_replay_transition_trace(ctx,')&&!source.includes('extern "C" void pes6_replay_transition_trace'))source='extern "C" void pes6_replay_transition_trace(PPCContext&,uint8_t*,unsigned);\n'+source;
 return source;
}
module.exports={patchCompetitionPresentation};
