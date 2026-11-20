'use strict';
function patchSubstitutionNavigation(source){
  const entry='DEFINE_REX_FUNC(sub_8225A928) {',start=source.indexOf(entry);
  if(start<0)return source;
  const end=source.indexOf('\n}\n',start);
  let body=source.slice(start,end);
  if(body.includes('PES6 substitution horizontal jump'))return source;
  const call='sub_8224E170(ctx, base);';
  if(body.split(call).length!==3)throw Error('Substitution input call sites changed');
  body=body.replaceAll(call,'// PES6 substitution horizontal jump\n\tif(pes6_substitution_jump(ctx,base,ctx.r31.u32))ctx.r3.u64=3;else '+call);
  const patched=source.slice(0,start)+body+source.slice(end);
  const include=/#include "pes6_funcs(?:\.\d+)?\.h"/;
  if(!include.test(patched))throw Error('Generated functions header missing');
  return patched.replace(include,'$&\n#include "../../src/substitution_navigation.h"');
}
module.exports={patchSubstitutionNavigation};
