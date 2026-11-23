#include <rex/ui/window.h>
#include <rex/ui/keybinds.h>
#include <rex/cvar.h>
#include "../src/frame_meter.h"
#include "../src/neural_rendering_status.h"
#include "../src/overlay_ui_tools.h"
#include <imgui.h>
#include <xinput.h>
#include <windows.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include "generated/overlay.h"

namespace {
void CloseGame(){
 EnumWindows([](HWND hwnd,LPARAM)->BOOL{DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);if(pid==GetCurrentProcessId()&&IsWindowVisible(hwnd)&&GetWindow(hwnd,GW_OWNER)==nullptr){PostMessageW(hwnd,WM_CLOSE,0,0);return FALSE;}return TRUE;},0);
}
}
extern "C" __declspec(dllexport) pes6::ui_tools::KeyboardState* Pes6GetKeyboardState() {
 return &pes6::ui_tools::g_keyboard;
}
extern "C" __declspec(dllexport) pes6::ui_tools::ColorPickerState* Pes6GetColorPickerState() {
 return &pes6::ui_tools::g_color_picker;
}

void Pr7OverlayPaint(){
 if(!ImGui::GetCurrentContext())return;
 // ReXApp registers its default shortcuts before the first UI frame.
 // Leave F7 available to the external Neural Rendering supervisor.
 static const bool shortcuts_ready=[](){rex::ui::UnregisterBind("bind_achievements");return true;}();
 (void)shortcuts_ready;
 static NativePcOverlay overlay([]{const auto* value=std::getenv("PES6_SHOW_FPS");return value&&value[0]=='1';}(),CloseGame);
 overlay.Paint();
 if (pes6::ui_tools::g_color_picker.active.load(std::memory_order_acquire)) {
  auto& cp = pes6::ui_tools::g_color_picker;
  std::lock_guard<std::mutex> lock(cp.mutex);
  const ImVec2 ds = ImGui::GetIO().DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(ds.x * 0.65f, ds.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(320.f, 380.f), ImGuiCond_Always);
  if (ImGui::Begin(cp.label.empty() ? "Selector de Color" : cp.label.c_str(), nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)) {
   ImGui::ColorPicker4("##picker", cp.rgba, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex);
   if (ImGui::Button("Aplicar", ImVec2(140.f, 32.f))) { cp.accepted = true; cp.active.store(false); cp.finished.store(true); }
   ImGui::SameLine();
   if (ImGui::Button("Cancelar", ImVec2(140.f, 32.f))) { cp.accepted = false; cp.active.store(false); cp.finished.store(true); }
  }
  ImGui::End();
 }
 const auto frames=native_overlay_frames.load(std::memory_order_relaxed);
 if(frames==1||frames%120==0){
  static const auto report=[](){wchar_t image[MAX_PATH]{};GetModuleFileNameW(nullptr,image,MAX_PATH);auto root=std::filesystem::path(image).parent_path();for(int i=0;i<6&&!std::filesystem::exists(root/"game/cv.img");i++)root=root.parent_path();return root/"data/native-renderer/extension-overlay-status.txt";}();
  std::ofstream out(report,std::ios::trunc);out<<"imgui_frames="<<frames<<"\nversion="<<ImGui::GetVersion()<<"\nachievements_shortcut=disabled\n";
 }
}
