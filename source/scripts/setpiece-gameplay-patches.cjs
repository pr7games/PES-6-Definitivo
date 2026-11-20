'use strict';
function patchSetPieces(s){let changed=false;
 if(s.includes("pes6_cr7_openplay_motion")){s=s.replaceAll("pes6_cr7_openplay_motion","pes6_openplay_motion");changed=true;}
 function edit(name,transform){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+name+') {');if(a<0)return;const b=s.indexOf('\n}\n',a);if(b<0)throw Error('Setpiece function end '+name);const old=s.slice(a,b+3);if(old.includes('PES6 imported setpiece'))return;const next=transform(old);if(next===old)throw Error('Setpiece anchor '+name);s=s.slice(0,a)+next+s.slice(b+3);changed=true;}
 const start='\tREX_FUNC_PROLOGUE();';
 // Empty replay motion lookups are legal at sample boundaries. The native
 // duration query already returns zero for an empty payload, but previously
 // dereferenced the wrapper before checking that the wrapper itself exists.
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82149A88) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 null motion duration v1')){
   const next=old.replace(start,start+'\n\t// PES6 null motion duration v1\n\tif(!ctx.r3.u32){ctx.f1.f64=0.0;return;}');s=s.slice(0,a)+next+s.slice(b+3);changed=true;
 }}}
 for(const fn of ['8247F960','82140F90','82141088']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 compact replay seek bridge')){s=s.slice(0,a)+old.replace(start,start+'\n\t// PES6 compact replay seek bridge: retain tagged-style decoding during rewind.')+s.slice(b+3);changed=true;}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82140DC0) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('pes6_openplay_compact_motion')){const load='ctx.r9.u64 = REX_LOAD_U16(ctx.r30.u32 + 12);';if(!old.includes(load)||!old.includes('loc_82140E3C:'))throw Error('Compact replay rewind anchors');const next=old.replace(load,'ctx.r9.u64 = pes6_openplay_decode_id(REX_LOAD_U16(ctx.r30.u32 + 12));').replace('loc_82140E3C:','loc_82140E3C:\n\tctx.r5.u64=pes6_openplay_compact_motion(ctx,base,ctx.r31.u32,ctx.r30.u32,ctx.r5.u32);');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 for(const fn of ['82479400','8247C5D8','8247C8A0','8247FE80','8247F310']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 compact manual replay bridge')){s=s.slice(0,a)+old.replace(start,start+'\n\t// PES6 compact manual replay bridge: real training playback path confirmed by capture.')+s.slice(b+3);changed=true;}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82140748) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('pes6_openplay_record_compact')){const next=old.replace(start,start+'\n\tconst uint32_t compactController=ctx.r3.u32,compactPayload=ctx.r4.u32;').replace('\treturn;','\tpes6_openplay_record_compact(base,compactController,compactPayload);\n\treturn;');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_822700B8) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('pes6_openplay_compact_motion')){const load='ctx.r9.u64 = REX_LOAD_U16(ctx.r31.u32 + 12);';if(!old.includes(load)||!old.includes('loc_82270118:'))throw Error('Compact replay motion anchors');const next=old.replace(load,'ctx.r9.u64 = pes6_openplay_decode_id(REX_LOAD_U16(ctx.r31.u32 + 12));').replace('loc_82270118:','loc_82270118:\n\tctx.r6.u64=pes6_openplay_compact_motion(ctx,base,ctx.r30.u32,ctx.r31.u32,ctx.r6.u32);');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 for(const fn of ['8247ABF4','8247AF08']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 manual dispatcher internal block')){s=s.slice(0,a)+old.replace(start,start+'\n\t// PES6 manual dispatcher internal block: compile with its instrumented parent.')+s.slice(b+3);changed=true;}}
 for(const fn of ['8247A3B0','8247AB98']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 manual dispatcher probe')){let next=old.replace(start,start+'\n\t// PES6 manual dispatcher probe: locate the actual virtual playback target.\n\tpes6_openplay_probe(ctx,base,14,0,0);');if(fn==='8247A3B0')next=next.replaceAll('\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);','\tpes6_openplay_probe(ctx,base,13,0,0);\n\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}
 for(const [fn,stage] of [['821408C8',1],['82140A78',3],['821400E0',5]]){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 bounded manual replay probe')){let next=old.replace(start,start+'\n\t// PES6 bounded manual replay probe: read-only, single requested session.\n\tpes6_openplay_probe(ctx,base,'+stage+',ctx.r3.u32,'+(stage===5?'0':'ctx.r4.u32')+');');if(stage===1)next=next.replace('pes6_openplay_record(base,strikeController,strikePayload);','pes6_openplay_record(base,strikeController,strikePayload);\n\tpes6_openplay_probe(ctx,base,2,strikeController,strikePayload);');if(stage===3)next=next.replace('pes6_openplay_replay_pose(ctx,base,ctx.r31.u32);','pes6_openplay_replay_pose(ctx,base,ctx.r31.u32);\n\tpes6_openplay_probe(ctx,base,4,ctx.r31.u32,ctx.r30.u32);');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}
 for(const [fn,stage]of [['822714B8',7],['822715B8',8],['82479800',9],['8247C3D8',10],['824EFC98',11],['8249E020',12]]){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 bounded replay bridge probe')){s=s.slice(0,a)+old.replace(start,start+'\n\t// PES6 bounded replay bridge probe.\n\tpes6_openplay_probe(ctx,base,'+stage+',0,0);')+s.slice(b+3);changed=true;}}
 // Carry the actual private strike style through native replay serialization.
 for(const fn of ['822714B8','82479800']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 strike recording bridge')){s=s.slice(0,a)+old.replace(start,start+'\n\t// PES6 strike recording bridge: preserve assigned style in replay snapshots.')+s.slice(b+3);changed=true;}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_821408C8) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('pes6_openplay_record')){const next=old.replace(start,start+'\n\tconst uint32_t strikeController=ctx.r3.u32,strikePayload=ctx.r4.u32;').replace('\treturn;','\tpes6_openplay_record(base,strikeController,strikePayload);\n\treturn;');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82140A78) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6OpenPlayReplayScope')){let next=old.replace(start,start+'\n\tPES6OpenPlayReplayScope recordedStrike(base,ctx.r4.u32);');for(const offset of [18,20]){const line='ctx.r11.u64 = REX_LOAD_U16(ctx.r30.u32 + '+offset+');';if(!next.includes(line))throw Error('Replay ID decode anchor '+offset);next=next.replace(line,'ctx.r11.u64 = pes6_openplay_decode_id(REX_LOAD_U16(ctx.r30.u32 + '+offset+'));');}s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 for(const [name,wrapper]of [['82270390',true],['8213F4D8',false]]){
   const a=s.indexOf('DEFINE_REX_FUNC(sub_'+name+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);
   if(!old.includes('pes6_penalty_hold_finish')){const next=old.replace(start,start+'\n\t// PES6 imported setpiece: keep original full follow-through before locomotion.\n\tif(pes6_penalty_hold_finish(base,ctx.r3.u32,ctx.r4.u32,'+wrapper+'))return;');s=s.slice(0,a)+next+s.slice(b+3);changed=true;}
 }
 // Placement belongs to native spawn, never to the delayed run-up selector.
 if(s.includes('pes6_penalty_initial_placement(base,ctx.r10.u32);')){s=s.replaceAll('\tpes6_penalty_initial_placement(base,ctx.r10.u32);\n','').replaceAll('PES6 initial actor/world placement only, before the run-up begins.','PES6 placement handled by native spawn before first pose.');changed=true;}
 // Remove the discarded per-frame replay constraint compensation from existing generated code.
 if(s.includes('pes6_setpiece_replay_prepare(base,ctx.r3.u32);')){s=s.replaceAll('\tpes6_setpiece_replay_prepare(base,ctx.r3.u32);\n','').replaceAll('reconstruct constraint state for random-access replay.','observe original replay pose.');changed=true;}

 // Upgrade existing state hooks: only the bank selector may change bank identity.
 for(const fn of ['8249D9F0','8249E020','826AA568']){
   const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);
   const next=old.replace(/PES6SetPieceStyleAlias importedStyle\(base,ctx.r3.u32,(true|false)\);/,'PES6SetPieceStyleAlias importedStyle(base,ctx.r3.u32,$1,false);');
   if(next!==old){s=s.slice(0,a)+next+s.slice(b+3);changed=true;}
 }
 for(const fn of ['824AF978','824ADB78','824B4B38','8249D630'])edit(fn,f=>{f=f.replace(start,start+'\n\t// PES6 imported setpiece cache stores only legal native styles.');return f.replace(/REX_STORE_U32\((ctx\.r\d+\.u32) \+ 248, (ctx\.r\d+\.u32)\);/g,(_,player,value)=>'REX_STORE_U32('+player+' + 248, pes6_setpiece_native_style(base,'+player+','+value+'));');});
 for(const fn of ['8249E178','826AA5C8','826AA754'])edit(fn,f=>f.replace(start,start+'\n\t// PES6 imported setpiece internal block, compiled with its patched parent.'));
 for(const [fn,wrapped]of [['824977E8',true],['82497710',true],['8249D9F0',false],['8249E020',false],['826AA568',true]])edit(fn,f=>{f=f.replace(start,start+'\n\t// PES6 imported setpiece: retain native state/event route.\n\tPES6SetPieceStyleAlias importedStyle(base,ctx.r3.u32,'+wrapped+(['8249D9F0','8249E020','826AA568'].includes(fn)?',false':'')+');');if(fn==='826AA568'){const anchor='loc_826AB044:';if(!f.includes(anchor))throw Error('PK shot anchor');f=f.replace(anchor,anchor+'\n\tctx.r31.u64=pes6_setpiece_penalty_shot(base,ctx.r31.u32);');}return f;});
 edit('82149E78',f=>f.replace(start,start+'\n\t// PES6 imported setpiece bank resource.\n\tctx.r5.u64=pes6_setpiece_load_file(ctx.r4.u32,ctx.r5.u32);'));
 edit('82271930',f=>f.replace(start,start+'\n\t// PES6 imported setpiece cache identity.\n\tconst uint32_t importedBank=ctx.r3.u32;').replaceAll('\treturn;','\tctx.r3.u64=pes6_setpiece_query_file(importedBank,ctx.r3.u32);\n\treturn;'));
 for(const fn of ['825F16A8','825F2430'])edit(fn,f=>{const call='\tsub_82271930(ctx, base);';if(!f.includes(call))throw Error('Motion alias lookup');f=f.replace(call,call+'\n\t// PES6 imported setpiece stable action lookup alias.\n\tctx.r3.u64=pes6_setpiece_lookup_file(ctx.r3.u32);');if(fn==='825F2430')f=f.replace(start,start+'\n\tctx.r3.u64=pes6_setpiece_reverse_clip(base,ctx.r3.u32);');return f;});
 edit('821F2DA8',f=>{const anchor='ctx.r30.s64 = int16_t(REX_LOAD_U16(ctx.r11.u32 + 4));';if(!f.includes(anchor))throw Error('Sampler frame limit anchor');return f.replace(anchor,anchor+'\n\t// PES6 imported setpiece: sampler range uses key units, duration remains frame30.\n\tctx.r30.u64=pes6_converted_sampler_frames(base,ctx.r11.u32,ctx.r30.u32);');});
 for(const fn of ['8248D718','8248D6C0','8248D390','8248D3E0','8248D548'])
   edit(fn,f=>f.replace(start,start+'\n\t// PES6 imported setpiece: local live transition boundary for full finish.'));
 for(const fn of ['82497710','824977E8','8249D9F0','8249E020','826AA568','826AA5C8','826AA754','824AF978','824ADB78','824B4B38','8249D630','824BBD00','8249FC10','824A7D18','824AD208','824A8728','824A83F8','824AA420','824A7598','824BB9A8','82494340']){
  const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);
  const next=old.replace(/(ctx\.r\d+\.u64 = )(REX_LOAD_U32\(ctx\.r\d+\.u32 \+ 248\));/g,'$1pes6_setpiece_dispatch_style(base,$2);');if(next!==old){s=s.slice(0,a)+next+s.slice(b+3);changed=true;}
 }
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_8213F4D8) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('pes6_openplay_motion')){const anchor='\tsub_82146858(ctx, base);';if(!old.includes(anchor))throw Error('Open-play native clip initialization anchor');const next=old.replace(anchor,'\t// CR7 100 percent open-play strike trial: private motion, native events unchanged.\n\tctx.r4.u64=pes6_openplay_motion(ctx,base,ctx.r31.u32,ctx.r30.u32,ctx.r4.u32);\n'+anchor);s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 for(const fn of ['822715B8','8247C3D8']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('Assigned strike cinematic replay bridge')){s=s.slice(0,a)+old.replace(start,start+'\n\t// Assigned strike cinematic replay bridge: reach modified native restoration.')+s.slice(b+3);changed=true;}}
 // Upgrade the existing trial call and cover native replay reconstruction before blending.
 s=s.replaceAll('pes6_cr7_openplay_motion','pes6_openplay_motion');
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82140A78) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('pes6_openplay_replay_pose')){const anchor='loc_82140C44:';if(!old.includes(anchor))throw Error('Replay native blend anchor');s=s.slice(0,a)+old.replace(anchor,anchor+'\n\t// Assigned open-play strike: restore both recorded motion sides, preserving clocks.\n\tpes6_openplay_replay_pose(ctx,base,ctx.r31.u32);')+s.slice(b+3);changed=true;}}}
 for(const fn of ['824EFC98','8249E020','8249E178']){const a=s.indexOf('DEFINE_REX_FUNC(sub_'+fn+') {');if(a<0)continue;const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6OpenPlayActorScope')){s=s.slice(0,a)+old.replace(start,start+'\n\t// Assigned open-play actor identity for replay/cinematic controllers.\n\tPES6OpenPlayActorScope openPlayActor(ctx.r3.u32);')+s.slice(b+3);changed=true;}}
 {let seen=false;const line='\tctx.r4.u64=pes6_openplay_motion(ctx,base,ctx.r31.u32,ctx.r30.u32,ctx.r4.u32);';const next=s.split('\n').filter(l=>{if(l!==line)return true;if(seen)return false;seen=true;return true;}).join('\n');if(next!==s){s=next;changed=true;}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_8249E178) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3),next=old.replace('PES6OpenPlayActorScope openPlayActor(ctx.r3.u32);','PES6OpenPlayActorScope openPlayActor(ctx.r31.u32);');if(next!==old){s=s.slice(0,a)+next+s.slice(b+3);changed=true;}}}
 {const next=s.replaceAll('// Assigned strike cinematic replay bridge','// PES6 Assigned strike cinematic replay bridge');if(next!==s){s=next;changed=true;}}
 // Do not clamp a missing compact sample to a negative duration epsilon.
 // 82146858 explicitly supports a null motion and clears that sample.
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82140DC0) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 empty compact sample v1')){
   const anchor='ctx.r5.u64=pes6_openplay_compact_motion(ctx,base,ctx.r31.u32,ctx.r30.u32,ctx.r5.u32);';
   if(!old.includes(anchor))throw Error('Empty compact replay sample anchor');
   const next=old.replace(anchor,anchor+'\n\t// PES6 empty compact sample v1\n\tif(!ctx.r5.u32){ctx.f12.f64=0.0;goto loc_82140E60;}');s=s.slice(0,a)+next+s.slice(b+3);changed=true;
 }}}
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_82146858) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 replay clock wrap v1')){
   const anchor='loc_82146894:';if(!old.includes(anchor))throw Error('Replay wrapping anchor');
   const next=old.replace(anchor,anchor+'\n\t// PES6 replay clock wrap v1\n\tctx.f12.f64=pes6_replay_wrap_clock(ctx.f12.f64,ctx.f1.f64);');s=s.slice(0,a)+next+s.slice(b+3);changed=true;
 }}}
 // Replay bank discovery must compare actual clip IDs, not our recorded
 // style tags. A tagged ordinary strike otherwise looks like a special
 // 1730+ clip and starts the native special-animation bank loading path.
 {const a=s.indexOf('DEFINE_REX_FUNC(sub_8247A238) {');if(a>=0){const b=s.indexOf('\n}\n',a),old=s.slice(a,b+3);if(!old.includes('PES6 replay bank untag v1')){
   const anchor='ctx.r5.u64 = REX_LOAD_U16(ctx.r10.u32 + 0);';if(!old.includes(anchor))throw Error('Replay bank scan anchor');
   const next=old.replace(anchor,'// PES6 replay bank untag v1\n\tctx.r5.u64 = pes6_openplay_decode_id(REX_LOAD_U16(ctx.r10.u32 + 0));');s=s.slice(0,a)+next+s.slice(b+3);changed=true;
 }}}
 const beforeCleanup=s;
 s=s.replace(/^.*pes6_setpiece_capture\(.*\n/gm,'').replace(/^.*(?:captureObject=|captureA=|setpieceCaptureController=|setpieceCaptureReplay=).*\n/gm,'').replace(/^.*PES6PenaltyPlacementScope spawnPlacement.*\n/gm,'');
 s=s.replace(/^.*\/\/ PES6 (?:setpiece capture|imported setpiece: (?:capture final|independent replay|local replay call boundary|initial placement before first|observe original replay)).*\n/gm,'');
 if(s!==beforeCleanup)changed=true;
 if(changed&&!s.includes('#include "../../src/setpiece_gameplay.h"'))s='#include "../../src/setpiece_gameplay.h"\n'+s;return s;
}
module.exports=patchSetPieces;
