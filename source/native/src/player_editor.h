#pragma once

#include <imgui.h>
#include <cstdint>
#include <string>
#include <vector>

namespace pes6::editor {

// UI snapshots contain no guest pointers. The engine must copy them at a safe
// update point, and own the texture handles until the UI releases the snapshot.
struct Boot {
  uint8_t appearance_index = 0;
  std::string name;
  ImTextureID thumbnail = 0;
};

struct Accessory {
  std::string id;
  std::string label;
  std::vector<std::string> choices;
  int selected = 0;
  bool available = false;
  int slider_value = -1;
};

struct Snapshot {
  uint16_t player_id = 0;
  uint64_t generation = 0;
  std::string player_name;
  std::string team_name;
  uint8_t boot_index = 0;
  std::vector<Boot> boots;
  std::vector<Accessory> accessories;
  std::vector<Accessory> animations;
  std::vector<Accessory> appearance;
  ImTextureID player_preview = 0;
  ImVec2 preview_size{1, 1};
  bool information_available = false;
  bool appearance_available = false;
  bool pending = false;
  std::string error;
};

enum class Action { Back, Information, Boot, Accessory, Reset, Animation, Appearance };
struct Request {
  Action action;
  uint16_t player_id;
  uint64_t generation;
  std::string field;
  int value = 0;
};

// Hosted by PlayerEditorDialog. No menu click writes guest memory directly;
// appearance changes are persisted and consumed at guest update/load points.
class PlayerEditorView {
 public:
  std::vector<Request> Draw(const Snapshot& snapshot);
  void Reset();

 private:
  enum class Page { Sections, Boots, Accessories, Animations };
  Page page_ = Page::Sections;
  uint16_t player_id_ = 0;
  uint64_t generation_ = 0;
  bool bound_ = false;
};
}  // namespace pes6::editor
