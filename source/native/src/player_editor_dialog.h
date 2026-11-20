#pragma once
#include "player_editor_runtime.h"
#include "boot_catalog.h"
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/immediate_drawer.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <cctype>

namespace pes6::editor {
class PlayerEditorDialog final : public rex::ui::ImGuiDialog {
 public:
  PlayerEditorDialog(rex::ui::ImGuiDrawer* drawer,rex::ui::ImmediateDrawer* textures,
                     std::filesystem::path root)
      : ImGuiDialog(drawer),textures_(textures),root_(std::move(root)) {
    // ImGuiDialog registers itself in its constructor. This editor must start
    // detached, even when no Toggle/auto-open callback has been invoked.
    drawer->RemoveDialog(this);
    open.store(false);
  }
  ~PlayerEditorDialog() { if(attached_)imgui_drawer()->RemoveDialog(this);open.store(false); }
  void Toggle() {
    const bool show=!open.load();open.store(show);f9_armed_=false;
    if(show&&!attached_){imgui_drawer()->AddDialog(this);attached_=true;}
    else if(!show&&attached_){imgui_drawer()->RemoveDialog(this);attached_=false;}
  }
  void SelectPlayer(uint16_t id) {
    pending_selection_=id;view_.Reset();
    if(!open.load())Toggle();
  }
 private:
  void OnDraw(ImGuiIO& io) override {
    io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
    if(!ImGui::IsKeyDown(ImGuiKey_F9))f9_armed_=true;
    if(f9_armed_&&ImGui::IsKeyPressed(ImGuiKey_F9,false)){Toggle();return;}
    if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){
      if(selected_) {selected_=0;view_.Reset();}else Toggle();
      return;
    }
    if(!loaded_&&textures_) {
      loaded_=true;
      for(int i=0;i<int(boot_count);i++) {
        std::ifstream f(root_/"data/editor/boot-previews"/(std::to_string(i)+".rgba"),std::ios::binary);
        if(!f)f.open(root_/"data/editor/boots"/("boot-"+std::to_string(2716+i)+".rgba"),std::ios::binary);
        std::vector<uint8_t> pixels(512*256*4);f.read(reinterpret_cast<char*>(pixels.data()),pixels.size());
        if(f.gcount()==static_cast<std::streamsize>(pixels.size()))
          thumbnails_[i]=textures_->CreateTexture(512,256,rex::ui::ImmediateTextureFilter::kLinear,false,pixels.data());
      }
    }
    const auto catalog=ReadCatalog();
    if(pending_selection_&&!catalog->players.empty()) {
      selected_=pending_selection_;pending_selection_=0;view_.Reset();
    }
    if(selected_) {
      auto snapshot=ReadPlayer(selected_);
      if(!snapshot.player_id){selected_=0;return;}
      for(auto& boot:snapshot.boots)if(boot.appearance_index<boot_count&&thumbnails_[boot.appearance_index])
        boot.thumbnail=reinterpret_cast<ImTextureID>(thumbnails_[boot.appearance_index].get());
      for(const auto& r:view_.Draw(snapshot)) {
        if(r.action==Action::Back){selected_=0;view_.Reset();}
        else if(r.action==Action::Information){Toggle();return;}
        else if(r.action==Action::Reset)ResetPlayer(r.player_id);
        else Apply(r);
      }
      return;
    }
    const ImGuiViewport* vp=ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x+vp->WorkSize.x*.1f,vp->WorkPos.y+vp->WorkSize.y*.1f));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x*.8f,vp->WorkSize.y*.8f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.20f,.15f,.08f,.98f));
    ImGui::PushStyleColor(ImGuiCol_Header,ImVec4(.75f,.48f,.04f,1));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,ImVec4(.65f,.4f,.03f,1));
    bool show=true;
    if(ImGui::Begin("Editar equipacion de jugador - F9",&show,ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings)) {
      ImGui::TextUnformatted("Selecciona un jugador. Los cambios se guardan automaticamente.");
      ImGui::TextUnformatted("Cierra con F9 y carga un partido NUEVO para comprobarlos.");
      ImGui::TextUnformatted("Informacion y atributos: disponibles en el editor original del juego.");
      ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##search","Buscar jugador...",search_,sizeof(search_));
      ImGui::Separator();
      if(catalog->players.empty())ImGui::TextWrapped("Cargando jugadores. Entra al menu principal del juego.");
      if(ImGui::BeginChild("##players",ImVec2(0,-45))) {
        std::string needle=search_;for(char& c:needle)c=char(std::tolower(static_cast<unsigned char>(c)));
        for(const auto& p:catalog->players) {
          std::string name=p.name;for(char& c:name)c=char(std::tolower(static_cast<unsigned char>(c)));
          if(!needle.empty()&&name.find(needle)==std::string::npos)continue;
          ImGui::PushID(int(p.id));
          if(ImGui::Selectable((p.name+"  ["+std::to_string(p.id)+"]").c_str())) {selected_=p.id;view_.Reset();}
          ImGui::PopID();
        }
      }
      ImGui::EndChild();
      if(ImGui::Button("Cerrar editor"))show=false;
    }
    ImGui::End();ImGui::PopStyleColor(3);
    if(!show)Toggle();
  }
  bool attached_=false,loaded_=false,f9_armed_=false;
  uint16_t selected_=0,pending_selection_=0;
  char search_[96]{};
  PlayerEditorView view_;
  rex::ui::ImmediateDrawer* textures_;
  std::filesystem::path root_;
  std::array<std::unique_ptr<rex::ui::ImmediateTexture>,boot_capacity> thumbnails_;
};
} // namespace pes6::editor
