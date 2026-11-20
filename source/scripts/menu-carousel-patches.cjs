const fs=require('fs');
function patchMenuCarousel(source){
 // V28 portraits use the actual split atlas, without any position correction.
 source=source.replaceAll('// PES6 selected portraits left v27\n\tpes6_menu_portrait_project(ctx,base,ctx.r31.u32);','sub_8273BB98(ctx, base);');
 source=source.replace(/^extern "C" void pes6_menu_portrait_project\([^\n]*\);\r?\n/gm,'');
 // Retire the rejected V25/V26 geometry and scissor changes, including already
 // generated sources. V24 rotation/native entrance remains unchanged.
 source=source.replaceAll('// PES6 portrait overscan v25\n\tpes6_menu_portrait_upload(ctx,base,ctx.r31.u32);','sub_821936C0(ctx, base);');
 source=source.replaceAll('// PES6 portrait scoped scissor v26\n\tpes6_menu_portrait_submit(ctx,base);','sub_821AC3F0(ctx, base);');
 source=source.replace(/^extern "C" void pes6_menu_portrait_(?:upload|submit)\([^\n]*\);\r?\n/gm,'');
 function edit(address,marker,change,declarations){
  const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
  const end=source.indexOf('\n}\n',start);if(end<0)throw Error('Incomplete UI function '+address);
  let body=source.slice(start,end);if(body.includes(marker))return;
  const next=change(body);if(next===body)throw Error('Missing carousel anchor '+address);
  source=source.slice(0,start)+next+source.slice(end);
  for(const d of declarations)if(!source.includes(d))source=d+'\n'+source;
 }
 edit('82736390','PES6 menu carousel source',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 menu carousel source\n\tconst uint32_t pes6MenuSource=ctx.r3.u32;').replace('\t// addi r1,r1,112','\tpes6_menu_carousel_loaded(ctx,base,pes6MenuSource);\n\t// addi r1,r1,112'),['extern "C" void pes6_menu_carousel_loaded(PPCContext&,uint8_t*,uint32_t);']);
 edit('82736310','PES6 menu carousel release',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 menu carousel release\n\tpes6_menu_carousel_released(base,ctx.r3.u32);'),['extern "C" void pes6_menu_carousel_released(uint8_t*,uint32_t);']);
 edit('82732BD0','PES6 menu carousel draw',body=>body.replace('ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);','ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 160);\n\t// PES6 menu carousel draw\n\tctx.r11.u64 = pes6_menu_carousel_texture(ctx.r11.u32);'),['extern "C" uint32_t pes6_menu_carousel_texture(uint32_t);']);
 edit('8218E880','PES6 menu carousel texture recognition',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 menu carousel texture recognition\n\tconst uint32_t pes6MenuImage=ctx.r3.u32;').replace('loc_8218E9A0:\n','loc_8218E9A0:\n\tpes6_menu_carousel_gpu_loaded(ctx,base,pes6MenuImage);\n'),['extern "C" void pes6_menu_carousel_gpu_loaded(PPCContext&,uint8_t*,uint32_t);']);
 edit('82128760','PES6 menu carousel binding',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 menu carousel binding\n\tpes6_menu_carousel_bind(ctx,base);'),['extern "C" void pes6_menu_carousel_bind(PPCContext&,uint8_t*);']);
 edit('8212D2C8','PES6 menu carousel direct binding',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 menu carousel direct binding\n\tpes6_menu_carousel_bind(ctx,base);'),['extern "C" void pes6_menu_carousel_bind(PPCContext&,uint8_t*);']);
 edit('82732BD0','PES6 menu carousel transition v23',body=>body.replace('ctx.r11.u64 = pes6_menu_carousel_texture(ctx.r11.u32);','// PES6 menu carousel transition v23\n\tctx.r11.u64 = pes6_menu_carousel_draw(ctx,base,ctx.r31.u32,ctx.r11.u32);'),['extern "C" uint32_t pes6_menu_carousel_draw(PPCContext&,uint8_t*,uint32_t,uint32_t);']);
 edit('826F0650','PES6 native menu carousel controller v24',body=>body.replace('\t// addi r1,r1,112','\t// PES6 native menu carousel controller v24\n\tpes6_menu_carousel_tick(ctx,base,ctx.r31.u32);\n\t// addi r1,r1,112'),['extern "C" void pes6_menu_carousel_tick(PPCContext&,uint8_t*,uint32_t);']);
 edit('8272B270','PES6 equal-height portrait mesh v29',body=>body.replace('sub_8273BCE8(ctx, base);','// PES6 equal-height portrait mesh v29\n\tpes6_menu_portrait_mesh(ctx,base,ctx.r29.u32,ctx.r31.u32,0);').replace('sub_8273BEA0(ctx, base);','pes6_menu_portrait_mesh(ctx,base,ctx.r29.u32,ctx.r31.u32,1);'),['extern "C" void pes6_menu_portrait_mesh(PPCContext&,uint8_t*,uint32_t,uint32_t,uint32_t);']);
 edit('8218E880','PES6 loading logo source before conversion',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 loading logo source before conversion\n\tconst bool pes6LoadingLogoSource=pes6_loading_logo_source(base,ctx.r3.u32);').replace('loc_8218E9A0:\n','loc_8218E9A0:\n\tpes6_loading_logo_gpu(ctx,base,pes6LoadingLogoSource);\n'),['extern "C" bool pes6_loading_logo_source(uint8_t*,uint32_t);','extern "C" void pes6_loading_logo_gpu(PPCContext&,uint8_t*,bool);']);
 edit('8218E828','PES6 loading logo GPU release',body=>body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 loading logo GPU release\n\tpes6_loading_logo_gpu_released(ctx.r3.u32);'),['extern "C" void pes6_loading_logo_gpu_released(uint32_t);']);
 return source;
}
module.exports={patchMenuCarousel};
