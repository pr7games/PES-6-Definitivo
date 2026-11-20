// Native elastica461: physical double LT, separate from shoulder bicycles.
module.exports=function patchElastica(text){let touched=false;
 text=text.replace(/DEFINE_REX_FUNC\(sub_([0-9A-F]+)\) \{[\s\S]*?\n\}/g,(fn,id)=>{
  if(id==='82134BB8'&&!fn.includes('pes6_skills_raw(base,')){
   const anchor='sub_8281FAC0(ctx, base);';if(!fn.includes(anchor))throw Error('Missing XInput poll');
   fn=fn.replace(anchor,anchor+'\n\tpes6_skills_raw(base,ctx.r31.u32,ctx.r1.u32+128,ctx.r3.u32==0);');touched=true;
  }
  if(fn.includes('// PES6 elastica analog LT v4'))return fn;
  if(fn.includes('// PES6 elastica analog LT v3')){
   fn=fn.replace('loc_822C2C64:\n\tctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32);','loc_822C2C64:\n\tctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32,0);');
   fn=fn.replace('loc_822C2D8C:\n\tctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32);','loc_822C2D8C:\n\tctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32,2);');
   touched=true;return fn.replace('// PES6 elastica analog LT v3','// PES6 elastica analog LT v4');
  }
  function insert(anchor,extra){if(!fn.includes(anchor))throw Error('Elastica anchor '+id+' '+anchor);fn=fn.replace(anchor,anchor+extra);}
  if(id==='822C2BE8'){
   if(fn.includes('// PES6 elastica double LT v2')){
    fn=fn.replaceAll('ctx.r11.u64=ctx.r11.u32&~0x4000u;','ctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32);');
    fn=fn.replace('\tpes6_elastica_pad_capture(ctx.r26.u32,ctx.r27.u32);\n','');
    fn=fn.replace('ctx.r27.u64=ctx.r27.u32&~0x4000u;','ctx.r27.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r27.u32);');
    touched=true;return fn.replace('// PES6 elastica double LT v2','// PES6 elastica analog LT v3');
   }
   insert('loc_822C2C64:\n','\tctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32,0);\n');
   insert('loc_822C2D10:\n','\tctx.r27.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r27.u32);\n');
   insert('loc_822C2D8C:\n','\tctx.r11.u64=pes6_elastica_strip(base,ctx.r26.u32,ctx.r11.u32,2);\n');
  }else if(fn.includes('// PES6 elastica double LT v2')||(fn.includes('// PES6 elastica double LT v1')&&id!=='82422A98'))return fn;
  else if(id==='82134BB8'){

   insert('sub_8281FAC0(ctx, base);','\n\tpes6_elastica_raw(REX_LOAD_U32(ctx.r31.u32+16),ctx.r3.u32==0?REX_LOAD_U8(ctx.r1.u32+134):0);');
  }else if(id==='823015E8'){
   insert('REX_FUNC_PROLOGUE();','\n\tif(pes6_elastica_press(base,ctx.r3.u32)){\n\t const uint32_t actor=ctx.r3.u32;const PPCContext saved=ctx;\n\t ctx.r4.u64=64;ctx.r5.u64=0;sub_82301578(ctx,base);ctx=saved;\n\t REX_STORE_U8(actor+198,6);REX_STORE_U16(actor+148,255);\n\t for(auto& pass:pes6PassGauges)if(pass.actor==actor)pass={};\n\t}');
  }else if(id==='82422A98'){
   fn=fn.replace('\n\t// PES6 elastica double LT v1','');
   insert('loc_82422B24:\n','\tif(pes6_elastica_human(base,ctx.r7.u32))goto loc_82422B74;\n');
  }else if(id==='82422A30'){
   insert('loc_824255B8:\n','\t// Human elastica is exclusively double LT. AI retains the original selection.\n\tif(pes6_elastica_human(base,ctx.r9.u32))goto loc_824255BC;\n');
   insert('loc_82424BDC:\n\t// bl 0x82500c98\n\tctx.lr = 0x82424BE0;\n\tsub_82500C98(ctx, base);','\n\t// Preserve the RNG call, but remove only the human elastica failure result.\n\tif(pes6_elastica_no_error(base,REX_LOAD_U32(REX_LOAD_U32(ctx.r31.u32))))ctx.r3.u64=1;');
  }else return fn;
  touched=true;return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t'+(id==='822C2BE8'?'// PES6 elastica analog LT v3':'// PES6 elastica double LT v2'));
 });
 if(touched&&!text.includes('#include "../../src/elastica_input.h"'))text='#include "../../src/elastica_input.h"\n'+text;
 return text;
};
