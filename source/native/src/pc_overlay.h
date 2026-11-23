#pragma once
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/window.h>
#include <rex/cvar.h>
#include "frame_meter.h"
#include "neural_rendering_status.h"
#include <imgui.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <algorithm>
#include <vector>
#include <windows.h>
#include <cstdlib>

// Shared Segoe UI face for the PC Menu.
inline ImFont* pes6_pc_menu_font = nullptr;

// Guest presentation requests are distinct from repeated host UI paints.
class PcOverlay final : public rex::ui::ImGuiDialog {
 public:
  PcOverlay(rex::ui::ImGuiDrawer* drawer, bool show_fps, rex::ui::Window* window,
            std::function<void()> exit)
      : ImGuiDialog(drawer), show_fps_(show_fps), window_(window), exit_(std::move(exit)) {
    SyncAttachment();
  }
  // Called on the UI thread. An invisible PC menu must not force UI-thread
  // presentation, which adds a scheduling hop and prevents direct guest paints.
  void ToggleMenu() { menu_.store(!menu_.load()); SyncAttachment(); }

 private:
  using Clock = std::chrono::steady_clock;
  void SyncAttachment() {
    const bool needed = show_fps_ || menu_.load();
    if (needed == attached_) return;
    if (needed) imgui_drawer()->AddDialog(this);
    else imgui_drawer()->RemoveDialog(this);
    attached_ = needed;
  }
  void OnDraw(ImGuiIO&) override {
    const auto now = Clock::now();
    if ((show_fps_ || menu_.load()) && now - last_request_ >= std::chrono::milliseconds(500)) {
      const uint64_t count = pes6_frame_submissions.load(std::memory_order_relaxed);
      using GpuCount = uint64_t (*)();
      static const GpuCount gpu_count = []() -> GpuCount {
        if (auto module = GetModuleHandleW(L"rexgpu-xenos.dll")) {
          if (auto proc = GetProcAddress(module, "Fifa08GpuSubmittedImages"))
            return reinterpret_cast<GpuCount>(proc);
        }
        if (auto module = GetModuleHandleW(L"rexgpu-xenosrd.dll")) {
          if (auto proc = GetProcAddress(module, "Fifa08GpuSubmittedImages"))
            return reinterpret_cast<GpuCount>(proc);
        }
        return nullptr;
      }();
      const uint64_t gpu_images = gpu_count ? gpu_count() : 0;
      gpu_available_ = gpu_count != nullptr;
      if (initialized_) {
        fps_ = double(count - previous_count_) / std::chrono::duration<double>(now - last_request_).count();
        gpu_fps_ = double(gpu_images - previous_gpu_count_) / std::chrono::duration<double>(now - last_request_).count();
        ready_ = true;

      }
      initialized_ = true;
      previous_count_ = count;
      previous_gpu_count_ = gpu_images;
      last_request_ = now;
    }
    if (show_fps_) {
      ImGui::SetNextWindowPos(ImVec2(14, 14), ImGuiCond_Always);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, 0.85f));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));

      constexpr auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing;
      if (ImGui::Begin("FPS##pes6_pc", nullptr, flags)) {
        // Neural Rendering is reserved for a future release. Report game FPS.
        if (ready_) {
          ImGui::Text("FPS: %.1f", gpu_available_ ? gpu_fps_ : fps_);
        } else {
          ImGui::TextUnformatted("FPS: --");
        }
        static const char* api = []() {
          const char* v = std::getenv("PES6_GRAPHICS_API");
          return (v && *v) ? v : "Vulkan";
        }();
        ImGui::Text("API: %s", api);
        const auto scale = std::max(1u, pes6_render_scale.load(std::memory_order_relaxed));
        const auto width = pes6_frontbuffer_width.load(std::memory_order_relaxed);
        const auto height = pes6_frontbuffer_height.load(std::memory_order_relaxed);
        static const uint32_t fsr_input_w = []() -> uint32_t {
          const char* v = std::getenv("FIFA08_FSR_INPUT_WIDTH");
          const int parsed = v ? std::atoi(v) : 0;
          return parsed > 0 ? uint32_t(parsed) : 0u;
        }();
        static const uint32_t fsr_input_h = []() -> uint32_t {
          const char* v = std::getenv("FIFA08_FSR_INPUT_HEIGHT");
          const int parsed = v ? std::atoi(v) : 0;
          return parsed > 0 ? uint32_t(parsed) : 0u;
        }();
        const uint32_t buffer_w = fsr_input_w ? fsr_input_w : width * scale;
        const uint32_t buffer_h = fsr_input_h ? fsr_input_h : height * scale;
        ImGui::Text("BUFFER: %u x %u", buffer_w, buffer_h);
        static const char* selected_resolution = std::getenv("PES6_SELECTED_RESOLUTION");
        ImGui::Text("RESOLUCI\xC3\x93N: %s", selected_resolution ? selected_resolution : "--");
        int win_w = 0;
        int win_h = 0;
        if (window_) {
          win_w = int(window_->GetActualPhysicalWidth());
          win_h = int(window_->GetActualPhysicalHeight());
        }
        if (!win_w || !win_h) {
          win_w = REXCVAR_QUERY(int32_t, window_width);
          win_h = REXCVAR_QUERY(int32_t, window_height);
        }
        ImGui::Text("VENTANA: %d x %d", win_w, win_h);
        ImGui::TextUnformatted("F10 para abrir Menu");
      }
      ImGui::End();
      ImGui::PopStyleColor(3);
      ImGui::PopStyleVar(3);
    }
    if (menu_.load()) {
      const auto* viewport = ImGui::GetMainViewport();
      ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
      ImGui::SetNextWindowSize(ImVec2(std::min(380.0f, viewport->WorkSize.x - 24.0f), 0.0f), ImGuiCond_Always);

      if (pes6_pc_menu_font) ImGui::PushFont(pes6_pc_menu_font);

      // Rounded PC menu, matching the PES 6 launcher typography & yellow accents.
      ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 18.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.2f);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.0f, 18.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 12.0f));
      ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 10.0f));

      ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.10f, 0.96f));
      ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.96f, 0.77f, 0.0f, 0.55f));
      ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.11f, 0.11f, 0.14f, 0.98f));
      ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.98f, 0.98f, 0.99f, 1.0f));

      // Checkbox style (PES Yellow CheckMark)
      ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.17f, 0.21f, 0.90f));
      ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.23f, 0.28f, 0.95f));
      ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.28f, 0.29f, 0.35f, 1.0f));
      ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.96f, 0.77f, 0.0f, 1.0f));

      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.96f, 0.77f, 0.0f, 0.85f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.78f, 0.62f, 0.0f, 1.0f));
      bool menu_open = true;
      ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.35f, 0.35f, 0.38f, 1.0f));
      if (ImGui::Begin("PES 6 - Menu PC", &menu_open,
                       ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse)) {
        ImGui::SetWindowFontScale(1.0f);

        ImGui::Checkbox("Mostrar FPS en pantalla", &show_fps_);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.70f, 0.73f, 0.78f, 1.0f), "Este menu no pausa el partido.");
        ImGui::Spacing();

        // Button: "Salir a Windows" (PES Yellow Accent with dark text)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.96f, 0.77f, 0.0f, 0.95f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 0.84f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.76f, 0.60f, 0.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.07f, 0.07f, 0.08f, 1.0f));
        if (ImGui::Button("Salir a Windows", ImVec2(-1.0f, 42.0f))) {
          menu_.store(false);
          exit_();
        }
        ImGui::PopStyleColor(4);

        ImGui::SetWindowFontScale(1.0f);
      }
      ImGui::End();

      if (pes6_pc_menu_font) ImGui::PopFont();
      if (!menu_open) menu_.store(false);
      ImGui::PopStyleColor(12);
      ImGui::PopStyleVar(6);
    }
    SyncAttachment();
  }
  rex::ui::Window* window_ = nullptr;
  bool attached_ = true;
  bool show_fps_;
  std::atomic<bool> menu_{false};
  std::function<void()> exit_;
  bool initialized_ = false;
  bool ready_ = false;
  uint64_t previous_count_ = 0;
  uint64_t previous_gpu_count_ = 0;
  bool gpu_available_ = false;
  double gpu_fps_ = 0.0;
  double fps_ = 0.0;
  Clock::time_point last_request_{};
};

