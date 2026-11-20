// Restore the two native board-impact cues without bypassing other sound filters.
function patchAdboardFilter(source) {
  const entry = 'DEFINE_REX_FUNC(sub_82180F30) {\n';
  const start = source.indexOf(entry);
  if (start < 0) return source;
  const end = source.indexOf('\n}\n', start);
  if (end < 0) throw Error('Adboard sound filter function end missing');
  let fn = source.slice(start, end);
  if (fn.includes('// PES6 enable native adboard impact sounds')) return source;
  const anchor = '\t// ori r11,r11,16\n\tctx.r11.u64 = ctx.r11.u64 | 16;\n\t// cmplw cr6,r27,r11\n\tctx.cr6.compare<uint32_t>(ctx.r27.u32, ctx.r11.u32, ctx.xer);\n\t// ble cr6,0x82180f48\n\tif (!ctx.cr6.gt) goto loc_82180F48;';
  if (fn.split(anchor).length !== 2) throw Error('Adboard sound rejection anchor changed');
  fn = fn.replace(anchor, anchor.replace('if (!ctx.cr6.gt) goto loc_82180F48;', '// PES6 enable native adboard impact sounds (0x8C00000F/10 only)\n\tif (!ctx.cr6.gt) goto loc_82181B98;'));
  return source.slice(0, start) + fn + source.slice(end);
}
function patchAdboardSound(source){
 source=patchAdboardFilter(source);
 // Restore native playback for sources already patched with the V26 gain.
 source=source.replace(/\n\t\/\/ PES6 adboard native gain v26\r?\n\tconst bool pes6AdboardImpact=ctx\.r3\.u32==0x8c00000fu\|\|ctx\.r3\.u32==0x8c000010u;/g,'');
 source=source.replaceAll('if(pes6AdboardImpact){ctx.f1.f64=3.0;sub_82188FD8(ctx,base);}else sub_82188D68(ctx, base);','sub_82188D68(ctx, base);');
 const start=source.indexOf('DEFINE_REX_FUNC(sub_82188408) {');if(start<0)return source;
 const end=source.indexOf('\n}\n',start);let body=source.slice(start,end);
 if(body.includes('PES6 confirmed adboard gain x6'))return source;
 body=body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\t// PES6 confirmed adboard gain x6: only SE_KANBAN_01, auditioned by user\n\tconst bool pes6ConfirmedAdboard=ctx.r3.u32==0x8c00000fu;');
 const call='sub_82188D68(ctx, base);';if(body.split(call).length!==2)throw Error('Native adboard playback anchor changed');
 body=body.replace(call,'if(pes6ConfirmedAdboard){ctx.f1.f64=6.0;sub_82188FD8(ctx,base);}else sub_82188D68(ctx, base);');
 return source.slice(0,start)+body+source.slice(end);
}
module.exports = {patchAdboardSound};
