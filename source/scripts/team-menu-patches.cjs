function editFunction(text, address, update) {
  const start = text.indexOf('DEFINE_REX_FUNC(sub_' + address + ')');
  if (start < 0) return text;
  const end = text.indexOf('\n}\n', start);
  if (end < 0) throw Error('Missing edit-menu function end ' + address);
  return text.slice(0, start) + update(text.slice(start, end)) + text.slice(end);
}

module.exports = function patchTeamMenu(text) {
  text = editFunction(text, '82737640', body => {
    if(body.includes('pes6_pes2008_text_offset'))return body;
    const anchor='loc_82737768:\n';
    if(!body.includes(anchor))throw Error('Text resolved-position anchor changed');
    return body.replace(anchor,anchor+'\tpes6_pes2008_text_offset(base,ctx.r31.u32);\n');
  });
  text = editFunction(text, '82727528', body => {
    return body
      .replace('\tconst uint64_t pes6PerfT0 = pes6_perf_now_us();\n', '')
      .replace('\tuint64_t pes6PerfT1 = 0, pes6PerfT2 = 0, pes6PerfT3 = 0, pes6PerfRootT0 = 0;\n\tuint32_t pes6PerfRootVt = 0;\n', '')
      .replace('\tuint64_t pes6PerfT1 = 0, pes6PerfT2 = 0, pes6PerfT3 = 0;\n', '')
      .replace('\tpes6PerfT1 = pes6_perf_now_us();\n', '')
      .replace('\tpes6_perf_add_us(4, pes6PerfT1);\n\tpes6PerfT2 = pes6_perf_now_us();\n', '')
      .replace('\tpes6_perf_add_us(5, pes6PerfT2);\n\tpes6PerfT3 = pes6_perf_now_us();\n', '')
      .replace('\tpes6PerfRootT0 = pes6_perf_now_us();\n\tpes6PerfRootVt = REX_LOAD_U32(ctx.r3.u32);\n', '')
      .replace('\n\tpes6_perf_root_sample(pes6PerfRootVt, pes6PerfRootT0);', '')
      .replace('\tif (pes6PerfT3) pes6_perf_add_us(6, pes6PerfT3);\n', '')
      .replace('\tpes6_perf_add_us(0, pes6PerfT0);\n', '');
  });
  text = editFunction(text, '8272CAB0', body => body.replace('\tpes6_perf_cab0();\n', ''));
  text = editFunction(text, '8272B270', body => body.replace('\tpes6_perf_b270();\n', ''));
  text = editFunction(text, '827277B0', body => {
    return body
      .replace('\tconst uint64_t pes6PerfT0 = pes6_perf_now_us();\n', '')
      .replace('\tpes6_perf_add_us(1, pes6PerfT0);\n', '');
  });
  text = editFunction(text, '82201370', body => {
    if (body.includes('pes6_ktmdl_decal_bias')) return body;
    return body.replace(
      '\tctx.r29.u64 = REX_LOAD_U32(ctx.r30.u32 + 12);',
      '\tpes6_ktmdl_decal_bias(base, ctx.r30.u32);\n\tctx.r29.u64 = REX_LOAD_U32(ctx.r30.u32 + 12);'
    );
  });
  if (text.includes('pes6_ktmdl_decal_bias(') && !text.includes('extern "C" void pes6_ktmdl_decal_bias'))
    text = 'extern "C" void pes6_ktmdl_decal_bias(uint8_t*, uint32_t);\n' + text;
  for (const decl of [
    'extern "C" uint64_t pes6_perf_now_us();\n',
    'extern "C" void pes6_perf_add_us(unsigned, uint64_t);\n',
    'extern "C" void pes6_perf_root_sample(uint32_t, uint64_t);\n',
    'extern "C" void pes6_perf_child_sample(uint32_t, uint64_t);\n',
    'extern "C" void pes6_perf_cab0();\n',
    'extern "C" void pes6_perf_b270();\n'
  ]) {
    text = text.replace(decl, '');
  }
  text=editFunction(text,'82738BF0',body=>{
    if(body.includes('pes6_edit_effects_collected'))return body;
    const anchor='ctx.lr = 0x82738C54;\n\tsub_82231B28(ctx, base);';
    if(!body.includes(anchor))throw Error('Native UI disposal anchor changed');
    return body.replace(anchor,anchor+'\n\tpes6_edit_effects_collected(ctx,base);');
  });
  if(text.includes('pes6_edit_effects_collected(')&&!text.includes('extern "C" void pes6_edit_effects_collected'))
    text='extern "C" void pes6_edit_effects_collected(PPCContext&,uint8_t*);\n'+text;
  text=editFunction(text,'8218F830',body=>{
    if(body.includes('pes6_menu_rgba_pixels'))return body;
    const anchor='\t// li r8,0\n\tctx.r8.s64 = 0;\nloc_8218F8C0:';
    if(!body.includes(anchor))throw Error('WE32 portrait pixel loop changed');
    return body.replace(anchor,'\tif(pes6_menu_rgba_pixels(base,ctx.r31.u32,ctx.r30.u32))goto loc_8218FACC;\n'+anchor);
  });
  if(text.includes('pes6_menu_rgba_pixels(')&&!text.includes('extern "C" bool pes6_menu_rgba_pixels'))
    text='extern "C" bool pes6_menu_rgba_pixels(uint8_t*,uint32_t,uint32_t);\n'+text;
  text=text.replace('extern "C" void pes6_edit_effect_geometry(uint8_t*,uint32_t);','extern "C" void pes6_edit_effect_geometry(PPCContext&,uint8_t*,uint32_t);')
    .replace('pes6_edit_effect_geometry(base, ctx.r3.u32);','pes6_edit_effect_geometry(ctx, base, ctx.r3.u32);');
  text = editFunction(text, '8272B270', body => {
    if(body.includes('pes6_edit_effect_geometry'))return body;
    return body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_edit_effect_geometry(ctx, base, ctx.r3.u32);');
  });
  text = editFunction(text, '827384D8', body => {
    if(body.includes('pes6_team_comparison_label'))return body;
    return body.replace('loc_8273859C:\n','loc_8273859C:\n\tif(pes6_team_comparison_label(base,ctx.r31.u32))goto loc_827385DC;\n');
  });
  text = editFunction(text, '827384D8', body => {
    if(body.includes('pes6_formation_text_cell'))return body;
    return body.replace('REX_FUNC_PROLOGUE();','REX_FUNC_PROLOGUE();\n\tpes6_formation_text_cell(base,ctx.r3.u32);');
  });
  if(text.includes('pes6_formation_text_cell(')&&!text.includes('extern "C" void pes6_formation_text_cell'))
    text='extern "C" void pes6_formation_text_cell(uint8_t*,uint32_t);\n'+text;
  text = editFunction(text, '822E7888', body => {
    if (body.includes('pes6_edit_menu_portrait')) return body;
    return body.replace('loc_822E796C:\n', 'loc_822E796C:\n\tpes6_edit_menu_portrait(ctx, base, ctx.r31.u32);\n');
  });
  text = editFunction(text, '822E7540', body => {
    if (body.includes('pes6_edit_menu_artwork_leave')) return body;
    return body.replace('REX_FUNC_PROLOGUE();', 'REX_FUNC_PROLOGUE();\n\tpes6_edit_menu_artwork_leave(ctx, base);');
  });
  text = editFunction(text, '822E7790', body => {
    if (body.includes('pes6_team_edit_menu')) return body
      .replace('ctx.r4.s64 = 3;\n\t// lwz r3,168(r31)', 'ctx.r4.s64 = 4;\n\t// lwz r3,168(r31)')
      .replace('ctx.r4.s64 = 2;\n\t// bl 0x82730f28', 'ctx.r4.s64 = 3;\n\t// bl 0x82730f28');
    const call = 'ctx.lr = 0x822E77D4;\n\tsub_827317F8(ctx, base);';
    const load = 'ctx.r4.s64 = 1;\n\t// bl 0x82730f28\n\tctx.lr = 0x822E77F0;';
    const save = 'ctx.r4.s64 = 2;\n\t// lwz r3,168(r31)';
    if (![call, load, save].every(anchor => body.includes(anchor))) {
      throw Error('Edit-menu construction anchors changed');
    }
    return body.replace(call, 'ctx.lr = 0x822E77D4;\n\tpes6_team_edit_menu(ctx, base);')
      .replace(save, 'ctx.r4.s64 = 4;\n\t// lwz r3,168(r31)')
      .replace(load, 'ctx.r4.s64 = 3;\n\t// bl 0x82730f28\n\tctx.lr = 0x822E77F0;');
  });
  text = editFunction(text, '822E7888', body => {
    if (body.includes('pes6_team_edit_choice')) return body;
    const call = 'ctx.lr = 0x822E7940;\n\tsub_82730FC0(ctx, base);';
    if (!body.includes(call)) throw Error('Edit-menu selection anchor changed');
    return body.replace(call, call + '\n\tpes6_team_edit_choice(ctx);');
  });
  text = editFunction(text, '822E7DF8', body => {
    if (body.includes('pes6_team_selector_header')) return body;
    const exit = '\t// addi r1,r1,96';
    if (!body.includes(exit)) throw Error('Team-selector header anchor changed');
    return body.replace(exit, '\tpes6_team_selector_header(ctx, base);\n' + exit);
  });
  text = editFunction(text, '822E9F08', body => {
    if(body.includes('pes6_native_team_selected'))return body;
    const confirmed='REX_STORE_U32(ctx.r31.u32 + 208, ctx.r11.u32);\n\t// beq cr6,0x822ea424\n\tif (ctx.cr6.eq) goto loc_822EA424;';
    if(!body.includes(confirmed))throw Error('Confirmed team selection anchor changed');
    return body.replace(confirmed,confirmed+'\n\tif(pes6_native_team_selected(ctx,base,ctx.r31.u32))goto loc_822EA424;');
  });
  text = editFunction(text, '822E9F08', body => {
    if(body.includes('pes6_transfer_selector_present'))return body;
    const exit='\t// addi r1,r1,224';
    if(!body.includes(exit))throw Error('Transfer selector presentation exit changed');
    return body.replace(exit,'\tpes6_transfer_selector_present(ctx,base,ctx.r31.u32);\n'+exit);
  });
  text = editFunction(text, '82727E70', body => {
    if (body.includes('pes6_init_pes2008_font')) return body;
    const anchor = 'ctx.lr = 0x82727F14;\n\tsub_82739850(ctx, base);';
    if (!body.includes(anchor)) throw Error('PES2008 font init anchor changed');
    return body.replace(anchor, anchor + '\n\tpes6_init_pes2008_font(ctx, base);');
  });
  text = editFunction(text, '827398E0', body => {
    if (body.includes('pes6_font_override')) return body;
    const anchor = 'ctx.lr = 0x8273990C;\n\tREX_CALL_INDIRECT_FUNC(ctx.ctr.u32);';
    if (!body.includes(anchor)) throw Error('PES2008 font get anchor changed');
    return body.replace(anchor, anchor + '\n\tctx.r3.u64 = pes6_font_override(base, ctx.r31.u32, ctx.r3.u32);');
  });
  for (const [hook, signature, result='void'] of [
    ['pes6_pes2008_text_offset', '(uint8_t*,uint32_t)'],
    ['pes6_init_pes2008_font', '(PPCContext&,uint8_t*)'],
    ['pes6_font_override', '(uint8_t*,uint32_t,uint32_t)', 'uint32_t'],
    ['pes6_edit_effect_geometry', '(PPCContext&,uint8_t*,uint32_t)'],
    ['pes6_team_comparison_label', '(uint8_t*,uint32_t)', 'bool'],
    ['pes6_edit_menu_portrait', '(PPCContext&,uint8_t*,uint32_t)'],
    ['pes6_edit_menu_artwork_leave', '(PPCContext&,uint8_t*)'],
    ['pes6_team_edit_menu', '(PPCContext&,uint8_t*)'],
    ['pes6_team_edit_choice', '(PPCContext&)'],
    ['pes6_team_selector_header', '(PPCContext&,uint8_t*)'],
    ['pes6_native_team_selected', '(PPCContext&,uint8_t*,uint32_t)', 'bool'],
    ['pes6_transfer_selector_present', '(PPCContext&,uint8_t*,uint32_t)']
  ]) {
    if (text.includes(hook + '(') && !text.includes('extern "C" ' + result + ' ' + hook)) {
      text = 'extern "C" ' + result + ' ' + hook + signature + ';\n' + text;
    }
  }
  return text;
};
