#include "player_editor.h"
#include <algorithm>
#include <cmath>

namespace pes6::editor {
namespace {
constexpr ImVec4 yellow{1.0f, 0.76f, 0.08f, 1.0f};
constexpr ImVec4 white{0.92f, 0.92f, 0.89f, 0.98f};
constexpr ImVec4 ink{0.08f, 0.08f, 0.07f, 1.0f};

bool MenuButton(const char* label, bool selected, ImVec2 size) {
  ImGui::PushStyleColor(ImGuiCol_Button, selected ? yellow : white);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, yellow);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.62f, 0.03f, 1));
  const bool clicked = ImGui::Button(label, size);
  ImGui::PopStyleColor(3);
  return clicked;
}
}  // namespace

void PlayerEditorView::Reset() {
  page_ = Page::Sections;
  bound_ = false;
}

std::vector<Request> PlayerEditorView::Draw(const Snapshot& s) {
  std::vector<Request> requests;
  if (!bound_ || player_id_ != s.player_id || generation_ != s.generation) {
    page_ = Page::Sections;
    player_id_ = s.player_id;
    generation_ = s.generation;
    bound_ = true;
  }
  auto send = [&](Action action, std::string field = {}, int value = 0) {
    if (requests.empty()) requests.push_back({action, s.player_id, s.generation,
                                            std::move(field), value});
  };
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const float scale = std::max(0.5f, std::min(viewport->WorkSize.x / 1280.0f,
                                             viewport->WorkSize.y / 720.0f));
  const ImVec2 size{1160 * scale, 620 * scale};
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + (viewport->WorkSize.x-size.x)/2,
                               viewport->WorkPos.y + (viewport->WorkSize.y-size.y)/2));
  ImGui::SetNextWindowSize(size);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24*scale, 20*scale));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7*scale);
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12*scale, 10*scale));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(14*scale, 12*scale));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.34f, 0.25f, 0.12f, 0.97f));
  ImGui::PushStyleColor(ImGuiCol_Text, ink);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, white);
  ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, yellow);
  ImGui::PushStyleColor(ImGuiCol_FrameBgActive, yellow);
  ImGui::PushStyleColor(ImGuiCol_Header, yellow);
  ImGui::PushStyleColor(ImGuiCol_HeaderHovered, yellow);
  ImGui::PushStyleColor(ImGuiCol_PopupBg, white);
  ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.2f, 0.18f, 0.12f, 1));
  if (ImGui::Begin("##pes6_player_editor", nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoSavedSettings)) {
    ImGui::SetWindowFontScale(scale);
    ImGui::TextColored(white, "Editar jugador");
    ImGui::TextColored(white, "%s  |  %s", s.team_name.c_str(), s.player_name.c_str());
    ImGui::Separator();
    const float panel_height = ImGui::GetContentRegionAvail().y - 60*scale;
    if (ImGui::BeginChild("##sections", ImVec2(650*scale, panel_height))) {
      ImGui::BeginDisabled(s.pending);
      if (page_ == Page::Sections) {
        ImGui::BeginDisabled(!s.information_available);
        if (MenuButton("Informaci\xc3\xb3n", false, ImVec2(-1, 52*scale)))
          send(Action::Information);
        ImGui::EndDisabled();
        if (MenuButton("Botas", false, ImVec2(-1, 52*scale))) page_ = Page::Boots;
        if (MenuButton("Accesorios", false, ImVec2(-1, 52*scale))) page_ = Page::Accessories;
        if (MenuButton("Animaciones", false, ImVec2(-1, 52*scale))) page_ = Page::Animations;
      } else if (page_ == Page::Boots) {
        ImGui::TextColored(white, "Botas");
        ImGui::BeginDisabled(!s.appearance_available);
        if (ImGui::BeginTable("##boot_grid", 3, ImGuiTableFlags_SizingStretchSame)) {
          for (const auto& boot : s.boots) {
            ImGui::TableNextColumn();
            ImGui::PushID(static_cast<int>(boot.appearance_index));
            const float width = ImGui::GetContentRegionAvail().x;
            if (boot.thumbnail) {
              ImGui::PushStyleColor(ImGuiCol_Button,
                  boot.appearance_index == s.boot_index ? yellow : white);
              if (ImGui::ImageButton("##boot", ImTextureRef(boot.thumbnail),
                    ImVec2(std::max(1.0f, width-24*scale), 80*scale)))
                send(Action::Boot, {}, boot.appearance_index);
              ImGui::PopStyleColor();
              ImGui::TextColored(white, "%s", boot.name.c_str());
            } else if (MenuButton(boot.name.c_str(), boot.appearance_index == s.boot_index,
                                  ImVec2(width, 80*scale))) {
              send(Action::Boot, {}, boot.appearance_index);
            }
            ImGui::PopID();
          }
          ImGui::EndTable();
        }
        ImGui::EndDisabled();
      } else {
        const bool motions=page_==Page::Animations;
        ImGui::TextColored(white, "%s", motions?"Animaciones":"Accesorios");
        for (const auto& accessory : motions?s.animations:s.accessories) {
          ImGui::PushID(accessory.id.c_str());
          ImGui::BeginDisabled(!s.appearance_available || !accessory.available);
          ImGui::TextColored(white, "%s", accessory.label.c_str());
          const bool valid = accessory.selected >= 0 &&
              accessory.selected < static_cast<int>(accessory.choices.size());
          const char* selected = valid ? accessory.choices[accessory.selected].c_str() : "--";
          ImGui::SetNextItemWidth(-1);
          if (ImGui::BeginCombo("##value", selected)) {
            for (int i = 0; i < static_cast<int>(accessory.choices.size()); ++i) {
              if (ImGui::Selectable(accessory.choices[i].c_str(), i == accessory.selected) &&
                  i != accessory.selected) send(motions?Action::Animation:Action::Accessory, accessory.id, i);
              if (i == accessory.selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
          }
          ImGui::EndDisabled();
          ImGui::PopID();
        }
      }
      ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    if (ImGui::BeginChild("##player_preview", ImVec2(0, panel_height))) {
      if (s.player_preview && s.preview_size.x > 0 && s.preview_size.y > 0) {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float fit = std::min(available.x/s.preview_size.x, available.y/s.preview_size.y);
        const ImVec2 image{s.preview_size.x*fit, s.preview_size.y*fit};
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x-image.x)/2);
        ImGui::Image(ImTextureRef(s.player_preview), image);
      } else {
        ImGui::TextColored(white, "Espacio reservado para vista 3D");
      }
    }
    ImGui::EndChild();
    if (MenuButton("Volver", false, ImVec2(180*scale, 38*scale))) {
      if (page_ == Page::Sections) send(Action::Back);
      else page_ = Page::Sections;
    }
    ImGui::SameLine();
    if(MenuButton("Restablecer jugador", false, ImVec2(220*scale,38*scale)))send(Action::Reset);
    if (!s.error.empty()) {
      ImGui::TextColored(white, "%s", s.error.c_str());
    }
    ImGui::SetWindowFontScale(1);
  }
  ImGui::End();
  ImGui::PopStyleColor(9);
  ImGui::PopStyleVar(4);
  return requests;
}
}  // namespace pes6::editor
