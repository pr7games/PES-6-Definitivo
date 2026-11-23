function patchStadium(source){
  // Retire lens experiment: user isolates the fault to cinematic dimming.
  source=source.split('\n').filter(line=>!line.includes('pes6_stadium_lens_overlay')).join('\n');
  // Remove only the black stencil-composition overlay during cinematic cameras.
  {const start=source.indexOf('DEFINE_REX_FUNC(sub_821AB7B0) {');if(start>=0){
    const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);
    const call='sub_821728F0(ctx, base);';
    if(!fn.includes('pes6_stadium_skip_cinematic_shade')){
      if(!fn.includes(call))throw Error('Cinematic black composition anchor');
      fn=fn.replace(call,'if(!pes6_stadium_skip_cinematic_shade(base))'+call);
      source=source.slice(0,start)+fn+source.slice(end);
    }
    const decl='extern "C" bool pes6_stadium_skip_cinematic_shade(uint8_t*);';
    if(!source.includes(decl))source=decl+'\n'+source;
  }}
  function patch(address,change){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);
    if(fn.includes('// PES6 new stadium slot v1'))return;
    const next=change(fn);if(next===fn)throw Error('Missing stadium anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 new stadium slot v1')+source.slice(end);
  }
  const table=(fn,reg,immediate,type)=>{
    const anchor=fn.split("\n").map(l=>l.trim()).find(l=>l.startsWith(`ctx.r${reg}.s64 = ctx.r`)&&l.endsWith(`.s64 + ${immediate};`));
    if(!anchor||!fn.includes(anchor))throw Error('Missing stadium table '+reg+':'+immediate);
    return fn.replaceAll(anchor,`ctx.r${reg}.u64 = pes6_stadium_table(ctx,base,${type});`);
  };
  // More panels, same1000-row limit and GPU buffers. All traversal/destruction
  // uses the actual count; only allocation, zero-fill and admission were fixed.
  patch('8215F610',fn=>fn.replace('ctx.r3.s64 = 420;','ctx.r3.s64 = 1044;').replace('ctx.r10.s64 = 100;','ctx.r10.s64 = 256;'));
  patch('8216BC78',fn=>fn.replace('ctx.r11.u32, 100,','ctx.r11.u32, 256,'));
  patch('8217EC30',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tif(pes6_stadium_name(ctx,base))return;'));
  patch('8219E398',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tif(pes6_stadium_bundle(ctx,base))return;'));
  patch('8214D498',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_stadium_shell_visible(ctx,base);'));
  patch('8214DE28',fn=>{
    const anchor='ctx.r4.u64 = REX_LOAD_U32(ctx.r11.u32 + ctx.r9.u32);';
    if(!fn.includes(anchor))throw Error('Missing stadium resource load');
    return fn.replace(anchor,'ctx.r4.u64 = ctx.r11.u32>=128&&ctx.r11.u32<144 ? pes6_stadium_resource(ctx.r11.u32,0) : REX_LOAD_U32(ctx.r11.u32 + ctx.r9.u32);');
  });
  patch('826F9260',fn=>table(fn,11,29844,1));
  patch('826F9ED0',fn=>table(fn,10,29844,1));
  patch('826F9600',fn=>table(fn,9,29832,0));
  patch('826F99C0',fn=>table(fn,10,29844,1).replace('ctx.r30.s32, 8,','ctx.r30.s32, 9,').replace('ctx.r10.s64 = 8;','ctx.r10.s64 = 9;'));
  patch('826F9828',fn=>fn.replace('ctx.r10.u32, 8,','ctx.r10.u32, 9,'));
  patch('826F9708',fn=>table(fn,10,29832,0).replace('ctx.r11.u32, 8,','ctx.r11.u32, 9,').replace('ctx.r4.s64 = 8;','ctx.r4.s64 = 9;'));
  patch('826FA478',fn=>fn.replace('ctx.r11.s64 = ctx.r11.s64 + 8;','ctx.r11.s64 = ctx.r11.s64 + 9;').replace('ctx.r10.s64 = 9;','ctx.r10.s64 = 10;').replace('static_cast<uint64_t>(9)','static_cast<uint64_t>(10)'));
  patch('826F9C68',fn=>{
    fn=table(fn,11,29832,0).replace('ctx.r10.u32, 8,','ctx.r10.u32, 9,');
    // New venue and Random use the existing neutral preview frame; their
    // labels and saved values remain distinct. Never index a fourth page.
    const a='ctx.r30.u64 = REX_LOAD_U8(ctx.r10.u32 + ctx.r11.u32);';
    if(!fn.includes(a))throw Error('Missing stadium preview mapping');
    return fn.replace(a,a+'\n\tif(ctx.r30.u32>8)ctx.r30.u64=8;');
  });
  patch('8217BA80',fn=>table(table(fn,11,-800,2),10,-800,2));
  patch('822325C8',fn=>table(fn,11,-800,2));
  patch('82460680',fn=>table(fn,11,-704,3));
  patch('8214E3B0',fn=>fn.replace('ctx.r8.u64 = ctx.r11.u64 + ctx.r9.u64;','ctx.r8.u64 = ctx.r11.u64 + ctx.r9.u64;\n\t// Imported geometry uses PES6 reference lighting/pitch parameters.\n\tif(ctx.r8.u32>=32&&ctx.r8.u32<36)ctx.r8.u64-=32;'));
  patch('82153018',fn=>fn.replace('ctx.r11.u64 = REX_LOAD_U8(ctx.r11.u32 + 130);','ctx.r11.u64 = REX_LOAD_U8(ctx.r11.u32 + 130);\n\tif(ctx.r11.u32==8)ctx.r11.u64=0;'));
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_826F9C68) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){let fn=source.slice(start,end);if(!fn.includes('// PES6 Konami preview v2')){
      const clamp='if(ctx.r30.u32>8)ctx.r30.u64=8;';
      if(!fn.includes(clamp))throw Error('Missing preview upgrade anchor');
      fn=fn.replace(clamp,'// PES6 Konami preview v2\n\tif(ctx.r10.u32==8)ctx.r30.u64=9;else if(ctx.r10.u32==9)ctx.r30.u64=8;');
      fn=table(fn,11,24052,4);
      const name='ctx.r3.u64 = ctx.r30.u64;\n\t// bl 0x8217ec30';
      if(!fn.includes(name))throw Error('Missing preview name anchor');
      fn=fn.replace(name,'ctx.r3.u64 = ctx.r30.u32==9?8:ctx.r30.u64;\n\t// bl 0x8217ec30');
      source=source.slice(0,start)+fn+source.slice(end);
    }}
  }
  // Konami is retained as a physical resource, temporarily hidden from UI.
  // Restore eight playable choices + Random, excluding it from random picks.
  const hide=(address,change)=>{
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
    if(fn.includes('// PES6 Konami hidden menu v1'))return;
    const next=change(fn);if(next===fn)throw Error('Missing hidden-menu anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 Konami hidden menu v1')+source.slice(end);
  };
  hide('826F99C0',fn=>fn.replace('ctx.r30.s32, 9,','ctx.r30.s32, 8,').replace('ctx.r10.s64 = 9;','ctx.r10.s64 = 8;'));
  hide('826F9708',fn=>fn.replace('ctx.r11.u32, 9,','ctx.r11.u32, 8,').replace('ctx.r4.s64 = 9;','ctx.r4.s64 = 8;'));
  hide('826F9828',fn=>fn.replace('ctx.r10.u32, 9,','ctx.r10.u32, 8,'));
  hide('826FA478',fn=>fn.replace('ctx.r11.s64 = ctx.r11.s64 + 9;','ctx.r11.s64 = ctx.r11.s64 + 8;').replace('ctx.r10.s64 = 10;','ctx.r10.s64 = 9;').replace('static_cast<uint64_t>(10)','static_cast<uint64_t>(9)'));
  hide('826F9C68',fn=>fn.replace('if(ctx.r10.u32==8)ctx.r30.u64=9;else if(ctx.r10.u32==9)ctx.r30.u64=8;','if(ctx.r10.u32>=8)ctx.r30.u64=8;').replace('ctx.r10.u32, 9,','ctx.r10.u32, 8,').replace('ctx.r3.u64 = ctx.r30.u32==9?8:ctx.r30.u64;','ctx.r3.u64 = ctx.r30.u64;'));
  for(const address of ['826F9C68','826FA478']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)continue;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
    if(!fn.includes('// PES6 clamp formerly saved extra UI choice')){
      const next=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 clamp formerly saved extra UI choice\n\tif(REX_LOAD_U8(ctx.r3.u32+182)>8)REX_STORE_U8(ctx.r3.u32+182,8);');
      source=source.slice(0,start)+next+source.slice(end);
    }
  }
  if(source.includes('// PES6 new stadium slot v1')&&!source.includes('extern "C" uint32_t pes6_stadium_table'))source='extern "C" uint32_t pes6_stadium_table(PPCContext&,uint8_t*,unsigned);\nextern "C" bool pes6_stadium_name(PPCContext&,uint8_t*);\nextern "C" uint32_t pes6_stadium_resource(uint32_t,uint32_t);\n'+source;
  if(source.includes('if(pes6_stadium_bundle(ctx,base))')&&!source.includes('extern "C" bool pes6_stadium_bundle'))source='extern "C" bool pes6_stadium_bundle(PPCContext&,uint8_t*);\n'+source;
  if(source.includes('pes6_stadium_shell_visible(ctx,base)')&&!source.includes('extern "C" void pes6_stadium_shell_visible'))source='extern "C" void pes6_stadium_shell_visible(PPCContext&,uint8_t*);\n'+source;
  // Independent Golazo slot9. Migrate both existing generated code and fresh
  // codegen after the historical hidden-Konami patches above.
  const golazoEdit=(address,change)=>{
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
    if(fn.includes('// PES6 independent Golazo slot9 v1'))return;
    const next=change(fn);if(next===fn)throw Error('Missing independent Golazo anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 independent Golazo slot9 v1')+source.slice(end);
  };
  golazoEdit('826F99C0',fn=>fn.replace('ctx.r30.s32, 8,','ctx.r30.s32, 9,').replace('ctx.r10.s64 = 8;','ctx.r10.s64 = 9;'));
  golazoEdit('826F9708',fn=>fn.replace('ctx.r11.u32, 8,','ctx.r11.u32, 9,').replace('ctx.r4.s64 = 8;','ctx.r4.s64 = 9;'));
  golazoEdit('826F9828',fn=>fn.replace('ctx.r10.u32, 8,','ctx.r10.u32, 9,'));
  golazoEdit('826FA478',fn=>fn.replace('ctx.r11.s64 = ctx.r11.s64 + 8;','ctx.r11.s64 = ctx.r11.s64 + 9;').replace('static_cast<uint64_t>(9)','static_cast<uint64_t>(10)').replace('if(REX_LOAD_U8(ctx.r3.u32+182)>8)REX_STORE_U8(ctx.r3.u32+182,8);','if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);'));
  golazoEdit('826F9C68',fn=>fn.replace('if(ctx.r10.u32>=8)ctx.r30.u64=8;','if(ctx.r10.u32==9)ctx.r30.u64=8;').replace('ctx.r10.u32, 8,','ctx.r10.u32, 9,').replace('if(REX_LOAD_U8(ctx.r3.u32+182)>8)REX_STORE_U8(ctx.r3.u32+182,8);','if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);'));
  golazoEdit('8214DE28',fn=>fn.replace('ctx.r11.u32>=128&&ctx.r11.u32<144','ctx.r11.u32>=128&&ctx.r11.u32<160'));
  golazoEdit('8214E3B0',fn=>fn.replace('if(ctx.r8.u32>=32&&ctx.r8.u32<36)ctx.r8.u64-=32;','if(ctx.r8.u32>=36&&ctx.r8.u32<40)ctx.r8.u64-=16;else if(ctx.r8.u32>=32&&ctx.r8.u32<36)ctx.r8.u64-=32;'));
  golazoEdit('82153018',fn=>fn.replace('if(ctx.r11.u32==8)ctx.r11.u64=0;','if(ctx.r11.u32==9)ctx.r11.u64=5;else if(ctx.r11.u32==8)ctx.r11.u64=0;'));
  // Golazo: reset the native training preset's stadium, before the options
  // screen; changing the stadium afterwards remains supported.
  for(const address of ['8217C810','8217C910']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)continue;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
    if(fn.includes('// PES6 Golazo training default v2'))continue;
    if(fn.includes('// PES6 Golazo training default v1')){
      source=source.slice(0,start)+fn.replace('// PES6 Golazo training default v1','// PES6 Golazo training default v2').replace('REX_STORE_U8(ctx.r31.u32+44,5);','REX_STORE_U8(ctx.r31.u32+44,9);')+source.slice(end);continue;
    }
    const anchor='sub_8217C6E0(ctx, base);';
    if(!fn.includes(anchor))throw Error('Golazo training preset anchor '+address);
    const next=fn.replace(anchor,anchor+'\n\t// PES6 Golazo training default v2\n\tREX_STORE_U8(ctx.r31.u32+44,9);');
    source=source.slice(0,start)+next+source.slice(end);
  }
  // Re-enable physical8 after the no-crowd/native-field Konami rebuild.
  // UI0 Golazo, UI1 Konami, eight originals, UI10 Random. Konami's preview
  // uses page4/frame1 (synthetic index10); its actual physical ID remains8.
  const restoreKonami=(address,change)=>{
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
    if(fn.includes('// PES6 Konami native field no crowd v2'))return;
    const next=change(fn);if(next===fn)throw Error('Konami menu restore anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 Konami native field no crowd v2')+source.slice(end);
  };
  restoreKonami('826F99C0',fn=>fn.replace('ctx.r30.s32, 9,','ctx.r30.s32, 10,').replace('ctx.r10.s64 = 9;','ctx.r10.s64 = 10;'));
  restoreKonami('826F9708',fn=>fn.replace('ctx.r11.u32, 9,','ctx.r11.u32, 10,').replace('ctx.r4.s64 = 9;','ctx.r4.s64 = 10;'));
  restoreKonami('826F9828',fn=>fn.replace('ctx.r10.u32, 9,','ctx.r10.u32, 10,'));
  restoreKonami('826FA478',fn=>fn.replace('ctx.r11.s64 = ctx.r11.s64 + 9;','ctx.r11.s64 = ctx.r11.s64 + 10;').replace('static_cast<uint64_t>(10)','static_cast<uint64_t>(11)').replace('if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);','if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);'));
  restoreKonami('826F9C68',fn=>fn.replace('if(ctx.r10.u32==9)ctx.r30.u64=8;','if(ctx.r10.u32==10)ctx.r30.u64=8;else if(ctx.r30.u32==8)ctx.r30.u64=10;').replace('ctx.r10.u32, 9,','ctx.r10.u32, 10,').replace('ctx.r3.u64 = ctx.r30.u64;\n\t// bl 0x8217ec30','ctx.r3.u64 = ctx.r30.u32==10?8:ctx.r30.u64;\n\t// bl 0x8217ec30').replace('if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);','if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);'));
  const literalHook=(address,change)=>{
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);if(fn.includes('// PES6 literal donor instances v1'))return;
    const next=change(fn);if(next===fn)throw Error('Literal stadium hook anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 literal donor instances v1')+source.slice(end);
  };
  literalHook('8214D920',fn=>fn.replace('loc_8214DA68:','loc_8214DA68:\n\tpes6_stadium_literal_load(ctx,base);'));
  literalHook('8214D498',fn=>fn.replace('pes6_stadium_shell_visible(ctx,base);','if(pes6_stadium_literal_draw(ctx,base))return;\n\tpes6_stadium_shell_visible(ctx,base);'));
  literalHook('8215FCB0',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_stadium_literal_release(ctx,base);'));
  for(const [address,marker,change] of [
    ['8214D920','// PES6 Konami orphan streams v1',fn=>fn
      .replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_stadium_begin_models(ctx,base);')
      .replace('sub_822015D0(ctx, base);','pes6_stadium_prune_model(ctx,base);\n\tsub_822015D0(ctx, base);')],
    ['8214DA70','// PES6 Konami orphan textures v1',fn=>fn
      .replace('sub_8218E880(ctx, base);','if(pes6_stadium_unused_texture(ctx,base))goto loc_8214DC04;\n\tsub_8218E880(ctx, base);')]
  ]){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)continue;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);if(fn.includes(marker))continue;
    const next=change(fn);if(next===fn)throw Error('Konami resource pruning anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t'+marker)+source.slice(end);
  }
  for(const [name,type] of [['begin_models','void'],['prune_model','void'],['unused_texture','bool']]){
    const decl=`extern "C" ${type} pes6_stadium_${name}(PPCContext&,uint8_t*);`;
    if(source.includes(`pes6_stadium_${name}(ctx,base)`)&&!source.includes(decl))source=decl+'\n'+source;
  }
  // Enhanced native net deformation and Wembley-only crowd tone.
  for(const [address,call,decl] of [
    ['82170030','pes6_native_net_rest(base,ctx.r3.u32);','extern "C" void pes6_native_net_rest(uint8_t*,uint32_t);'],
    ['8216E190','pes6_native_net_upload(ctx,base);','extern "C" void pes6_native_net_upload(PPCContext&,uint8_t*);'],
    ['8216C190','pes6_wembley_crowd_color(ctx,base,false);','extern "C" void pes6_wembley_crowd_color(PPCContext&,uint8_t*,bool);'],
    ['8216C3A0','pes6_wembley_crowd_color(ctx,base,true);','extern "C" void pes6_wembley_crowd_color(PPCContext&,uint8_t*,bool);']
  ]){
    const start=source.indexOf('DEFINE_REX_FUNC(sub_'+address+') {');if(start<0)continue;
    const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);
    if(!fn.includes(call)){
      if(address==='82170030')fn=fn.replace('// addi r1,r1,128',''+call+'\n\t// addi r1,r1,128');
      else if(address==='8216E190')fn=fn.replace('sub_82195A80(ctx, base);',call);
      else fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t'+call);
      if(!fn.includes(call))throw Error('Native net/crowd anchor '+address);
      source=source.slice(0,start)+fn+source.slice(end);
    }
    if(!source.includes(decl))source=decl+'\n'+source;
  }
  // Restore the original native net renderer after retiring imported nets.
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_8216E190) {');
    if(start>=0){
      const end=source.indexOf('\n}\n',start);
      let fn=source.slice(start,end);
      fn=fn.replace('\n\t// PES6 literal donor instances v1','').replace('\n\tif(pes6_stadium_literal_flags(ctx,base))return;','');
      source=source.slice(0,start)+fn+source.slice(end);
    }
  }
  // The enlarged crowd admission table belongs to custom venues. Native
  // stadiums must retain their original allocation and 100-panel budget.
  for(const address of ['8215F610','8216BC78']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)continue;
    const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);
    if(fn.includes('// PES6 custom venue crowd capacity v3'))continue;
    if(fn.includes('// PES6 custom venue crowd capacity v2')){
      fn=fn.replace('// PES6 custom venue crowd capacity v2','// PES6 custom venue crowd capacity v3')
        .replace('base[pes6StadiumConfig+130]>=8&&base[pes6StadiumConfig+130]<=9','base[pes6StadiumConfig+130]>=8&&base[pes6StadiumConfig+130]<=10');
      source=source.slice(0,start)+fn+source.slice(end);
      continue;
    }
    fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 custom venue crowd capacity v3\n\tconst auto pes6StadiumConfig=REX_LOAD_U32(0x83139360u);\n\tconst bool pes6ExpandedPanels=pes6StadiumConfig&&base[pes6StadiumConfig+130]>=8&&base[pes6StadiumConfig+130]<=10;');
    if(address==='8215F610')fn=fn.replace('ctx.r3.s64 = 1044;','ctx.r3.s64 = pes6ExpandedPanels?1044:420;').replace('ctx.r10.s64 = 256;','ctx.r10.s64 = pes6ExpandedPanels?256:100;');
    else fn=fn.replace('ctx.r11.u32, 256,','ctx.r11.u32, pes6ExpandedPanels?256:100,');
    source=source.slice(0,start)+fn+source.slice(end);
  }
  for(const [name,type]of [['load','void'],['release','void'],['draw','bool'],['flags','bool']])if(source.includes(`pes6_stadium_literal_${name}(ctx,base)`)&&!source.includes(`extern "C" ${type} pes6_stadium_literal_${name}`))source=`extern "C" ${type} pes6_stadium_literal_${name}(PPCContext&,uint8_t*);\n`+source;
  // Keep every PES2008/PES2009 panel and row. These limits are scoped to physical8 and physical10;
  // original stadiums and Golazo retain their existing GPU/row capacities.
  for(const address of ['8216BC78','8216C710','8216C3A0']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)continue;
    const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);
    if(fn.includes('// PES6 complete Konami crowd v2'))continue;
    if(fn.includes('// PES6 complete Konami crowd v1')){
      fn=fn.replace('// PES6 complete Konami crowd v1','// PES6 complete Konami crowd v2')
        .replace('base[pes6CrowdConfig+130]==8;','(base[pes6CrowdConfig+130]==8||base[pes6CrowdConfig+130]==10);');
      source=source.slice(0,start)+fn+source.slice(end);
      continue;
    }
    fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 complete Konami crowd v2\n\tconst auto pes6CrowdConfig=REX_LOAD_U32(0x83139360u);\n\tconst bool pes6FullKonamiCrowd=pes6CrowdConfig&&(base[pes6CrowdConfig+130]==8||base[pes6CrowdConfig+130]==10);');
    if(address==='8216BC78')fn=fn.replace('ctx.r11.u32, 1000,','ctx.r11.u32, pes6FullKonamiCrowd?4096:1000,');
    if(address==='8216C710')fn=fn.replace('ctx.r4.s64 = 2000;','ctx.r4.s64 = pes6FullKonamiCrowd?16384:2000;').replace('ctx.r9.s64 = 2000;','ctx.r9.s64 = pes6FullKonamiCrowd?16384:2000;').replace('static_cast<uint64_t>(8000)','static_cast<uint64_t>(pes6FullKonamiCrowd?65536:8000)');
    if(address==='8216C3A0')fn=fn.replace('ctx.r5.u32, 2000,','ctx.r5.u32, pes6FullKonamiCrowd?16384:2000,');
    source=source.slice(0,start)+fn+source.slice(end);
  }
  const sunsetEdit=(address,change)=>{
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;
    const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);if(fn.includes('// PES6 donor sunset v1'))return;
    const next=change(fn);if(next===fn)throw Error('Missing sunset anchor '+address);
    source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 donor sunset v1')+source.slice(end);
  };
  sunsetEdit('826F9708',fn=>fn.replace('const uint32_t pes6WeatherChoice=ctx.r5.u32;', 'const auto pes6TimePointer=ctx.r4.u32;\n\tconst auto pes6TimeChoice=pes6_stadium_time_begin(base,ctx.r3.u32,pes6TimePointer);\n\tconst uint32_t pes6WeatherChoice=ctx.r5.u32;').replace('pes6_weather_menu_restore(base,pes6WeatherChoice,pes6HeavyRain);','pes6_weather_menu_restore(base,pes6WeatherChoice,pes6HeavyRain);\n\tpes6_stadium_time_end(base,pes6TimePointer,pes6TimeChoice);'));
  sunsetEdit('826FA198',fn=>fn.replace('pes6_weather_menu_ready(ctx,base,ctx.r31.u32);','pes6_weather_menu_ready(ctx,base,ctx.r31.u32);\n\tpes6_stadium_time_ready(ctx,base,ctx.r31.u32);'));
  sunsetEdit('826F9C68',fn=>fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_stadium_time_ready(ctx,base,ctx.r3.u32);'));
  sunsetEdit('826F9308',fn=>fn.replace('ctx.r11.u32/4)*4:ctx.r11.u32)','ctx.r11.u32/4)*4:ctx.r30.u32==1?pes6_stadium_time_key(ctx.r11.u32/4)*4:ctx.r11.u32)').replace('if(ctx.r30.u32!=2||!pes6_weather_menu_caption(ctx,base,ctx.r11.u32/4))sub_82734390(ctx, base);','if(ctx.r30.u32==1&&pes6_stadium_time_caption(ctx,base,ctx.r11.u32/4)){}else if(ctx.r30.u32!=2||!pes6_weather_menu_caption(ctx,base,ctx.r11.u32/4))sub_82734390(ctx, base);'));
  // Correct the earlier sunset insertion: native row0=Time, row1=Season.
  source=source.replaceAll('ctx.r30.u32==1?pes6_stadium_time_key','ctx.r30.u32==0?pes6_stadium_time_key').replaceAll('ctx.r30.u32==1&&pes6_stadium_time_caption','ctx.r30.u32==0&&pes6_stadium_time_caption');
  for(const decl of ['uint32_t pes6_stadium_time_begin(uint8_t*,uint32_t,uint32_t);','void pes6_stadium_time_end(uint8_t*,uint32_t,uint32_t);','void pes6_stadium_time_ready(PPCContext&,uint8_t*,uint32_t);','uint32_t pes6_stadium_time_key(uint32_t);','bool pes6_stadium_time_caption(PPCContext&,uint8_t*,uint32_t);'])if(!source.includes('extern "C" '+decl))source='extern "C" '+decl+'\n'+source;
  // PES2008 original environment injection v1, without a San Siro fallback.
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_8214E3B0) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){let fn=source.slice(start,end);
      fn=fn.replace('// Imported geometry uses PES6 reference lighting/pitch parameters.','// Imported Konami environment comes from converted donor records.').replace('if(ctx.r8.u32>=36&&ctx.r8.u32<40)ctx.r8.u64-=16;else if(ctx.r8.u32>=32&&ctx.r8.u32<36)ctx.r8.u64-=32;','if(ctx.r8.u32>=36&&ctx.r8.u32<40)ctx.r8.u64-=16;');
      if(!fn.includes('pes6_stadium_environment_record(ctx'))fn=fn.replace('sub_8215F188(ctx, base);','ctx.r4.u64=pes6_stadium_environment_record(ctx,base,ctx.r4.u32);\n\tsub_8215F188(ctx, base);');
      source=source.slice(0,start)+fn+source.slice(end);
      const decl='extern "C" uint32_t pes6_stadium_environment_record(PPCContext&,uint8_t*,uint32_t);';if(!source.includes(decl))source=decl+'\n'+source;
    }
  }
  // Apply imported blend/depth at the actual mesh draw's state flush.
  // 82193FE0 belongs to effects and is not the KTMDL stadium draw path.
  source=source.replaceAll('pes6_stadium_lightmap_state(ctx,base);','');
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_821AC7F8) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){
      let fn=source.slice(start,end);
      fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_stadium_lightmap_state(ctx,base);');
      if(!fn.includes('pes6_stadium_lightmap_draw_end(ctx,base);'))fn=fn.replace('// addi r1,r1,240','pes6_stadium_lightmap_draw_end(ctx,base);\n\t// addi r1,r1,240');
      source=source.slice(0,start)+fn+source.slice(end);
      const decl='extern "C" void pes6_stadium_lightmap_state(PPCContext&,uint8_t*);';
      if(!source.includes(decl))source=decl+'\n'+source;
      const endDecl='extern "C" void pes6_stadium_lightmap_draw_end(PPCContext&,uint8_t*);';
      if(!source.includes(endDecl))source=endDecl+'\n'+source;
    }
  }
  // PES2008 logical replay hour v1
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_826F9ED0) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){
      let fn=source.slice(start,end);
      if(!fn.includes('// PES6 initialize extended hour after venue v2')){
        const call=fn.indexOf('sub_826F94D0(ctx, base);');
        const begin=fn.lastIndexOf('\t// mr r3,r31',call);
        const anchor='REX_STORE_U8(ctx.r31.u32 + 182, ctx.r11.u8);';
        if(call<0||begin<0||!fn.includes(anchor))throw Error('Extended hour initialization anchor');
        fn=fn.slice(0,begin)+fn.slice(call+'sub_826F94D0(ctx, base);'.length);
        fn=fn.replace(anchor,anchor+'\n\t// PES6 initialize extended hour after venue v2\n\tpes6_stadium_time_ready(ctx,base,ctx.r31.u32);\n\tctx.r3.u64=ctx.r31.u64;\n\tctx.r4.u64=REX_LOAD_U8(ctx.r30.u32+13);\n\tif(ctx.r4.u32==3&&base[ctx.r31.u32+182]!=1)ctx.r4.u64=0;\n\tsub_826F94D0(ctx,base);');
        source=source.slice(0,start)+fn+source.slice(end);
      }
    }
  }
  source=source.replaceAll('const auto pes6TimePointer=ctx.r4.u32;','const auto pes6TimePointer=ctx.r6.u32;');
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_8214C9A0) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){let fn=source.slice(start,end);if(!fn.includes('pes6_stadium_time_decode(base')){
      fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst auto pes6EnvironmentPacket=ctx.r3.u32;\n\tconst auto pes6LogicalHour=pes6_stadium_time_decode(base,pes6EnvironmentPacket);').replace('// blr','pes6_stadium_time_decode_end(base,pes6EnvironmentPacket,pes6LogicalHour);\n\t// blr');
      source=source.slice(0,start)+fn+source.slice(end);
    }for(const decl of ['extern "C" uint32_t pes6_stadium_time_decode(uint8_t*,uint32_t);','extern "C" void pes6_stadium_time_decode_end(uint8_t*,uint32_t,uint32_t);'])if(!source.includes(decl))source=decl+'\n'+source;}
  }
  for(const [address, replay] of [['825DFAA0',false],['8247A3B0',true]]){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`),end=source.indexOf('\n}\n',start);
    if(start<0)continue;
    let fn=source.slice(start,end);
    if(!fn.includes('// PES2008 commercial clock v1')){
      const anchor=replay?'\t// addi r1,r1,144':'\t// addi r1,r1,208';
      if(!fn.includes(anchor))throw Error('Missing commercial clock anchor '+address);
      if(replay)fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst auto stadiumAdvertReplayManager=ctx.r3.u32;');
      fn=fn.replace(anchor,'\t// PES2008 commercial clock v1\n\t'+(replay?'pes6_stadium_advert_replay(base,stadiumAdvertReplayManager);':'pes6_stadium_advert_tick(base);')+'\n'+anchor);
      source=source.slice(0,start)+fn+source.slice(end);
    }
    const decl=replay?'extern "C" void pes6_stadium_advert_replay(uint8_t*,uint32_t);':'extern "C" void pes6_stadium_advert_tick(uint8_t*);';
    if(!source.includes(decl))source=decl+'\n'+source;
  }
  for(const [address, helper] of [
    ['82199438','pes6_stadium_hemisphere_shade(ctx,base);'],
    ['82103C58','pes6_stadium_hemisphere_constants(base,ctx.r3.u32,false);'],
    ['82103D68','pes6_stadium_hemisphere_constants(base,ctx.r3.u32,true);']
  ]){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`),end=source.indexOf('\n}\n',start);
    if(start<0)continue;
    const fn=source.slice(start,end);
    // User selected the original PES6 player lighting on 2026-11-16.
    source=source.slice(0,start)+fn.replaceAll(helper,'')+source.slice(end);
    const decl=address==='82199438'?'extern "C" void pes6_stadium_hemisphere_shade(PPCContext&,uint8_t*);':
      'extern "C" void pes6_stadium_hemisphere_constants(uint8_t*,uint32_t,bool);';
    source=source.replaceAll(decl+'\n','');
  }
  // Current menu: nine venues plus Random. This migration follows historical
  // patches so both an existing build and fresh codegen converge identically.
  const current=(address,change)=>{const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);if(fn.includes('// PES6 nine venues ordered dusk v3'))return;const next=change(fn);if(next===fn)throw Error('Current stadium anchor '+address);source=source.slice(0,start)+next.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 nine venues ordered dusk v3')+source.slice(end);};
  current('826F99C0',fn=>fn.replace('ctx.r30.s32, 10,','ctx.r30.s32, 9,').replace('ctx.r10.s64 = 10;','ctx.r10.s64 = 9;'));
  current('826F9708',fn=>fn.replace('ctx.r11.u32, 10,','ctx.r11.u32, 9,').replace('ctx.r4.s64 = 10;','ctx.r4.s64 = 9;'));
  current('826F9828',fn=>fn.replace('ctx.r10.u32, 10,','ctx.r10.u32, 9,'));
  current('826FA478',fn=>fn.replace('if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);','if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);').replace('ctx.r11.s64 = ctx.r11.s64 + 10;','ctx.r11.s64 = ctx.r11.s64 + 9;').replace('ctx.r10.s64 = 9;\n\t// divw','ctx.r10.s64 = 10;\n\t// divw').replace('static_cast<uint64_t>(11)','static_cast<uint64_t>(10)').replace('REX_STORE_U8(ctx.r29.u32 + ctx.r30.u32, ctx.r3.u8);','if(ctx.r30.u32==0)ctx.r3.u64=pes6_stadium_time_choice(ctx.r3.u32);\n\tREX_STORE_U8(ctx.r29.u32 + ctx.r30.u32, ctx.r3.u8);'));
  current('826F9C68',fn=>fn.replace('if(ctx.r10.u32==10)ctx.r30.u64=8;','if(ctx.r10.u32==9)ctx.r30.u64=8;').replace('ctx.r10.u32, 10,','ctx.r10.u32, 9,').replace('if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);','if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);'));
  // Expand menu from 9 venues + Random (UI 9) to 10 venues + Random (UI 10, Wembley ID 10 at UI 9).
  const wembley=(address,change)=>{const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)return;const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);if(fn.includes('// PES6 ten venues wembley v1'))return;const next=change(fn);if(next===fn)throw Error('Wembley stadium anchor '+address);source=source.slice(0,start)+next.replace('// PES6 nine venues ordered dusk v3','// PES6 ten venues wembley v1')+source.slice(end);};
  wembley('826F99C0',fn=>fn.replace('ctx.r30.s32, 9,','ctx.r30.s32, 10,').replace('ctx.r10.s64 = 9;','ctx.r10.s64 = 10;'));
  wembley('826F9708',fn=>fn.replace('ctx.r11.u32, 9,','ctx.r11.u32, 10,').replace('ctx.r4.s64 = 9;','ctx.r4.s64 = 10;'));
  wembley('826F9828',fn=>fn.replace('ctx.r10.u32, 9,','ctx.r10.u32, 10,'));
  wembley('826FA478',fn=>fn.replace('if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);','if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);').replace('ctx.r11.s64 = ctx.r11.s64 + 9;','ctx.r11.s64 = ctx.r11.s64 + 10;').replace('ctx.r10.s64 = 10;\n\t// divw','ctx.r10.s64 = 11;\n\t// divw').replace('static_cast<uint64_t>(10)','static_cast<uint64_t>(11)'));
  wembley('826F9C68',fn=>fn.replace('if(ctx.r10.u32==9)ctx.r30.u64=8;','if(ctx.r10.u32==10)ctx.r30.u64=8;').replace('ctx.r10.u32, 9,','ctx.r10.u32, 10,').replace('if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);','if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);'));
  current('8214DE28',fn=>fn.replace('ctx.r11.u32<160','ctx.r11.u32<144'));
  current('8214E3B0',fn=>fn.replace('if(ctx.r8.u32>=36&&ctx.r8.u32<40)ctx.r8.u64-=16;',''));
  current('82153018',fn=>fn.replace('if(ctx.r11.u32==9)ctx.r11.u64=5;else if(ctx.r11.u32==8)','if(ctx.r11.u32==8)'));
  // Wembley ID 10 also maps to native service index 0 in 82153018 and full crowd capacity.
  for(const address of ['8216BC78','8216C710','8216C3A0']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`),end=source.indexOf('\n}\n',start);
    if(start>=0){const fn=source.slice(start,end);if(fn.includes('base[pes6CrowdConfig+130]==8;'))source=source.slice(0,start)+fn.replace('base[pes6CrowdConfig+130]==8;','(base[pes6CrowdConfig+130]==8||base[pes6CrowdConfig+130]==10);')+source.slice(end);}
  }
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_82153018) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){const fn=source.slice(start,end);if(fn.includes('if(ctx.r11.u32==8)ctx.r11.u64=0;'))source=source.slice(0,start)+fn.replace('if(ctx.r11.u32==8)ctx.r11.u64=0;','if(ctx.r11.u32==8)ctx.r11.u64=0;else if(ctx.r11.u32==10)ctx.r11.u64=4;')+source.slice(end);}
  }
  for(const a of ['8217C810','8217C910'])current(a,fn=>fn.replace('REX_STORE_U8(ctx.r31.u32+44,9);','REX_STORE_U8(ctx.r31.u32+44,8);'));
  current('826F94D0',fn=>fn.replace('ctx.r31.u64 = ctx.r4.u64;','ctx.r31.u64 = pes6_stadium_time_row(ctx.r4.u32);'));
  current('826F95B8',fn=>fn.replace('// blr','ctx.r3.u64=pes6_stadium_time_choice(ctx.r3.u32);\n\t// blr'));
  source=source.replaceAll('if(ctx.r4.u32==3&&base[ctx.r31.u32+182]!=1)ctx.r4.u64=0;','');
  for(const decl of ['uint32_t pes6_stadium_time_choice(uint32_t);','uint32_t pes6_stadium_time_row(uint32_t);'])if(!source.includes('extern "C" '+decl))source='extern "C" '+decl+'\n'+source;
  // Preserve the extended logical hour instead of copying the old binary
  // day/night match value back over the user's new Dusk selection.
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_826F99C0) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){const fn=source.slice(start,end);source=source.slice(0,start)+fn.replace('ctx.r11.u64 = REX_LOAD_U8(ctx.r28.u32 + 325);','ctx.r11.u64 = ctx.r3.u32==3?3:REX_LOAD_U8(ctx.r28.u32 + 325);')+source.slice(end);}
  }
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_826F99C0) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){const fn=source.slice(start,end);if(!fn.includes('pes6_stadium_time_commit('))source=source.slice(0,start)+fn.replace('sub_826F95B8(ctx, base);','sub_826F95B8(ctx, base);\n\tpes6_stadium_time_commit(ctx.r3.u32);')+source.slice(end);}
  }
  source=source.replace('ctx.r4.u64=REX_LOAD_U8(ctx.r30.u32+13);','ctx.r4.u64=pes6_stadium_time_saved(REX_LOAD_U8(ctx.r30.u32+13));');
  for(const decl of ['uint32_t pes6_stadium_time_saved(uint32_t);','void pes6_stadium_time_commit(uint32_t);'])if(!source.includes('extern "C" '+decl))source='extern "C" '+decl+'\n'+source;
  // Night light-vector attenuation and shadow receiver initialization must
  // use the logical hour, independently of Konami's compatible scene route.
  for(const address of ['82161B40','82165260']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`),end=source.indexOf('\n}\n',start);
    if(start<0)continue;
    const fn=source.slice(start,end),next=fn.replace(/(ctx\.r\d+\.u64 = )(REX_LOAD_U8\(ctx\.r\d+\.u32 \+ 132\));/g,'$1pes6_stadium_lighting_hour(base,$2);');
    source=source.slice(0,start)+next+source.slice(end);
    const decl='extern "C" uint32_t pes6_stadium_lighting_hour(uint8_t*,uint32_t);';if(!source.includes(decl))source=decl+'\n'+source;
  }
  // Explicit menu conditions are authoritative. The original persistence
  // path copied match+325/326/327 from the PREVIOUS resolved match when a
  // selection changed. Preserve native RNG only for the Random choices.
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_826F99C0) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){let fn=source.slice(start,end);
      fn=fn.replace('ctx.r11.u64 = ctx.r3.u32==3?3:REX_LOAD_U8(ctx.r28.u32 + 325);','ctx.r11.u64 = ctx.r3.u64;')
        .replace('ctx.r11.u64 = ctx.r27.u32==3?3:REX_LOAD_U8(ctx.r28.u32 + 326);','ctx.r11.u64 = ctx.r27.u64;')
        .replace('ctx.r11.u64 = REX_LOAD_U8(ctx.r28.u32 + 327);','ctx.r11.u64 = ctx.r29.u64;');
      // The selected venue is already an UI index; do not derive it from
      // match+324 through the inverse map after changing that selection.
      fn=fn.replace('ctx.r7.u64 = REX_LOAD_U8(ctx.r28.u32 + 324);','ctx.r7.u64 = ctx.r30.u64;')
        .replace('ctx.r10.u64 = REX_LOAD_U8(ctx.r7.u32 + ctx.r10.u32);','ctx.r10.u64 = ctx.r7.u64;');
      for(const reg of ['r30','r29','r27','r3'])fn=fn.replace(`ctx.cr6.compare<int32_t>(ctx.r7.s32, ctx.${reg}.s32, ctx.xer);`,`ctx.cr6.compare<int32_t>(ctx.r10.s32, ctx.${reg}.s32, ctx.xer);`);
      // Re-resolve current conditions even if the saved preferences match:
      // another mode or the startup demo may have replaced the active match.
      fn=fn.replace('loc_826F9B38:\n\t// cmpwi cr6,r8,0\n\tctx.cr6.compare<int32_t>(ctx.r8.s32, 0, ctx.xer);\n\t// beq cr6,0x826f9b48\n\tif (ctx.cr6.eq) goto loc_826F9B48;',
        'loc_826F9B38:\n\t// Always apply the conditions currently selected in the menu.');
      source=source.slice(0,start)+fn+source.slice(end);
    }
  }
  // Lighten only position-dependent daylight shade, including characters.
  // At82199438 f1 blends light/shade profile endpoints;82161B40 consumes
  // the same shade weight for the native field/receiver light vectors.
  for(const address of ['82199438','82161B40']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`),end=source.indexOf('\n}\n',start);if(start<0)continue;
    const fn=source.slice(start,end);
    if(!fn.includes('pes6_stadium_shadow_weight(base,'))source=source.slice(0,start)+fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tctx.f1.f64=pes6_stadium_shadow_weight(base,ctx.f1.f64);')+source.slice(end);
    const decl='extern "C" double pes6_stadium_shadow_weight(uint8_t*,double);';if(!source.includes(decl))source=decl+'\n'+source;
  }
  // External stadium editor: native net color descriptor, no texture swap.
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_8216E190) {');
    if(start>=0){const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);if(!fn.includes('pes6_stadium_edit_net(base,ctx.r3.u32)')){fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_stadium_edit_net(base,ctx.r3.u32);');source=source.slice(0,start)+fn+source.slice(end);}}
    if(source.includes('pes6_stadium_edit_net(base,ctx.r3.u32)')&&!source.includes('extern "C" void pes6_stadium_edit_net'))source='extern "C" void pes6_stadium_edit_net(uint8_t*,uint32_t);\n'+source;
  }
  // Teatro's near-side audience is visible from the extended Wide camera.
  // Keep native per-panel frustum tests, placement, LOD and buffer budgets;
  // remove only whole-stand/height suppression and back-side rejection here.
  for(const address of ['8216BD30','8216EF90']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);if(start<0)continue;
    const end=source.indexOf('\n}\n',start);let fn=source.slice(start,end);
    if(fn.includes('// PES6 Teatro near terrace v1'))continue;
    if(address==='8216BD30')fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 Teatro near terrace v1\n\tif(pes6_stadium_teatro_crowd(base)&&ctx.r3.u32){REX_STORE_U32(ctx.r3.u32+8,0);REX_STORE_U32(ctx.r3.u32+12,0);}');
    else{
      const anchor='if (ctx.cr6.lt) goto loc_8216F29C;';
      if(!fn.includes(anchor))throw Error('Missing Teatro crowd back-side anchor');
      fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 Teatro near terrace v1\n\tconst bool pes6TeatroCrowd=pes6_stadium_teatro_crowd(base);').replace(anchor,'if (ctx.cr6.lt&&!pes6TeatroCrowd) goto loc_8216F29C;');
    }
    source=source.slice(0,start)+fn+source.slice(end);
    if(!source.includes('extern "C" bool pes6_stadium_teatro_crowd'))source='extern "C" bool pes6_stadium_teatro_crowd(uint8_t*);\n'+source;
  }

  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_8216EF90) {');if(start>=0){
      const end=source.indexOf('\n}\n',start),fn=source.slice(start,end);
      if(!fn.includes('PES6 Teatro camera row clipping v1')){
        const anchor='ctx.r4.u64 = ctx.r30.u64 + ctx.r11.u64;';if(!fn.includes(anchor))throw Error('Teatro row clipping anchor');
        const next=fn.replace(anchor,anchor+'\n\t// PES6 Teatro camera row clipping v1\n\tif(pes6_stadium_teatro_row_occludes(base,ctx.r4.u32))goto loc_8216F288;');source=source.slice(0,start)+next+source.slice(end);
      }
      const decl='extern "C" bool pes6_stadium_teatro_row_occludes(uint8_t*,uint32_t);';if(!source.includes(decl))source=decl+'\n'+source;
    }
  }
  // Canonical Wembley selector repair: UI9=Wembley, UI10=Random.
  {
    const start=source.indexOf('DEFINE_REX_FUNC(sub_826F9C68) {'),end=source.indexOf('\n}\n',start);
    if(start>=0){let fn=source.slice(start,end);
      fn=fn.replaceAll('if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);','if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);');
      fn=fn.replace(/(\/\/ PES6 Konami preview v2\r?\n)[^\n]+/g,'$1\tif(ctx.r10.u32==0)ctx.r30.u64=10;else if(ctx.r10.u32==9)ctx.r30.u64=11;else if(ctx.r10.u32==10)ctx.r30.u64=8;');
      fn=fn.replace('ctx.cr6.compare<uint32_t>(ctx.r10.u32, 9, ctx.xer);','ctx.cr6.compare<uint32_t>(ctx.r10.u32, 10, ctx.xer);');
      fn=fn.replace('ctx.r3.u64 = ctx.r30.u32==10?8:ctx.r30.u64;','ctx.r3.u64 = REX_LOAD_U8(pes6_stadium_table(ctx,base,0)+REX_LOAD_U8(ctx.r31.u32+182));');
      source=source.slice(0,start)+fn+source.slice(end);
    }
  }

  // Wembley circular navigation and commit: 10 venues + Random = 11 rows.
  for(const address of ['826FA478','826F9708','826F9828','826F99C0']){
    const start=source.indexOf('DEFINE_REX_FUNC(sub_'+address+') {'),end=source.indexOf('\n}\n',start);if(start<0)continue;
    let fn=source.slice(start,end);
    if(address==='826FA478'){
      fn=fn.replaceAll('if(REX_LOAD_U8(ctx.r3.u32+182)>9)REX_STORE_U8(ctx.r3.u32+182,9);','if(REX_LOAD_U8(ctx.r3.u32+182)>10)REX_STORE_U8(ctx.r3.u32+182,10);');
      fn=fn.replace(/ctx\.r11\.s64 = ctx\.r11\.s64 \+ (?:8|9|10);/g,'ctx.r11.s64 = ctx.r11.s64 + 10;');
      fn=fn.replace(/ctx\.r10\.s64 = (?:9|10|11);/g,'ctx.r10.s64 = 11;');
      fn=fn.replace(/static_cast<uint64_t>\((?:9|10|11)\)/g,'static_cast<uint64_t>(11)');
      const time='if(ctx.r30.u32==0)ctx.r3.u64=pes6_stadium_time_choice(ctx.r3.u32);';
      while(fn.includes(time+'\n\t'+time))fn=fn.replace(time+'\n\t'+time,time);
    }else{
      fn=fn.replace(/(compare<(?:u?int32_t)>\(ctx\.r(?:10|11|30)\.(?:u32|s32), )9(, ctx\.xer\);)/g,'$1'+'10'+'$2');
      if(address==='826F9708')fn=fn.replace('ctx.r4.s64 = 9;','ctx.r4.s64 = 10;');
      if(address==='826F99C0')fn=fn.replace('ctx.r10.s64 = 9;','ctx.r10.s64 = 10;');
    }
    source=source.slice(0,start)+fn+source.slice(end);
  }

  // Wembley resource dispatch must reach the helper for ID10's four slots.
  source=source.replaceAll('ctx.r11.u32>=128&&ctx.r11.u32<144 ? pes6_stadium_resource(ctx.r11.u32,0)',
    '((ctx.r11.u32>=128&&ctx.r11.u32<144)||(ctx.r11.u32>=160&&ctx.r11.u32<176)) ? pes6_stadium_resource(ctx.r11.u32,0)');
  return source;
}
module.exports={patchStadium};
