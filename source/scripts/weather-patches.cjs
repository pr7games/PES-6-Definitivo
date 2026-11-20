// Persistent, function-scoped weather patches. All buffer capacities change
// together; renderer-only expansion never changes the ball simulation.
const drops=2500,vertices=drops*2+864*60+128*60,capacity=Math.ceil(vertices/3);
const optional=(enhanced,original)=>`(pes6_weather_enabled()?${enhanced}:${original})`;
function patchWeather(source){
  // Native rain lines plus a separate closed-surface triangle batch.
  for(const [old,next] of [[48640,977920],[1520,20374],[3040,61120],[6080,122240],
    [52736,977920],[1648,20374],[3296,61120],[6592,122240],
    [58880,977920],[1840,20374],[3680,61120],[7360,122240],
    [91648,977920],[2864,20374],[5728,61120],[11456,122240],[161280,977920],[3360,20374],[10080,61120],[20160,122240],[235008,977920],[4896,20374],[14688,61120],[29376,122240],[4800,3200],[28800,19200],[416640,977920],[8680,20374],[26040,61120],[52080,122240],[758400,977920],[15800,20374],[47400,61120],[94800,122240],[6000,3200],[36000,19200]])
    source=source.replaceAll('(pes6_weather_enabled()?'+old+':','(pes6_weather_enabled()?'+next+':');
  // Migrate the installed800-drop layout and legacy capacities together.
  for(const [old,next] of [[1016320,vertices*16],[21174,capacity],[63520,vertices],[127040,vertices*2],[8000,drops*4],[48000,drops*24],[977920,vertices*16],[20374,capacity],[61120,vertices],[122240,vertices*2],[3200,drops*4],[19200,drops*24]])
    source=source.replaceAll('(pes6_weather_enabled()?'+old+':','(pes6_weather_enabled()?'+next+':');
  // Upgrade the previously approved800-drop patch back to its source anchors
  // before applying v2. This also keeps a fresh codegen reproducible.
  for(const address of ['82151420','82151818','82151A80','825DFAA0','8215E450']){
    const start=source.indexOf(`DEFINE_REX_FUNC(sub_${address}) {`);
    if(start<0)continue;const end=source.indexOf('\n}\n',start);
    let fn=source.slice(start,end),old=fn;
    if(/PES6 optional rain (capacities v[345678]|update700 v3|update800 v[45]|update1000 v6|update1200 v[78]|and water draw v[345678])/.test(fn))fn=fn
      .replace(/\n\t\/\/ PES6 optional rain (capacities v[345678]|update700 v3|update800 v[45]|update1000 v6|update1200 v[78]|and water draw v[345678])/,'')
      .replace(/\(pes6_weather_enabled\(\)\?\d+:(\d+)\)/g,'$1')
      .replace('\tpes6_weather_initialized(base,ctx.r30.u32);\n','')
      .replace('\n\tpes6_weather_draw(base,ctx.r31.u32,ctx.r1.u32+80);','');
    if(fn.includes('// PES6 rain independent of stadium visibility setting'))fn=fn
      .replace('\n\t// PES6 rain independent of stadium visibility setting','')
      .replace('ctx.r9.s64 = 1;','ctx.r9.s64 = int16_t(REX_LOAD_U16(ctx.r10.u32 + 34));');
    if(fn.includes('// PES6 rain capacities v2'))fn=fn
      .replace('\n\t// PES6 rain capacities v2','')
      .replace('\tpes6_weather_initialized(base,ctx.r30.u32);\n','')
      .replaceAll('ctx.r3.s64 = 12096;','ctx.r3.s64 = 2000;')
      .replaceAll('ctx.r3.s64 = 4000;','ctx.r3.s64 = 2000;')
      .replaceAll('s32, 4000,','s32, 2000,')
      .replace('ctx.r3.s64 = 72576;','ctx.r3.s64 = 12000;')
      .replace('ctx.r3.s64 = 24000;','ctx.r3.s64 = 12000;')
      .replace('ctx.r10.s64 = 3024;','ctx.r10.s64 = 500;')
      .replace('ctx.r9.s64 = 6048;','ctx.r9.s64 = 1000;');
    if(fn.includes('// PES6 rain update1000'))fn=fn.replace('\n\t// PES6 rain update1000','').replaceAll('s32, 4000,','s32, 2000,');
    if(fn.includes('// PES6 rain and water draw v2'))fn=fn
      .replace('\n\t// PES6 rain and water draw v2','')
      .replace('ctx.r5.s64 = 24000;','ctx.r5.s64 = 12000;')
      .replace('\n\tpes6_weather_draw(base,ctx.r31.u32,ctx.r1.u32+80);','');
    if(fn.includes('// PES6 rain capacities v1'))fn=fn
      .replace('\n\t// PES6 rain capacities v1','')
      .replace('\tpes6_weather_initialized(base,ctx.r30.u32);\n','')
      .replaceAll('ctx.r3.s64 = 6912;','ctx.r3.s64 = 2000;')
      .replace('ctx.r3.s64 = 41472;','ctx.r3.s64 = 12000;')
      .replace('ctx.r3.s64 = 19200;','ctx.r3.s64 = 12000;')
      .replaceAll('ctx.r3.s64 = 3200;','ctx.r3.s64 = 2000;')
      .replaceAll('s32, 3200,','s32, 2000,')
      .replace('ctx.r10.s64 = 1728;','ctx.r10.s64 = 500;')
      .replace('ctx.r9.s64 = 3456;','ctx.r9.s64 = 1000;');
    if(fn.includes('// PES6 rain update800'))fn=fn.replace('\n\t// PES6 rain update800','').replaceAll('s32, 3200,','s32, 2000,');
    if(fn.includes('// PES6 rain wider strokes and splash draw'))fn=fn
      .replace('\n\t// PES6 rain wider strokes and splash draw','')
      .replace('ctx.r5.s64 = 19200;','ctx.r5.s64 = 12000;')
      .replace('\n\tpes6_weather_draw(base,ctx.r31.u32,ctx.r1.u32+80);','');
    if(fn.includes('// PES6 splash simulation tick\n'))fn=fn
      .replace('\n\t// PES6 splash simulation tick','').replace('\n\tpes6_weather_tick(base);','');
    if(fn!==old)source=source.slice(0,start)+fn+source.slice(end);
  }
  const declarations=[];
  function edit(address,marker,transform){
    const entry=`DEFINE_REX_FUNC(sub_${address}) {\n\tREX_FUNC_PROLOGUE();`;
    const start=source.indexOf(entry);if(start<0)return;
    const end=source.indexOf('\n}\n',start);if(end<0)throw Error('Weather function end '+address);
    let fn=source.slice(start,end);
    if(fn.includes(marker))return;
    function once(from,to){
      if(!fn.includes(from)||fn.indexOf(from)!==fn.lastIndexOf(from))throw Error('Weather anchor '+address+': '+from);
      fn=fn.replace(from,to);
    }
    transform(once,()=>fn,value=>{fn=value;});
    fn=fn.replace(entry,entry+'\n\t// '+marker);
    source=source.slice(0,start)+fn+source.slice(end);
  }
  edit('8215E450','PES6 optional rain visibility v3',(once)=>{
    // This field is a render-effect enable, not a resource pointer. The native
    // untextured line emitter requires no stadium-specific rain asset.
    once('ctx.r9.s64 = int16_t(REX_LOAD_U16(ctx.r10.u32 + 34));','ctx.r9.s64 = pes6_weather_enabled()?1:int16_t(REX_LOAD_U16(ctx.r10.u32 + 34));');
  });
  edit('82151420','PES6 optional rain capacities v9',(once,get,set)=>{
    once('ctx.r3.s64 = 2000;\n\t// stw r11,0(r31)',`ctx.r3.s64 = ${optional(vertices*2,2000)};\n\t// stw r11,0(r31)`);
    once('ctx.r3.s64 = 12000;\n\t// stw r31,96(r30)',`ctx.r3.s64 = ${optional(vertices*16,12000)};\n\t// stw r31,96(r30)`);
    once('ctx.r3.s64 = 2000;\n\t// stw r11,104(r30)',`ctx.r3.s64 = ${optional(vertices*2,2000)};\n\t// stw r11,104(r30)`);
    once('ctx.r10.s64 = 500;',`ctx.r10.s64 = ${optional(capacity,500)};`);
    once('ctx.r9.s64 = 1000;',`ctx.r9.s64 = ${optional(vertices,1000)};`);
    let fn=get().replaceAll('ctx.r3.s64 = 2000;',`ctx.r3.s64 = ${optional(drops*4,2000)};`)
      .replaceAll('ctx.r6.s32, 2000,',`ctx.r6.s32, ${optional(drops*4,2000)},`)
      .replaceAll('ctx.r29.s32, 2000,',`ctx.r29.s32, ${optional(drops*4,2000)},`)
      .replaceAll('ctx.r3.s64 = 12000;',`ctx.r3.s64 = ${optional(drops*24,12000)};`);
    set(fn);
    once('loc_82151784:\n', 'loc_82151784:\n\tpes6_weather_initialized(base,ctx.r30.u32);\n');
    declarations.push('void pes6_weather_initialized(uint8_t*,uint32_t);');
  });
  edit('82151818','PES6 optional rain update1200 v9',(once,get,set)=>{
    const fn=get();if((fn.match(/s32, 2000,/g)||[]).length!==2)throw Error('Rain update limits changed');
    set(fn.replaceAll('s32, 2000,',`s32, ${optional(drops*4,2000)},`));
  });
  edit('82151A80','PES6 optional rain and water draw v9',(once)=>{
    once('ctx.r5.s64 = 12000;',`ctx.r5.s64 = ${optional(drops*24,12000)};`);
    once('REX_STORE_U32(ctx.r1.u32 + 92, ctx.r10.u32);',
      'REX_STORE_U32(ctx.r1.u32 + 92, ctx.r10.u32);\n\tpes6_weather_draw(base,ctx.r31.u32,ctx.r1.u32+80);');
    declarations.push('void pes6_weather_draw(uint8_t*,uint32_t,uint32_t);');
  });
  edit('825DFAA0','PES6 splash simulation tick v2',(once)=>{
    once('\t// addi r1,r1,208','\tpes6_weather_tick(base);\n\t// addi r1,r1,208');
    declarations.push('void pes6_weather_tick(uint8_t*);');
  });
  // Replace previous skinning registration hook without duplicating it.
  source=source.replaceAll('PES6 wet foot contacts','PES6 slide actor registry v5')
    .replaceAll('pes6_weather_feet','pes6_weather_actor');
  edit('821A6500','PES6 slide actor registry v5',(once)=>{
    once('\t// addi r1,r1,224','\tpes6_weather_actor(base,ctx.r31.u32);\n\t// addi r1,r1,224');
    declarations.push('void pes6_weather_actor(uint8_t*,uint32_t);');
  });
  edit('822F2010','PES6 unregister wet feet v4',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_weather_forget(base,ctx.r3.u32);');
    declarations.push('void pes6_weather_forget(uint8_t*,uint32_t);');
  });
  edit('8247A3B0','PES6 replay water timeline',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6WeatherReplay=ctx.r3.u32;\n\tpes6_weather_replay_begin(base,pes6WeatherReplay);');
    once('\t// addi r1,r1,144','\tpes6_weather_replay_end(base,pes6WeatherReplay);\n\t// addi r1,r1,144');
    declarations.push('void pes6_weather_replay_begin(uint8_t*,uint32_t);','void pes6_weather_replay_end(uint8_t*,uint32_t);');
  });
  edit('82284910','PES6 copy effect sidecar with native replay frame v1',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_weather_frame_copy(base,ctx.r3.u32,ctx.r4.u32);');
    declarations.push('void pes6_weather_frame_copy(uint8_t*,uint32_t,uint32_t);');
  });
  source=source.replaceAll('pes6_player_match_end();','pes6_player_match_transition();');
  edit('824F9230','PES6 clear appearance at native match destruction v20',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_player_match_transition();');
    declarations.push('void pes6_player_match_transition();');
  });
  edit('82200518','PES6 player attached surface effects v1',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6SurfaceInstance=ctx.r3.u32;');
    once('loc_822005D8:\n','loc_822005D8:\n\tpes6_player_surface_pose(base,pes6SurfaceInstance);\n');
    declarations.push('void pes6_player_surface_pose(uint8_t*,uint32_t);');
  });
  edit('822000A8','PES6 attached surfaces on every native LOD draw v9',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_player_surface_pose(base,ctx.r3.u32);');
    declarations.push('void pes6_player_surface_pose(uint8_t*,uint32_t);');
  });
  // Capture after native skin refresh/callbacks, never from the previous pose
  // at the common draw's entry. All LOD paths still reach this common exit.
  edit('82200518','PES6 defer attached surfaces to completed draw v21',(once)=>{
    once('\tpes6_player_surface_pose(base,pes6SurfaceInstance);','\t// Attached surfaces captured after completed native draw.');
  });
  edit('822000A8','PES6 attached surfaces after completed native draw v21',(once)=>{
    once('\tpes6_player_surface_pose(base,ctx.r3.u32);','\t// Wait for the refreshed native pose.');
    once('\t// addi r1,r1,160','\tpes6_player_surface_pose(base,ctx.r31.u32);\n\t// addi r1,r1,160');
  });
  edit('822000A8','PES6 preserve original surface instance v25',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6CompletedSurfaceInstance=ctx.r3.u32;');
    once('pes6_player_surface_pose(base,ctx.r31.u32);','pes6_player_surface_pose(base,pes6CompletedSurfaceInstance);');
  });
  edit('822000A8','PES6 draw completed pose gate v25',(once)=>{
    once('const uint32_t pes6CompletedSurfaceInstance=ctx.r3.u32;','const uint32_t pes6CompletedSurfaceInstance=ctx.r3.u32;\n\tbool pes6SurfacePoseReady=false;');
    once('sub_82200518(ctx, base);','sub_82200518(ctx, base);\n\tpes6SurfacePoseReady=true;');
    once('pes6_player_surface_pose(base,pes6CompletedSurfaceInstance);','if(pes6SurfacePoseReady)pes6_player_surface_pose(base,pes6CompletedSurfaceInstance);');
  });
  edit('82202890','PES6 capture attached surfaces at actual player draw v2',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_player_surface_draw(base,ctx.r3.u32);');
    declarations.push('void pes6_player_surface_draw(uint8_t*,uint32_t);');
  });
  edit('822007F0','PES6 preserve player surface CPU samples before GPU preparation v5',(once)=>{
    once('loc_8220092C:\n','loc_8220092C:\n\tpes6_player_surface_source(base,ctx.r26.u32,ctx.r14.u32);\n');
    once('sub_82201370(ctx, base);','sub_82201370(ctx, base);\n\tpes6_player_surface_resident(base,ctx.r26.u32);');
    declarations.push('void pes6_player_surface_source(uint8_t*,uint32_t,uint32_t);','void pes6_player_surface_resident(uint8_t*,uint32_t);');
  });
  edit('82201480','PES6 release owned player surface samples v5',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_player_surface_unload(base,ctx.r3.u32);');
    declarations.push('void pes6_player_surface_unload(uint8_t*,uint32_t);');
  });
  edit('821AB368','PES6 attached player surfaces after opaque actors v6',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t pes6ActorGroup=ctx.r3.u32,pes6ActorPass=ctx.r4.u32;');
    once('\t// addi r1,r1,256','\tpes6_player_surface_render(ctx,base,pes6ActorGroup,pes6ActorPass,ctx.r1.u32+96);\n\t// addi r1,r1,256');
    declarations.push('void pes6_player_surface_render(PPCContext&,uint8_t*,uint32_t,uint32_t,uint32_t);');
  });
  edit('821AB368','PES6 refresh water reflections for every camera view v8',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_player_surface_view_begin(base,ctx.r3.u32,ctx.r4.u32);');
    declarations.push('void pes6_player_surface_view_begin(uint8_t*,uint32_t,uint32_t);');
  });
  edit('82202BF0','PES6 native skin normal specular moisture v8',(once)=>{
    once('ctx.lr = 0x82202D24;','pes6_player_wet_material(base,ctx.r31.u32,ctx.r1.u32+80);\n\tctx.lr = 0x82202D24;');
    declarations.push('void pes6_player_wet_material(uint8_t*,uint32_t,uint32_t);');
  });
  for(const address of ['82292010','822926D4'])edit(address,'PES6 capture effects with half-time highlight frame v1',(once)=>{
    once('sub_82284818(ctx, base);','if(ctx.r26.u32==0)pes6_weather_frame_copy(base,ctx.r31.u32+16,ctx.r28.u32+48);\n\tsub_82284818(ctx, base);');
    declarations.push('void pes6_weather_frame_copy(uint8_t*,uint32_t,uint32_t);');
  });
  edit('825DFF38','PES6 actual ground impact splash',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst bool pes6ActualBallStep=ctx.r3.u32==0;');
    once('loc_825E0E30:\n','loc_825E0E30:\n\tif(pes6ActualBallStep)pes6_weather_impact(base,ctx.r24.u32,ctx.r19.u32,ctx.r23.u32,std::bit_cast<float>(REX_LOAD_U32(ctx.r22.u32)));\n');
    declarations.push('void pes6_weather_impact(uint8_t*,uint32_t,uint32_t,uint32_t,float);');
  });
  edit('8215E450','PES6 all-weather surface renderer v1',(once)=>{
    once('REX_STORE_U32(ctx.r4.u32 + 0, ctx.r11.u32);','if(pes6_weather_enabled())ctx.r11.u64|=16;\n\tREX_STORE_U32(ctx.r4.u32 + 0, ctx.r11.u32);');
  });
  edit('82151818','PES6 skip dry rain simulation v1',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tif(pes6_weather_enabled()&&!pes6_weather_wet(base))return;');
    declarations.push('bool pes6_weather_wet(uint8_t*);');
  });
  edit('82151A80','PES6 surface debris draw v1',(once)=>{
    once('sub_82195A80(ctx, base);','pes6_surface_render(ctx, base);');
    declarations.push('void pes6_surface_render(PPCContext&,uint8_t*);');
  });
  edit('82151420','PES6 colored surface buffer layout v1',(once)=>{
    once('REX_STORE_U32(ctx.r31.u32 + 4, ctx.r29.u32);','REX_STORE_U32(ctx.r31.u32 + 4, pes6_weather_enabled()?1:ctx.r29.u32);');
    once('ctx.r8.s64 = 12;','ctx.r8.s64 = pes6_weather_enabled()?16:12;');
  });
  edit('82151A80','PES6 skip empty dry surface draw v1',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tif(pes6_weather_enabled()&&!pes6_surface_pending(base))return;');
    declarations.push('bool pes6_surface_pending(uint8_t*);');
  });
  edit('8213F4D8','PES6 classify actual player motion for turf v1',(once)=>{
    once('\t// addi r1,r1,144','\tpes6_surface_motion(base,ctx.r31.u32);\n\t// addi r1,r1,144');
    declarations.push('void pes6_surface_motion(uint8_t*,uint32_t);');
  });
  edit('821400E0','PES6 observe live body controller v1',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_surface_motion(base,ctx.r3.u32);');
    declarations.push('void pes6_surface_motion(uint8_t*,uint32_t);');
  });
  edit('82195A30','PES6 release owned surface GPU resources v1',(once)=>{
    once('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_surface_release(ctx,base,ctx.r3.u32);');
    declarations.push('void pes6_surface_release(PPCContext&,uint8_t*,uint32_t);');
  });
  source=source.split('\n').filter(line=>!line.includes('pes6_grass_enabled')&&!line.includes('PES6 native grass height8percent')&&!line.includes('ctx.r31.u32+32,std::bit_cast<uint32_t>(std::bit_cast<float>(REX_LOAD_U32(ctx.r31.u32+32))*1.08f)')).join('\n');
  if(source.includes('pes6_weather_enabled()')&&!source.includes('extern "C" bool pes6_weather_enabled();'))declarations.push('bool pes6_weather_enabled();');
  if(declarations.length)source='#include <bit>\n'+declarations.map(d=>'extern "C" '+d).join('\n')+'\n'+source;
  return require('./torrential-weather-patches.cjs').patchTorrential(source);
}
module.exports={patchWeather,drops:1500,capacity:72854,vertices:218560,maxDrops:10000,torrentialDrops:10000,indexVertices:59520};
