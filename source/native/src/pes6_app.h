// pes6 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/rex_app.h>
#include <rex/memory.h>
#include <rex/system/gpu_plugin.h>
#include <rex/logging.h>
#include <rex/ui/window.h>
#include <rex/ui/ui_event.h>
#include <rex/ui/keybinds.h>
#include <rex/cvar.h>
#include <rex/runtime.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/ui/presenter.h>
#include <fstream>
#include <filesystem>
#include "pc_overlay.h"
#include "player_editor_runtime.h"
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

class Pes6App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Pes6App>(new Pes6App(ctx, "PES 6 DEFINITIVO by Golazo Virtual",
        PPCImageConfig));
  }

  void OnConfigureFonts(ImFontAtlas* atlas) override {
    wchar_t windows[MAX_PATH]{};
    GetWindowsDirectoryW(windows, MAX_PATH);
    auto path = std::filesystem::path(windows) / "Fonts" / "segoeui.ttf";
    if (std::filesystem::exists(path)) {
      pes6_pc_menu_font = atlas->AddFontFromFileTTF(path.string().c_str(), 18.0f);
    }
    if (!pes6_pc_menu_font) {
      ImFontConfig cfg;
      cfg.SizePixels = 18.0f;
      pes6_pc_menu_font = atlas->AddFontDefault(&cfg);
    }
  }

  void OnPreSetup(rex::RuntimeConfig& config) override {
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr,executable,MAX_PATH);
    editor_root_=std::filesystem::path(executable).parent_path();
    for(int i=0;i<6&&!std::filesystem::exists(editor_root_/"game/cv.img");i++)
      editor_root_=editor_root_.parent_path();
    pes6::editor::Initialize(editor_root_.string());
    config.gpu_plugin = "xenos";
    const char* api = std::getenv("PES6_GRAPHICS_API");
    // Default to Vulkan as requested by user. D3D12 is selectable manually.
    // If one fails, do NOT call fallback - exit and let user switch manually.
    const std::string_view backend = (api && (_stricmp(api, "d3d12") == 0 || _stricmp(api, "dx12") == 0)) ? "d3d12" : "vulkan";
    config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
    if (!config.graphics) {
      REXLOG_ERROR("PES6: Failed to load requested graphics API: {}", backend);
      std::exit(EXIT_FAILURE);
    }
    REXLOG_INFO("PES6 graphics API selected: {}", backend);
    rex::cvar::ApplyEnvironment();
    const auto set_default_cvar = [](std::string_view name, std::string_view value) {
      if (rex::cvar::GetFlagInfo(name) &&
          rex::cvar::GetFlagSource(name) == rex::cvar::Source::kDefault) {
        rex::cvar::SetFlagByName(name, value);
      }
    };
    set_default_cvar("gpu_debug_markers", "false");
    set_default_cvar("clear_memory_page_state", "false");
    set_default_cvar("scribble_heap", "false");
    set_default_cvar("protect_on_release", "false");
    set_default_cvar("async_shader_compilation", "true");
    set_default_cvar("vulkan_pipeline_creation_threads", "-1");
    set_default_cvar("d3d12_pipeline_creation_threads", "-1");
  }

  void OnPostSetup() override {
    pes6_start_timing();
    pes6_render_scale.store(REXCVAR_QUERY(int32_t, draw_resolution_scale_x));
    window()->SetTitle("PES 6 DEFINITIVO by Golazo Virtual");
#if defined(_WIN32)
    if (HWND hwnd = static_cast<HWND>(window()->GetNativeWindowHandle())) {
      HICON icon_big = (HICON)LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
          IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
      if (!icon_big) {
        icon_big = (HICON)LoadImageW(nullptr, L"launcher-assets\\icon.ico",
            IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
      }
      if (!icon_big) {
        icon_big = (HICON)LoadImageW(nullptr, L"..\\..\\launcher-assets\\icon.ico",
            IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
      }
      if (!icon_big) {
        icon_big = (HICON)LoadImageW(nullptr, L"..\\..\\..\\launcher-assets\\icon.ico",
            IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
      }
      if (icon_big) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(icon_big));
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(icon_big));
      }
      SetWindowTextW(hwnd, L"PES 6 DEFINITIVO by Golazo Virtual");
    }
#endif
    REXLOG_INFO("PES6 presentation: effect={}, scale={}x{}, vsync={}",
        REXCVAR_QUERY(std::string, present_effect),
        REXCVAR_QUERY(int32_t, draw_resolution_scale_x),
        REXCVAR_QUERY(int32_t, draw_resolution_scale_y),
        REXCVAR_QUERY(bool, vsync));
  }

  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (e.virtual_key() == rex::ui::VirtualKey::kF10 && e.prev_state()) {
      e.set_handled(true);
      return;
    }
    if (e.virtual_key() == rex::ui::VirtualKey::kReturn && e.is_alt_pressed()) {
      if (!e.prev_state()) {
        const bool fullscreen = !window()->IsFullscreen();
        rex::cvar::SetFlagByName("fullscreen", fullscreen ? "true" : "false");
        window()->SetFullscreen(fullscreen);
        pes6_pacing_generation.fetch_add(1, std::memory_order_relaxed);
        REXLOG_INFO("PES6 Alt+Enter: fullscreen={}", window()->IsFullscreen());
      }
      e.set_handled(true);
      return;
    }
    rex::ui::ProcessKeyEvent(e);
  }

  void OnWindowPixelSizeChanged(uint32_t width, uint32_t height) override {
    pes6_pacing_generation.fetch_add(1, std::memory_order_relaxed);
    REXLOG_INFO("PES6 window: {}x{}; render buffer unchanged", width, height);
  }

  void OnWindowFocusChanged(bool focused) override {
    pes6_pacing_generation.fetch_add(1, std::memory_order_relaxed);
    if (focused) RecoverPresentation();
  }

  void OnWindowRestored() override {
    pes6_pacing_generation.fetch_add(1, std::memory_order_relaxed);
    RecoverPresentation();
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    pes6::editor::open.store(false);
    pes6::editor::SetOpenCallback({});
    rex::ui::UnregisterBind("bind_pes6_player_editor");
    const char* show = std::getenv("PES6_SHOW_FPS");
    pc_overlay_ = std::make_unique<PcOverlay>(drawer, show && std::string_view(show) == "1", window(),
        [this] {
          app_context().CallInUIThreadDeferred([this] { window()->RequestClose(); });
        });
    rex::ui::RegisterBind("bind_pes6_pc_menu", "F10", "Open PES 6 PC menu",
        [this] { if (pc_overlay_) pc_overlay_->ToggleMenu(); });
    rex::ui::UnregisterBind("bind_debug_overlay");
    rex::ui::UnregisterBind("bind_console");
    rex::ui::UnregisterBind("bind_settings");
    rex::ui::UnregisterBind("bind_achievements");
  }

  void OnShutdown() override {
    pes6::editor::SetOpenCallback({});
    rex::ui::UnregisterBind("bind_pes6_player_editor");
    pes6_stop_timing();
    rex::ui::UnregisterBind("bind_pes6_pc_menu");
    pc_overlay_.reset();
  }

 private:
  void RecoverPresentation() {
    // Focus/restore may leave the presenter waiting for a paint event that was
    // coalesced while covered or minimized. Resume its connection on the UI
    // thread, just as a resize does, and force one paint through the coalescer.
    // Never resize the actual window or alter the guest render resolution.
    app_context().CallInUIThreadDeferred([this] {
      if (!runtime() || !runtime()->graphics_system() || !window()) return;
      auto* presenter = runtime()->graphics_system()->presenter();
      if (!presenter || !window()->GetActualPhysicalWidth() ||
          !window()->GetActualPhysicalHeight()) return;
      presenter->OnSurfaceResizeFromUIThread();
      presenter->PaintFromUIThread(true);
      window()->RequestPaint();
      REXLOG_WARN("PES6 presentation resumed after focus/restore");
    });
  }
  std::filesystem::path editor_root_;
  std::unique_ptr<PcOverlay> pc_overlay_;
};
