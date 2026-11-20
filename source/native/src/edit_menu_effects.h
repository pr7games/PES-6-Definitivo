#pragma once
extern "C" void pes6_edit_effects(PPCContext&,uint8_t*,uint32_t foreground,uint32_t screen);
extern "C" void pes6_edit_effects_close(PPCContext&,uint8_t*);
extern "C" void pes6_edit_text_foreground(uint8_t*,uint32_t);

extern "C" void pes6_edit_foreground_tree(uint8_t*,uint32_t);
