#pragma once
#include "player_editor.h"
#include "player_body_fields.h"
#include "player_face_fields.h"
#include "player_nationalities.h"
#include <atomic>
#include <memory>
#include <vector>
#include <functional>
#include <array>

namespace pes6::editor {
struct Player { uint16_t id; std::string name; uint8_t boot; uint8_t sleeves;
  std::array<int,6> animations{}; std::array<int,15> body{}; std::array<int,3> hair_color{}; std::array<int,face_fields.size()> face{}; uint8_t face_kind=0; unsigned nationality=0; };
struct Catalog { std::vector<Player> players; uint64_t generation = 0; };
extern std::atomic<bool> open;
std::shared_ptr<const Catalog> ReadCatalog();
Snapshot ReadPlayer(uint16_t id);
int ReadNationality(uint16_t id);
bool Apply(const Request& request);
bool BeginFieldEdit(uint16_t id);
void EndFieldEdit();
bool CancelFieldEdit(uint16_t id);
bool ResetField(const Request& request);
void Initialize(const std::string& root);
enum class ResetSection { All, Appearance, Boots, Accessories, Animations, Head, Face, Hair };
bool ResetPlayer(uint16_t id,ResetSection section=ResetSection::All);
std::string Status();
void SetOpenCallback(std::function<void(uint16_t)> callback);
void RefreshNativeCatalog(uint8_t* base);
int PreviewAnimationStyle(uint16_t id,unsigned field,int native_style);
}  // namespace pes6::editor
