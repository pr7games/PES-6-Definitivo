// Added after legacy weather migrations so fresh and installed code converge.
function patchTorrential(source){
  const declarations=new Set();
  function edit(address,change,version='v1'){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
    if(fn.includes('// PES6 torrential weather '+version))return;
    let next=change(fn);
    if(next===fn)throw Error('Missing torrential anchor '+address);
    next=next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 torrential weather '+version);
    source=source.slice(0,start)+next+source.slice(end);
  }
  edit('82151420',fn=>{
    const replacements=[[1032320,1528320],[21507,31840],[64520,95520],[129040,119040],[10000,24000],[60000,144000]];
    for(const [old,next] of replacements){
      const from=`(pes6_weather_enabled()?${old}:`;
      if(!fn.includes(from))throw Error('Missing torrential capacity '+old);
      fn=fn.replaceAll(from,`(pes6_weather_enabled()?${next}:`);
    }
    return fn;
  });
  edit('82151420',fn=>{
    declarations.add('uint32_t pes6_weather_drop_count();');
    // Reserve for the maximum, but initialize only the selected population.
    // Ordinary rain retains its original random-number consumption as well.
    return fn.replaceAll('ctx.r6.s32, (pes6_weather_enabled()?24000:2000)',
      'ctx.r6.s32, (pes6_weather_enabled()?pes6_weather_drop_count()*4:2000)')
      .replaceAll('ctx.r29.s32, (pes6_weather_enabled()?24000:2000)',
      'ctx.r29.s32, (pes6_weather_enabled()?pes6_weather_drop_count()*4:2000)');
  },'v2');
  edit('82151818',fn=>{
    declarations.add('uint32_t pes6_weather_drop_count();');
    return fn.replaceAll('(pes6_weather_enabled()?10000:2000)','(pes6_weather_enabled()?pes6_weather_drop_count()*4:2000)');
  });
  edit('82151A80',fn=>{
    declarations.add('uint32_t pes6_weather_drop_count();');
    return fn.replace('(pes6_weather_enabled()?60000:12000)','(pes6_weather_enabled()?pes6_weather_drop_count()*24:12000)');
  });
  edit('826FA198',fn=>{
    declarations.add('void pes6_weather_menu_ready(PPCContext&,uint8_t*,uint32_t);');
    const anchor='if (ctx.cr6.lt) goto loc_826FA28C;';
    return fn.replace(anchor,anchor+'\n\tpes6_weather_menu_ready(ctx,base,ctx.r31.u32);');
  });
  edit('826F9708',fn=>{
    declarations.add('bool pes6_weather_menu_resolve(uint8_t*,uint32_t);');
    declarations.add('void pes6_weather_menu_restore(uint8_t*,uint32_t,bool);');
    return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6WeatherChoice=ctx.r5.u32;\n\tconst bool pes6HeavyRain=pes6_weather_menu_resolve(base,pes6WeatherChoice);')
      .replace('sub_82178930(ctx, base);','sub_82178930(ctx, base);\n\tpes6_weather_menu_restore(base,pes6WeatherChoice,pes6HeavyRain);');
  });
  edit('826F99C0',fn=>fn.replace('ctx.r11.u64 = REX_LOAD_U8(ctx.r28.u32 + 326);',
    'ctx.r11.u64 = ctx.r27.u32==3?3:REX_LOAD_U8(ctx.r28.u32 + 326);'));
  // The editable environment snapshot is still a native 0/1 weather enum.
  edit('826F9600',fn=>fn.replace('REX_STORE_U8(ctx.r1.u32 + 90, ctx.r29.u8);',
    'REX_STORE_U8(ctx.r1.u32 + 90, ctx.r29.u32==3?1:ctx.r29.u8);'));
  // UI remembers the explicit choice while native match config stores rain.
  edit('826F9478',fn=>{
    declarations.add('bool pes6_torrential_selected();');
    return fn.replace('ctx.r31.u64 = ctx.r4.u64;',
      'ctx.r31.u64 = ctx.r4.u32==1&&pes6_torrential_selected()?3:ctx.r4.u64;');
  });
  edit('82151420',fn=>fn.replaceAll('?1528320:', '?1144320:').replaceAll('?31840:', '?23840:').replaceAll('?95520:', '?71520:'),'v3');
  edit('826F9478',fn=>{
    declarations.add('uint32_t pes6_weather_menu_row(uint32_t);');
    return fn.replace('ctx.r31.u64 = ctx.r4.u32==1&&pes6_torrential_selected()?3:ctx.r4.u64;',
      'ctx.r31.u64 = pes6_weather_menu_row(ctx.r4.u32);');
  },'v3');
  edit('826F9570',fn=>{
    declarations.add('uint32_t pes6_weather_menu_choice(uint32_t);');
    return fn.replace('REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);','REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);\n\tctx.r3.u64=pes6_weather_menu_choice(ctx.r3.u32);');
  },'v3');
  edit('826F9600',fn=>{
    declarations.add('uint32_t pes6_weather_menu_choice(uint32_t);');
    return fn.replace('ctx.r28.u64 = ctx.r3.u64;','ctx.r28.u64 = pes6_weather_menu_choice(ctx.r3.u32);');
  },'v3');
  edit('826F9F48',fn=>{
    declarations.add('uint32_t pes6_weather_menu_choice(uint32_t);');
    return fn.replace('REX_STORE_U8(ctx.r1.u32 + 80, ctx.r11.u8);','REX_STORE_U8(ctx.r1.u32 + 80, pes6_weather_menu_choice(ctx.r11.u32));');
  },'v3');
  edit('826F9308',fn=>{
    declarations.add('uint32_t pes6_weather_menu_key_index(uint32_t);');
    declarations.add('bool pes6_weather_menu_caption(PPCContext&,uint8_t*,uint32_t);');
    return fn.replace('ctx.r4.u64 = REX_LOAD_U32(ctx.r8.u32 + ctx.r11.u32);',
      'ctx.r4.u64 = REX_LOAD_U32(ctx.r8.u32 + (ctx.r30.u32==2?pes6_weather_menu_key_index(ctx.r11.u32/4)*4:ctx.r11.u32));')
      .replace('sub_82734390(ctx, base);','if(ctx.r30.u32!=2||!pes6_weather_menu_caption(ctx,base,ctx.r11.u32/4))sub_82734390(ctx, base);');
  },'v3');
  for(const [address,kind] of [['822324B0',0],['82232398',1]]){
    edit(address,fn=>{
      declarations.add('void pes6_weather_object_contact(uint8_t*,unsigned);');
      return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_weather_object_contact(base,'+kind+');');
    },'v3');
  }
  edit('82151420',fn=>fn.replaceAll('?1144320:', '?2416640:').replaceAll('?23840:', '?50347:')
    .replaceAll('?71520:', '?151040:').replaceAll('?24000:', '?32000:').replaceAll('?144000:', '?192000:'),'v4');
  edit('82151420',fn=>fn.replaceAll('?2416640:', '?1984640:').replaceAll('?50347:', '?41347:')
    .replaceAll('?151040:', '?124040:').replaceAll('?32000:', '?10000:').replaceAll('?192000:', '?60000:'),'v5');
  edit('82151420',fn=>fn.replaceAll('?1984640:', '?2176640:').replaceAll('?41347:', '?45347:')
    .replaceAll('?124040:', '?136040:').replaceAll('?119040:', '?131040:'),'v6');
  edit('82151420',fn=>fn.replaceAll('?2176640:', '?1984640:').replaceAll('?45347:', '?41347:')
    .replaceAll('?136040:', '?124040:').replaceAll('?131040:', '?119040:'),'v7');
  for(const address of ['826F9828','826F9ED0','826FA198'])edit(address,fn=>{
    declarations.add('void pes6_weather_normalize_saved(uint8_t*);');
    return fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_weather_normalize_saved(base);');
  },'v8');
  edit('82151420',fn=>fn.replaceAll('?1984640:', '?2224640:').replaceAll('?41347:', '?46347:')
    .replaceAll('?124040:', '?139040:').replaceAll('?10000:', '?20000:').replaceAll('?60000:', '?120000:'),'v9');
  edit('82151420',fn=>fn.replaceAll('?2224640:', '?2288640:').replaceAll('?46347:', '?47680:')
    .replaceAll('?139040:', '?143040:').replaceAll('?20000:', '?24000:').replaceAll('?120000:', '?144000:'),'v10');
  edit('82151420',fn=>fn.replaceAll('?2288640:', '?2416640:').replaceAll('?47680:', '?50347:')
    .replaceAll('?143040:', '?151040:').replaceAll('?24000:', '?32000:').replaceAll('?144000:', '?192000:'),'v11');
  edit('82151420',fn=>fn.replaceAll('?2416640:', '?3368960:').replaceAll('?50347:', '?70187:')
    .replaceAll('?151040:', '?210560:'),'v12');
  edit('82151420',fn=>fn.replaceAll('?3368960:', '?3496960:').replaceAll('?70187:', '?72854:')
    .replaceAll('?210560:', '?218560:').replaceAll('?32000:', '?40000:').replaceAll('?192000:', '?240000:'),'v13');
  const missing=[...declarations].filter(d=>!source.includes('extern "C" '+d));
  if(missing.length)source=missing.map(d=>'extern "C" '+d).join('\n')+'\n'+source;
  return source;
}
module.exports={patchTorrential};
