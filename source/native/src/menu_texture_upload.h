#pragma once
extern "C" void pes6_menu_rgba_scope(bool);
// Only the explicit custom portrait loaders use the fast RGBA converter.
struct Pes6MenuRgbaScope {
  Pes6MenuRgbaScope(){pes6_menu_rgba_scope(true);}
  ~Pes6MenuRgbaScope(){pes6_menu_rgba_scope(false);}
  Pes6MenuRgbaScope(const Pes6MenuRgbaScope&)=delete;
  Pes6MenuRgbaScope& operator=(const Pes6MenuRgbaScope&)=delete;
};
