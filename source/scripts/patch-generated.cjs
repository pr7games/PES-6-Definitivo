const fs = require('fs');
const path = require('path');
const {patchWeather} = require('./weather-patches.cjs');
const {patchStadium} = require('./stadium-patches.cjs');
const {patchBalls} = require('./ball-patches.cjs');
const {patchCompetitionPresentation}=require('./competition-presentation-patches.cjs');
const patchLeagueOrder=require('./league-order-patches.cjs');
const {patchMenuCarousel}=require('./menu-carousel-patches.cjs');
const patchSetPieces=require('./setpiece-gameplay-patches.cjs');
const patchGameplay=require('./gameplay-tuning-patches.cjs');
const patchShotDispersion=require('./shot-dispersion-patches.cjs');
const patchChargedPasses=require('./charged-pass-patches.cjs');
const patchDistribution=require('./distribution-charge-patches.cjs');
const patchGeneralPrecision=require('./general-precision-patches.cjs');
const patchElastica=require('./elastica-patches.cjs');
const patchCR7Skills=require('./cr7-skills-patches.cjs');
const patchTeamMenu=require('./team-menu-patches.cjs');

const root = path.resolve(__dirname, '..');
const dir = path.join(root, 'native/generated/default');

if (!fs.existsSync(dir)) {
  console.log('Generated directory not found yet; skipping patch.');
  process.exit(0);
}


const {patchNetSound}=require('./net-sound-patches.cjs');
const {patchAdboardSound}=require('./adboard-sound-patches.cjs');
const {patchSubstitutionNavigation}=require('./substitution-navigation-patches.cjs');
const {patchCloth}=require('./cloth-patches.cjs');
let patchedCount = 0;
for (const f of fs.readdirSync(dir)) {
  if (!f.startsWith('pes6_recomp.') || !f.endsWith('.cpp')) continue;
  const filePath = path.join(dir, f);
  let content = fs.readFileSync(filePath, 'utf8');
  let next = patchSubstitutionNavigation(patchMenuCarousel(patchCloth(patchAdboardSound(patchNetSound(patchStadium(patchWeather(content)))))));

  if(next.includes('DEFINE_REX_FUNC(sub_822E9F08)')&&!next.includes('pes6_native_team_update')){
    const entry='DEFINE_REX_FUNC(sub_822E9F08) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw Error('Team editor input anchor changed');
    next='extern "C" bool pes6_native_team_update(PPCContext&,uint8_t*);\n'+next.replace(entry,entry+'\n\tif(pes6_native_team_update(ctx,base))return;');
  }
  if(next.includes('DEFINE_REX_FUNC(sub_822E7BF8)')&&!next.includes('pes6_native_team_leave')){
    const entry='DEFINE_REX_FUNC(sub_822E7BF8) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw Error('Team editor cleanup anchor changed');
    next='extern "C" void pes6_native_team_leave(PPCContext&,uint8_t*);\n'+next.replace(entry,entry+'\n\tpes6_native_team_leave(ctx,base);');
  }
  if(next.includes('DEFINE_REX_FUNC(sub_828209B8)')){
    next=next.replace(/DEFINE_REX_FUNC\(sub_828209B8\) \{[\s\S]*?\n\}/, 'DEFINE_REX_FUNC(sub_828209B8) {\n\tREX_FUNC_PROLOGUE();\n\textern void pes6_show_keyboard_ui(PPCContext&, uint8_t*);\n\tpes6_show_keyboard_ui(ctx, base);\n\treturn;\n}');
  }
  // Replace the earlier broad classification override with field-specific getters.
  next=next.replace('extern "C" bool pes6_team_tactics_custom(uint8_t*,uint32_t);\n','')
    .replace('\n\tif(pes6_team_tactics_custom(base,ctx.r4.u32)){ctx.r3.u64=0;return;}','');
  for(const [address,hook]of [['8217B3E8','pes6_team_formation'],['8217B360','pes6_team_role']]){
    if(next.includes('DEFINE_REX_FUNC(sub_'+address+')')&&!next.includes(hook)){
      const entry='DEFINE_REX_FUNC(sub_'+address+') {\n\tREX_FUNC_PROLOGUE();';
      if(!next.includes(entry))throw Error('Team tactics getter anchor changed '+address);
      next='extern "C" bool '+hook+'(PPCContext&,uint8_t*);\n'+next.replace(entry,entry+'\n\tif('+hook+'(ctx,base))return;');
    }
  }
  if(next.includes('DEFINE_REX_FUNC(sub_82178520)')&&!next.includes('pes6_team_configuration')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_82178520)'),end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);const exit='\t// blr \n\treturn;';
    if(!fn.includes(exit))throw Error('Team configuration getter exit changed');
    fn=fn.replace(exit,'\tpes6_team_configuration(base,ctx.r3.u32,ctx.r4.u32);\n'+exit);
    next='extern "C" void pes6_team_configuration(uint8_t*,uint32_t,uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }
  next=patchTeamMenu(next);
  next=patchLeagueOrder(next);
  next=patchSetPieces(next);
  next=patchGameplay(next);
  next=patchShotDispersion(next);
  next=patchChargedPasses(next);
  next=patchDistribution(next);
  next=patchGeneralPrecision(next);
  next=patchElastica(next);
  next=patchCR7Skills(next);
  next=next.replace('void pes6_editor_hair_tint(uint8_t*,uint32_t,uint32_t);','void pes6_editor_hair_tint(PPCContext&,uint8_t*,uint32_t,uint32_t);')
    .replace('pes6_editor_hair_tint(base,hairKey,hairHead);','pes6_editor_hair_tint(ctx,base,hairKey,hairHead);');
  if(next.includes('DEFINE_REX_FUNC(sub_822FC8E0)')&&!next.includes('pes6_editor_fit_hair')){
    const entry='DEFINE_REX_FUNC(sub_822FC8E0) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw Error('Hair geometry process anchor changed');
    next='extern "C" void pes6_editor_fit_hair(uint8_t*,uint32_t);\n'+next.replace(entry,entry+'\n\tpes6_editor_fit_hair(base,ctx.r3.u32);');
  }
  if(next.includes('DEFINE_REX_FUNC(sub_822FC7D8)')&&!next.includes('pes6_editor_hair_color8')){
    const entry='DEFINE_REX_FUNC(sub_822FC7D8) {\n\tREX_FUNC_PROLOGUE();';
    const start=next.indexOf(entry),end=next.indexOf('\n}\n',start);
    if(start<0||end<0)throw Error('Hair color callback anchor changed');
    let fn=next.slice(start,end);const exit='\t// blr \n\treturn;';
    if(!fn.includes(exit))throw Error('Hair color callback exit changed');
    fn=fn.replace(entry,entry+'\n\tconst uint32_t hairColorEffect=ctx.r3.u32;')
      .replace(exit,'\tpes6_editor_hair_color8(base,hairColorEffect);\n'+exit);
    next='extern "C" void pes6_editor_hair_color8(uint8_t*,uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  if(next.includes('DEFINE_REX_FUNC(sub_822FCCE0)')&&!next.includes('pes6_editor_hair_tint')){
    const entry='DEFINE_REX_FUNC(sub_822FCCE0) {\n\tREX_FUNC_PROLOGUE();';
    const start=next.indexOf(entry),end=next.indexOf('\n}\n',start);
    if(start<0||end<0)throw Error('Hair tint cache anchors changed');
    let fn=next.slice(start,end);
    const exit='\t// blr \n\treturn;';
    if(!fn.includes(exit))throw Error('Hair tint cache return changed');
    fn=fn.replace(entry,entry+'\n\tconst uint32_t hairKey=ctx.r3.u32,hairHead=ctx.r4.u32;')
      .replace(exit,'\tpes6_editor_hair_tint(ctx,base,hairKey,hairHead);\n'+exit);
    next='extern "C" void pes6_editor_hair_tint(PPCContext&,uint8_t*,uint32_t,uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  // Use the shipped complete ball catalog consistently: labels, saved-ID lookup,
  // selected-ID/Random resolution and preview. No writes to image tables.
  for (const [address, register] of [['827142F0',4],['82714348',5],['827143A8',5],['82714488',6]]) {
    const entry = `DEFINE_REX_FUNC(sub_${address}) {\n\tREX_FUNC_PROLOGUE();`;
    const marker = `// PES6 complete native ball catalog (${address})`;
    if (next.includes(`DEFINE_REX_FUNC(sub_${address})`) && !next.includes(marker)) {
      if (!next.includes(entry)) throw Error('Ball catalog entry changed: '+address);
      next = next.replace(entry, entry+`\n\t${marker}\n\tctx.r${register}.u64 = 1;`);
    }
  }

  next=patchBalls(next);

  // Extra native camera menu entry; camera12 is UI-only and aliases Wide3
  // before any native configuration or match dispatcher sees the selection.
  if(next.includes('DEFINE_REX_FUNC(sub_8246F810)')&&!next.includes('pes6_camera_menu_labels')){
    const entry='DEFINE_REX_FUNC(sub_8246F810) {\n\tREX_FUNC_PROLOGUE();';
    const labels='ctx.lr = 0x8246F980;\n\tsub_8246B5F8(ctx, base);';
    const exit='\t// addi r1,r1,624';
    if(!next.includes(entry)||!next.includes(labels)||!next.includes(exit))throw Error('Native camera menu anchors changed');
    next='extern "C" void pes6_camera_menu_labels(PPCContext&,uint8_t*,uint32_t);\nextern "C" void pes6_camera_menu_ready(PPCContext&,uint8_t*,uint32_t);\n'+next
      .replace(entry,entry+'\n\tconst uint32_t cameraMenuOwner=ctx.r3.u32;')
      .replace(labels,'pes6_camera_menu_labels(ctx,base,cameraMenuOwner);\n\t'+labels);
    // Limit the exit replacement to this function, not another624-byte frame.
    const start=next.indexOf('DEFINE_REX_FUNC(sub_8246F810)'),end=next.indexOf('\n}\n',start);
    const fn=next.slice(start,end);
    next=next.slice(0,start)+fn.replace(exit,'\tpes6_camera_menu_ready(ctx,base,cameraMenuOwner);\n'+exit)+next.slice(end);
  }
  if(next.includes('DEFINE_REX_FUNC(sub_8246F218)')&&!next.includes('pes6_camera_choice')){
    const anchor='ctx.lr = 0x8246F2FC;\n\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);';
    if(!next.includes(anchor))throw Error('Native camera choice anchor changed');
    next='extern "C" uint32_t pes6_camera_choice(uint32_t);\n'+next.replace(anchor,anchor+'\n\tctx.r3.u64=pes6_camera_choice(ctx.r3.u32);');
  }
  // Upgrade the old one-row alias, including previously generated files.
  next=next.replace('\t// PES6 far-wide UI alias\n\tif(ctx.r3.u32==12)ctx.r3.u64=3;','\t// PES6 ordered wide UI mapping\n\tctx.r3.u64=pes6_camera_native_choice(ctx.r3.u32);');
  if(next.includes('DEFINE_REX_FUNC(sub_8246F640)')){
    const anchor='ctx.lr = 0x8246F664;\n\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);';
    if(!next.includes('// PES6 ordered wide UI mapping')){
      if(!next.includes(anchor))throw Error('Native camera angle controls anchor changed');
  next=next.replace(anchor,anchor+'\n\t// PES6 ordered wide UI mapping\n\tctx.r3.u64=pes6_camera_native_choice(ctx.r3.u32);');
    }
    if(!next.includes('extern "C" uint32_t pes6_camera_native_choice'))
      next='extern "C" uint32_t pes6_camera_native_choice(uint32_t);\n'+next;
  }
  if(next.includes('DEFINE_REX_FUNC(sub_824AB330)')&&!next.includes('pes6_camera_parameters')){
    const entry='DEFINE_REX_FUNC(sub_824AB330) {\n\tREX_FUNC_PROLOGUE();';
    const start=next.indexOf(entry),end=next.indexOf('\n}\n',start);
    if(start<0)throw Error('Native camera parameters entry changed');
    let fn=next.slice(start,end);
    const exit='\t// addi r1,r1,160';
    if(!fn.includes(exit))throw Error('Native camera parameters return changed');
    fn=fn.replace(entry,entry+'\n\tconst uint32_t matchCamera=ctx.r3.u32;')
      .replace(exit,'\tpes6_camera_parameters(ctx,base,matchCamera);\n'+exit);
    next='extern "C" void pes6_camera_parameters(PPCContext&,uint8_t*,uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  if(next.includes('DEFINE_REX_FUNC(sub_822FB9E8)')&&!next.includes('pes6_patch_club_kit')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822FB9E8)'),end=next.indexOf('\n}\n',start);let fn=next.slice(start,end);
    fn=fn.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tconst uint32_t customKitOutput=ctx.r3.u32, customKitRole=ctx.r4.u32, customKitTeam=ctx.r5.u32, customKitVariant=ctx.r6.u32;');
    const exit='\t// addi r1,r1,848';if(!fn.includes(exit))throw Error('Kit descriptor return missing');
    fn=fn.replace(exit,'\tpes6_patch_club_kit(ctx,base,customKitOutput,customKitTeam,customKitRole,customKitVariant);\n'+exit);
    next='extern "C" void pes6_patch_club_kit(PPCContext&,uint8_t*,uint32_t,uint32_t,uint32_t,uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }
  for(const [fn,hook,result]of [['822F6280','pes6_editor_name_resource',true],['822F6A68','pes6_editor_number_resource',true],['822F1460','pes6_editor_copy_lettering',false],['822FB9E8','pes6_editor_kit_model',false],['8217EC30','pes6_home_stadium_name',true],['826F9ED0','pes6_home_stadium_select',false],['822FBE00','pes6_generic_kit_colors',false]]){
    const entry='DEFINE_REX_FUNC(sub_'+fn+') {\n\tREX_FUNC_PROLOGUE();';
    if(next.includes(entry)&&!next.includes(hook)){
      next='extern "C" '+(result?'bool':'void')+' '+hook+'(PPCContext&,uint8_t*);\n'+next.replace(entry,entry+'\n\t'+(result?'if('+hook+'(ctx,base))return;':hook+'(ctx,base);'));
    }
  }
  for(const [fn,exit,reg,numbers]of [['822F62C8','822F6460','r28',false],['822F6AD8','822F6D80','r31',true]]){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_'+fn+')');if(start<0)continue;const end=next.indexOf('\n}\n',start);let body=next.slice(start,end);const branch='if (ctx.cr0.eq) goto loc_'+exit+';';if(body.includes(branch)){body=body.replace(branch,'if (ctx.cr0.eq && !pes6_editor_lettering_recolor(ctx.'+reg+'.u32,'+numbers+')) goto loc_'+exit+';');if(!next.includes('extern "C" bool pes6_editor_lettering_recolor'))next='extern "C" bool pes6_editor_lettering_recolor(uint32_t,bool);\n'+next;const actual=next.indexOf('DEFINE_REX_FUNC(sub_'+fn+')'),stop=next.indexOf('\n}\n',actual);next=next.slice(0,actual)+body+next.slice(stop);}
  }
  if(next.includes('DEFINE_REX_FUNC(sub_822F6AD8)')&&!next.includes('pes6_editor_lettering_nativo(base,ctx.r31.u32')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822F6AD8)'),end=next.indexOf('\n}\n',start);let body=next.slice(start,end);
    const branch='if (ctx.cr6.eq) goto loc_822F6C0C;';
    if(body.includes(branch)){body=body.replace(branch,'if (pes6_editor_lettering_nativo(base,ctx.r31.u32,ctx.r29.u32,ctx.r30.u32,true)) goto loc_822F6D80;\n\tif (ctx.cr6.eq) goto loc_822F6C0C;');if(!next.includes('extern "C" bool pes6_editor_lettering_nativo'))next='extern "C" bool pes6_editor_lettering_nativo(uint8_t*,uint32_t,uint32_t,uint32_t,bool);\n'+next;const actual=next.indexOf('DEFINE_REX_FUNC(sub_822F6AD8)'),stop=next.indexOf('\n}\n',actual);next=next.slice(0,actual)+body+next.slice(stop);}
  }
  if(next.includes('DEFINE_REX_FUNC(sub_822F62C8)')&&!next.includes('pes6_editor_lettering_nativo(base,ctx.r28.u32')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822F62C8)'),end=next.indexOf('\n}\n',start);let body=next.slice(start,end);
    const branch='if (!ctx.cr0.eq) goto loc_822F638C;';
    if(body.includes(branch)){body=body.replace(branch,'if (pes6_editor_lettering_nativo(base,ctx.r28.u32,ctx.r31.u32,0,false)) goto loc_822F6460;\n\tif (!ctx.cr0.eq) goto loc_822F638C;');if(!next.includes('extern "C" bool pes6_editor_lettering_nativo'))next='extern "C" bool pes6_editor_lettering_nativo(uint8_t*,uint32_t,uint32_t,uint32_t,bool);\n'+next;const actual=next.indexOf('DEFINE_REX_FUNC(sub_822F62C8)'),stop=next.indexOf('\n}\n',actual);next=next.slice(0,actual)+body+next.slice(stop);}
  }
  if(next.includes('DEFINE_REX_FUNC(sub_82178580)')&&!next.includes('pes6_apply_real_team_names')){
    const entry='DEFINE_REX_FUNC(sub_82178580) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw Error('Missing database initialization entry');
    next='extern "C" void pes6_apply_real_team_names(PPCContext&, uint8_t*);\n'+next.replace(entry,entry+'\n\tpes6_apply_real_team_names(ctx, base);');
  }

  if(next.includes('DEFINE_REX_FUNC(sub_821A4628)')&&!next.includes('pes6_decode_premier_crest')){
    const entry='DEFINE_REX_FUNC(sub_821A4628) {\n\tREX_FUNC_PROLOGUE();';
    next='extern "C" bool pes6_decode_premier_crest(PPCContext&, uint8_t*);\n'+next.replace(entry,entry+'\n\tif(pes6_decode_premier_crest(ctx, base))return;');
  }
  // Retire the depth-only experiment: it also blurred the pitch/players.
  // Crowd contour filtering requires a dedicated coverage mask/composition.
  next=next.split('\n').filter(line=>!line.includes('pes6_crowd_dof_')&&!line.includes('pes6CrowdDof')&&!line.includes('pes6CrowdFilter')).join('\n');
  // Retire the forced-impostor experiment: native near rows have no complete
  // impostor coverage. Preserve the original crowd geometry/LOD/atlas sizes.
  next=next.split('\n').filter(line=>!line.includes('pes6_soft_crowd_atlas')&&!line.includes('pes6_soft_crowd_impostors')&&!line.includes('pes6_soft_crowd_material')).join('\n');
  // Retired contour experiments. Keep only the approved texture replacement.
  next=next.split('\n').filter(line=>!line.includes('pes6_crowd_pass_')&&!line.includes('pes6CrowdPassActive')).join('\n');
  next=next.split('\n').filter(line=>!line.includes('pes6_konami_flare_atlas')).join('\n');
  // The crowd atlas builder calls the WE reader directly, bypassing 8218E880.
  if(next.includes('DEFINE_REX_FUNC(sub_8218F4A8)')){
    const entry='DEFINE_REX_FUNC(sub_8218F4A8) {\n\tREX_FUNC_PROLOGUE();';
    const begin=next.indexOf(entry),end=next.indexOf('\n}\n',begin);
    if(begin<0)throw Error('Missing native WE reader entry');
    if(!next.slice(begin,end).includes('pes6_soft_crowd_source')){
      next='extern "C" void pes6_soft_crowd_source(uint8_t*, uint32_t);\n'+next.replace(entry,entry+'\n\tpes6_soft_crowd_source(base,ctx.r4.u32);');
    }
  }
  if(next.includes('DEFINE_REX_FUNC(sub_8218E880)')){
    const entry='DEFINE_REX_FUNC(sub_8218E880) {\n\tREX_FUNC_PROLOGUE();';
    const begin=next.indexOf(entry),end=next.indexOf('\n}\n',begin);
    if(begin<0)throw Error('Missing GPU image conversion entry');
    if(!next.slice(begin,end).includes('pes6_competition_texture_source')){
      next='extern "C" void pes6_competition_texture_source(uint8_t*, uint32_t);\n'+next.replace(entry,entry+'\n\tpes6_competition_texture_source(base,ctx.r3.u32);');
    }
  }
  // Competition emblems (CV2699/2700) use codec0/copy, not the zlib decoder.
  // Keep both dispatcher targets connected; direct helper-only tests miss this.
  if(next.includes('DEFINE_REX_FUNC(sub_821A45A8)')){
    const entry='DEFINE_REX_FUNC(sub_821A45A8) {\n\tREX_FUNC_PROLOGUE();';
    const begin=next.indexOf(entry),end=next.indexOf('\n}\n',begin);
    if(begin<0)throw Error('Missing native copy decoder entry');
    if(!next.slice(begin,end).includes('pes6_decode_premier_crest')){
      if(!next.includes('extern "C" bool pes6_decode_premier_crest'))next='extern "C" bool pes6_decode_premier_crest(PPCContext&, uint8_t*);\n'+next;
  next=next.replace(entry,entry+'\n\tif(pes6_decode_premier_crest(ctx, base))return;');
    }
  }
  if(next.includes('DEFINE_REX_FUNC(sub_8219E398)')&&!next.includes('pes6_load_expanded_boot')){
    const entry='DEFINE_REX_FUNC(sub_8219E398) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw Error('Missing native resource decoder entry');
    next='extern "C" bool pes6_load_expanded_boot(PPCContext&, uint8_t*);\n'+next.replace(entry,entry+'\n\tif(pes6_load_expanded_boot(ctx, base))return;');
  }

  // Expanded boot IDs map to appended AFS packages, never adjacent player meshes.
  for(const bootFn of ['sub_822F3920','sub_822F95E8','sub_822F3CD0'])if(next.includes('DEFINE_REX_FUNC('+bootFn+')')){
    const start=next.indexOf('DEFINE_REX_FUNC('+bootFn+')'),end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    if(!fn.includes('pes6::editor::BootResource')){
      const add='ctx.r11.s64 = ctx.r11.s64 + 2716;';
      if(!fn.includes(add))throw Error('Missing native boot resource addition '+bootFn);
      fn=fn.replace(add,add+'\n\tctx.r11.u64 = pes6::editor::BootResource(ctx.r11.u32 - 2716);');
      next=next.slice(0,start)+fn+next.slice(end);
      if(!next.includes('#include "../../src/boot_catalog.h"'))next='#include "../../src/boot_catalog.h"\n'+next;
    }
  }

  if(next.includes('DEFINE_REX_FUNC(sub_822E7F58)')&&!next.includes('pes6_native_preview_warm_team')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822E7F58)');
    const end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    const exit='\t// addi r1,r1,176';
    if(!fn.includes(exit))throw Error('Missing roster prefetch exit');
    fn=fn.replace(exit,'\tpes6_native_preview_warm_team(ctx, base);\n'+exit);
    next='extern "C" void pes6_native_preview_warm_team(PPCContext&, uint8_t*);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  if(next.includes('DEFINE_REX_FUNC(sub_822E7790)')&&!next.includes('pes6_native_preview_warm')){
    const entry='DEFINE_REX_FUNC(sub_822E7790) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw Error('Missing editor warmup entry');
    next='extern "C" void pes6_native_preview_warm(PPCContext&, uint8_t*);\n'+next.replace(entry,entry+'\n\tpes6_native_preview_warm(ctx, base);');
  }

  if(next.includes('DEFINE_REX_FUNC(sub_827277B0)')&&!next.includes('pes6_native_preview_draw')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_827277B0)');
    const end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    const exit='\t// addi r1,r1,128';
    if(!fn.includes(exit))throw Error('Missing UI render exit');
    fn=fn.replace('\tREX_FUNC_PROLOGUE();','\tREX_FUNC_PROLOGUE();\n\tconst uint32_t previewQueue=ctx.r3.u32;');
    fn=fn.replace(exit,'\tpes6_native_preview_draw(ctx, base, previewQueue);\n'+exit);
    next='extern "C" void pes6_native_preview_draw(PPCContext&, uint8_t*, uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  if(next.includes('DEFINE_REX_FUNC(sub_822E5810)')&&!next.includes('pes6_native_editor_leave')){
    const entry='DEFINE_REX_FUNC(sub_822E5810) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw new Error('Missing native editor cleanup anchor');
    next='extern "C" void pes6_native_editor_leave(PPCContext&, uint8_t*);\n'+
      next.replace(entry,entry+'\n\tpes6_native_editor_leave(ctx, base);');
  }

  if(next.includes('DEFINE_REX_FUNC(sub_822E5D38)')&&!next.includes('pes6_native_editor_enter')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822E5D38)');
    const end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    const prologue='\tREX_FUNC_PROLOGUE();';
    const exit='\t// addi r1,r1,208';
    if(!fn.includes(exit))throw new Error('Missing native editor initialization exit');
    fn=fn.replace(prologue,prologue+'\n\tconst uint32_t pes6_native_editor_owner = ctx.r3.u32;');
    fn=fn.replace(exit,'\tpes6_native_editor_enter(ctx, base, pes6_native_editor_owner);\n'+exit);
    next='extern "C" void pes6_native_editor_enter(PPCContext&, uint8_t*, uint32_t);\n'+
      next.slice(0,start)+fn+next.slice(end);
  }
  if(next.includes('DEFINE_REX_FUNC(sub_822E6828)')&&!next.includes('pes6_native_editor_update')){
    const entry='DEFINE_REX_FUNC(sub_822E6828) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw new Error('Missing native editor input anchor');
    next='extern "C" bool pes6_native_editor_update(PPCContext&, uint8_t*);\n'+
      next.replace(entry,entry+'\n\tif (pes6_native_editor_update(ctx, base)) return;');
  }

  // Native Information selectors already support circular arithmetic. Enable
  // that branch for Age as well; custom Nationality is handled independently.
  if(next.includes('DEFINE_REX_FUNC(sub_822E6D58)')&&!next.includes('pes6_native_editor_circular')){
    const anchor='loc_822E6E04:';
    if(!next.includes(anchor))throw new Error('Missing native Information circular anchor');
    next='extern "C" bool pes6_native_editor_circular();\n'+next.replace(anchor,
      anchor+'\n\tif (pes6_native_editor_circular()) ctx.r23.u64 = 1;');
  }
  // Native Ability values range 1..99, or 1..8 for condition/weak foot.
  if(next.includes('DEFINE_REX_FUNC(sub_822EDE80)')&&!next.includes('pes6_native_editor_circular')){
    const anchor='loc_822EE01C:';
    if(!next.includes(anchor))throw new Error('Missing native Ability circular anchor');
    next='extern "C" bool pes6_native_editor_circular();\n'+next.replace(anchor,
      anchor+'\n\tif (pes6_native_editor_circular()) {\n'+
      '\t\tconst int maximum = ctx.r28.u32 >= 1 && ctx.r27.u32 >= 11 ? 8 : 99;\n'+
      '\t\tctx.r4.s64 = ((ctx.r4.s32 - 1) % maximum + maximum) % maximum + 1;\n'+
      '\t\tgoto loc_822EE0A8;\n\t}');
  }
  // Refresh Information arrows after native focus/value updates, using the
  // final text and edit mode of the current frame rather than stale values.
  if(next.includes('DEFINE_REX_FUNC(sub_822E6D58)')&&!next.includes('pes6_native_editor_after_update')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822E6D58)');
    const end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    const entry='DEFINE_REX_FUNC(sub_822E6D58) {\n\tREX_FUNC_PROLOGUE();';
    if(!fn.includes(entry))throw new Error('Missing native Information edit entry anchor');
    fn=fn.replace(entry,entry+'\n\tconst uint32_t pes6_editor_frame_owner = ctx.r3.u32;');
    const exit='\t__restgprlr_22(ctx, base);\n\treturn;';
    if(!fn.includes(exit))throw new Error('Missing native Information edit exit anchor');
    fn=fn.replace(exit,'\t__restgprlr_22(ctx, base);\n'+
      '\tpes6_native_editor_after_update(ctx, base, pes6_editor_frame_owner);\n\treturn;');
    next='extern "C" void pes6_native_editor_after_update(PPCContext&, uint8_t*, uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }
  if(next.includes('DEFINE_REX_FUNC(sub_822E6828)')&&!next.includes('pes6_native_editor_after_update')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_822E6828)');
    const end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    const entry='\tif (pes6_native_editor_update(ctx, base)) return;';
    if(!fn.includes(entry))throw new Error('Missing native editor update wrapper anchor');
    fn=fn.replace(entry,'\tconst uint32_t pes6_editor_frame_owner = ctx.r3.u32;\n'+
      '\tif (pes6_native_editor_update(ctx, base)) { pes6_native_editor_after_update(ctx, base, pes6_editor_frame_owner); return; }');
    const exit='\t__restgprlr_27(ctx, base);\n\treturn;';
    if(!fn.includes(exit))throw new Error('Missing native editor update exit anchor');
    fn=fn.replace(exit,'\t__restgprlr_27(ctx, base);\n'+
      '\tpes6_native_editor_after_update(ctx, base, pes6_editor_frame_owner);\n\treturn;');
    next='extern "C" void pes6_native_editor_after_update(PPCContext&, uint8_t*, uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  if (next.includes('DEFINE_REX_FUNC(sub_822E7380)') && !next.includes('pes6_native_editor_direct_subpage_exit')) {
    const anchor = 'loc_822E73E4:\n';
    if (!next.includes(anchor)) throw new Error('Missing native editor direct subpage exit anchor');
    next = 'extern "C" bool pes6_native_editor_direct_subpage_exit(PPCContext&, uint8_t*);\n' +
      next.replace(anchor, anchor + '\tif (pes6_native_editor_direct_subpage_exit(ctx, base)) goto loc_822E7424;\n');
  }
  if (next.includes('DEFINE_REX_FUNC(sub_822E5D38)') && !next.includes('pes6_editor_selected')) {
    const entry = 'DEFINE_REX_FUNC(sub_822E5D38) {\n\tREX_FUNC_PROLOGUE();';
    if(!next.includes(entry))throw new Error('Missing native editor player-selection anchor');
    next='extern "C" void pes6_editor_selected(uint8_t*);\n'+
      next.replace(entry,entry+'\n\tpes6_editor_selected(base);');
  }

  // Separate scanned-face and hair resources after the packed decoder aliases them.
  if(next.includes('DEFINE_REX_FUNC(sub_821A7EA8)')&&!next.includes('pes6_editor_head')){
    const start=next.indexOf('DEFINE_REX_FUNC(sub_821A7EA8)');
    const end=next.indexOf('\n}\n',start);
    let fn=next.slice(start,end);
    fn=fn.replace('\tREX_FUNC_PROLOGUE();','\tREX_FUNC_PROLOGUE();\n\tconst uint32_t headTarget=ctx.r3.u32,headSource=ctx.r4.u32;');
    if(!fn.includes('\t// blr'))throw Error('Missing appearance decoder exit');
    fn=fn.replaceAll('\t// blr','\tpes6_editor_head(base,headTarget,headSource);\n\t// blr');
    next='extern "C" void pes6_editor_head(uint8_t*, uint32_t, uint32_t);\n'+next.slice(0,start)+fn+next.slice(end);
  }

  for (const [fn, hook] of [['821A7EA8','pes6_editor_decode'], ['822F3218','pes6_editor_instance'], ['82811A30','pes6_editor_copy']]) {
    const entry = `DEFINE_REX_FUNC(sub_${fn}) {\n\tREX_FUNC_PROLOGUE();`;
    if (next.includes(entry) && !next.includes(hook))
      next = `extern "C" void ${hook}(PPCContext&, uint8_t*);\n` +
        next.replace(entry, entry + `\n\t${hook}(ctx, base);`);
  }
  if (next.includes('DEFINE_REX_FUNC(sub_822F2C10)') && !next.includes('pes6_editor_groups')) {
    const start = next.indexOf('DEFINE_REX_FUNC(sub_822F2C10)');
    const end = next.indexOf('\n}\n', start);
    const functionText = next.slice(start, end);
    const anchor = '\t// addi r1,r1,96';
    if (!functionText.includes(anchor)) throw new Error('Missing per-instance clothing group anchor');
    next = 'extern "C" void pes6_editor_groups(PPCContext&, uint8_t*);\n' +
      next.slice(0,start) + functionText.replace(anchor,'\tpes6_editor_groups(ctx, base);\n'+anchor) + next.slice(end);
  }

  if (next.includes('DEFINE_REX_FUNC(sub_82115B30)') &&
      !next.includes('pes6_record_present_interval')) {
    const entry = 'DEFINE_REX_FUNC(sub_82115B30) {\n\tREX_FUNC_PROLOGUE();';
    if (!next.includes(entry)) throw new Error('Missing presentation interval setter');
    next = 'extern "C" void pes6_record_present_interval(PPCContext&);\n' +
      next.replace(entry, entry + '\n\tpes6_record_present_interval(ctx);');
  }

  // Retire the FPS experiment, including hooks already present in generated
  // files. Native wait and Swap must keep their original device state.
  next = next.replace(/^extern "C" void pes6_before_vblank_wait\([^\n]*\);\r?\n/gm, '')
    .replace(/^\tpes6_before_vblank_wait\(ctx, base\);\r?\n/gm, '');

  // Configure the game's render targets before device initialization.
  if (next.includes('DEFINE_REX_FUNC(sub_82102F28)') &&
      !next.includes('pes6_configure_render_size')) {
    const anchor = 'loc_82103364:\n';
    if (!next.includes(anchor)) throw new Error('Missing render-size hook anchor');
    next = 'extern "C" void pes6_configure_render_size(PPCContext&, uint8_t*);\n' +
      next.replace(anchor, anchor + '\tpes6_configure_render_size(ctx, base);\n');
  }

  // 1. Redirect out-of-bounds switch jumps (>= 0x827CD5D0) in sub_827CD050 to the function exit (loc_827CD5C8)
  if (next.includes('DEFINE_REX_FUNC(sub_827CD050)')) {
    next = next.replace(/goto loc_(827CD[0-9A-Fa-f]{3});/gi, (match, hex) => {
      const addr = parseInt(hex, 16);
      if (addr >= 0x827CD5D0) {
        return 'goto loc_827CD5C8;';
      }
      return match;
    });
  }

  // 2. Fix switch case 1 in sub_82178848 (French commentary initialization) where 0x821788C4 was split
  if (next.includes('DEFINE_REX_FUNC(sub_82178848)')) {
    next = next.replace(
      /case 1:\s*sub_821788C4\(ctx, base\);\s*return;/g,
      'case 1:\n\t\tctx.r10.s64 = 90;\n\t\tgoto loc_821788E0;'
    );
    // Inject independent Text/Commentary language override at entry of sub_82178848
    if (!next.includes('pes6_override_language_set')) {
      next = 'extern "C" void pes6_override_language_set(PPCContext& ctx);\n' +
        next.replace(
          'DEFINE_REX_FUNC(sub_82178848) {\n\tREX_FUNC_PROLOGUE();',
          'DEFINE_REX_FUNC(sub_82178848) {\n\tREX_FUNC_PROLOGUE();\n\tpes6_override_language_set(ctx);'
        );
    }
  }

  // 3. Inject independent Text/Commentary AFS mount override at entry of sub_8219AB10
  if (next.includes('DEFINE_REX_FUNC(sub_8219AB10)')) {
    if (!next.includes('pes6_override_afs_language')) {
      next = 'extern "C" void pes6_override_afs_language(PPCContext& ctx);\n' +
        next.replace(
          'DEFINE_REX_FUNC(sub_8219AB10) {\n\tREX_FUNC_PROLOGUE();',
          'DEFINE_REX_FUNC(sub_8219AB10) {\n\tREX_FUNC_PROLOGUE();\n\tpes6_override_afs_language(ctx);'
        );
    }
  }

  next=patchCompetitionPresentation(next);
  if (next !== content) {
    fs.writeFileSync(filePath, next, 'utf8');
    console.log(`Patched ${f}`);
    patchedCount++;
  }
}

if (patchedCount === 0) {
  console.log('Generated files already patched.');
}


