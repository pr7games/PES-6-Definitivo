// Shares the native widget constructors with native_player_editor.cpp.
// The team selector owns this scene. Its input and lifetime are independent
// of the player preview and its edit_basic instance.
#include "team_edit_runtime.h"
#include "team_lettering_catalog.h"
#include "team_management.h"
#include "team_editor_presentation.h"
#include "edit_menu_effects.h"
#include <random>
namespace {
bool team_entry_mode = false;
bool transfer_entry_mode = false;
uint32_t edit_menu_strings = 0;
pes6::team_presentation::CoachClock coach_rotation;
uint32_t coach_session = 0;
bool coach_session_started = false;
constexpr int coach_artwork_request = 100;
constexpr unsigned player_portrait_count = 6;
constexpr int transfer_artwork_index = 13, transfer_coach_index = 14;
constexpr unsigned edit_artwork_count = 15;
unsigned player_session_index = 0;
uint32_t player_session = 0;
bool player_session_started = false;
uint64_t player_session_entered = 0;
uint32_t PlayerSessionScreen() { return 0x61000000u | player_session; }
int PlayerSessionArtwork() { return 7 + int(player_session_index); }
unsigned RandomPortraitIndex(unsigned count, bool started, unsigned current) {
  if (count <= 1) return 0;
  static std::mt19937 rng{std::random_device{}()};
  std::uniform_int_distribution<unsigned> dist(0, count - 1);
  unsigned next = dist(rng);
  if (started && next == current)
    next = (next + 1 + dist(rng) % (count - 1)) % count;
  return next;
}
void BeginPlayerSession() {
  player_session_index = RandomPortraitIndex(player_portrait_count, player_session_started, player_session_index);
  player_session_started = true;
  ++player_session;
  player_session_entered = GetTickCount64();
}
uint32_t CoachSessionScreen() { return 0x60000000u | coach_session; }
void BeginCoachSession() {
  coach_rotation.index = RandomPortraitIndex(5, coach_session_started, coach_rotation.index);
  coach_session_started = true; ++coach_session;
  coach_rotation.entered = GetTickCount64();
  coach_rotation.seen = coach_rotation.visible = true;
  coach_rotation.screen = CoachSessionScreen();
}
uint32_t selector_art_screen = 0x300;
uint32_t selector_art_foreground = 0;
uint32_t menu_art_foreground = 0;
bool selector_player_portrait = true;
struct EditArtworkLayer {
  uint32_t root = 0, text = 0;
  Quad image;
  int kind = -1;
  uint64_t changed = 0;
  unsigned drawn_alpha = 256;
  float drawn_width = 0;
  pes6::team_presentation::CoachClock clock;
} edit_artwork;
enum class TeamPage {
  Menu,
  Squad,
  Numbers,
  TransferSquad,
  TransferLeague,
  TransferTeam,
  TransferPlayer,
  Formation,
  Position,
  CopyFormation,
  Roles,
  Kits,
  Kit,
  KitModel,
  KitDesign,
  KitColors,
  KitLettering,
  Identity,
  Text
};
struct TeamLeague {
  const char *name;
  unsigned begin, end;
};
constexpr std::array<TeamLeague, 12> team_leagues{{
    {"Europa A", 0, 16},
    {"Europa B", 16, 32},
    {"África", 32, 40},
    {"América", 40, 52},
    {"Asia y Oceanía", 52, 57},
    {"Selecciones Clásicas", 57, 64},
    {"Premier League", 64, 84},
    {"Ligue 1", 84, 104},
    {"Serie A", 104, 124},
    {"Eredivisie", 124, 142},
    {"La Liga", 142, 162},
    {"Otros clubes", 162, 186},
}};
struct TransferRow {
  Quad fill, badge;
  uint32_t pos = 0, name = 0;
};
struct TransferIconCell {
  Quad border, bg, icon;
};
struct TransferUI {
  bool ready = false;
  uint32_t pos_tex = 0;
  Quad top_bar, left_panel, mid_panel, right_panel;
  Quad left_header, mid_header, right_header;
  Quad nat_hdr, club_hdr, mid_pos_badge;
  uint32_t left_team_name = 0, right_team_name = 0;
  uint32_t left_title = 0, mid_title = 0, right_count = 0;
  uint32_t nat_lbl = 0, club_lbl = 0;
  uint32_t league_bank = 0, league_dummy = 0;
  std::array<TransferRow, 10> left_rows, right_rows;
  std::array<TransferIconCell, 20> right_grid;
  std::array<std::pair<uint32_t, uint32_t>, 16> stat_rows{};
};
struct TeamEditor {
  uint32_t owner = 0, root = 0, text = 0, heading = 0, title = 0, help = 0;
  uint32_t measure = 0, font = 0;
  float font_size = 1;
  std::array<uint32_t, 2> arrows{};
  std::map<uint32_t, std::string> text_cache;
  std::map<std::string, float> widths;
  uint32_t management = 0, management_input = 0, management_match = 0, previous_match = 0;
  uint32_t previous_manager = 0, management_settings = 0;
  std::array<uint32_t,8> management_layer_stamp{};
  std::array<uint8_t, 8> previous_bindings{};
  std::array<uint8_t, 5> visible{};
  std::array<Cell, 11> cells;
  Quad panel;
  Quad cat_top_bar, cat_ok_fill, scroll_track, scroll_thumb;
  uint32_t cat_ok_label = 0;
  bool menu_ok = false;
  Quad coach;
  uint32_t coach_root = 0, wide_geometry = 0, narrow_geometry = 0;
  unsigned coach_alpha = 256;
  int coach_bound_index = -1;
  pes6::team_presentation::CoachClock coach_clock;
  std::array<Quad, 11> markers;
  unsigned id = 0, selected = 0, offset = 0, slot = 0, variant = 1, target = 0, character = 0;
  int swap = -1;
  bool transfer_register = false;
  bool transfer_selector_active = false;
  bool transfer_selector_resume = false;
  uint32_t transfer_preview_team=0xffffffffu;
  std::array<uint32_t,5> transfer_native_roots{};
  std::array<std::array<float,2>,5> transfer_native_offsets{};
  unsigned transfer_league = 0;
  unsigned transfer_squad_selected = 0;
  TransferUI transfer_ui;
  bool redraw = true;
  unsigned row_count = 1;
  TeamPage page = TeamPage::Menu;
  std::map<unsigned, pes6::teams::Squad> drafts;
  std::map<unsigned, pes6::teams::Squad> originals;
  std::array<std::array<int, 2>, 4> models;
  std::array<std::array<int, 21>, 4> colors;
  std::array<std::array<int, 8>, 4> designs;
  std::array<pes6_team_info::Lettering, 4> lettering;
  std::array<int, 4> crests;
  std::array<bool, 4> atlases{};
  std::array<uint8_t, 432> original_uniform{};
  bool before_atlas = false;
  std::vector<unsigned> registrations;
  std::vector<std::pair<std::string, std::string>> registration_rows;
  std::string message, name, short_name, edit_text;
  std::string home_name, before_home_name;
  int home_model = -1, before_home_model = -1;
  bool short_edit = false;
  bool stadium_edit = false;
  bool editing = false;
  int number_draft = -1;
  bool preview_active = false, committed = false;
  uint64_t preview_signature = 0;
  uint16_t preview_player = 0;
  std::array<uint16_t, 4> preview_players{};
  decltype(models) saved_models;
  decltype(colors) saved_colors;
  decltype(designs) saved_designs;
  decltype(lettering) saved_lettering;
  decltype(crests) saved_crests;
  decltype(atlases) saved_atlases;
  pes6::teams::Squad before_squad;
  std::array<int, 2> before_model;
  std::array<int, 21> before_colors;
  std::array<int, 8> before_design;
  pes6_team_info::Lettering before_lettering;
  int before_crest = 0;
} team_editor;
uint32_t TeamKit(uint8_t *base) {
  return REX_LOAD_U32(0x83139440u) + 778148 + (team_editor.id == 186 ? 185 : team_editor.id) * 728;
}
uint8_t OriginalKitByte(unsigned offset) {
  return team_editor.original_uniform[team_editor.variant * 108 + offset];
}
unsigned OriginalKitColor(unsigned slot) {
  return unsigned(OriginalKitByte(slot * 4)) << 8 | OriginalKitByte(slot * 4 + 1);
}
constexpr std::array<unsigned, 5> team_roots{176, 180, 192, 196, 200};
std::string TeamName(uint8_t *base, unsigned id) {
  const auto at = 0x82D55218u + id * 24;
  const auto name = REX_LOAD_U32(at);
  if (!Valid(base, name, 64))
    return "Equipo " + std::to_string(id);
  return std::string(reinterpret_cast<char *>(base + name),
                     strnlen(reinterpret_cast<char *>(base + name), 63));
}
std::string TeamPlayer(PPCContext &ctx, uint8_t *base, unsigned id, unsigned team_id = 0xffffu) {
  if (!id)
    return "Vacante";
  const auto db = REX_LOAD_U32(0x83139440u), rec = db + id * 152, at = rec + 4;
  if (!db || id >= 4210 || !Valid(base, at, 64))
    return "Jugador " + std::to_string(id);
  auto decode = [](const char *ptr, size_t max_len, bool &has_high, bool &valid_utf8) {
    const size_t len = strnlen(ptr, max_len);
    std::string raw(ptr, len);
    has_high = false;
    valid_utf8 = true;
    for (size_t i = 0; i < raw.size();) {
      const uint8_t c = uint8_t(raw[i]);
      if (c < 0x80) { ++i; continue; }
      has_high = true;
      size_t need = (c & 0xe0) == 0xc0 ? 1 : (c & 0xf0) == 0xe0 ? 2 : (c & 0xf8) == 0xf0 ? 3 : 0;
      if (!need || i + need >= raw.size()) { valid_utf8 = false; break; }
      for (size_t k = 1; k <= need; ++k) {
        if ((uint8_t(raw[i + k]) & 0xc0) != 0x80) { valid_utf8 = false; break; }
      }
      if (!valid_utf8) break;
      i += need + 1;
    }
    if (valid_utf8) return raw;
    std::string out;
    out.reserve(raw.size() * 2);
    for (uint8_t c : raw) {
      if (c < 0x80) out.push_back(char(c));
      else { out.push_back(char(0xc0 | (c >> 6))); out.push_back(char(0x80 | (c & 0x3f))); }
    }
    return out;
  };
  bool has_high = false, valid_utf8 = true;
  std::string out = decode(reinterpret_cast<const char *>(base + at), 45, has_high, valid_utf8);
  if (out.empty()) {
    // Use PES 6's native player name resolver (sub_8217D5F0) when +4 is empty
    const auto resolved = Native(ctx, base, sub_8217D5F0, at, id, team_id & 0xffffu);
    if (Valid(base, resolved, 46))
      out = decode(reinterpret_cast<const char *>(base + resolved), 45, has_high, valid_utf8);
    if (out.empty())
      out = decode(reinterpret_cast<const char *>(base + rec + 50), 16, has_high, valid_utf8);
    if (out.empty())
      out = decode(reinterpret_cast<const char *>(base + rec + 36), 16, has_high, valid_utf8);
  }
  if (out.empty()) return "Jugador " + std::to_string(id);
  return out;
}
void TeamText(PPCContext &ctx, uint8_t *base, uint32_t node, const std::string &value) {
  if (!node)
    return;
  // TeamDraw only runs on an input/layout change. Even unchanged strings must
  // pass through the native setter after PlaceText: it restores all glyph dirty
  // bits (0x67), including bit 0x04 required by 82737838 after glyph recreation.
  // Caching by string alone left that bit clear and the rebuilt text invisible.
  const auto length = std::min<size_t>(value.size(), 1023);
  std::memcpy(base + team_editor.text, value.data(), length);
  base[team_editor.text + length] = 0;
  const auto table = REX_LOAD_U32(node + 96);
  if (Valid(base, table, 20))
    if (auto setter = rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(table + 16)))
      {
      Native(ctx, base, setter, node + 96, team_editor.text);
      // The native setter allocates length+1 but copies only length bytes.
      // Terminate our plain UTF-8 labels explicitly when reusing heap blocks.
      const auto owned = REX_LOAD_U32(node + 100);
      if (Valid(base, owned, length + 1)) base[owned + length] = 0;
    }
  base[node + 52] |= 0x67;
}
float TeamTextWidth(PPCContext &ctx, uint8_t *base, const std::string &value) {
  auto &e = team_editor;
  if (value.empty() || !e.measure)
    return 0;
  if (const auto found = e.widths.find(value); found != e.widths.end())
    return found->second;
  Native(ctx, base, sub_82733D98, e.measure);
  REX_STORE_U32(e.measure, 0x820bdbccu);
  REX_STORE_U32(e.measure + 52, e.font);
  Float(base, e.measure + 56, e.font_size);
  std::memcpy(base + e.text, value.c_str(), std::min<size_t>(value.size() + 1, 1024));
  Native(ctx, base, sub_82734708, e.measure, e.text);
  Native(ctx, base, sub_8273C320, e.measure);
  const float width = ReadFloat(base, e.measure + 36);
  Native(ctx, base, sub_8273C0B0, e.measure);
  e.widths[value] = width;
  return width;
}
std::string TeamFitText(PPCContext &ctx, uint8_t *base, std::string value, float maximum) {
  if (TeamTextWidth(ctx, base, value) <= maximum)
    return value;
  do {
    // Remove one complete UTF-8 character, preserving the original font scale.
    size_t at = value.size() - 1;
    while (at && (uint8_t(value[at]) & 0xc0) == 0x80)
      --at;
    value.resize(at);
  } while (!value.empty() && TeamTextWidth(ctx, base, value + "...") > maximum);
  return value + "...";
}
bool TeamInput(PPCContext &ctx, uint8_t *base, unsigned key, bool repeat = false) {
  const auto input = REX_LOAD_U32(team_editor.owner + 144);
  const bool pressed = input && Native(ctx, base, repeat ? sub_8272E038 : sub_8272DF98, input, key);
  if (pressed)
    team_editor.redraw = true;
  return pressed;
}
bool TeamHeld(PPCContext &ctx, uint8_t *base, unsigned key) {
  const auto input = REX_LOAD_U32(team_editor.owner + 144);
  return input && Native(ctx, base, sub_8272E0D8, input, key) != 0;
}
pes6::teams::Squad &TeamSquad(uint8_t *base, unsigned id) {
  auto [it, inserted] = team_editor.drafts.try_emplace(id);
  if (inserted) {
    pes6::teams::Read(base, id, it->second);
    team_editor.originals[id] = it->second;
  }
  return it->second;
}
unsigned TeamCount(const pes6::teams::Squad &squad) {
  return unsigned(std::find(squad.players.begin(), squad.players.end(), 0) - squad.players.begin());
}
void TeamPageTo(TeamPage page) {
  team_editor.redraw = true;
  team_editor.page = page;
  team_editor.selected = 0;
  team_editor.offset = 0;
  team_editor.swap = -1;
  team_editor.message.clear();
  team_editor.editing = false;
  team_editor.number_draft = -1;
  team_editor.character = 0;
  team_editor.menu_ok = false;
}
bool TeamScalarPage() {
  const auto page = team_editor.page;
  return page == TeamPage::Numbers || page == TeamPage::Roles || page == TeamPage::Position ||
         page == TeamPage::KitModel || page == TeamPage::KitDesign || page == TeamPage::KitColors ||
         page == TeamPage::KitLettering ||
         (page == TeamPage::Identity && team_editor.selected == 2);
}
std::vector<std::string> TeamCharacters(const std::string &value) {
  std::vector<std::string> result;
  for (size_t at = 0; at < value.size();) {
    const auto first = uint8_t(value[at]);
    const size_t length = first < 0x80 ? 1 : first < 0xe0 ? 2 : first < 0xf0 ? 3 : 4;
    result.push_back(value.substr(at, std::min(length, value.size() - at)));
    at += length;
  }
  return result;
}
void TeamJoinCharacters(const std::vector<std::string> &letters) {
  std::string value;
  for (const auto &letter : letters)
    value += letter;
  if (value.size() <= (team_editor.short_edit ? 8 : 63))
    team_editor.edit_text = std::move(value);
}
bool TeamManagementUpdate(PPCContext &ctx, uint8_t *base);
void TransferSelectorClose(PPCContext& ctx,uint8_t* base){
  auto& e=team_editor;
  if(!e.transfer_selector_active)return;
  const auto selector=REX_LOAD_U32(e.owner+172);
  if(Valid(base,selector,40))Native(ctx,base,sub_82712B30,selector,1);
  for(unsigned i=0;i<e.transfer_native_roots.size();i++){
    const auto node=e.transfer_native_roots[i];if(!Valid(base,node,72))continue;
    Float(base,node+32,e.transfer_native_offsets[i][0]);
    Float(base,node+36,e.transfer_native_offsets[i][1]);
    base[node+53]=0;base[node+52]|=0x23;
  }
  // Native 822E9F08 may have re-enabled its independent roster/header roots.
  for(auto offset:{176u,180u,196u,200u}){
    const auto node=REX_LOAD_U32(e.owner+offset);
    if(Valid(base,node,54))base[node+53]=0;
  }
  e.transfer_selector_active=false;e.transfer_native_roots={};
  e.transfer_preview_team=0xffffffffu;
}
void TeamClose(PPCContext &ctx, uint8_t *base, bool restore = true) {
  auto &e = team_editor;
  if (!e.owner)
    return;
  TransferSelectorClose(ctx,base);
  if (e.management) {
    // Owner teardown must also release a native submenu still in progress.
    base[e.management + 168] = 5;
    TeamManagementUpdate(ctx, base);
  }
  if (e.preview_active) preview::Leave(ctx, base);
  if (!e.committed) {
    pes6_team_info::models[e.id] = e.saved_models;
    pes6_team_info::colors[e.id] = e.saved_colors;
    pes6_team_info::designs[e.id] = e.saved_designs;
    pes6_team_info::lettering[e.id] = e.saved_lettering;
    pes6_team_info::atlases[e.id] = e.saved_atlases;
    pes6::crest::modes[e.id] = e.saved_crests;
    pes6_team_info::RefreshOverrides(e.id);
    pes6_team_info::PatchModels(base, e.id, TeamKit(base));
    pes6_team_info::PatchColors(base, e.id, TeamKit(base));
  }
  if (e.root)
    Native(ctx, base, sub_82735EC8, e.root);
  if (e.coach_root)
    Native(ctx, base, sub_82735EC8, e.coach_root);
  if (e.text)
    Native(ctx, base, sub_8280E7B8, e.text);
  if (e.measure)
    Native(ctx, base, sub_8280E7B8, e.measure);
  if (e.transfer_selector_active && e.owner) {
    const auto selector = REX_LOAD_U32(e.owner + 172);
    if (selector) {
      const auto sub = Valid(base, selector + 24, 4) ? REX_LOAD_U32(selector + 20) : 0;
      const auto ui_root = sub && Valid(base, sub + 212, 4) ? REX_LOAD_U32(sub + 208) : 0;
      if (Valid(base, ui_root, 56)) {
        Float(base, ui_root + 16, 0.f);
        Float(base, ui_root + 20, 0.f);
        base[ui_root + 52] |= 0x23;
      }
      Native(ctx, base, sub_82712B30, selector, 1);
    }
  }
  if (restore) {
    Native(ctx, base, sub_82712D58, REX_LOAD_U32(e.owner + 172));
    for (unsigned i = 0; i < team_roots.size(); i++) {
      const auto root = REX_LOAD_U32(e.owner + team_roots[i]);
      if (Valid(base, root, 54))
        base[root + 53] = e.visible[i];
    }
  }
  team_editor = {};
}

// Standalone instance of the very same pre-match formation controller. Input
// squads are copied by its factory; no match is started or registered as a scene.
bool TeamManagementOpen(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  e.management_input = Allocate(ctx, base, 3772 * 2);
  e.management_match = Allocate(ctx, base, 32 + 15000 * 2);
  if (!e.management_input || !e.management_match) {
    if (e.management_input) Native(ctx, base, sub_8280E7B8, e.management_input);
    if (e.management_match) Native(ctx, base, sub_8280E7B8, e.management_match);
    e.management_input = e.management_match = 0;
    return false;
  }
  e.previous_match = REX_LOAD_U32(0x83139448u);
  e.previous_manager = REX_LOAD_U32(0x83139808u);
  const auto &squad = TeamSquad(base, e.id);
  for (unsigned side = 0; side < 2; side++) {
    const auto input = e.management_input + side * 3772;
    const auto match_team = e.management_match + 32 + side * 15000;
    REX_STORE_U32(e.management_match + 4 + side * 4, match_team);
    // Build player names, abilities and compatibility through the native DB
    // loader. Temporarily expose the draft roster only during this synchronous call.
    const auto roster_at = pes6::teams::Address(base, "ROSTER", e.id);
    const auto numbers_at = pes6::teams::Address(base, "NUMBERS", e.id);
    std::array<uint8_t, 64> roster_before{};
    std::array<uint8_t, 32> numbers_before{};
    std::memcpy(roster_before.data(), base + roster_at, 64);
    std::memcpy(numbers_before.data(), base + numbers_at, 32);
    for (unsigned slot = 0; slot < 32; slot++) {
      REX_STORE_U16(roster_at + slot * 2, squad.players[slot]);
      base[numbers_at + slot] = squad.numbers[slot];
    }
    Native(ctx, base, sub_82178F30, match_team, e.id);
    std::memcpy(base + roster_at, roster_before.data(), 64);
    std::memcpy(base + numbers_at, numbers_before.data(), 32);
    std::memcpy(base + match_team + 9784, squad.configuration.data(), 358);
    Native(ctx, base, sub_825690E8, input, match_team);
    // 825690E8 only builds the first 2236 bytes. The factory also consumes
    // 32 packed 48-byte ability records, normally prepared by 82247DE0.
    for (unsigned slot = 0; slot < 32; slot++) {
      const auto source = match_team + 228 + slot * 304;
      const auto record = input + 2236 + slot * 48;
      Native(ctx, base, sub_8222CE70, record);
      std::memcpy(base + record, base + source, 36);
      const auto a = REX_LOAD_U32(source + 32), b = REX_LOAD_U32(source + 36);
      auto flags = REX_LOAD_U32(record + 32);
      flags = (flags & ~0x700f0u) | (a & 0x700f0u);
      REX_STORE_U32(record + 32, flags);
      base[record + 40] = uint8_t((b >> 26) & 63);
      base[record + 41] = uint8_t((b >> 17) & 127);
      base[record + 42] = uint8_t(REX_LOAD_U32(source + 76));
      base[record + 43] = uint8_t((REX_LOAD_U32(source + 80) >> 18) & 31);
    }
    // Use the native 23-player match view, even though the database and
    // packed configuration retain 32 slots. Never expose a 32-row view:
    // 82261D00 then dereferences a missing widget (guest 0x35).
    // The match's roster-limit byte is not initialized in the edit scene.
    base[input + 2] = 23;
    base[input + 1614] = std::min<uint8_t>(base[input + 1614], 23);
    for (unsigned slot = 0; slot < 32; slot++)
      REX_STORE_U16(input + 1796 + slot * 2, squad.numbers[slot]);
    std::memcpy(base + input + 1869, squad.roles.data(), 6);
    for (unsigned saved = 0; saved < 10; saved++) {
      const auto record = match_team + 10164 + saved * 406;
      const auto saved_record = squad.saved.data() + saved * 406;
      if (std::any_of(saved_record, saved_record + 406, [](uint8_t v) { return v != 0; }))
        std::memcpy(base + record, saved_record, 406);
      else {
        REX_STORE_U16(record, 65535);
        std::memcpy(base + record + 48, squad.configuration.data(), 358);
      }
    }
  }
  REX_STORE_U32(0x83139448u, e.management_match);
  e.management_settings = REX_LOAD_U32(0x83139438u);
  std::memcpy(e.previous_bindings.data(), base + e.management_settings + 508, 8);
  // Edit mode has no pre-match controller assignment. Temporarily bind Local.
  base[e.management_settings + 508] = 0;
  base[e.management_settings + 512] = 255;
  base[e.management_settings + 514] = 255;
  // Native editor mode 6 applies roster swaps immediately. Modes 1/2 are
  // in-match modes: 8225B008 queues substitutions at +1744 instead.
  e.management = Native(ctx, base, sub_82243350, e.management_input, 0, 0, 6);
  if (!e.management) {
    std::memcpy(base + e.management_settings + 508, e.previous_bindings.data(), 8);
    REX_STORE_U32(0x83139448u, e.previous_match);
    REX_STORE_U32(0x83139808u, e.previous_manager);
    Native(ctx, base, sub_8280E7B8, e.management_input);
    Native(ctx, base, sub_8280E7B8, e.management_match);
    e.management_input = e.management_match = 0;
    return false;
  }
  base[e.root + 53] = 0;
  if (const auto header = Native(ctx, base, sub_82730A50); Valid(base, header, 20)) {
    const auto label = REX_LOAD_U32(header + 16);
    if (Valid(base, label, 54)) base[label + 53] = 0;
  }
  return true;
}

void TeamManagementPresentation(uint8_t *base) {
  const auto &e = team_editor;
  // These are independently owned native roots, not a screen-sized cover.
  // Preserve both data contexts; only the redundant opponent presentation goes.
  const auto right = e.management + 392 + 3776;
  const auto hide = [&](uint32_t node) { if (Valid(base, node, 54)) base[node + 53] = 0; };
  const auto pitch = REX_LOAD_U32(right + 3704);
  if (Valid(base, pitch, 1744)) {
    hide(REX_LOAD_U32(pitch + 1728));
    hide(REX_LOAD_U32(pitch + 1732));
    // Copied native labels have their own roots outside the pitch hierarchy.
    hide(REX_LOAD_U32(pitch + 1736));
    hide(REX_LOAD_U32(pitch + 1740));
  }
  const auto roster = REX_LOAD_U32(right + 3712);
  if (Valid(base, roster, 960)) {
    const auto list = REX_LOAD_U32(roster + 956);
    if (Valid(base, list, 80)) { hide(REX_LOAD_U32(list + 68)); hide(REX_LOAD_U32(list + 76)); }
  }
  const auto name = REX_LOAD_U32(right + 3720);
  if (Valid(base, name, 8)) hide(REX_LOAD_U32(name + 4));
}
void TeamManagementForeground(PPCContext &ctx, uint8_t *base) {
  const auto pitch = REX_LOAD_U32(team_editor.management + 392 + 3704);
  if (!Valid(base, pitch, 1732)) return;
  const auto root = REX_LOAD_U32(pitch + 1728);
  if (!Valid(base, root, 132)) return;
  const auto resource = REX_LOAD_U32(root + 128);
  const auto list = REX_LOAD_U32(0x82d90000u - 21328u);
  if (!resource || !Valid(base, list, 8)) return;
  const auto sentinel = REX_LOAD_U32(list + 4);
  if (!Valid(base, sentinel, 12)) return;
  const std::array<uint32_t,8> stamp{root,resource,list,sentinel,REX_LOAD_U32(list),
    REX_LOAD_U32(sentinel),REX_LOAD_U32(sentinel+4),
    (uint32_t(base[team_editor.management+168])<<8)|base[team_editor.management+169]};
  if(team_editor.management_layer_stamp==stamp&&REX_LOAD_U32(root+68)==2)return;
  team_editor.management_layer_stamp=stamp;
  auto link = REX_LOAD_U32(sentinel);
  for (unsigned i = 0; i < 1024 && link != sentinel && Valid(base, link, 12); i++, link = REX_LOAD_U32(link)) {
    const auto node = REX_LOAD_U32(link + 8);
    if (Valid(base, node, 224) && REX_LOAD_U32(node + 128) == resource && REX_LOAD_U32(node + 68) != 2)
      pes6_edit_foreground_tree(base, node);
  }
}
bool TeamManagementUpdate(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  if (!e.management)
    return false;
  // State 5 of the inner controller starts the match loading/fade path.
  // State 4 has already completed its native menu and committed its edits.
  if (base[e.management + 168] == 2 && base[e.management + 169] == 5)
    base[e.management + 168] = 5;
  else
    Native(ctx, base, sub_822430E0, e.management);
  if (base[e.management + 168] < 5) {
    if (const auto header = Native(ctx, base, sub_82730A50); Valid(base, header, 20)) {
      const auto label = REX_LOAD_U32(header + 16);
      if (Valid(base, label, 54)) base[label + 53] = 0;
    }
    TeamManagementForeground(ctx, base);
    TeamManagementPresentation(base);
    return true;
  }
  auto &squad = TeamSquad(base, e.id);
  // Native result contains the whole 358-byte setup, including three plans,
  // team strategy, individual instructions, set pieces and formation data.
  std::memcpy(squad.configuration.data(), base + e.management + 2672, 358);
  std::memcpy(squad.roles.data(), squad.configuration.data() + 105, 6);
  for (unsigned i = 0; i < 10; i++) {
    squad.formation[i] = squad.configuration[111 + 22 + i];
    squad.formation[10 + i] = squad.configuration[111 + 1 + i * 2];
    squad.formation[20 + i] = squad.configuration[111 + 2 + i * 2];
  }
  for (unsigned i = 0; i < 11; i++) {
    squad.formation[30 + i] = squad.configuration[111 + 32 + i];
    squad.formation[41 + i] = squad.configuration[111 + 43 + i];
  }
  const auto saved = REX_LOAD_U32(e.management + 392 + 104);
  if (Valid(base, saved, squad.saved.size()))
    std::memcpy(squad.saved.data(), base + saved, squad.saved.size());
  Native(ctx, base, sub_82243C10, e.management);
  Native(ctx, base, sub_82243510, e.management, 1);
  std::memcpy(base + e.management_settings + 508, e.previous_bindings.data(), 8);
  REX_STORE_U32(0x83139448u, e.previous_match);
  REX_STORE_U32(0x83139808u, e.previous_manager);
  Native(ctx, base, sub_8280E7B8, e.management_input);
  Native(ctx, base, sub_8280E7B8, e.management_match);
  e.management = e.management_input = e.management_match = 0;
  if (pes6::teams::SaveTactics(e.id, squad, e.originals.at(e.id))) {
    pes6::teams::Apply(base);
    const auto db = REX_LOAD_U32(0x83139440u);
    pes6::teams::Configuration(base, db + 915740 + e.id * 406, e.id);
    e.message.clear();
  } else {
    e.message = "No se pudo guardar la táctica. Conservada en el borrador";
  }
  base[e.root + 53] = 1;
  if (const auto header = Native(ctx, base, sub_82730A50); Valid(base, header, 20)) {
    const auto label = REX_LOAD_U32(header + 16);
    if (Valid(base, label, 54)) base[label + 53] = 1;
    if (edit_menu_strings) Native(ctx, base, sub_82730828, header, edit_menu_strings + (transfer_entry_mode ? 40 : 24));
  }
  e.text_cache.clear();
  e.redraw = true;
  return true;
}
std::array<uint32_t, 2> TeamFrameGeometry(PPCContext &ctx, uint8_t *base, uint32_t source) {
  // Keep the native rounded corners, gradients and border thickness. Move
  // only the right-hand vertices of the outer frame and team-name header.
  static std::array<uint32_t, 2> frames{};
  if (!Valid(base, source, 24)) return {};
  const unsigned count = REX_LOAD_U16(source);
  const auto table = REX_LOAD_U32(source + 12);
  if (count < 32 || count > 128 || !Valid(base, table, count * 4)) return {};
  for (unsigned narrow = 0; narrow < 2; narrow++) {
    if (!frames[narrow]) frames[narrow] = Allocate(ctx, base, 16384);
    if (!frames[narrow]) return {};
    const auto target = frames[narrow], pointers = target + 32;
    std::memcpy(base + target, base + source, 24);
    REX_STORE_U32(target + 12, pointers);
    auto cursor = pointers + count * 4;
    for (unsigned i = 0; i < count; i++) {
      const auto record = REX_LOAD_U32(table + i * 4);
      if (!Valid(base, record, 28)) return {};
      REX_STORE_U32(pointers + i * 4, cursor);
      std::memcpy(base + cursor, base + record, 28);
      const auto copy = cursor;
      cursor += 28;
      if (i >= 18) { base[copy] = 0; continue; }
      if (!narrow || i >= 14) continue;
      const unsigned vertices = REX_LOAD_U16(record + 14);
      const auto original = REX_LOAD_U32(record + 16);
      if (vertices > 128 || !Valid(base, original, vertices * 6)) return {};
      std::memcpy(base + cursor, base + original, vertices * 6);
      REX_STORE_U32(copy + 16, cursor);
      for (unsigned v = 0; v < vertices; v++) {
        const int x = int16_t(REX_LOAD_U16(cursor + v * 6));
        if (x >= 676 * 16) REX_STORE_U16(cursor + v * 6, uint16_t(x - 168 * 16));
      }
      cursor += vertices * 6;
    }
  }
  return frames;
}
bool TeamOpen(PPCContext &ctx, uint8_t *base, uint32_t owner, unsigned id) {
  auto &e = team_editor;
  pes6::teams::Squad initial;
  if (!pes6::teams::Read(base, id, initial))
    return false;
  e.owner = owner;
  if (edit_artwork.root) base[edit_artwork.root + 53] = 0;
  edit_artwork.kind = -1;
  e.id = id;
  e.drafts[id] = initial;
  e.originals[id] = initial;
  pes6_team_info::Load();
  e.models = pes6_team_info::models[id];
  e.colors = pes6_team_info::colors[id];
  e.designs = pes6_team_info::designs[id];
  e.lettering = pes6_team_info::lettering[id];
  e.crests = pes6::crest::modes[id];
  e.atlases = pes6_team_info::atlases[id];
  e.saved_models = e.models; e.saved_colors = e.colors; e.saved_designs = e.designs;
  e.saved_lettering = e.lettering; e.saved_crests = e.crests; e.saved_atlases = e.atlases;
  e.original_uniform = pes6_team_info::OriginalUniform(base, TeamKit(base));
  e.home_model = pes6_team_info::homes[id].model;
  e.home_name = pes6_team_info::homes[id].name.value;
  for (unsigned i = 0; i < team_roots.size(); i++) {
    const auto root = REX_LOAD_U32(owner + team_roots[i]);
    if (Valid(base, root, 54)) {
      e.visible[i] = base[root + 53];
      base[root + 53] = 0;
    }
  }
  e.text = Allocate(ctx, base, 1024);
  const auto resource = Native(ctx, base, sub_8272A848, 0x820c77ecu);
  if (!resource || !e.text) {
    TeamClose(ctx, base);
    return false;
  }
  if (!REX_LOAD_U32(resource + 40))
    Native(ctx, base, sub_8272A9C0, resource);
  e.root = Native(ctx, base, sub_8272FA80, resource, 0, 0, 0xffffffffu);
  if (!e.root) {
    TeamClose(ctx, base);
    return false;
  }
  const auto form = Native(ctx, base, sub_8272F650, e.root, 1);
  const auto row = Native(ctx, base, sub_8272E410, form, 2);
  const auto sample = VectorItem(base, row, 312, 0);
  if (!Valid(base, sample, 272)) {
    TeamClose(ctx, base);
    return false;
  }
  for (unsigned i = 0; i < 3; i++) {
    const auto page = Native(ctx, base, sub_8272F650, e.root, i);
    if (Valid(base, page, 54))
      base[page + 53] = 0;
  }
  HideVector(base, e.root, 312);
  HideVector(base, e.root, 280);
  const auto frames = TeamFrameGeometry(ctx, base, REX_LOAD_U32(e.root + 140));
  e.wide_geometry = frames[0]; e.narrow_geometry = frames[1];
  if (!e.wide_geometry || !e.narrow_geometry) { TeamClose(ctx, base); return false; }
  REX_STORE_U32(e.root + 140, e.narrow_geometry);
  base[e.root + 52] |= 0x23;
  // Independent edit_basic coordinate system keeps the coach visible while
  // Gestión hides the editor scene. No opaque cover hides the duplicate team.
  e.coach_root = Native(ctx, base, sub_8272FA80, resource, 0, 0, 0xffffffffu);
  static uint32_t coach_geometry = 0;
  if (!coach_geometry) coach_geometry = Allocate(ctx, base, 24);
  if (!e.coach_root || !coach_geometry) { TeamClose(ctx, base); return false; }
  std::memcpy(base + coach_geometry, base + e.wide_geometry, 24);
  REX_STORE_U16(coach_geometry, 0);
  REX_STORE_U32(e.coach_root + 140, coach_geometry);
  base[e.coach_root + 52] |= 0x23;
  HideVector(base, e.coach_root, 312); HideVector(base, e.coach_root, 280);
  for (unsigned i = 0; i < 3; i++) {
    const auto page = Native(ctx, base, sub_8272F650, e.coach_root, i);
    if (Valid(base, page, 54)) base[page + 53] = 0;
  }
  e.coach = NewQuad(ctx, base, e.coach_root, true);
  if (!e.coach.primitive) { TeamClose(ctx, base); return false; }
  e.measure = Allocate(ctx, base, 128);
  if (!e.measure) {
    TeamClose(ctx, base);
    return false;
  }
  e.font = REX_LOAD_U32(sample + 148);
  e.font_size = ReadFloat(base, sample + 152);
  e.panel = NewQuad(ctx, base, e.root);
  e.cat_top_bar = NewQuad(ctx, base, e.root);
  e.cat_ok_fill = NewQuad(ctx, base, e.root);
  e.scroll_track = NewQuad(ctx, base, e.root);
  e.scroll_thumb = NewQuad(ctx, base, e.root);
  Rect(base, e.panel, -332, -157, 503, 287, 0xe9e9e7ff, 0xe9e9e7ff);
  e.heading = Label(ctx, base, e.root, sample);
  e.title = Label(ctx, base, e.root, sample);
  e.help = Label(ctx, base, e.root, sample);
  e.cat_ok_label = Label(ctx, base, e.root, sample);
  if (e.heading) {
    FontScale(base, e.heading, e.font_size * .90f);
    REX_STORE_U32(e.heading + 48, 0xffffffffu);
  }
  for (auto &cell : e.cells) {
    cell.border = NewQuad(ctx, base, e.root);
    cell.fill = NewQuad(ctx, base, e.root);
    cell.label = Label(ctx, base, e.root, sample);
    cell.value = Label(ctx, base, e.root, sample);
    if (!cell.border.primitive || !cell.fill.primitive || !cell.label || !cell.value) {
      TeamClose(ctx, base);
      return false;
    }
  }
  for (auto &marker : e.markers)
    marker = NewQuad(ctx, base, e.root);
  // Native children paint in registration order. Create the Age arrows LAST,
  // like the working player editor: panel/row fills otherwise cover them.
  for (unsigned direction = 0; direction < 2; direction++) {
    e.arrows[direction] = NewArrow(ctx, base, e.root, VectorItem(base, row, 376, direction));
    if (!e.arrows[direction]) {
      TeamClose(ctx, base);
      return false;
    }
  }
  if (!e.heading || !e.title || !e.help || !e.panel.primitive) {
    TeamClose(ctx, base);
    return false;
  }
  e.name = TeamName(base, id);
  const auto short_at = REX_LOAD_U32(0x82D5521cu + id * 24);
  if (Valid(base, short_at, 9))
    e.short_name = std::string(reinterpret_cast<char *>(base + short_at),
                               strnlen(reinterpret_cast<char *>(base + short_at), 8));
  return true;
}
void TeamRemove(pes6::teams::Squad &squad, unsigned slot) {
  for (unsigned i = slot; i < 31; i++) {
    squad.players[i] = squad.players[i + 1];
    squad.numbers[i] = squad.numbers[i + 1];
  }
  squad.players[31] = 0;
  squad.numbers[31] = 255;
  for (auto &role : squad.roles) {
    if (role == slot)
      role = 0;
    else if (role > slot)
      --role;
  }
}
unsigned TeamFreeNumber(const pes6::teams::Squad &squad, unsigned wanted) {
  const auto used = [&](unsigned number) {
    for (unsigned i = 0; i < 32; i++)
      if (squad.players[i] && squad.numbers[i] == number)
        return true;
    return false;
  };
  if (wanted >= 1 && wanted <= 99 && !used(wanted))
    return wanted;
  for (unsigned number = 1; number <= 99; number++)
    if (!used(number))
      return number;
  return 99;
}
void TeamAssignNumber(pes6::teams::Squad &squad, unsigned slot, uint8_t wanted) {
  unsigned displaced = 32;
  for (unsigned i = 0; i < 32; i++)
    if (i != slot && squad.players[i] && squad.numbers[i] == wanted)
      displaced = i;
  squad.numbers[slot] = wanted;
  if (displaced == 32)
    return;
  squad.numbers[displaced] = 255;
  for (unsigned step = 1; step <= 99; step++) {
    const auto candidate = (unsigned(wanted) - 1 + step) % 99 + 1;
    if (TeamFreeNumber(squad, candidate) == candidate) {
      squad.numbers[displaced] = uint8_t(candidate);
      return;
    }
  }
}
bool TeamSaveIdentity(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  const auto path = AssetRoot() / "data/editor/team-info.txt";
  std::ifstream input(path, std::ios::binary);
  std::vector<std::string> lines;
  std::string line;
  std::getline(input, line);
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  const bool legacy = line == "PES6_TEAM_INFO_V1";
  if (!line.empty() && line != "PES6_TEAM_INFO_V2" && !legacy) {
    e.message = "Perfil de identidad incompatible";
    return false;
  }
  while (std::getline(input, line)) {
    std::istringstream row(line);
    std::string kind;
    unsigned id;
    if (row >> kind >> id) {
      if (legacy && kind == "TEAM") {
        id += 7;
        std::string rest;
        std::getline(row, rest);
        line = "TEAM " + std::to_string(id) + rest;
      }
      if (id == e.id &&
          (kind == "TEAM" || kind == "HOME" || kind == "KIT_MODEL" || kind == "KIT_COLOR" ||
           kind == "KIT_DESIGN" || kind == "KIT_ATLAS" || kind == "KIT_CREST" ||
           kind == "KIT_LETTERING_V2" || kind == "KIT_LETTERING"))
        continue;
    }
    lines.push_back(line);
  }
  auto temporary = path;
  temporary += L".team-tmp";
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  out << "PES6_TEAM_INFO_V2\n";
  for (const auto &preserved : lines)
    out << preserved << '\n';
  out << "TEAM " << e.id << ' ' << std::quoted(e.name) << ' ' << std::quoted(e.short_name) << '\n';
  if (e.home_model >= 0)
    out << "HOME " << e.id << ' ' << e.home_model << ' ' << std::quoted(e.home_name) << '\n';
  for (unsigned v = 0; v < 4; v++) {
    if (e.atlases[v])
      out << "KIT_ATLAS " << e.id << ' ' << v << " 1\n";
    const auto model = e.models[v];
    if (model[0] >= 0)
      out << "KIT_MODEL " << e.id << ' ' << v << ' ' << model[0] << ' ' << model[1] << '\n';
    for (unsigned c = 0; c < 21; c++)
      if (e.colors[v][c] >= 0)
        out << "KIT_COLOR " << e.id << ' ' << v << ' ' << c << ' ' << e.colors[v][c] << '\n';
    for (unsigned slot = 0; slot < 8; slot++)
      if (e.designs[v][slot] >= 0)
        out << "KIT_DESIGN " << e.id << ' ' << v << ' ' << slot << ' ' << e.designs[v][slot]
            << '\n';
    if (e.crests[v] > 0)
      out << "KIT_CREST " << e.id << ' ' << v << ' ' << e.crests[v] << '\n';
    if (e.lettering[v].Enabled()) {
      out << "KIT_LETTERING_V2 " << e.id << ' ' << v;
      for (auto value : e.lettering[v].values)
        out << ' ' << value;
      out << '\n';
    }
  }
  out.flush();
  if (!out) {
    e.message = "No se pudo guardar el equipo";
    return false;
  }
  out.close();
  if (!MoveFileExW(temporary.c_str(), path.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    e.message = "No se pudo guardar el equipo";
    return false;
  }

  return true;
}
void TeamApplyIdentity(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  pes6_team_info::models[e.id] = e.models;
  pes6_team_info::colors[e.id] = e.colors;
  pes6_team_info::designs[e.id] = e.designs;
  pes6_team_info::atlases[e.id] = e.atlases;
  pes6_team_info::RefreshOverrides(e.id);
  auto &home = pes6_team_info::homes[e.id];
  home.model = e.home_model;
  if (home.name.value != e.home_name) {
    home.name.value = e.home_name;
    home.name.address = 0;
  }
  pes6_team_info::lettering[e.id] = e.lettering;
  pes6::crest::modes[e.id] = e.crests;
  for (unsigned part = 0; part < 2; part++) {
    auto &text = pes6_team_info::teams[e.id][part];
    const auto value = part ? e.short_name : e.name;
    if (text.value != value) {
      text.value = value;
      text.address = 0;
    }
    if (auto address = pes6_team_info::Address(text, ctx, base))
      REX_STORE_U32(0x82D55218u + e.id * 24 + part * 4, address);
  }
}
std::vector<std::pair<std::string, std::string>> TeamRows(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  auto &squad = TeamSquad(base, e.id);
  std::vector<std::pair<std::string, std::string>> rows;
  switch (e.page) {
  case TeamPage::Menu:
    for (const auto label :
         {"Nombre del equipo", "Gestión del equipo", "Dorsales",
          "Equipaciones", "Guardar y volver"})
      rows.emplace_back(label, "");
    break;
  case TeamPage::Squad:
  case TeamPage::Numbers:
    for (unsigned i = 0; i < TeamCount(squad); i++)
      rows.emplace_back(TeamPlayer(ctx, base, squad.players[i], e.id),
                        squad.players[i] ? std::to_string(e.page == TeamPage::Numbers &&
                            e.editing && i == e.selected && e.number_draft >= 0
                                ? e.number_draft : squad.numbers[i]) : "-");
    break;
  case TeamPage::TransferSquad:
    rows.emplace_back("+ Agregar", "");
    for (unsigned i = 0; i < TeamCount(squad); i++)
      rows.emplace_back(TeamPlayer(ctx, base, squad.players[i], e.id),
                        squad.players[i] ? std::to_string(squad.numbers[i]) : "-");
    break;
  case TeamPage::TransferLeague:
    for (const auto &league : team_leagues)
      rows.emplace_back(league.name, "");
    break;
  case TeamPage::TransferTeam: {
    const auto &league = team_leagues[std::min<unsigned>(e.transfer_league, team_leagues.size() - 1)];
    for (unsigned i = league.begin; i < league.end; i++)
      rows.emplace_back(TeamName(base, i), "");
    break;
  }
  case TeamPage::CopyFormation:
    for (unsigned i = 0; i < 189; i++)
      rows.emplace_back(TeamName(base, i), std::to_string(i));
    break;
  case TeamPage::TransferPlayer: {
    const auto &target = TeamSquad(base, e.target);
    for (unsigned i = 0; i < TeamCount(target); i++)
      rows.emplace_back(TeamPlayer(ctx, base, target.players[i], e.target), std::to_string(target.numbers[i]));
    break;
  }
  case TeamPage::Formation:
    rows.emplace_back("Copiar formación de equipo", "");
    for (unsigned i = 0; i < 11; i++)
      rows.emplace_back(std::to_string(i + 1) + " " + TeamPlayer(ctx, base, squad.players[i], e.id), "");
    break;
  case TeamPage::Position:
    if (e.slot) {
      rows.emplace_back("Posición nativa", std::to_string(squad.formation[e.slot - 1]));
      rows.emplace_back("Profundidad", std::to_string(squad.formation[9 + e.slot]));
      rows.emplace_back("Anchura", std::to_string(squad.formation[19 + e.slot]));
    }
    rows.emplace_back("Dirección de ataque", std::to_string(squad.formation[30 + e.slot]));
    rows.emplace_back("Defensa", std::to_string(squad.formation[41 + e.slot]));
    break;
  case TeamPage::Roles: {
    constexpr std::array<const char *, 6> names{"Falta 1",        "Falta 2", "Córner izquierdo",
                                                "Córner derecho", "Penalti", "Capitán"};
    for (unsigned i = 0; i < 6; i++)
      rows.emplace_back(names[i],
                        TeamPlayer(ctx, base, squad.players[std::min<unsigned>(squad.roles[i], 31)], e.id));
    break;
  }
  case TeamPage::Kits:
    for (const auto label :
         {"Local", "Visitante", "Arquero", "Arquero suplente"})
      rows.emplace_back(label, "");
    break;
  case TeamPage::Kit:
    rows = {{"Modelo y cuello", ""},
            {"Diseño del uniforme", ""},
            {"Colores", ""},
            {"Números y nombre", ""},
            {"Restaurar uniforme original", ""}};
    break;
  case TeamPage::KitModel: {
    const auto &model = e.models[e.variant];
    rows.emplace_back("Modelo", std::to_string(model[0] >= 0 ? model[0] : OriginalKitByte(101)));
    rows.emplace_back("Cuello", std::to_string(model[1] >= 0 ? model[1] : OriginalKitByte(84)));
    constexpr std::array<const char *, 3> crest_modes{"Original", "Escudo del juego",
                                                      "Escudo en la textura"};
    rows.emplace_back("Escudo del pecho", crest_modes[std::clamp(e.crests[e.variant], 0, 2)]);
    break;
  }
  case TeamPage::KitDesign: {
    constexpr std::array<const char *, 7> labels{"Camiseta · capa 1", "Camiseta · capa 2",
                                                 "Camiseta · capa 3", "Camiseta · capa 4",
                                                 "Pantalón",          "Medias",
                                                 "Brazalete"};
    for (unsigned i = 1; i < 8; i++) {
      const auto value = e.designs[e.variant][i];
      rows.emplace_back(
          labels[i - 1],
          std::to_string(value < 0 ? OriginalKitByte(pes6_team_info::design_offsets[i]) : value));
    }
    break;
  }
  case TeamPage::KitColors: {
    constexpr std::array<const char *, 21> labels{
        "Camiseta · base",        "Camiseta · capa 4",
        "Camiseta · capa 3",      "Camiseta · capa 2",
        "Camiseta · capa 1",      "Pantalón · base",
        "Pantalón · color 2",     "Pantalón · color 3",
        "Pantalón · color 4",     "Medias · base",
        "Medias · color 2",       "Medias · color 3",
        "Brazalete · base",       "Brazalete · color 2",
        "Brazalete · color 3",    "Nombre · contorno",
        "Nombre · color",         "Dorsal · contorno",
        "Dorsal · color",         "Pantalón · número contorno",
        "Pantalón · número color"};
    for (unsigned i = 0; i < 21; i++) {
      const auto value = e.colors[e.variant][i];
      const auto color = value >= 0 ? unsigned(value) : OriginalKitColor(i);
      const unsigned r5 = (color >> 11) & 31, g5 = (color >> 6) & 31, b5 = (color >> 1) & 31;
      const unsigned r8 = (r5 * 255 + 15) / 31, g8 = (g5 * 255 + 15) / 31, b8 = (b5 * 255 + 15) / 31;
      const char *cname = "Color";
      if (r5 <= 3 && g5 <= 3 && b5 <= 3) cname = "Negro";
      else if (r5 >= 27 && g5 >= 27 && b5 >= 27) cname = "Blanco";
      else if (std::abs(int(r5) - int(g5)) <= 3 && std::abs(int(g5) - int(b5)) <= 3) cname = "Gris";
      else if (r5 >= g5 + 8 && r5 >= b5 + 8) cname = g5 >= 14 ? "Naranja" : "Rojo";
      else if (r5 >= 20 && g5 >= 20 && b5 <= 10) cname = "Amarillo";
      else if (g5 >= r5 + 6 && g5 >= b5 + 4) cname = "Verde";
      else if (b5 >= r5 + 6 && b5 >= g5 + 4) cname = (r5 >= 10 && g5 >= 16) ? "Celeste" : "Azul";
      else if (r5 >= 16 && b5 >= 16 && g5 <= 10) cname = "Morado";
      else if (r5 >= 20 && g5 >= 12 && b5 >= 16) cname = "Rosa";
      char hex[16];
      std::snprintf(hex, sizeof(hex), "#%02X%02X%02X", r8, g8, b8);
      rows.emplace_back(labels[i], std::string(cname) + " " + hex + " (" + std::to_string(r5) + "/" +
                                       std::to_string(g5) + "/" + std::to_string(b5) + ")");
    }
    break;
  }
  case TeamPage::KitLettering: {
    const auto &values = e.lettering[e.variant].values;
    const auto number = std::find_if(pes6::teams::numbers.begin(), pes6::teams::numbers.end(),
                                     [&](const auto &style) { return style.id == values[0]; });
    const auto name = std::find_if(pes6::teams::names.begin(), pes6::teams::names.end(),
                                   [&](const auto &style) { return style.id == values[1]; });
    rows.emplace_back("Tipo de números",
                      number == pes6::teams::numbers.end() ? "Original" : number->name);
    rows.emplace_back("Fuente del nombre",
                      name == pes6::teams::names.end() ? "Original" : name->name);
    constexpr std::array<const char *, 6> labels{
        "Dorsal · color",          "Dorsal · contorno",
        "Pantalón · número color", "Pantalón · número contorno",
        "Nombre · color",          "Nombre · contorno"};
    for (unsigned i = 2; i < 8; i++) {
      const auto color = values[i];
      rows.emplace_back(labels[i - 2], color < 0 ? "Original"
                                                 : std::to_string(color >> 11 & 31) + " / " +
                                                       std::to_string(color >> 6 & 31) + " / " +
                                                       std::to_string(color >> 1 & 31));
    }
    rows.emplace_back("Vincular nombre y dorsal", values[8] ? "Sí" : "No");
    break;
  }
  case TeamPage::Identity:
    rows = {{"Nombre", e.name},
            {"Abreviatura", e.short_name},
            {"Estadio local", e.home_model < 0 ? "Original" : e.home_name},
            {"Nombre del estadio", e.home_name}};
    break;
  case TeamPage::Text:
    rows = {{"Texto", e.edit_text}, {"Carácter", std::to_string(e.character + 1)}, {"Aceptar", ""}};
    break;
  }
  return rows;
}
void TeamFooter(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  if (e.page == TeamPage::TransferSquad)
    e.text_cache[0] = std::string(e.selected == 0 ? "A Agregar jugador   B Volver" : "A Traspasar jugador   Y Quitar   B Volver") +
        (transfer_entry_mode ? "   X Guardar y volver" : "");
  else if (e.page == TeamPage::TransferLeague || e.page == TeamPage::TransferTeam)
    e.text_cache[0] = "A Elegir   B Volver";
  else if (e.page == TeamPage::TransferPlayer)
    e.text_cache[0] = "A Agregar a tu equipo   B Volver";
  const auto found = e.text_cache.find(0);
  if (found == e.text_cache.end()) return;
  // Keep the native single-line bar and its built-in horizontal scrolling.
  const auto help = e.message.empty() ? found->second : e.message + "   |   " + found->second;
  const auto length = std::min<size_t>(help.size(), 1023);
  std::memcpy(base + e.text, help.data(), length);
  base[e.text + length] = 0;
  const auto footer = Native(ctx, base, sub_8272DEF0);
  if (footer) Native(ctx, base, sub_8272DA10, footer, e.text);
}
bool TeamPreviewPage() {
  const auto page = team_editor.page;
  return page == TeamPage::Kits || page == TeamPage::Kit || page == TeamPage::KitModel || page == TeamPage::KitDesign ||
         page == TeamPage::KitColors || page == TeamPage::KitLettering;
}
bool TeamCompactSubPage() {
  const auto page = team_editor.page;
  return page == TeamPage::Identity || page == TeamPage::Text || page == TeamPage::Numbers ||
         page == TeamPage::Kits || page == TeamPage::Kit || page == TeamPage::KitModel ||
         page == TeamPage::KitDesign || page == TeamPage::KitColors || page == TeamPage::KitLettering;
}
uint32_t EditArtworkTexture(PPCContext &ctx, uint8_t *base, unsigned index, uint32_t text) {
  static std::array<uint32_t, edit_artwork_count> textures{};
  static std::array<bool, edit_artwork_count> attempted{};
  if (index >= textures.size()) return 0;
  if (attempted[index]) return textures[index];
  attempted[index] = true;
  std::ifstream file(AssetRoot() / "data/editor/coaches" /
                     ((index < 9 ? "0" : "") + std::to_string(index + 1) + ".we"), std::ios::binary | std::ios::ate);
  constexpr unsigned size = 16 + 1024 * 1024 * 4;
  if (!file || file.tellg() != size) return 0;
  const auto data = Native(ctx, base, sub_826D61B0, size);
  if (!data) return 0;
  file.seekg(0); file.read(reinterpret_cast<char *>(base + data), size);
  if (file && !std::memcmp(base + data, "WE00", 4) && base[data + 6] == 3 && base[data + 7] == 32 &&
      REX_LOAD_U16(data + 8) == 1024 && REX_LOAD_U16(data + 10) == 1024 && REX_LOAD_U32(data + 12) == 16) {
    const auto name = "pes6_edit_artwork_" + std::to_string(index);
    std::memcpy(base + text, name.c_str(), name.size() + 1);
    Pes6MenuRgbaScope rgba_scope;
    textures[index] = Native(ctx, base, sub_82736390, data, text);
    if (textures[index]) {
      const auto gpu = REX_LOAD_U32(textures[index] + 36);
      if (Valid(base, gpu, 56)) Native(ctx, base, sub_8218FB18, gpu, 0, 3);
    }
  }
  Native(ctx, base, sub_8280E7B8, data);
  return textures[index];
}
void TeamCoach(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  if (!e.coach_root) return;
  const bool show = !TeamPreviewPage();
  const auto now = GetTickCount64();
  const uint32_t screen = transfer_entry_mode ? 0x62000000u : CoachSessionScreen();
  const int artwork = transfer_entry_mode ? transfer_coach_index : int(coach_rotation.index);
  const bool changed = e.coach_bound_index != artwork;
  base[e.coach_root + 53] = show;
  pes6_edit_effects(ctx, base, e.root, screen);
  if (REX_LOAD_U32(e.coach_root + 68) != 2) pes6_edit_foreground_tree(base, e.coach_root);
  if (!show) return;
  const auto texture = EditArtworkTexture(ctx, base, unsigned(artwork), e.text);
  Show(e.coach, base, texture != 0);
  if (!texture) return;
  if (changed) {
    Native(ctx, base, sub_82733030, e.coach.primitive, texture);
    e.coach_bound_index = artwork;
  }
  const auto alpha = coach_rotation.Alpha(now);
  if (changed || e.coach_alpha != alpha) {
    const auto color = 0xffffff00u | alpha;
    // The visible background ends at y232. No floating lower crop edge.
    Rect(base, e.coach, 172, -224, 340, 464, color, color);
    e.coach_alpha = alpha;
  }
}
void EditArtworkClose(PPCContext &ctx, uint8_t *base) {
  if (edit_artwork.root) Native(ctx, base, sub_82735EC8, edit_artwork.root);
  if (edit_artwork.text) Native(ctx, base, sub_8280E7B8, edit_artwork.text);
  edit_artwork = {};
}
bool EditArtworkOpen(PPCContext &ctx, uint8_t *base) {
  auto &e = edit_artwork;
  if (e.root) return true;
  if (!e.text) e.text = Allocate(ctx, base, 1024);
  const auto resource = Native(ctx, base, sub_8272A848, 0x820c77ecu);
  if (!resource || !e.text) return false;
  if (!REX_LOAD_U32(resource + 40))
    Native(ctx, base, sub_8272A9C0, resource);
  e.root = Native(ctx, base, sub_8272FA80, resource, 0, 0, 0xffffffffu);
  if (!e.root) return false;
  static uint32_t blank = 0;
  if (!blank) blank = Allocate(ctx, base, 24);
  const auto geometry = REX_LOAD_U32(e.root + 140);
  if (!blank || !Valid(base, geometry, 24)) { EditArtworkClose(ctx, base); return false; }
  std::memcpy(base + blank, base + geometry, 24);
  REX_STORE_U16(blank, 0);
  REX_STORE_U32(e.root + 140, blank); base[e.root + 52] |= 0x23;
  HideVector(base, e.root, 312); HideVector(base, e.root, 280);
  for (unsigned i = 0; i < 3; i++) {
    const auto page = Native(ctx, base, sub_8272F650, e.root, i);
    if (Valid(base, page, 54)) base[page + 53] = 0;
  }
  e.image = NewQuad(ctx, base, e.root, true);
  if (!e.image.primitive) { EditArtworkClose(ctx, base); return false; }
  return true;
}
void EditArtworkDraw(PPCContext &ctx, uint8_t *base, int requested, bool selecting = false) {
  auto &e = edit_artwork;
  const auto now = GetTickCount64();
  if (requested < 0) {
    if (e.root) base[e.root + 53] = 0;
    e.kind = -1;
    return;
  }
  if (!EditArtworkOpen(ctx, base)) return;
  const bool cycle = requested == coach_artwork_request;
  const bool session_player = requested >= 7 && requested < int(7 + player_portrait_count);
  uint32_t screen = 0x400u + unsigned(requested);
  if (cycle) screen = CoachSessionScreen();
  else if (requested == transfer_coach_index) screen = 0x62000000u;
  else if (session_player) screen = PlayerSessionScreen();
  else if (requested == 5) screen = 0x405u;
  else if (selecting) screen = selector_art_screen;
  const auto kind = cycle ? int(coach_rotation.index) : requested;
  const auto foreground = cycle || selecting ? selector_art_foreground : menu_art_foreground;
  pes6_edit_effects(ctx, base, foreground ? foreground : e.root, screen);
  if (REX_LOAD_U32(e.root + 68) != 2) pes6_edit_foreground_tree(base, e.root);
  const auto texture = EditArtworkTexture(ctx, base, unsigned(kind), e.text);
  base[e.root + 53] = 1; Show(e.image, base, texture != 0);
  if (!texture) return;
  if (e.kind != kind) {
    e.kind = kind;
    e.drawn_alpha = 256;
    e.changed = now;
    if (cycle) e.changed = coach_rotation.entered;
    else if (session_player) e.changed = player_session_entered;
    Native(ctx, base, sub_82733030, e.image.primitive, texture);
  }
  const auto frame = (now - e.changed) * 60 / 1000;
  const auto alpha = 255 - team_coach_fade_mask[frame < 40 ? unsigned(frame) : 39];
  const auto color = 0xffffff00u | alpha;
  const float width = cycle || requested == transfer_coach_index ? 340.f : 592.f;
  // Once the fade settles the quad is static. Avoid dirtying/uploading its
  // vertices every frame; kind changes invalidate this cache above.
  if (e.drawn_alpha != alpha || e.drawn_width != width) {
    Rect(base, e.image, 512 - width, -224, width, 464, color, color);
    e.drawn_alpha = alpha;
    e.drawn_width = width;
  }
}
void TeamSelectorArtworkFrame(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  const auto selector = REX_LOAD_U32(owner + 172);
  const unsigned phase = Valid(base, selector, 8) ? base[selector + 6] : 0;
  // 0/1 are transitional: keep the last logical screen until ready.
  if (phase == 2) selector_art_screen = 0x300;
  else if (phase == 3 || phase == 4) selector_art_screen = 0x301;
  else if (phase >= 5) selector_art_screen = 0x302;
  selector_player_portrait = phase < 5 && base[owner + 216] == 1;
  selector_art_foreground = REX_LOAD_U32(owner + 192);
  pes6_edit_effects(ctx, base, selector_art_foreground, transfer_entry_mode ? 0x62000000u : team_entry_mode ? CoachSessionScreen() : PlayerSessionScreen());
  for (const auto offset : team_roots) {
    const auto node = REX_LOAD_U32(owner + offset);
    if (Valid(base, node, 224) && REX_LOAD_U32(node + 68) != 2) pes6_edit_foreground_tree(base, node);
  }
  const auto root = REX_LOAD_U32(owner + 200);
  if (!Valid(base, root, 144)) return;
  const auto source = REX_LOAD_U32(root + 140);
  static uint32_t geometry = 0;
  if (source == geometry || !Valid(base, source, 24)) return;
  const unsigned count = REX_LOAD_U16(source);
  const auto table = REX_LOAD_U32(source + 12);
  if (!count || count > 128 || !Valid(base, table, count * 4)) return;
  // The name/emblem strip is a separate native root. Preserve its rounded
  // right cap rather than covering its extension with the coach portrait.
  float right = -10000;
  const float origin = ReadFloat(base, root + 16);
  for (unsigned i = 0; i < count; i++) {
    const auto mesh = REX_LOAD_U32(table + i * 4);
    if (!Valid(base, mesh, 28)) return;
    const unsigned n = REX_LOAD_U16(mesh + 14); const auto points = REX_LOAD_U32(mesh + 16);
    if (n > 256 || !Valid(base, points, n * 6)) return;
    const float x = origin + float(int16_t(REX_LOAD_U16(mesh + 8))) / 16;
    for (unsigned v = 0; v < n; v++) right = std::max(right, x + float(int16_t(REX_LOAD_U16(points + v * 6))) / 16);
  }
  if (right <= 112) return;
  if (!geometry) geometry = Allocate(ctx, base, 131072);
  if (!geometry) return;
  const auto pointers = geometry + 32; auto cursor = pointers + count * 4;
  std::memcpy(base + geometry, base + source, 24); REX_STORE_U32(geometry + 12, pointers);
  for (unsigned i = 0; i < count; i++) {
    const auto mesh = REX_LOAD_U32(table + i * 4), copy = cursor; cursor += 28;
    const unsigned n = REX_LOAD_U16(mesh + 14); const auto points = REX_LOAD_U32(mesh + 16);
    if (cursor + n * 6 > geometry + 131072) return;
    std::memcpy(base + copy, base + mesh, 28); REX_STORE_U32(pointers + i * 4, copy);
    std::memcpy(base + cursor, base + points, n * 6); REX_STORE_U32(copy + 16, cursor);
    const float x = origin + float(int16_t(REX_LOAD_U16(mesh + 8))) / 16;
    for (unsigned v = 0; v < n; v++) {
      const int old = int16_t(REX_LOAD_U16(cursor + v * 6));
      if (x + float(old) / 16 >= right - 24)
        REX_STORE_U16(cursor + v * 6, uint16_t(old - int((right - 112) * 16)));
    }
    cursor = (cursor + n * 6 + 3) & ~3u;
  }
  REX_STORE_U32(root + 140, geometry); base[root + 52] |= 0x23;
}
uint16_t TeamPreviewPlayer(uint8_t *base) {
  auto &e = team_editor;
  if (!e.redraw && e.preview_players[e.variant]) return e.preview_players[e.variant];
  const auto &squad = TeamSquad(base, e.id);
  std::array<pes6::team_presentation::Player, 32> players{};
  const auto db = REX_LOAD_U32(0x83139440u);
  for (unsigned i = 0; i < players.size(); i++) {
    const auto id = squad.players[i];
    const auto record = db + unsigned(id) * 152;
    if (!db || !id || id >= 4210 || !Valid(base, record, 152)) continue;
    auto &p = players[i]; p.id = id; p.keeper = (base[record + 73] & 15) == 0;
    for (unsigned a = 0; a < p.ability.size(); a++) p.ability[a] = base[record + 74 + a] >> 1;
  }
  e.preview_players = pes6::team_presentation::PreviewPlayers(players, squad.configuration);
  if (squad.roles[5] < 32) {
    const auto cap_id = squad.players[squad.roles[5]];
    if (cap_id && cap_id < 4210) {
      e.preview_players[1] = cap_id;
      e.preview_players[3] = cap_id;
    }
  }
  return e.preview_players[e.variant];
}
void TeamPreview(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  const bool visible = !e.management && TeamPreviewPage();
  if (!visible) {
    if (e.preview_active) { preview::Visible(false); preview::Leave(ctx, base); e.preview_active = false; e.preview_player = 0; }
    preview::Tick(ctx, base);
    return;
  }
  if (e.page == TeamPage::Kits) e.variant = pes6::team_presentation::kit_variants[std::min(e.selected, 3u)];
  const auto player = TeamPreviewPlayer(base);
  if (!player) return;
  uint64_t signature = 1469598103934665603ull;
  const auto hash = [&](unsigned value) { signature = (signature ^ value) * 1099511628211ull; };
  hash(e.variant);
  for (auto value : e.models[e.variant]) hash(unsigned(value));
  for (auto value : e.colors[e.variant]) hash(unsigned(value));
  for (auto value : e.designs[e.variant]) hash(unsigned(value));
  for (auto value : e.lettering[e.variant].values) hash(unsigned(value));
  hash(unsigned(e.crests[e.variant])); hash(e.atlases[e.variant]);
  hash(player);
  const uint64_t prev_sig = e.preview_signature;
  const bool player_changed = !e.preview_active || e.preview_player != player;
  if (e.preview_signature != signature) {
    pes6_team_info::models[e.id] = e.models;
    pes6_team_info::colors[e.id] = e.colors;
    pes6_team_info::designs[e.id] = e.designs;
    pes6_team_info::lettering[e.id] = e.lettering;
    pes6_team_info::atlases[e.id] = e.atlases;
    pes6::crest::modes[e.id] = e.crests;
    pes6_team_info::RefreshOverrides(e.id);
    pes6_team_info::PatchModels(base, e.id, TeamKit(base));
    pes6_team_info::PatchColors(base, e.id, TeamKit(base));
    if (!player_changed && prev_sig != 0) {
      preview::TeamKit(ctx, base, e.id, e.variant);
      preview::Changed(ctx, base, true);
    }
    e.preview_signature = signature;
  }
  if (!e.preview_active) {
    preview::Enter(ctx, base, player, int(e.id), e.variant);
    e.preview_active = true;
    e.preview_player = player;
  } else if (e.preview_player != player) {
    preview::TeamKit(ctx, base, e.id, e.variant);
    preview::SwitchPlayer(player);
    e.preview_player = player;
  }
  preview::Container(e.root); preview::Frame(6); preview::Visible(true);
  preview::Tick(ctx, base);
}
bool IsTransferPage(TeamPage page) {
  return page == TeamPage::TransferSquad || page == TeamPage::TransferLeague ||
         page == TeamPage::TransferTeam || page == TeamPage::TransferPlayer;
}
std::pair<const char *, uint32_t> PlayerPosBadge(PPCContext &ctx, uint8_t *base, uint16_t player) {
  if (!player) return {"+", 0x2068c8ffu};
  struct PosInfo { uint32_t id; const char *abbr; uint32_t color; };
  static constexpr std::array<PosInfo, 12> table{{
      {0, "PT", 0xd4a017ffu}, {2, "LIB", 0x1f8ea8ffu}, {3, "CT", 0x1f8ea8ffu},
      {4, "LAT", 0x1f8ea8ffu}, {5, "MCD", 0x3ea33affu}, {6, "CA", 0x3ea33affu},
      {7, "CC", 0x3ea33affu}, {8, "INT", 0x3ea33affu}, {9, "MP", 0x3ea33affu},
      {10, "EX", 0xc93b2bffu}, {11, "SD", 0xc93b2bffu}, {12, "DC", 0xc93b2bffu},
  }};
  for (const auto &p : table) {
    if (Native(ctx, base, sub_82763DD8, player, p.id) & 0xFFu)
      return {p.abbr, p.color};
  }
  return {"CC", 0x3ea33affu};
}
uint32_t NativePosBadgeTexture(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  // 822E8160 uses owner+196, sprite[0] at +296 for the native position
  // atlas. The roster form lookup can return a row without this sprite.
  // Resolve from the current owner's resource rather than retaining a texture
  // handle after that selector has been destroyed.
  const auto detail = Valid(base, owner, 200) ? REX_LOAD_U32(owner + 196) : 0;
  const auto sprite = Valid(base, detail, 308) ? VectorItem(base, detail, 296, 0) : 0;
  const auto prim = Valid(base, sprite, 148) ? VectorItem(base, sprite, 136, 0) : 0;
  if (!Valid(base, prim, 168)) return 0;
  // Original resource sprites bind their atlas at +160. +164 is an optional
  // runtime override (827331A0 checks it first), and may be zero here.
  const auto override_texture = REX_LOAD_U32(prim + 164);
  return override_texture ? override_texture : REX_LOAD_U32(prim + 160);
}
void ApplyPosBadge(PPCContext &ctx, uint8_t *base, Quad q, uint32_t tex, uint16_t pid,
                   float x, float y, float w, float h) {
  if (!q.primitive || !tex) return;
  static uint32_t crop = 0;
  if (!crop) crop = Allocate(ctx, base, 8);
  if (!crop) return;
  const auto db = REX_LOAD_U32(0x83139440u);
  const auto rec = db && pid && pid < 4210 ? db + unsigned(pid) * 152 : 0;
  const uint32_t pos_id = rec && Valid(base, rec + 72, 2) ? (REX_LOAD_U16(rec + 72) & 0xFu) : 7u;
  // Match 8273A5A8/8273A4E8's atlas lookup and crop, but apply only
  // the primitive UV setter. Their final 8273A498 call expects a specialized
  // sprite control at widget+160; a NewQuad has no such child controller.
  const auto tile = REX_LOAD_U32(0x8200B5CCu + pos_id * 4);
  REX_STORE_U16(crop, uint16_t((tile >> 3) * 58 + 2));
  REX_STORE_U16(crop + 2, uint16_t((tile & 7) * 32 + 2));
  REX_STORE_U16(crop + 4, 56);
  REX_STORE_U16(crop + 6, 30);
  Native(ctx, base, sub_82733030, q.primitive, tex);
  Native(ctx, base, sub_827331A0, q.primitive, crop);
  Rect(base, q, x, y, w, h, 0xffffffffu, 0xffffffffu);
}
uint32_t PlayerStatValue(PPCContext &ctx, uint8_t *base, uint16_t player, unsigned stat) {
  if (!player) return 0;
  const auto db = REX_LOAD_U32(0x83139440u);
  const auto rec = db ? db + unsigned(player) * 152u : 0u;
  if (rec && Valid(base, rec, 152)) {
    if (stat <= 25) return (base[rec + 74 + stat] >> 1) & 0x7Fu;
    if (stat == 26) return ((REX_LOAD_U32(rec + 100) >> 26) & 7u) + 1u;
    if (stat == 27) return ((REX_LOAD_U32(rec + 100) >> 21) & 7u) + 1u;
    if (stat == 28) return ((REX_LOAD_U32(rec + 100) >> 18) & 7u) + 1u;
    return 0;
  }
  if (stat <= 22) return Native(ctx, base, sub_82786CF0, player, stat, 0) & 0xFFu;
  if (stat <= 28) {
    const uint32_t v = Native(ctx, base, sub_82753B18, player, stat, 0) & 0xFFu;
    return stat >= 26 ? (v + 1u) : v;
  }
  return 0;
}
uint32_t StatColor(uint32_t v) {
  if (v >= 95) return 0xc82020ffu;
  if (v >= 85) return 0xd06000ffu;
  if (v >= 75) return 0x9a7000ffu;
  return 0x181818ffu;
}
void InitTransferUI(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  auto &ui = e.transfer_ui;
  if (ui.ready || !e.root || !e.title) return;
  ui.top_bar = NewQuad(ctx, base, e.root);
  ui.left_panel = NewQuad(ctx, base, e.root);
  ui.mid_panel = NewQuad(ctx, base, e.root);
  ui.right_panel = NewQuad(ctx, base, e.root);
  ui.left_header = NewQuad(ctx, base, e.root);
  ui.mid_header = NewQuad(ctx, base, e.root);
  ui.right_header = NewQuad(ctx, base, e.root);
  ui.nat_hdr = NewQuad(ctx, base, e.root);
  ui.club_hdr = NewQuad(ctx, base, e.root);
  ui.left_team_name = Label(ctx, base, e.root, e.heading);
  ui.right_team_name = Label(ctx, base, e.root, e.heading);
  ui.left_title = Label(ctx, base, e.root, e.title);
  ui.mid_title = Label(ctx, base, e.root, e.title);
  ui.right_count = Label(ctx, base, e.root, e.title);
  ui.nat_lbl = Label(ctx, base, e.root, e.title);
  ui.club_lbl = Label(ctx, base, e.root, e.title);
  ui.pos_tex = NativePosBadgeTexture(ctx, base, e.owner);
  ui.mid_pos_badge = NewQuad(ctx, base, e.root, true);
  for (auto &r : ui.left_rows) {
    r.fill = NewQuad(ctx, base, e.root);
    r.badge = NewQuad(ctx, base, e.root, ui.pos_tex != 0);
    r.pos = Label(ctx, base, e.root, e.heading);
    r.name = Label(ctx, base, e.root, e.title);
  }
  for (auto &s : ui.stat_rows) {
    s.first = Label(ctx, base, e.root, e.title);
    s.second = Label(ctx, base, e.root, e.title);
  }
  for (auto &r : ui.right_rows) {
    r.fill = NewQuad(ctx, base, e.root);
    r.badge = NewQuad(ctx, base, e.root, ui.pos_tex != 0);
    r.pos = Label(ctx, base, e.root, e.heading);
    r.name = Label(ctx, base, e.root, e.title);
  }
  for (auto &g : ui.right_grid) {
    g.border = NewQuad(ctx, base, e.root);
    g.bg = NewQuad(ctx, base, e.root);
    g.icon = NewQuad(ctx, base, e.root, true);
  }
  ui.ready = true;
}
uint32_t TransferLeagueBank(PPCContext &ctx, uint8_t *base) {
  auto &ui = team_editor.transfer_ui;
  if (ui.league_bank) return ui.league_bank;
  const auto res = Native(ctx, base, sub_8272A848, 0x82008350u);
  if (!res) return 0;
  if (!REX_LOAD_U32(res + 40)) Native(ctx, base, sub_8272A9C0, res);
  ui.league_dummy = Native(ctx, base, sub_8272FA80, res, 0, 0, 0xffffffffu);
  if (!ui.league_dummy) return 0;
  base[ui.league_dummy + 53] = 0;
  const auto page = Native(ctx, base, sub_8272F650, ui.league_dummy, 3);
  if (!Valid(base, page, 256)) return 0;
  const auto a = REX_LOAD_U32(page + 248);
  const auto b = Valid(base, a, 4) ? REX_LOAD_U32(a) : 0;
  const auto c = Valid(base, b, 412) ? REX_LOAD_U32(b + 408) : 0;
  const auto w = Valid(base, c, 4) ? REX_LOAD_U32(c) : 0;
  if (Valid(base, w, 168)) ui.league_bank = REX_LOAD_U32(w + 164);
  return ui.league_bank;
}
uint32_t TransferLeagueIconTexture(PPCContext &ctx, uint8_t *base, unsigned league_idx) {
  static constexpr std::array<uint32_t, 12> kLeagueIconIds{
      0x10u, 0x5u, 0x3u, 0x0u, 0x1u, 0x2u, 0x15u, 0x19u, 0x17u, 0x13u, 0x11u, 0x6u};
  static std::array<uint32_t, 12> cache{};
  if (league_idx >= kLeagueIconIds.size()) return 0;
  if (cache[league_idx]) return cache[league_idx];
  const auto bank = TransferLeagueBank(ctx, base);
  if (!bank) return 0;
  const auto res = Native(ctx, base, sub_821A5188, 2, kLeagueIconIds[league_idx], bank);
  if (!res) return 0;
  cache[league_idx] = Native(ctx, base, sub_82736450, res);
  return cache[league_idx];
}
uint32_t TransferEmblemTexture(PPCContext &ctx, uint8_t *base, unsigned team_id, uint32_t text_buf) {
  static std::array<uint32_t, 189> cache{};
  static bool loaded = false;
  static std::vector<uint8_t> pack;
  if (team_id >= cache.size() || !text_buf) return 0;
  if (cache[team_id]) return cache[team_id];
  if (!loaded) {
    loaded = true;
    std::ifstream in("data/patches/premier-clubs/emblems_64x64.pack", std::ios::binary | std::ios::ate);
    if (in && in.tellg() > 16) {
      pack.resize(size_t(in.tellg()));
      in.seekg(0);
      in.read(reinterpret_cast<char *>(pack.data()), pack.size());
      if (!in) pack.clear();
    }
  }
  if (pack.size() < 4 + 189 * 8) return 0;
  uint32_t off = 0, len = 0;
  std::memcpy(&off, pack.data() + 4 + team_id * 8, 4);
  std::memcpy(&len, pack.data() + 4 + team_id * 8 + 4, 4);
  if (!len || uint64_t(off) + len > pack.size()) return 0;
  const auto data = Allocate(ctx, base, len);
  if (!data) return 0;
  std::memcpy(base + data, pack.data() + off, len);
  const auto name = "pes6_tr_emblem_" + std::to_string(team_id);
  std::memcpy(base + text_buf, name.c_str(), name.size() + 1);
  if (pack[off + 7] == 32) {
    Pes6MenuRgbaScope rgba_scope;
    cache[team_id] = Native(ctx, base, sub_82736390, data, text_buf);
  } else {
    cache[team_id] = Native(ctx, base, sub_82736390, data, text_buf);
  }
  if (cache[team_id]) {
    const auto gpu = REX_LOAD_U32(cache[team_id] + 36);
    if (Valid(base, gpu, 56)) Native(ctx, base, sub_8218FB18, gpu, 0, 3);
  }
  Native(ctx, base, sub_8280E7B8, data);
  return cache[team_id];
}
void ShowTransferUI(uint8_t *base, bool show) {
  auto &ui = team_editor.transfer_ui;
  if (!ui.ready) return;
  Show(ui.top_bar, base, show);
  Show(ui.left_panel, base, show);
  Show(ui.mid_panel, base, show);
  Show(ui.right_panel, base, show);
  Show(ui.left_header, base, show);
  Show(ui.mid_header, base, show);
  Show(ui.right_header, base, show);
  Show(ui.nat_hdr, base, show);
  Show(ui.club_hdr, base, show);
  if (ui.left_team_name) base[ui.left_team_name + 53] = show;
  if (ui.right_team_name) base[ui.right_team_name + 53] = show;
  if (ui.left_title) base[ui.left_title + 53] = show;
  if (ui.mid_title) base[ui.mid_title + 53] = show;
  if (ui.right_count) base[ui.right_count + 53] = show;
  if (ui.nat_lbl) base[ui.nat_lbl + 53] = show;
  if (ui.club_lbl) base[ui.club_lbl + 53] = show;
  for (auto &r : ui.left_rows) {
    Show(r.fill, base, show); Show(r.badge, base, show);
    if (r.pos) base[r.pos + 53] = show;
    if (r.name) base[r.name + 53] = show;
  }
  for (auto &s : ui.stat_rows) {
    if (s.first) base[s.first + 53] = show;
    if (s.second) base[s.second + 53] = show;
  }
  for (auto &r : ui.right_rows) {
    Show(r.fill, base, show); Show(r.badge, base, show);
    if (r.pos) base[r.pos + 53] = show;
    if (r.name) base[r.name + 53] = show;
  }
  for (auto &g : ui.right_grid) {
    Show(g.border, base, show);
    Show(g.bg, base, show);
    Show(g.icon, base, show);
  }
}
void TransferDraw(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  InitTransferUI(ctx, base);
  auto &ui = e.transfer_ui;
  if (!ui.ready) return;
  if (e.coach_root && !(transfer_entry_mode && e.page == TeamPage::TransferSquad)) base[e.coach_root + 53] = 0;
  // Hide the native outer container frame underneath using a valid 0-primitive geometry.
  static uint32_t empty_geometry = 0;
  if (!empty_geometry && e.wide_geometry) {
    empty_geometry = Allocate(ctx, base, 64);
    if (empty_geometry) {
      std::memcpy(base + empty_geometry, base + e.wide_geometry, 24);
      REX_STORE_U16(empty_geometry, 0);
    }
  }
  if (empty_geometry && REX_LOAD_U32(e.root + 140) != empty_geometry) {
    REX_STORE_U32(e.root + 140, empty_geometry);
    base[e.root + 52] |= 0x23;
  }
  Show(e.panel, base, false);
  Show(e.scroll_track, base, false);
  Show(e.scroll_thumb, base, false);
  base[e.title + 53] = 0;
  for (auto &cell : e.cells) {
    Show(cell.border, base, false);
    Show(cell.fill, base, false);
    if (cell.label) base[cell.label + 53] = 0;
    if (cell.value) base[cell.value + 53] = 0;
  }
  for (auto arrow : e.arrows) if (arrow) base[arrow + 53] = 0;

  // Single top bar spanning full width (-332..+352, flush with left & right panels)
  if (e.heading) base[e.heading + 53] = 0;
  Show(ui.top_bar, base, true);
  Rect(base, ui.top_bar, -332, -203, 684, 28, 0x383c44f0u, 0x22262cf0u);
  if (ui.left_team_name) {
    base[ui.left_team_name + 53] = 1;
    PlaceText(base, ui.left_team_name, -322, -200, 280, 24);
    FontScale(base, ui.left_team_name, e.font_size * .90f);
    REX_STORE_U32(ui.left_team_name + 48, 0xffffffffu);
    TeamText(ctx, base, ui.left_team_name, TeamFitText(ctx, base, e.name, 280));
  }
  PlaceText(base, e.help, 0, -199, 10, 10);
  TeamText(ctx, base, e.help, "");

  const auto &league = team_leagues[std::min<unsigned>(e.transfer_league, team_leagues.size() - 1)];
  std::string right_hdr;
  if (e.page == TeamPage::TransferLeague) {
    right_hdr = "Seleccionar liga / equipo";
  } else if (e.page == TeamPage::TransferPlayer)
    right_hdr = TeamName(base, e.target);
  if (ui.right_team_name) {
    base[ui.right_team_name + 53] = !right_hdr.empty() ? 1 : 0;
    if (!right_hdr.empty()) {
      const std::string fitted = TeamFitText(ctx, base, right_hdr, 280);
      const float tw = std::min(280.f, TeamTextWidth(ctx, base, fitted) * 0.90f);
      PlaceText(base, ui.right_team_name, 342.f - tw, -200, tw + 8.f, 24);
      FontScale(base, ui.right_team_name, e.font_size * .90f);
      REX_STORE_U32(ui.right_team_name + 48, 0xffffffffu);
      TeamText(ctx, base, ui.right_team_name, fitted);
    }
  }

  // 3 PES 6 light-gray containers below (middle container hidden during league/team selection)
  const bool show_mid = (e.page == TeamPage::TransferSquad || e.page == TeamPage::TransferPlayer);
  const bool show_right_box = (e.page != TeamPage::TransferLeague && e.page != TeamPage::TransferTeam &&
      !(transfer_entry_mode && e.page == TeamPage::TransferSquad));
  Show(ui.left_panel, base, true);
  Show(ui.left_header, base, true);
  Rect(base, ui.left_panel, -332, -163, 214, 298, 0xe9e9e7ffu, 0xd9d9d6ffu);
  Rect(base, ui.left_header, -332, -163, 214, 24, 0xd0d0ceffu, 0xc2c2c0ffu);
  Show(ui.mid_panel, base, show_mid);
  Show(ui.mid_header, base, show_mid);
  if (show_mid) {
    Rect(base, ui.mid_panel, -110, -163, 214, 298, 0xe9e9e7ffu, 0xd9d9d6ffu);
    Rect(base, ui.mid_header, -110, -163, 214, 24, 0xd0d0ceffu, 0xc2c2c0ffu);
  }
  Show(ui.right_panel, base, show_right_box);
  Show(ui.right_header, base, show_right_box);
  if (show_right_box) {
    Rect(base, ui.right_panel, 112, -163, 220, 298, 0xe9e9e7ffu, 0xd9d9d6ffu);
    Rect(base, ui.right_header, 112, -163, 220, 24, 0xd0d0ceffu, 0xc2c2c0ffu);
  }

  const bool left_focused = e.page == TeamPage::TransferSquad;
  const auto &squad = TeamSquad(base, e.id);
  const unsigned left_count = 1 + TeamCount(squad);
  const unsigned left_sel = left_focused ? e.selected : e.transfer_squad_selected;
  const unsigned left_off = left_sel >= 10 ? left_sel - 9 : 0;

  if (ui.left_title) {
    base[ui.left_title + 53] = 1;
    PlaceText(base, ui.left_title, -322, -161, 194, 20);
    FontScale(base, ui.left_title, e.font_size * 0.90f);
    REX_STORE_U32(ui.left_title + 48, 0x181818ffu);
    TeamText(ctx, base, ui.left_title, "Plantilla (" + std::to_string(TeamCount(squad)) + "/32)");
  }

  for (unsigned i = 0; i < 10; i++) {
    auto &r = ui.left_rows[i];
    const unsigned idx = left_off + i;
    const bool vis = idx < left_count;
    Show(r.fill, base, vis); Show(r.badge, base, vis);
    if (r.pos) base[r.pos + 53] = vis;
    if (r.name) base[r.name + 53] = vis;
    if (!vis) continue;
    const float y = -135.f + 26.f * i;
    const bool sel = idx == left_sel;
    const uint32_t bg = sel ? (left_focused ? 0xffc62bffu : 0xc2c2c0ffu)
                            : (i % 2 ? 0xefefedffu : 0xd9d9d6ffu);
    Rect(base, r.fill, -328, y, 206, 24, bg, bg);
    const uint16_t pid = idx == 0 ? 0 : squad.players[idx - 1];
    if (ui.pos_tex && pid) {
      Show(r.badge, base, true);
      ApplyPosBadge(ctx, base, r.badge, ui.pos_tex, pid, -324, y + 3, 34, 18);
      if (r.pos) base[r.pos + 53] = 0;
    } else if (ui.pos_tex && !pid) {
      Show(r.badge, base, false);
      PlaceText(base, r.pos, -324, y + 2, 34, 18);
      FontScale(base, r.pos, e.font_size * 0.90f);
      REX_STORE_U32(r.pos + 48, 0x2068c8ffu);
      TeamText(ctx, base, r.pos, "+");
    } else {
      const auto [abbr, col] = PlayerPosBadge(ctx, base, pid);
      Rect(base, r.badge, -324, y + 3, 34, 18, col, col);
      PlaceText(base, r.pos, -324, y + 2, 34, 18);
      FontScale(base, r.pos, e.font_size * 0.80f);
      REX_STORE_U32(r.pos + 48, 0xffffffffu);
      TeamText(ctx, base, r.pos, abbr);
    }
    PlaceText(base, r.name, -284, y + 2, 158, 20);
    FontScale(base, r.name, e.font_size * 0.90f);
    REX_STORE_U32(r.name + 48, 0x181818ffu);
    TeamText(ctx, base, r.name, TeamFitText(ctx, base, idx == 0 ? "Agregar" : TeamPlayer(ctx, base, pid, e.id), 158));
  }

  // Right Container: Visual League Grid -> Visual Team Emblem Grid -> Player List
  const auto &target = TeamSquad(base, e.target);
  const unsigned right_total =
      e.page == TeamPage::TransferLeague   ? unsigned(team_leagues.size())
      : e.page == TeamPage::TransferTeam   ? (league.end - league.begin)
      : e.page == TeamPage::TransferPlayer ? TeamCount(target)
                                           : 0u;
  std::string right_hdr_title = "Traspasos";
  if (e.page == TeamPage::TransferLeague)
    right_hdr_title = "Ligas y selecciones";
  else if (e.page == TeamPage::TransferTeam)
    right_hdr_title = league.name;
  else if (e.page == TeamPage::TransferPlayer)
    right_hdr_title = std::to_string(e.selected + 1) + "/" + std::to_string(std::max(1u, right_total));
  if (ui.right_count) {
    base[ui.right_count + 53] = show_right_box ? 1 : 0;
    if (show_right_box) {
      PlaceText(base, ui.right_count, 120, -161, 204, 20);
      FontScale(base, ui.right_count, e.font_size * 0.90f);
      REX_STORE_U32(ui.right_count + 48, 0x181818ffu);
      TeamText(ctx, base, ui.right_count, TeamFitText(ctx, base, right_hdr_title, 204));
    }
  }

  const bool is_player_list = e.page == TeamPage::TransferPlayer;
  Show(ui.nat_hdr, base, false);
  Show(ui.club_hdr, base, false);
  if (ui.nat_lbl) base[ui.nat_lbl + 53] = 0;
  if (ui.club_lbl) base[ui.club_lbl + 53] = 0;
  for (auto &g : ui.right_grid) {
    Show(g.border, base, false);
    Show(g.bg, base, false);
    Show(g.icon, base, false);
  }

  const unsigned right_off = (is_player_list && e.selected >= 10) ? e.selected - 9 : 0;
  for (unsigned i = 0; i < 10; i++) {
    auto &r = ui.right_rows[i];
    const unsigned idx = right_off + i;
    const bool vis = is_player_list && idx < right_total;
    Show(r.fill, base, vis);
    Show(r.badge, base, vis);
    if (r.pos) base[r.pos + 53] = vis;
    if (r.name) base[r.name + 53] = vis;
    if (!vis) continue;
    const float y = -135.f + 26.f * i;
    const bool sel = idx == e.selected;
    const uint32_t bg = sel ? 0xffc62bffu : (i % 2 ? 0xefefedffu : 0xd9d9d6ffu);
    Rect(base, r.fill, 116, y, 212, 24, bg, bg);
    const uint16_t pid = target.players[idx];
    if (ui.pos_tex && pid) {
      Show(r.badge, base, true);
      ApplyPosBadge(ctx, base, r.badge, ui.pos_tex, pid, 120, y + 3, 34, 18);
      if (r.pos) base[r.pos + 53] = 0;
    } else {
      const auto [abbr, col] = PlayerPosBadge(ctx, base, pid);
      Rect(base, r.badge, 120, y + 3, 34, 18, col, col);
      PlaceText(base, r.pos, 120, y + 2, 34, 18);
      FontScale(base, r.pos, e.font_size * 0.80f);
      REX_STORE_U32(r.pos + 48, 0xffffffffu);
      TeamText(ctx, base, r.pos, abbr);
    }
    PlaceText(base, r.name, 160, y + 2, 164, 20);
    FontScale(base, r.name, e.font_size * 0.90f);
    REX_STORE_U32(r.name + 48, 0x181818ffu);
    TeamText(ctx, base, r.name, TeamFitText(ctx, base, TeamPlayer(ctx, base, pid, e.target), 164));
  }

  // Middle Container: Active Player's Stats (hidden during league/team selection)
  const uint16_t active_player = (e.page == TeamPage::TransferPlayer)
      ? target.players[e.selected]
      : (left_sel == 0 ? 0 : squad.players[left_sel - 1]);
  if (ui.mid_title) {
    base[ui.mid_title + 53] = show_mid ? 1 : 0;
    if (show_mid) {
      PlaceText(base, ui.mid_title, -102, -161, 198, 20);
      FontScale(base, ui.mid_title, e.font_size * 0.90f);
      REX_STORE_U32(ui.mid_title + 48, 0x181818ffu);
      TeamText(ctx, base, ui.mid_title, active_player ? "Habilidades" : "Selecciona un jugador");
    }
  }
  static constexpr std::array<const char *, 16> stat_names{{
      "Posición", "Ataque", "Defensa", "Balance", "Resistencia",
      "Vel. punta", "Aceleración", "Respuesta", "Agilidad",
      "Prec. regate", "Vel. regate", "Prec. pase c.", "Vel. pase c.",
      "Prec. pase l.", "Vel. pase l.", "Prec. tiro"
  }};
  for (unsigned j = 0; j < 16; j++) {
    auto &sr = ui.stat_rows[j];
    const float sy = -134.f + 16.f * j;
    if (sr.first) {
      base[sr.first + 53] = show_mid ? 1 : 0;
      if (show_mid) {
        PlaceText(base, sr.first, -104, sy, 146, 16);
        FontScale(base, sr.first, e.font_size * 0.80f);
        REX_STORE_U32(sr.first + 48, 0x202020ffu);
        TeamText(ctx, base, sr.first, stat_names[j]);
      }
    }
    if (sr.second) {
      const bool use_native_badge = (j == 0 && show_mid && active_player && ui.pos_tex != 0);
      base[sr.second + 53] = (show_mid && !use_native_badge) ? 1 : 0;
      if (j == 0) {
        Show(ui.mid_pos_badge, base, use_native_badge);
        if (use_native_badge) ApplyPosBadge(ctx, base, ui.mid_pos_badge, ui.pos_tex, active_player, 56, sy - 1, 30, 16);
      }
      if (show_mid && !use_native_badge) {
        PlaceText(base, sr.second, 56, sy, 42, 16);
        FontScale(base, sr.second, e.font_size * 0.80f);
        if (!active_player) {
          REX_STORE_U32(sr.second + 48, 0x707070ffu);
          TeamText(ctx, base, sr.second, "-");
        } else if (j == 0) {
          const auto [abbr, col] = PlayerPosBadge(ctx, base, active_player);
          REX_STORE_U32(sr.second + 48, col);
          TeamText(ctx, base, sr.second, abbr);
        } else {
          const uint32_t val = PlayerStatValue(ctx, base, active_player, j - 1);
          REX_STORE_U32(sr.second + 48, StatColor(val));
          TeamText(ctx, base, sr.second, std::to_string(val));
        }
      }
    }
  }
  TeamFooter(ctx, base);
}
std::string TeamPageTitle(uint8_t *base) {
  switch (team_editor.page) {
  case TeamPage::Numbers: return "Dorsales";
  case TeamPage::Identity: return "Nombre y estadio";
  case TeamPage::KitModel: return "Modelo y cuello";
  case TeamPage::KitDesign: return "Diseño del uniforme";
  case TeamPage::KitColors: return "Colores";
  case TeamPage::KitLettering: return "Números y nombre";
  case TeamPage::Roles: return "Lanzadores y capitán";
  case TeamPage::Position: return "Editar posición";
  case TeamPage::Text: return "Editar texto";
  case TeamPage::TransferSquad: return "Traspasos";
  case TeamPage::TransferPlayer: return TeamName(base, team_editor.target);
  case TeamPage::Squad: return "Plantilla";
  default: return "Selecciona un equipo";
  }
}
void TeamDraw(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  if (IsTransferPage(e.page)) {
    if (transfer_entry_mode && e.page == TeamPage::TransferSquad) TeamCoach(ctx, base);
    else if (e.coach_root) base[e.coach_root + 53] = 0;
  } else {
    TeamCoach(ctx, base);
  }
  TeamPreview(ctx, base);
  if (e.management)
    return;
  // 822E7DF8 publishes the selector's help every frame, before this hook.
  // Re-publish our cached help even when no row/layout refresh is needed.
  if (!e.redraw) {
    TeamFooter(ctx, base);
    return;
  }
  e.redraw = false;
  if (e.root) pes6_edit_foreground_tree(base, e.root);
  const auto rows = TeamRows(ctx, base);
  e.row_count = unsigned(rows.size());
  if (rows.empty())
    return;
  e.selected = std::min(e.selected, unsigned(rows.size() - 1));
  if (IsTransferPage(e.page)) {
    Show(e.cat_top_bar, base, false);
    Show(e.cat_ok_fill, base, false);
    if (e.cat_ok_label) base[e.cat_ok_label + 53] = 0;
    TransferDraw(ctx, base);
    return;
  }
  ShowTransferUI(base, false);
  if (e.heading) base[e.heading + 53] = 1;
  Show(e.panel, base, true);
  const bool menu_card = e.page == TeamPage::Menu;
  const bool compact_sub = TeamCompactSubPage();
  const unsigned cap = compact_sub ? 11u : 9u;
  if (e.selected < e.offset)
    e.offset = e.selected;
  if (e.selected >= e.offset + cap)
    e.offset = e.selected - (cap - 1);
  const bool wide = TeamPreviewPage();
  static uint32_t empty_menu_geometry = 0;
  if ((menu_card || compact_sub) && !empty_menu_geometry && e.wide_geometry) {
    empty_menu_geometry = Allocate(ctx, base, 64);
    if (empty_menu_geometry) {
      std::memcpy(base + empty_menu_geometry, base + e.wide_geometry, 24);
      REX_STORE_U16(empty_menu_geometry, 0);
    }
  }
  const auto geometry = (menu_card || compact_sub) && empty_menu_geometry ? empty_menu_geometry : wide ? e.wide_geometry : e.narrow_geometry;
  if (geometry && REX_LOAD_U32(e.root + 140) != geometry) {
    REX_STORE_U32(e.root + 140, geometry); base[e.root + 52] |= 0x23;
  }
  if (menu_card || compact_sub) {
    const float pw = menu_card ? 320.f : 372.f;
    const float ph = menu_card ? 212.f : 326.f;
    const float okX = menu_card ? -232.f : -206.f;
    const float okY = menu_card ? 24.f : 143.f;
    Rect(base, e.cat_top_bar, -332, -180, pw, 28, 0x383c44f0u, 0x22262cf0u);
    Rect(base, e.panel, -332, -152, pw, ph, 0xe9e9e7ff, 0xd9d9d6ff);
    Show(e.cat_top_bar, base, true);
    PlaceText(base, e.heading, -322, -177, pw - 20.f, 24);
    FontScale(base, e.heading, e.font_size * .90f);
    REX_STORE_U32(e.heading + 48, 0xffffffffu);
    std::string hdr = e.name;
    if (compact_sub) {
      const std::string sub = e.page == TeamPage::Kits ? "Equipaciones"
                            : e.page == TeamPage::Kit ? "Equipación"
                                                      : TeamPageTitle(base);
      hdr += "  -  " + sub;
    }
    TeamText(ctx, base, e.heading, TeamFitText(ctx, base, hdr, pw - 20.f));
    if (e.help) base[e.help + 53] = 0;
    const uint32_t ok_col = e.menu_ok ? 0xffc62bffu : 0xc2c2c0ffu;
    Rect(base, e.cat_ok_fill, okX, okY, 120, 24, ok_col, ok_col);
    Show(e.cat_ok_fill, base, true);
    if (e.cat_ok_label) {
      base[e.cat_ok_label + 53] = 1;
      FontScale(base, e.cat_ok_label, e.font_size * .90f);
      const float ok_w = TeamTextWidth(ctx, base, "OK") * 0.90f;
      PlaceText(base, e.cat_ok_label, okX + 60.f - ok_w * 0.5f, okY + 2.f, 120, 20);
      REX_STORE_U32(e.cat_ok_label + 48, 0x181818ffu);
      TeamText(ctx, base, e.cat_ok_label, "OK");
    }
  } else {
    Show(e.cat_top_bar, base, false);
    Show(e.cat_ok_fill, base, false);
    if (e.cat_ok_label) base[e.cat_ok_label + 53] = 0;
    if (e.help) base[e.help + 53] = 1;
    Rect(base, e.panel, -332, -157, 503, 287, 0xe9e9e7ff, 0xe9e9e7ff);
    const float heading_width = wide ? 400.f : 224.f;
    PlaceText(base, e.heading, -65, -199, heading_width, 25);
    FontScale(base, e.heading, e.font_size * .90f);
    REX_STORE_U32(e.heading + 48, 0xffffffffu);
    TeamText(ctx, base, e.heading, TeamFitText(ctx, base, e.name, heading_width));
    PlaceText(base, e.help, -234 - TeamTextWidth(ctx, base, "OK") * .5f, -199, 195, 25);
    TeamText(ctx, base, e.help, "OK");
  }
  const bool show_scroll = compact_sub && rows.size() > cap;
  Show(e.scroll_track, base, show_scroll);
  Show(e.scroll_thumb, base, show_scroll);
  if (show_scroll) {
    const float scrollH = 284.f;
    const float height = scrollH * float(cap) / float(rows.size());
    const float maxOffset = float(rows.size() - cap);
    Rect(base, e.scroll_track, 32.f, -145.f, 3.f, scrollH, 0xa7a59dffu, 0xa7a59dffu);
    Rect(base, e.scroll_thumb, 32.f, -145.f + (scrollH - height) * float(e.offset) / maxOffset, 3.f, height, 0xe9a900ffu, 0xe9a900ffu);
  }
  const bool categories = e.page == TeamPage::Menu || e.page == TeamPage::Kits || e.page == TeamPage::Kit;
  const bool title_visible = !categories && !compact_sub;
  base[e.title + 53] = title_visible;
  PlaceText(base, e.title, -321, -153, 483, 22);
  TeamText(ctx, base, e.title, TeamFitText(ctx, base,
      TeamPageTitle(base), 483));
  for (auto arrow : e.arrows)
    if (arrow)
      base[arrow + 53] = 0;
  for (unsigned i = 0; i < 11; i++) {
    auto &cell = e.cells[i];
    const auto index = e.offset + i;
    const bool visible = i < cap && index < rows.size();
    Show(cell.border, base, visible);
    Show(cell.fill, base, visible);
    base[cell.label + 53] = visible;
    base[cell.value + 53] = visible && !menu_card;
    if (!visible)
      continue;
    if (menu_card) {
      const float rowY = -145 + 26.f * float(i);
      const bool selected = !e.menu_ok && index == e.selected;
      const uint32_t fill = selected ? 0xffc62bffu : i % 2 ? 0xefefedffu : 0xd9d9d6ffu;
      Rect(base, cell.border, -327, rowY - 1, 310, 25,
           selected ? 0xffffffffu : 0xe9e9e7ffu, selected ? 0xffffffffu : 0xe9e9e7ffu);
      Rect(base, cell.fill, -326, rowY, 308, 23, fill, fill);
      PlaceText(base, cell.label, -318, rowY + 1, 292, 22);
      FontScale(base, cell.label, e.font_size * .88f);
      TeamText(ctx, base, cell.label, TeamFitText(ctx, base, rows[index].first, 292));
      continue;
    }
    if (compact_sub) {
      const float rowY = -145 + 26.f * float(i);
      const bool selected = !e.menu_ok && index == e.selected;
      const uint32_t fill = selected ? 0xffc62bffu : i % 2 ? 0xefefedffu : 0xd9d9d6ffu;
      Rect(base, cell.border, -327, rowY - 1, 352, 24,
           selected ? 0xffffffffu : 0xe9e9e7ffu, selected ? 0xffffffffu : 0xe9e9e7ffu);
      Rect(base, cell.fill, -326, rowY, 350, 22, fill, fill);
      const float label_width = rows[index].second.empty() ? 330.f : 170.f;
      const float value_x = -140.f, value_width = 158.f;
      PlaceText(base, cell.label, -318, rowY + 1, label_width, 20);
      FontScale(base, cell.label, e.font_size * .85f);
      PlaceText(base, cell.value, value_x, rowY + 1, value_width, 20);
      FontScale(base, cell.value, e.font_size * .85f);
      TeamText(ctx, base, cell.label, TeamFitText(ctx, base, rows[index].first, label_width));
      const auto displayed = TeamFitText(ctx, base, rows[index].second, value_width);
      TeamText(ctx, base, cell.value, displayed);
      const bool adjusting = selected && e.editing && TeamScalarPage();
      const bool text_cursor = e.page == TeamPage::Text && index == 1;
      if ((adjusting || text_cursor) && !rows[index].second.empty()) {
        const auto width = TeamTextWidth(ctx, base, displayed) * 0.85f;
        Arrow(ctx, base, e.arrows[0], true, -155.f, rowY + 1.f, true);
        Arrow(ctx, base, e.arrows[1], true, value_x + std::min(width, value_width) + 8.f, rowY + 1.f, true);
      }
      continue;
    }
    FontScale(base, cell.label, e.font_size);
    FontScale(base, cell.value, e.font_size);
    const float y = (title_visible ? -130 : -148) + 29.f * i;
    const bool selected = index == e.selected;
    const uint32_t fill = selected ? 0xffc62bff : i % 2 ? 0xefefedff : 0xd9d9d6ff;
    Rect(base, cell.border, -329, y - 1, 497, 26,
         selected ? 0xffffffff : 0xe9e9e7ff, selected ? 0xffffffff : 0xe9e9e7ff);
    Rect(base, cell.fill, -328, y, 495, 24, fill, fill);
    const float label_width = rows[index].second.empty() ? 480.f : 198.f;
    const float value_x = -97.f, value_width = 240.f;
    PlaceText(base, cell.label, -321, y + 1, label_width, 24);
    PlaceText(base, cell.value, value_x, y + 1, value_width, 24);
    TeamText(ctx, base, cell.label, TeamFitText(ctx, base, rows[index].first, label_width));
    const auto displayed =
        TeamFitText(ctx, base, rows[index].second, value_width);
    TeamText(ctx, base, cell.value, displayed);
    const bool adjusting = selected && e.editing && TeamScalarPage();
    const bool text_cursor = e.page == TeamPage::Text && index == 1;
    if ((adjusting || text_cursor) && !rows[index].second.empty()) {
      const auto width = TeamTextWidth(ctx, base, displayed);
      Arrow(ctx, base, e.arrows[0], true, value_x - 16, y + 1, true);
      Arrow(ctx, base, e.arrows[1], true, value_x + std::min(width, value_width) + 10, y + 1, true);
    }
  }
  const bool formation = e.page == TeamPage::Position;
  for (unsigned i = 0; i < 11; i++) {
    Show(e.markers[i], base, formation);
    if (!formation)
      continue;
    const auto &squad = TeamSquad(base, e.id);
    const float x = i ? float(squad.formation[19 + i]) : 52.f,
                y = i ? float(squad.formation[9 + i]) : 2.f;
    Rect(base, e.markers[i], 170 + x, 5 + 1.7f * y, 6, 6, i == e.slot ? 0xe9b936ff : 0x235c8aff,
         i == e.slot ? 0xe9b936ff : 0x235c8aff);
  }
  std::string help = "A Seleccionar   B Volver   Arr./Ab. Desplazar";
  if (e.page == TeamPage::Squad)
    help = e.swap >= 0 ? "A Intercambiar   B Cancelar"
                       : "A Elegir titular   X Traspasar   Y Quitar   B Volver";
  if (e.page == TeamPage::TransferSquad)
    help = e.selected == 0 ? "A Agregar jugador   B Volver"
                           : "A Traspasar jugador   Y Quitar   B Volver";
  else if (e.page == TeamPage::TransferLeague)
    help = "A Elegir liga   B Volver";
  else if (e.page == TeamPage::TransferTeam)
    help = e.transfer_register ? "A Ver plantilla   B Volver" : "A Traspasar aquí   B Volver";
  else if (e.page == TeamPage::TransferPlayer)
    help = "A Agregar a tu equipo   B Volver";
  if (transfer_entry_mode && e.page == TeamPage::TransferSquad) help += "   X Guardar y volver";
  if (TeamScalarPage())
    help = e.editing ? "Izq./Der. Cambiar   A Confirmar   Y Por defecto   B Cancelar"
                     : "A Editar valor   Y Por defecto   B Volver";
  if (e.editing && (e.page == TeamPage::KitColors || e.page == TeamPage::KitLettering))
    help = "Izq./Der. Cambiar   X Componente RGB · " +
           std::string(e.character == 0   ? "Rojo"
                       : e.character == 1 ? "Verde"
                                          : "Azul") +
           "   A Confirmar   Y Original   B Cancelar";
  if (e.page == TeamPage::Text)
    help = "Izq./Der. Carácter   Arr./Ab. Letra   A Aceptar   X Borrar   Y Añadir   B Cancelar";
  if (TeamPreviewPage()) help += "   LB/RB Girar";
  for (const auto &[plain, icon] :
       std::array<std::pair<const char *, const char *>, 4>{{{"A ", "\x11"
                                                                    "ka "},
                                                             {"B ", "\x11"
                                                                    "kb "},
                                                             {"X ", "\x11"
                                                                    "kg "},
                                                             {"Y ", "\x11"
                                                                    "kh "}}}) {
    size_t at = 0;
    while ((at = help.find(plain, at)) != std::string::npos) {
      if (at && help[at - 1] != ' ') {
        at += 2;
        continue;
      }
      help.replace(at, 2, icon);
      at += 4;
    }
  }
  e.text_cache[0] = help;
  TeamFooter(ctx, base);
}
void TeamChange(PPCContext &ctx, uint8_t *base, int direction) {
  auto &e = team_editor;
  auto &squad = TeamSquad(base, e.id);
  const auto wrap = [&](int value, int maximum) {
    return (value + direction + maximum + 1) % (maximum + 1);
  };
  if (e.page == TeamPage::Identity && e.selected == 2) {
    e.home_model = (e.home_model + 1 + direction + 9) % 9 - 1;
    if (e.home_model < 0)
      e.home_name.clear();
    else {
      const auto address = REX_LOAD_U32(0x82D4FD40u + e.home_model * 4);
      e.home_name = Valid(base, address, 64)
                        ? std::string(reinterpret_cast<char *>(base + address),
                                      strnlen(reinterpret_cast<char *>(base + address), 63))
                        : "Estadio " + std::to_string(e.home_model + 1);
    }
  } else if (e.page == TeamPage::Numbers && squad.players[e.selected]) {
    auto &number = e.number_draft;
    const int current = number <= 99 && number >= 1 ? int(number) : direction > 0 ? 0 : 100;
    const auto next = uint8_t((current - 1 + direction + 99) % 99 + 1);
    number = next;
  } else if (e.page == TeamPage::Roles)
    squad.roles[e.selected] = uint8_t(wrap(squad.roles[e.selected], int(TeamCount(squad)) - 1));
  else if (e.page == TeamPage::Position) {
    unsigned index = 30 + e.slot, maximum = 255;
    if (e.slot && e.selected < 3) {
      index = e.slot - 1 + e.selected * 10;
      maximum = e.selected == 0 ? 40 : e.selected == 1 ? 52 : 104;
    } else if (e.selected == (e.slot ? 4 : 1)) {
      index = 41 + e.slot;
      maximum = 2;
    }
    squad.formation[index] = uint8_t(wrap(squad.formation[index], maximum));
  } else if (e.page == TeamPage::KitModel) {
    auto &model = e.models[e.variant];
    if (e.selected < 2) {
      if (model[0] < 0)
        model = {OriginalKitByte(101), OriginalKitByte(84)};
      model[e.selected] = wrap(model[e.selected], e.selected ? 1 : 61);
      if (e.selected == 0 && e.crests[e.variant] == 1) {
        const auto support = pes6::crest::model_flags[model[0]];
        if (support != 1 && support != 2)
          e.crests[e.variant] = 0;
      }
    } else {
      const auto selected = model[0] < 0 ? OriginalKitByte(101) : model[0];
      const auto support = pes6::crest::model_flags[selected];
      auto next = wrap(e.crests[e.variant], 2);
      if (next == 1 && support != 1 && support != 2)
        next = wrap(next, 2);
      e.crests[e.variant] = next;
    }
  } else if (e.page == TeamPage::KitDesign) {
    e.atlases[e.variant] = false;
    const auto slot = e.selected + 1;
    auto &value = e.designs[e.variant][slot];
    value = wrap(value < 0 ? OriginalKitByte(pes6_team_info::design_offsets[slot]) : value,
                 int(pes6_team_info::design_counts[slot]) - 1);
  } else if (e.page == TeamPage::KitColors) {
    e.atlases[e.variant] = false;
    const auto slot = e.selected;
    auto &selected = e.colors[e.variant][slot];
    unsigned word = selected >= 0 ? unsigned(selected) : OriginalKitColor(slot);
    // X/Y select the component while the row retains its RGB5551 alpha.
    const unsigned shift = e.character == 0 ? 11 : e.character == 1 ? 6 : 1;
    selected =
        int((word & ~(31u << shift)) | (unsigned(wrap(int(word >> shift & 31), 31)) << shift));
  } else if (e.page == TeamPage::KitLettering) {
    auto &values = e.lettering[e.variant].values;
    if (e.selected < 2) {
      const auto choose = [&](const auto &styles) {
        const auto found = std::find_if(styles.begin(), styles.end(), [&](const auto &style) {
          return style.id == values[e.selected];
        });
        const auto index = found == styles.end() ? 0 : int(found - styles.begin());
        return styles[(index + direction + int(styles.size())) % int(styles.size())].id;
      };
      values[e.selected] = e.selected ? choose(pes6::teams::names) : choose(pes6::teams::numbers);
    } else if (e.selected == 8)
      values[8] = !values[8];
    else {
      auto &color = values[e.selected];
      const unsigned shift = e.character == 0 ? 11 : e.character == 1 ? 6 : 1;
      if (color < 0)
        color = 1;
      color = (color & ~(31 << shift)) | (wrap(color >> shift & 31, 31) << shift) | 1;
    }
  }
  Sound(ctx, base, 6);
}
void TeamConfirm(PPCContext &ctx, uint8_t *base) {
  auto &e = team_editor;
  auto &squad = TeamSquad(base, e.id);
  if (TeamScalarPage()) {
    if (!e.editing) {
      e.before_squad = squad;
      if (e.page == TeamPage::Numbers)
        e.number_draft = squad.numbers[e.selected];
      e.before_model = e.models[e.variant];
      e.before_colors = e.colors[e.variant];
      e.before_design = e.designs[e.variant];
      e.before_lettering = e.lettering[e.variant];
      e.before_crest = e.crests[e.variant];
      e.before_atlas = e.atlases[e.variant];
      e.before_home_model = e.home_model;
      e.before_home_name = e.home_name;
    } else if (e.page == TeamPage::Numbers) {
      if (e.number_draft >= 1 && e.number_draft <= 99)
        TeamAssignNumber(squad, e.selected, uint8_t(e.number_draft));
      e.number_draft = -1;
    }
    e.editing = !e.editing;
    return;
  }
  if (e.menu_ok && TeamCompactSubPage()) {
    if (e.page == TeamPage::Text) TeamPageTo(TeamPage::Identity);
    else if (e.page == TeamPage::Kit) TeamPageTo(TeamPage::Kits);
    else if (e.page == TeamPage::KitModel || e.page == TeamPage::KitDesign ||
             e.page == TeamPage::KitColors || e.page == TeamPage::KitLettering)
      TeamPageTo(TeamPage::Kit);
    else TeamPageTo(TeamPage::Menu);
    return;
  }
  if (e.page == TeamPage::Menu) {
    if (e.menu_ok) {
      TeamClose(ctx, base);
      return;
    }
    constexpr std::array<TeamPage, 4> pages{
        TeamPage::Identity, TeamPage::Squad, TeamPage::Numbers,
        TeamPage::Kits};
    if (e.selected < pages.size()) {
      if (e.selected == 1) {
        if (!TeamManagementOpen(ctx, base))
          e.message = "No se pudo abrir la gestión del equipo";
        return;
      }
      pes6_team_info::Load();
      TeamPageTo(pages[e.selected]);
      return;
    }
    if (!pes6::teams::BeginTransaction()) {
      e.message = "No se pudo preparar el guardado";
      return;
    }
    const auto previous = pes6::teams::overrides;
    for (const auto &[id, draft] : e.drafts)
      if (draft != e.originals.at(id))
        pes6::teams::Set(id, draft, &e.originals.at(id));
    if (!pes6::teams::Save() || !TeamSaveIdentity(ctx, base) || !pes6::teams::CommitTransaction()) {
      pes6::teams::overrides = previous;
      pes6::teams::dirty = true;
      e.message = pes6::teams::Recover() ? "Guardado cancelado. Se conservaron los datos anteriores"
                                         : "No se pudo recuperar el guardado";
      return;
    }
    TeamApplyIdentity(ctx, base);
    e.committed = true;
    pes6::teams::Apply(base);
    const auto owner = e.owner;
    TeamClose(ctx, base);
    Native(ctx, base, sub_822E9E00, owner);
    Native(ctx, base, sub_822E8160, owner);
    return;
  }
  if (e.page == TeamPage::Squad) {
    if (!squad.players[e.selected])
      return;
    if (e.swap < 0) {
      e.swap = int(e.selected);
      e.message = "Selecciona la plaza de destino";
      return;
    }
    const unsigned first = unsigned(e.swap), second = e.selected;
    std::swap(squad.players[first], squad.players[second]);
    std::swap(squad.numbers[first], squad.numbers[second]);
    for (auto &role : squad.roles) {
      if (role == first)
        role = uint8_t(second);
      else if (role == second)
        role = uint8_t(first);
    }
    e.swap = -1;
    e.message.clear();
    return;
  }
  if (e.page == TeamPage::TransferSquad) {
    e.transfer_squad_selected = e.selected;
    if (e.selected == 0) {
      e.transfer_register = true;
      TeamPageTo(TeamPage::TransferLeague);
      return;
    }
    const unsigned slot = e.selected - 1;
    if (!squad.players[slot])
      return;
    e.transfer_register = false;
    e.slot = slot;
    TeamPageTo(TeamPage::TransferLeague);
    return;
  }
  if (e.page == TeamPage::TransferLeague) {
    e.transfer_league = e.selected;
    TeamPageTo(TeamPage::TransferTeam);
    return;
  }
  if (e.page == TeamPage::TransferTeam) {
    const auto &league = team_leagues[std::min<unsigned>(e.transfer_league, team_leagues.size() - 1)];
    const unsigned target_id = league.begin + e.selected;
    if (target_id == e.id) {
      e.message = "Selecciona otro equipo";
      return;
    }
    if (e.transfer_register) {
      e.target = target_id;
      TeamPageTo(TeamPage::TransferPlayer);
      return;
    }
    auto &target = TeamSquad(base, target_id);
    const auto player = squad.players[e.slot];
    if (!player) {
      e.message = "Selecciona un jugador de origen";
      return;
    }
    if (TeamCount(squad) <= 11) {
      e.message = "El origen necesita once jugadores";
      return;
    }
    const auto target_count = TeamCount(target);
    if (target_count >= 32) {
      e.message = "Plantilla de destino completa";
      return;
    }
    if (std::find(target.players.begin(), target.players.end(), player) != target.players.end()) {
      e.message = "Jugador ya inscrito en el destino";
      return;
    }
    target.numbers[target_count] = uint8_t(TeamFreeNumber(target, squad.numbers[e.slot]));
    target.players[target_count] = player;
    TeamRemove(squad, e.slot);
    TeamPageTo(TeamPage::TransferSquad);
    e.selected = std::min(e.transfer_squad_selected, TeamCount(squad));
    e.message = "Traspasado a " + TeamName(base, target_id);
    return;
  }
  if (e.page == TeamPage::TransferPlayer) {
    const auto &target = TeamSquad(base, e.target);
    const auto player = target.players[e.selected];
    const auto count = TeamCount(squad);
    if (count >= 32) {
      e.message = "Plantilla completa";
      return;
    }
    if (!player || TeamPlayer(ctx, base, player, e.target).empty()) {
      e.message = "Jugador vacío";
      return;
    }
    if (std::find(squad.players.begin(), squad.players.end(), player) != squad.players.end()) {
      e.message = "Jugador ya inscrito";
      return;
    }
    squad.numbers[count] =
        uint8_t(TeamFreeNumber(squad, target.numbers[e.selected] ? target.numbers[e.selected] : 1));
    squad.players[count] = player;
    TeamPageTo(TeamPage::TransferSquad);
    e.selected = e.transfer_squad_selected;
    e.message = TeamPlayer(ctx, base, player, e.target) + " agregado";
    return;
  }
  if (e.page == TeamPage::Formation) {
    if (!e.selected)
      TeamPageTo(TeamPage::CopyFormation);
    else {
      e.slot = e.selected - 1;
      TeamPageTo(TeamPage::Position);
    }
    return;
  }
  if (e.page == TeamPage::CopyFormation) {
    squad.formation = TeamSquad(base, e.selected).formation;
    TeamPageTo(TeamPage::Formation);
    return;
  }
  if (e.page == TeamPage::Kits) {
    e.variant = pes6::team_presentation::kit_variants[std::min(e.selected, 3u)];
    TeamPageTo(TeamPage::Kit);
    return;
  }
  if (e.page == TeamPage::Kit) {
    if (e.selected == 4) {
      e.models[e.variant] = {-1, -1};
      e.colors[e.variant].fill(-1);
      e.designs[e.variant].fill(-1);
      e.lettering[e.variant] = {};
      e.crests[e.variant] = 0;
      e.atlases[e.variant] = false;
      e.message = "Uniforme original pendiente de guardar";
      return;
    }
    const auto at = TeamKit(base);
    if ((e.selected == 1 || e.selected == 2) && REX_LOAD_U16(at + 104) != 0xffff) {
      e.message = "Este uniforme usa un atlas. Rotulación en Números y nombre";
      return;
    }
    constexpr std::array<TeamPage, 4> pages{TeamPage::KitModel, TeamPage::KitDesign,
                                            TeamPage::KitColors, TeamPage::KitLettering};
    TeamPageTo(pages[e.selected]);
    return;
  }
  if (e.page == TeamPage::Identity) {
    if (e.selected == 3 && e.home_model < 0) {
      e.message = "Selecciona un estadio local";
      return;
    }
    e.short_edit = e.selected == 1;
    e.stadium_edit = e.selected == 3;
    e.edit_text = e.stadium_edit ? e.home_name : e.short_edit ? e.short_name : e.name;
    e.character = 0;
    TeamPageTo(TeamPage::Text);
    return;
  }
  if (e.page == TeamPage::Text) {
    if (e.edit_text.find_first_not_of(' ') == std::string::npos) {
      e.message = "El nombre no puede estar vacío";
      return;
    }
    if (e.stadium_edit)
      e.home_name = e.edit_text;
    else if (e.short_edit)
      e.short_name = e.edit_text;
    else
      e.name = e.edit_text;
    TeamPageTo(TeamPage::Identity);
    return;
  }
}
} // namespace
extern "C" void pes6_team_management_frame(uint8_t *base) { pes6::teams::Apply(base); }
void PlayerRosterClose(PPCContext &ctx, uint8_t *base);
extern "C" void pes6_native_team_leave(PPCContext &ctx, uint8_t *base) {
  EditArtworkClose(ctx, base);
  pes6_edit_effects_close(ctx, base);
  PlayerRosterClose(ctx, base);
  if (team_editor.owner == ctx.r3.u32)
    TeamClose(ctx, base, false);
}
namespace {
bool TransferNativePicked(PPCContext& ctx,uint8_t* base,uint32_t picked){
  if(picked<189){
    auto& e=team_editor;
    REX_STORE_U32(e.owner+208,e.id);
    base[e.root+53]=1;
        TransferSelectorClose(ctx,base);
        if (picked == e.id) {
          Sound(ctx, base, 1);
          e.message = "Selecciona otro equipo";
          e.redraw = true;
          return true;
        }
        e.target = picked;
        if (e.transfer_register) {
          TeamPageTo(TeamPage::TransferPlayer);
        } else {
          auto &squad = TeamSquad(base, e.id);
          auto &target = TeamSquad(base, e.target);
          const uint16_t player = squad.players[e.slot];
          if (TeamCount(squad) <= 11) {
            e.message = "El origen necesita once jugadores";
            TeamPageTo(TeamPage::TransferSquad);
            e.selected = e.transfer_squad_selected;
          } else if (std::find(target.players.begin(), target.players.end(), player) != target.players.end()) {
            e.message = "Jugador ya inscrito en el destino";
            TeamPageTo(TeamPage::TransferSquad);
            e.selected = e.transfer_squad_selected;
          } else if (TeamCount(target) >= 32) {
            e.message = "Plantilla de destino completa";
            TeamPageTo(TeamPage::TransferSquad);
            e.selected = e.transfer_squad_selected;
          } else {
            const unsigned dst_slot = TeamCount(target);
            target.players[dst_slot] = player;
            target.numbers[dst_slot] = uint8_t(TeamFreeNumber(target, squad.numbers[e.slot]));
            TeamRemove(squad, e.slot);
            e.message = TeamPlayer(ctx, base, player, e.id) + " traspasado a " + TeamName(base, e.target);
            TeamPageTo(TeamPage::TransferSquad);
            e.selected = std::min(e.transfer_squad_selected, TeamCount(squad));
          }
        }
        TeamDraw(ctx, base);
        return true;
      }
  return true;
}
}
extern "C" void pes6_transfer_selector_present(PPCContext& ctx,uint8_t* base,uint32_t owner){
  auto& e=team_editor;
  if(e.owner!=owner||!e.transfer_selector_active)return;
  const auto selector=REX_LOAD_U32(owner+172);
  if(!Valid(base,selector,52))return;
  const auto phase=base[selector+6];
  const bool teams=phase==3||phase==4;
  // The completed team grid remains resident when returning to leagues.
  // Its parent is shared, so hide this child explicitly during league choice.
  const auto grid=REX_LOAD_U32(selector+20);
  if(Valid(base,grid,54))base[grid+53]=teams;
  for(auto offset:{176u,196u,200u}){
    const auto node=REX_LOAD_U32(owner+offset);
    if(Valid(base,node,54))base[node+53]=0;
  }
  const auto roster=REX_LOAD_U32(owner+180);
  if(Valid(base,roster,224)){
    if(e.transfer_native_roots[1]!=roster){
      e.transfer_native_roots[1]=roster;
      e.transfer_native_offsets[1]={ReadFloat(base,roster+32),ReadFloat(base,roster+36)};
      Float(base,roster+32,e.transfer_native_offsets[1][0]+232.f);
      Float(base,roster+36,e.transfer_native_offsets[1][1]+33.f);
      pes6_edit_foreground_tree(base,roster);
      base[roster+52]|=0x23;
    }
    base[roster+53]=teams;
  }
  auto& ui=e.transfer_ui;
  if(!ui.ready)return;
  Show(ui.right_header,base,teams);
  if(ui.right_count)base[ui.right_count+53]=teams;
  const auto selected=Native(ctx,base,sub_82712A50,selector);
  if(!teams){e.transfer_preview_team=0xffffffffu;return;}
  if(selected>=189||e.transfer_preview_team==selected)return;
  e.transfer_preview_team=selected;
  Rect(base,ui.right_header,160,-163,192,24,0xd0d0ceffu,0xc2c2c0ffu);
  PlaceText(base,ui.right_count,168,-161,176,20);
  FontScale(base,ui.right_count,e.font_size*.90f);
  REX_STORE_U32(ui.right_count+48,0x181818ffu);
  TeamText(ctx,base,ui.right_count,TeamFitText(ctx,base,TeamName(base,selected),176));
}
extern "C" bool pes6_native_team_selected(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  if(team_editor.owner==owner&&team_editor.transfer_selector_active)
    return TransferNativePicked(ctx,base,REX_LOAD_U32(owner+208));
  if (!team_entry_mode || team_editor.owner)
    return false;
  const auto selected = REX_LOAD_U32(owner + 208);
  if (selected >= 189)
    return false;
  Native(ctx, base, sub_822E9E00, owner);
  if (TeamOpen(ctx, base, owner, selected)) {
    Sound(ctx, base, 0);
    if (transfer_entry_mode) TeamPageTo(TeamPage::TransferSquad);
    TeamDraw(ctx, base);
  } else {
    Native(ctx, base, sub_82712D58, REX_LOAD_U32(owner + 172));
  }
  return true;
}
struct PlayerRosterUI {
  bool active = false;
  bool ready = false;
  bool redraw = true;
  uint32_t owner = 0;
  uint32_t root = 0;
  uint32_t text = 0;
  uint32_t font = 0;
  uint32_t pos_tex = 0;
  float font_size = 1.f;
  unsigned team_id = 0xffffffffu;
  unsigned selected = 0;
  unsigned offset = 0;
  unsigned category = 0; // 0..5 (1/6..6/6)
  uint16_t preview_player = 0;
  std::array<uint8_t, 5> visible{};
  Quad top_bar, emblem, left_panel, left_header, mid_panel, mid_header, detail_pos_badge;
  uint32_t team_name = 0, left_title = 0, mid_title = 0;
  std::array<TransferRow, 12> rows{};
  std::array<std::pair<uint32_t, uint32_t>, 12> detail_rows{};
  std::map<uint32_t, std::string> text_cache;
} player_roster_ui;

void PlayerRosterClose(PPCContext &ctx, uint8_t *base) {
  auto &ui = player_roster_ui;
  if (!ui.active && !ui.root) return;
  preview::Visible(false);
  preview::Leave(ctx, base);
  if (ui.root && Valid(base, ui.root, 240)) {
    base[ui.root + 53] = 0;
    Native(ctx, base, sub_82735EC8, ui.root);
  }
  if (ui.text) Native(ctx, base, sub_8280E7B8, ui.text);
  ui = {};
}

bool PlayerRosterInit(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  auto &ui = player_roster_ui;
  if (ui.ready && ui.root) return true;
  const auto selector = REX_LOAD_U32(owner + 172);
  if (!Valid(base, selector, 132)) return false;
  const auto resource = Native(ctx, base, sub_8272A848, 0x820c77ecu);
  if (!resource) return false;
  if (!REX_LOAD_U32(resource + 40)) Native(ctx, base, sub_8272A9C0, resource);
  ui.root = Native(ctx, base, sub_8272FA80, resource, 0, 0, 0xffffffffu);
  if (!ui.root) return false;
  static uint32_t empty_geo = 0;
  if (!empty_geo) {
    empty_geo = Allocate(ctx, base, 64);
    const auto src = REX_LOAD_U32(ui.root + 140);
    if (empty_geo && Valid(base, src, 24)) {
      std::memcpy(base + empty_geo, base + src, 24);
      REX_STORE_U16(empty_geo, 0);
    }
  }
  if (empty_geo) {
    REX_STORE_U32(ui.root + 140, empty_geo);
    base[ui.root + 52] |= 0x23;
  }
  HideVector(base, ui.root, 312);
  HideVector(base, ui.root, 280);
  for (unsigned i = 0; i < 3; i++) {
    const auto page = Native(ctx, base, sub_8272F650, ui.root, i);
    if (Valid(base, page, 54)) base[page + 53] = 0;
  }
  ui.text = Allocate(ctx, base, 1024);
  const auto form = Native(ctx, base, sub_8272F650, ui.root, 1);
  const auto row = Native(ctx, base, sub_8272E410, form, 2);
  const auto sample = VectorItem(base, row, 312, 0);
  if (!ui.text || !Valid(base, sample, 272)) {
    PlayerRosterClose(ctx, base);
    return false;
  }
  ui.font = REX_LOAD_U32(sample + 148);
  ui.font_size = ReadFloat(base, sample + 152);
  ui.top_bar = NewQuad(ctx, base, ui.root);
  ui.emblem = NewQuad(ctx, base, ui.root, true);
  ui.left_panel = NewQuad(ctx, base, ui.root);
  ui.left_header = NewQuad(ctx, base, ui.root);
  ui.mid_panel = NewQuad(ctx, base, ui.root);
  ui.mid_header = NewQuad(ctx, base, ui.root);
  ui.pos_tex = NativePosBadgeTexture(ctx, base, owner);
  ui.detail_pos_badge = NewQuad(ctx, base, ui.root, true);
  ui.team_name = Label(ctx, base, ui.root, sample);
  ui.left_title = Label(ctx, base, ui.root, sample);
  ui.mid_title = Label(ctx, base, ui.root, sample);
  for (auto &r : ui.rows) {
    r.fill = NewQuad(ctx, base, ui.root);
    r.badge = NewQuad(ctx, base, ui.root, true);
    r.pos = Label(ctx, base, ui.root, sample);
    r.name = Label(ctx, base, ui.root, sample);
  }
  for (auto &d : ui.detail_rows) {
    d.first = Label(ctx, base, ui.root, sample);
    d.second = Label(ctx, base, ui.root, sample);
  }
  ui.ready = true;
  return true;
}

void PlayerRosterText(PPCContext &ctx, uint8_t *base, uint32_t node, const std::string &value) {
  if (!node || !player_roster_ui.text) return;
  // PlaceText invalidates glyph layout even when the string is unchanged.
  // Match TeamText: republish through the setter and include glyph bit 0x04.
  // This draw only runs on navigation/layout changes, not on idle frames.
  player_roster_ui.text_cache[node] = value;
  const auto len = std::min<size_t>(value.size(), 1023);
  std::memcpy(base + player_roster_ui.text, value.data(), len);
  base[player_roster_ui.text + len] = 0;
  const auto table = REX_LOAD_U32(node + 96);
  if (Valid(base, table, 20)) {
    if (auto setter = rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(table + 16))) {
      Native(ctx, base, setter, node + 96, player_roster_ui.text);
      const auto owned = REX_LOAD_U32(node + 100);
      if (Valid(base, owned, len + 1)) base[owned + len] = 0;
    }
  }
  base[node + 52] |= 0x67;
}

void PlayerRosterDraw(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  auto &ui = player_roster_ui;
  if (!ui.ready) return;
  base[ui.root + 53] = 1;
  pes6_edit_foreground_tree(base, ui.root);

  const auto &squad = TeamSquad(base, ui.team_id);
  const unsigned count = std::max(1u, TeamCount(squad));
  ui.selected = std::min(ui.selected, count - 1);
  if (ui.selected < ui.offset) ui.offset = ui.selected;
  if (ui.selected >= ui.offset + 12) ui.offset = ui.selected - 11;

  // Barra superior oscura cortada/cuadrada con el borde derecho de los 2 paneles juntos (+96)
  Rect(base, ui.top_bar, -332, -203, 428, 28, 0x383c44f0u, 0x22262cf0u);
  const auto emb_tex = TransferEmblemTexture(ctx, base, ui.team_id, ui.text);
  Show(ui.emblem, base, emb_tex != 0);
  if (emb_tex) {
    Native(ctx, base, sub_82733030, ui.emblem.primitive, emb_tex);
    Rect(base, ui.emblem, -326, -200, 22, 22, 0xffffffffu, 0xffffffffu);
  }
  if (ui.team_name) {
    base[ui.team_name + 53] = 1;
    PlaceText(base, ui.team_name, emb_tex ? -298.f : -322.f, -200, 390, 24);
    FontScale(base, ui.team_name, ui.font_size * 0.90f);
    REX_STORE_U32(ui.team_name + 48, 0xffffffffu);
    PlayerRosterText(ctx, base, ui.team_name, TeamName(base, ui.team_id));
  }

  // Panel izquierdo: lista de jugadores
  Show(ui.left_panel, base, true);
  Show(ui.left_header, base, true);
  Rect(base, ui.left_panel, -332, -175, 214, 354, 0xe9e9e7ffu, 0xd9d9d6ffu);
  Rect(base, ui.left_header, -332, -175, 214, 24, 0xd0d0ceffu, 0xc2c2c0ffu);
  if (ui.left_title) {
    base[ui.left_title + 53] = 1;
    PlaceText(base, ui.left_title, -322, -173, 194, 20);
    FontScale(base, ui.left_title, ui.font_size * 0.90f);
    REX_STORE_U32(ui.left_title + 48, 0x181818ffu);
    PlayerRosterText(ctx, base, ui.left_title, "Jugadores (" + std::to_string(TeamCount(squad)) + ")");
  }

  for (unsigned i = 0; i < 12; i++) {
    auto &r = ui.rows[i];
    const unsigned idx = ui.offset + i;
    const bool vis = idx < TeamCount(squad);
    Show(r.fill, base, vis);
    Show(r.badge, base, vis);
    if (r.pos) base[r.pos + 53] = vis;
    if (r.name) base[r.name + 53] = vis;
    if (!vis) continue;
    const float y = -147.f + 26.f * i;
    const bool sel = idx == ui.selected;
    const uint32_t bg = sel ? 0xffc62bffu : (i % 2 ? 0xefefedffu : 0xd9d9d6ffu);
    Rect(base, r.fill, -328, y, 206, 24, bg, bg);
    const uint16_t pid = squad.players[idx];
    if (ui.pos_tex && pid) {
      Show(r.badge, base, true);
      ApplyPosBadge(ctx, base, r.badge, ui.pos_tex, pid, -324, y + 3, 34, 18);
      if (r.pos) base[r.pos + 53] = 0;
    } else {
      const auto [abbr, col] = PlayerPosBadge(ctx, base, pid);
      Rect(base, r.badge, -324, y + 3, 34, 18, col, col);
      PlaceText(base, r.pos, -324, y + 2, 34, 18);
      FontScale(base, r.pos, ui.font_size * 0.80f);
      REX_STORE_U32(r.pos + 48, 0xffffffffu);
      PlayerRosterText(ctx, base, r.pos, abbr);
    }
    PlaceText(base, r.name, -284, y + 2, 158, 20);
    FontScale(base, r.name, ui.font_size * 0.90f);
    REX_STORE_U32(r.name + 48, 0x181818ffu);
    PlayerRosterText(ctx, base, r.name, TeamPlayer(ctx, base, pid, ui.team_id));
  }

  // Panel central: detalles agrupados por categoría (1/6 .. 6/6 con LB/RB)
  Rect(base, ui.mid_panel, -118, -175, 214, 354, 0xe9e9e7ffu, 0xd9d9d6ffu);
  Rect(base, ui.mid_header, -118, -175, 214, 24, 0xd0d0ceffu, 0xc2c2c0ffu);
  static constexpr std::array<const char *, 6> kCatNames{{
      "1/6  Básicos", "2/6  Ataque", "3/6  Físico",
      "4/6  Técnica", "5/6  Pase / Tiro", "6/6  Portero / Otros"
  }};
  if (ui.mid_title) {
    base[ui.mid_title + 53] = 1;
    PlaceText(base, ui.mid_title, -114, -173, 206, 20);
    FontScale(base, ui.mid_title, ui.font_size * 0.72f);
    REX_STORE_U32(ui.mid_title + 48, 0x181818ffu);
    PlayerRosterText(ctx, base, ui.mid_title, std::string("\x11kc ") + kCatNames[ui.category % 6] + " \x11kd");
  }

  const uint16_t pid = squad.players[ui.selected];
  struct DetailItem { std::string label, value; uint32_t color; };
  std::vector<DetailItem> items;
  if (pid) {
    if (ui.category == 0) {
      const auto db = REX_LOAD_U32(0x83139440u);
      const auto rec = db ? db + unsigned(pid) * 152 : 0;
      int nat = pes6::editor::ReadNationality(pid);
      if (nat < 0 && rec && Valid(base, rec + 152, 4)) nat = int(REX_LOAD_U32(rec + 152) >> 23);
      const std::string nat_name = (nat >= 0 && nat < int(pes6::editor::nationality_names.size()))
          ? pes6::editor::nationality_names[nat] : "-";
      const int height = rec && Valid(base, rec + 108, 4) ? pes6::editor::ReadBodyField(base + rec + 108, 0) : 0;
      const int age = rec && Valid(base, rec + 152, 4) ? int(((REX_LOAD_U32(rec + 152) >> 18) & 0x1Fu) + 15u) : 0;
      const bool left_foot = rec && Valid(base, rec + 72, 1) ? ((base[rec + 72] & 0x80) != 0) : false;
      const auto [abbr, col] = PlayerPosBadge(ctx, base, pid);
      items.push_back({"Posición", abbr, col});
      items.push_back({"Altura", height ? std::to_string(height) + " cm" : "-", 0x181818ffu});
      items.push_back({"Edad", age ? std::to_string(age) : "-", 0x181818ffu});
      items.push_back({"Pie hábil", left_foot ? "Izquierdo" : "Derecho", 0x181818ffu});
      items.push_back({"Nacionalidad", nat_name, 0x181818ffu});
    } else {
      struct StatDef { const char *label; unsigned idx; };
      static constexpr std::array<std::array<StatDef, 6>, 5> kStatGroups{{
          {{{"Ataque", 0}, {"Defensa", 1}, {"Balance", 2}, {"Resistencia", 3}, {"Vel. punta", 4}, {"Aceleración", 5}}},
          {{{"Respuesta", 6}, {"Agilidad", 7}, {"Prec. regate", 8}, {"Vel. regate", 9}, {"Salto", 20}, {"Cabezazo", 19}}},
          {{{"Prec. pase c.", 10}, {"Vel. pase c.", 11}, {"Prec. pase l.", 12}, {"Vel. pase l.", 13}, {"Técnica", 22}, {"Efecto", 18}}},
          {{{"Prec. tiro", 14}, {"Potencia tiro", 15}, {"Técnica tiro", 16}, {"Tiro libre", 17}, {"Agresividad", 23}, {"Mentalidad", 24}}},
          {{{"Hab. portero", 25}, {"Trabajo equipo", 21}, {"Regularidad", 26}, {"Pie malo (uso)", 28}, {"Pie malo (prec.)", 27}, {"Lesiones", 29}}}
      }};
      for (const auto &st : kStatGroups[(ui.category - 1) % 5]) {
        if (st.idx == 29) {
          const auto db = REX_LOAD_U32(0x83139440u);
          const auto rec = db ? db + unsigned(pid) * 152 : 0;
          const unsigned inj = rec && Valid(base, rec + 100, 4) ? ((REX_LOAD_U32(rec + 100) >> 24) & 3u) : 0u;
          static constexpr std::array<const char *, 4> kInj{{"C", "B", "A", "A"}};
          items.push_back({st.label, kInj[inj], 0x181818ffu});
        } else {
          const uint32_t v = PlayerStatValue(ctx, base, pid, st.idx);
          items.push_back({st.label, std::to_string(v), st.idx >= 26 ? 0x181818ffu : StatColor(v)});
        }
      }
    }
  }

  const bool show_detail_pos = (ui.category == 0 && pid != 0 && ui.pos_tex != 0);
  Show(ui.detail_pos_badge, base, show_detail_pos);
  if (show_detail_pos) ApplyPosBadge(ctx, base, ui.detail_pos_badge, ui.pos_tex, pid, 12, -144, 34, 18);

  for (unsigned i = 0; i < ui.detail_rows.size(); i++) {
    auto &dr = ui.detail_rows[i];
    const bool vis = i < items.size();
    const bool hide_val_for_badge = (i == 0 && show_detail_pos);
    if (dr.first) base[dr.first + 53] = vis;
    if (dr.second) base[dr.second + 53] = vis && !hide_val_for_badge;
    if (!vis) continue;
    const float sy = -144.f + 22.f * i;
    if (dr.first) {
      PlaceText(base, dr.first, -112, sy, 122, 18);
      FontScale(base, dr.first, ui.font_size * 0.82f);
      REX_STORE_U32(dr.first + 48, 0x202020ffu);
      PlayerRosterText(ctx, base, dr.first, items[i].label);
    }
    if (dr.second && !hide_val_for_badge) {
      PlaceText(base, dr.second, 12, sy, 82, 18);
      FontScale(base, dr.second, ui.font_size * 0.82f);
      REX_STORE_U32(dr.second + 48, items[i].color);
      PlayerRosterText(ctx, base, dr.second, TeamFitText(ctx, base, items[i].value, 80.f));
    }
  }

  // Footer
  const char *help = "\x11ka Editar jugador   \x11kb Volver   \x11kc/\x11kd Categoría";
  std::memcpy(base + ui.text, help, std::strlen(help) + 1);
  if (const auto footer = Native(ctx, base, sub_8272DEF0))
    Native(ctx, base, sub_8272DA10, footer, ui.text);
}

bool PlayerRosterUpdate(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  if (base[owner + 216] != 0) {
    if (player_roster_ui.active) PlayerRosterClose(ctx, base);
    return false;
  }
  auto &ui = player_roster_ui;
  static unsigned last_team_id = 0xffffffffu;
  static unsigned last_selected = 0;
  static unsigned last_offset = 0;
  static unsigned last_category = 0;
  if (!ui.active) {
    if (!PlayerRosterInit(ctx, base, owner)) return false;
    pes6::editor::RefreshNativeCatalog(base);
    if (edit_artwork.root) base[edit_artwork.root + 53] = 0;
    edit_artwork.kind = -1;
    for (unsigned i = 0; i < team_roots.size(); i++) {
      const auto node = REX_LOAD_U32(owner + team_roots[i]);
      if (Valid(base, node, 54)) {
        ui.visible[i] = base[node + 53];
        base[node + 53] = 0;
      }
    }
    ui.active = true;
    ui.owner = owner;
    ui.team_id = REX_LOAD_U32(owner + 208);
    if (ui.team_id == last_team_id) {
      ui.selected = last_selected;
      ui.offset = last_offset;
      ui.category = last_category;
    } else {
      ui.selected = 0;
      ui.offset = 0;
      ui.category = 0;
    }
    ui.preview_player = 0;
    ui.redraw = true;
  }

  const auto input = REX_LOAD_U32(owner + 144);
  const auto pressed = [&](unsigned key, bool rep = false) {
    return input && Native(ctx, base, rep ? sub_8272E038 : sub_8272DF98, input, key) != 0;
  };
  const auto &squad = TeamSquad(base, ui.team_id);
  const unsigned count = std::max(1u, TeamCount(squad));

  if (pressed(9)) { // B: volver a selección de equipo
    Sound(ctx, base, 1);
    last_team_id = 0xffffffffu;
    for (unsigned i = 0; i < team_roots.size(); i++) {
      const auto node = REX_LOAD_U32(owner + team_roots[i]);
      if (Valid(base, node, 54)) base[node + 53] = (team_roots[i] == 192) ? 1 : 0;
    }
    PlayerRosterClose(ctx, base);
    base[owner + 216] = 1;
    const auto selector = REX_LOAD_U32(owner + 172);
    if (selector) {
      Native(ctx, base, sub_82712D58, selector);
      Native(ctx, base, sub_827116E8, selector, 1, 1);
      Native(ctx, base, sub_82712B30, selector, 0);
    }
    Native(ctx, base, sub_822E7DF8, owner, 1, 1);
    return true;
  }

  if (pressed(8)) { // A: abrir editor del jugador
    const uint16_t pid = squad.players[ui.selected];
    if (pid) {
      Sound(ctx, base, 0);
      last_team_id = ui.team_id;
      last_selected = ui.selected;
      last_offset = ui.offset;
      last_category = ui.category;
      PlayerRosterClose(ctx, base);
      REX_STORE_U16(owner + 284, pid);
      const auto selector = REX_LOAD_U32(owner + 172);
      const auto sel_res = selector ? Native(ctx, base, sub_82712A50, selector) : ui.team_id;
      REX_STORE_U32(0x831398A4u - 4u, sel_res);
      REX_STORE_U32(0x831398A4u, pid);
      const auto vt = REX_LOAD_U32(owner);
      if (Valid(base, vt, 72)) {
        if (auto *fn = rex::runtime::ResolveIndirectFunction(REX_LOAD_U32(vt + 68)))
          Native(ctx, base, fn, owner);
      }
      Native(ctx, base, sub_827290F8, owner, 7, owner);
      return true;
    }
  }

  if (pressed(0, true)) { ui.selected = (ui.selected + count - 1) % count; ui.redraw = true; Sound(ctx, base, 5); }
  if (pressed(1, true)) { ui.selected = (ui.selected + 1) % count; ui.redraw = true; Sound(ctx, base, 5); }
  if (pressed(12)) { ui.selected = ui.selected >= 12 ? ui.selected - 12 : 0; ui.redraw = true; Sound(ctx, base, 5); }
  if (pressed(13)) { ui.selected = std::min(count - 1, ui.selected + 12); ui.redraw = true; Sound(ctx, base, 5); }
  if (pressed(10)) { ui.category = (ui.category + 5) % 6; ui.redraw = true; Sound(ctx, base, 6); }
  if (pressed(11)) { ui.category = (ui.category + 1) % 6; ui.redraw = true; Sound(ctx, base, 6); }

  const uint16_t pid = squad.players[ui.selected];
  preview::Container(ui.root);
  preview::Frame(5);
  if (pid && ui.preview_player != pid) {
    if (ui.preview_player == 0) preview::Enter(ctx, base, pid, int(ui.team_id), 4);
    else { preview::TeamKit(ctx, base, ui.team_id, 4); preview::SwitchPlayer(pid); }
    ui.preview_player = pid;
  }
  preview::Visible(pid != 0);
  preview::Tick(ctx, base);

  if (ui.redraw) {
    ui.redraw = false;
    PlayerRosterDraw(ctx, base, owner);
  }
  return true;
}

extern "C" bool pes6_native_team_update(PPCContext &ctx, uint8_t *base) {
  const auto owner = ctx.r3.u32;
  auto &e = team_editor;
  if (!e.owner) {
    if (!team_entry_mode && PlayerRosterUpdate(ctx, base, owner))
      return true;
    // League/country and roster selectors also need a per-frame transition
    // clock; their native header only refreshes on navigation events.
    TeamSelectorArtworkFrame(ctx, base, owner);
    EditArtworkDraw(ctx, base, transfer_entry_mode ? transfer_coach_index : team_entry_mode ? coach_artwork_request : selector_player_portrait ? PlayerSessionArtwork() : -1, true);
    return false;
  }
  if (e.owner != owner)
    return false;
  if (!e.management && TeamPreviewPage()) {
    if (TeamHeld(ctx, base, 10)) preview::Rotate(-1);
    if (TeamHeld(ctx, base, 11)) preview::Rotate(1);
  }
  if (e.management) TeamCoach(ctx, base);
  if (TeamManagementUpdate(ctx, base))
    return true;
  if(e.page==TeamPage::TransferLeague){
    const auto selector=REX_LOAD_U32(owner+172);
    if(!Valid(base,selector,52))return true;
    if(!e.transfer_selector_active){
      Native(ctx,base,sub_82712D58,selector);
      // Reuse 822E9F08 and its original selector, exactly as on initial entry.
      // Its league/team scenes are CHILDREN of owner+192, hidden by TeamOpen.
      const auto parent=REX_LOAD_U32(owner+192);
      if(!Valid(base,parent,224))return true;
      e.transfer_native_roots[0]=parent;
      e.transfer_native_offsets[0]={ReadFloat(base,parent+32),ReadFloat(base,parent+36)};
      Float(base,parent+32,e.transfer_native_offsets[0][0]+226.f);
      Float(base,parent+36,e.transfer_native_offsets[0][1]+6.f);
      base[parent+53]=1;base[parent+52]|=0x23;
      pes6_edit_foreground_tree(base,parent);
      if(!e.transfer_selector_resume){
        base[selector+6]=1; // new transfer: initialize league selection
        base[selector+7]=0;
      }
      e.transfer_selector_resume=false;
      Native(ctx,base,sub_82712B30,selector,0);
      e.transfer_selector_active=true;e.redraw=true;
      TeamDraw(ctx,base);
    }
    if(base[selector+6]==2&&TeamInput(ctx,base,9)){
      TransferSelectorClose(ctx,base);base[e.root+53]=1;
      TeamPageTo(TeamPage::TransferSquad);e.selected=e.transfer_squad_selected;
      Sound(ctx,base,1);TeamDraw(ctx,base);return true;
    }
    // Let the original parent handle updates, visibility, selection and results.
    // pes6_native_team_selected intercepts the result before native roster edit.
    return false;
  }
  if (TeamInput(ctx, base, 9)) {
    Sound(ctx, base, 1);
    if (e.editing) {
      TeamSquad(base, e.id) = e.before_squad;
      e.models[e.variant] = e.before_model;
      e.colors[e.variant] = e.before_colors;
      e.designs[e.variant] = e.before_design;
      e.lettering[e.variant] = e.before_lettering;
      e.crests[e.variant] = e.before_crest;
      e.atlases[e.variant] = e.before_atlas;
      e.editing = false;
      e.number_draft = -1;
      e.home_model = e.before_home_model;
      e.home_name = e.before_home_name;
    } else if (e.swap >= 0) {
      e.swap = -1;
      e.message.clear();
    } else if (e.page == TeamPage::Menu) {
      TeamClose(ctx, base);
      return true;
    } else if (transfer_entry_mode && e.page == TeamPage::TransferSquad) {
      TeamClose(ctx, base);
      return true;
    } else if (e.page == TeamPage::Text)
      TeamPageTo(TeamPage::Identity);
    else if (e.page == TeamPage::Position || e.page == TeamPage::CopyFormation)
      TeamPageTo(TeamPage::Formation);
    else if (e.page == TeamPage::Kit)
      TeamPageTo(TeamPage::Kits);
    else if (e.page == TeamPage::KitModel || e.page == TeamPage::KitDesign ||
             e.page == TeamPage::KitColors || e.page == TeamPage::KitLettering)
      TeamPageTo(TeamPage::Kit);
    else if (e.page == TeamPage::TransferPlayer) {
      // 82712D58 resumes native state 5 at its saved state 4, preserving
      // the league and hovered emblem instead of rebuilding the league page.
      e.transfer_selector_resume=true;
      TeamPageTo(TeamPage::TransferLeague);
    }
    else if (e.page == TeamPage::TransferTeam)
      TeamPageTo(TeamPage::TransferLeague);
    else if (e.page == TeamPage::TransferLeague) {
      TeamPageTo(TeamPage::TransferSquad);
      e.selected = e.transfer_squad_selected;
    } else
      TeamPageTo(TeamPage::Menu);
  } else if (e.page == TeamPage::Text) {
    // PC keyboard input support alongside controller D-pad.
    static std::array<bool, 256> prev_keys{};
    const auto key_pressed = [&](int vk) {
      const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
      const bool edge = down && !prev_keys[vk];
      prev_keys[vk] = down;
      return edge;
    };
    const size_t max_len = e.short_edit ? 8 : 63;
    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    for (int vk = 'A'; vk <= 'Z'; vk++) {
      if (key_pressed(vk) && e.edit_text.size() < max_len) {
        e.edit_text.push_back(char(shift ? vk : (vk + 32)));
        e.character = unsigned(std::max<size_t>(1, TeamCharacters(e.edit_text).size()) - 1);
      }
    }
    for (int vk = '0'; vk <= '9'; vk++) {
      if (key_pressed(vk) && e.edit_text.size() < max_len) {
        e.edit_text.push_back(char(vk));
        e.character = unsigned(std::max<size_t>(1, TeamCharacters(e.edit_text).size()) - 1);
      }
    }
    if (key_pressed(VK_SPACE) && e.edit_text.size() < max_len) {
      e.edit_text.push_back(' ');
      e.character = unsigned(std::max<size_t>(1, TeamCharacters(e.edit_text).size()) - 1);
    }
    if (key_pressed(VK_OEM_MINUS) && e.edit_text.size() < max_len) {
      e.edit_text.push_back('-');
      e.character = unsigned(std::max<size_t>(1, TeamCharacters(e.edit_text).size()) - 1);
    }
    if (key_pressed(VK_OEM_PERIOD) && e.edit_text.size() < max_len) {
      e.edit_text.push_back('.');
      e.character = unsigned(std::max<size_t>(1, TeamCharacters(e.edit_text).size()) - 1);
    }
    if (key_pressed(VK_BACK) && !e.edit_text.empty()) {
      auto ls = TeamCharacters(e.edit_text);
      if (!ls.empty()) { ls.pop_back(); TeamJoinCharacters(ls); }
      e.character = unsigned(ls.empty() ? 0 : ls.size() - 1);
    }
    auto letters = TeamCharacters(e.edit_text);
    const bool left = TeamInput(ctx, base, 2, true), right = TeamInput(ctx, base, 3, true);
    if (left && e.character)
      e.character--;
    if (right && e.character + 1 < letters.size())
      e.character++;
    static const auto alphabet = TeamCharacters(
        " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.-áéíóúÁÉÍÓÚñÑüÜ'&");
    const int direction = TeamInput(ctx, base, 0, true)   ? 1
                          : TeamInput(ctx, base, 1, true) ? -1
                                                          : 0;
    if (direction || left || right)
      Sound(ctx, base, 6);
    if (direction && !letters.empty()) {
      const auto found = std::find(alphabet.begin(), alphabet.end(), letters[e.character]);
      const auto at = found == alphabet.end() ? 0 : int(found - alphabet.begin());
      letters[e.character] =
          alphabet[(at + direction + int(alphabet.size())) % int(alphabet.size())];
      TeamJoinCharacters(letters);
    }
    if (TeamInput(ctx, base, 14) && !letters.empty()) {
      letters.erase(letters.begin() + e.character);
      TeamJoinCharacters(letters);
      e.character = std::min(e.character, unsigned(letters.empty() ? 0 : letters.size() - 1));
    }
    if (TeamInput(ctx, base, 15) && e.edit_text.size() < (e.short_edit ? 8 : 63)) {
      e.edit_text.push_back(' ');
      e.character = unsigned(TeamCharacters(e.edit_text).size() - 1);
    }
    if (TeamInput(ctx, base, 8)) {
      Sound(ctx, base, 0);
      TeamConfirm(ctx, base);
    }
  } else {
    const auto count = std::max(1u, e.row_count);
    if (!e.editing && e.page == TeamPage::TransferLeague) {
      // Visual 4-row grid: [0..3], [4..5], [6..9], [10..11]
      static constexpr std::array<std::pair<unsigned, unsigned>, 4> kRows{{{0, 4}, {4, 2}, {6, 4}, {10, 2}}};
      unsigned r = 0, c = 0;
      for (unsigned i = 0; i < 4; i++) {
        if (e.selected >= kRows[i].first && e.selected < kRows[i].first + kRows[i].second) {
          r = i; c = e.selected - kRows[i].first; break;
        }
      }
      if (TeamInput(ctx, base, 2, true)) {
        e.selected = (e.selected + count - 1) % count;
        Sound(ctx, base, 5);
      } else if (TeamInput(ctx, base, 3, true)) {
        e.selected = (e.selected + 1) % count;
        Sound(ctx, base, 5);
      } else if (TeamInput(ctx, base, 0, true)) {
        r = (r + 3) % 4;
        e.selected = kRows[r].first + std::min(c, kRows[r].second - 1);
        Sound(ctx, base, 5);
      } else if (TeamInput(ctx, base, 1, true)) {
        r = (r + 1) % 4;
        e.selected = kRows[r].first + std::min(c, kRows[r].second - 1);
        Sound(ctx, base, 5);
      }
    } else if (!e.editing && e.page == TeamPage::TransferTeam) {
      if (TeamInput(ctx, base, 2, true)) {
        e.selected = (e.selected + count - 1) % count;
        Sound(ctx, base, 5);
      } else if (TeamInput(ctx, base, 3, true)) {
        e.selected = (e.selected + 1) % count;
        Sound(ctx, base, 5);
      } else if (TeamInput(ctx, base, 0, true)) {
        if (e.selected >= 4) e.selected -= 4;
        else {
          const unsigned last_row = ((count - 1) / 4) * 4;
          e.selected = std::min(count - 1, last_row + (e.selected % 4));
        }
        Sound(ctx, base, 5);
      } else if (TeamInput(ctx, base, 1, true)) {
        if (e.selected + 4 < count) e.selected += 4;
        else e.selected = e.selected % 4;
        Sound(ctx, base, 5);
      }
    } else if (!e.editing) {
      const bool ok_nav = e.page == TeamPage::Menu || (TeamCompactSubPage() && e.page != TeamPage::Text);
      const unsigned cap = TeamCompactSubPage() ? 11u : 9u;
      if (TeamInput(ctx, base, 0, true)) {
        if (ok_nav) {
          if (e.menu_ok) { e.menu_ok = false; e.selected = count - 1; }
          else if (e.selected == 0) e.menu_ok = true;
          else e.selected--;
        } else {
          e.selected = (e.selected + count - 1) % count;
        }
        Sound(ctx, base, 5);
      }
      if (TeamInput(ctx, base, 1, true)) {
        if (ok_nav) {
          if (e.menu_ok) { e.menu_ok = false; e.selected = 0; }
          else if (e.selected + 1 >= count) e.menu_ok = true;
          else e.selected++;
        } else {
          e.selected = (e.selected + 1) % count;
        }
        Sound(ctx, base, 5);
      }
      if (TeamInput(ctx, base, 12)) {
        e.menu_ok = false;
        e.selected = (e.selected + count - std::min(count, cap)) % count;
        Sound(ctx, base, 5);
      }
      if (TeamInput(ctx, base, 13)) {
        e.menu_ok = false;
        e.selected = (e.selected + cap) % count;
        Sound(ctx, base, 5);
      }
    }
    if (e.editing && TeamInput(ctx, base, 2, true))
      TeamChange(ctx, base, -1);
    if (e.editing && TeamInput(ctx, base, 3, true))
      TeamChange(ctx, base, 1);
    if (transfer_entry_mode && e.page == TeamPage::TransferSquad && TeamInput(ctx, base, 14)) {
      TeamPageTo(TeamPage::Menu);
      e.selected = 4;
      TeamConfirm(ctx, base);
      if (e.owner) {
        const auto message = e.message;
        TeamPageTo(TeamPage::TransferSquad);
        e.message = message;
      }
      return true;
    }
    if (e.page == TeamPage::Squad && TeamInput(ctx, base, 14)) {
      e.transfer_register = false;
      e.slot = e.selected;
      e.transfer_squad_selected = e.selected + 1;
      TeamPageTo(TeamPage::TransferLeague);
    }
    if (e.page == TeamPage::Squad && TeamInput(ctx, base, 15)) {
      auto &squad = TeamSquad(base, e.id);
      if (TeamCount(squad) > 11 && squad.players[e.selected])
        TeamRemove(squad, e.selected);
      else
        e.message = "El equipo necesita once jugadores";
    }
    if (e.page == TeamPage::TransferSquad && e.selected > 0 && TeamInput(ctx, base, 15)) {
      auto &squad = TeamSquad(base, e.id);
      const unsigned slot = e.selected - 1;
      if (TeamCount(squad) > 11 && squad.players[slot])
        TeamRemove(squad, slot);
      else
        e.message = "El equipo necesita once jugadores";
    }
    if ((e.page == TeamPage::KitColors || e.page == TeamPage::KitLettering) &&
        TeamInput(ctx, base, 14)) {
      e.character = (e.character + 1) % 3;
      Sound(ctx, base, 5);
    }
    if (e.page == TeamPage::KitLettering && TeamInput(ctx, base, 15)) {
      e.lettering[e.variant].values[e.selected] = e.selected == 8 ? 1 : -1;
      Sound(ctx, base, 0);
    }
    if (e.page == TeamPage::KitColors && TeamInput(ctx, base, 15)) {
      e.colors[e.variant][e.selected] = -1;
      Sound(ctx, base, 0);
    }
    if (TeamScalarPage() && e.page != TeamPage::KitColors && e.page != TeamPage::KitLettering &&
        TeamInput(ctx, base, 15)) {
      auto &squad = TeamSquad(base, e.id);
      if (e.page == TeamPage::KitModel) {
        if (e.selected == 0)
          e.models[e.variant] = {-1, -1};
        else if (e.selected == 1)
          e.models[e.variant][1] = e.models[e.variant][0] < 0 ? -1 : OriginalKitByte(84);
        else
          e.crests[e.variant] = 0;
      } else if (e.page == TeamPage::KitDesign)
        e.designs[e.variant][e.selected + 1] = -1;
      else if (e.page == TeamPage::Identity) {
        e.home_model = -1;
        e.home_name.clear();
      } else if (e.page == TeamPage::Numbers) {
        if (!e.editing)
          TeamConfirm(ctx, base);
        const auto &original = e.originals.at(e.id);
        for (unsigned i = 0; i < 32; i++)
          if (original.players[i] == squad.players[e.selected]) {
            const auto wanted = original.numbers[i];
            // Reuse collision handling, including circular next-free allocation.
            if (wanted >= 1 && wanted <= 99) {
              e.number_draft = wanted;
            }
            break;
          }
      } else if (e.page == TeamPage::Roles)
        squad.roles[e.selected] = e.originals.at(e.id).roles[e.selected];
      Sound(ctx, base, 0);
    }
    if (TeamInput(ctx, base, 8)) {
      Sound(ctx, base, 0);
      TeamConfirm(ctx, base);
    }
  }
  if (e.owner)
    TeamDraw(ctx, base);
  return true;
}

extern "C" bool pes6_team_formation(PPCContext &ctx, uint8_t *base) {
  if (const auto at = pes6::teams::EditedFormation(base, ctx.r4.u32)) {
    ctx.r3.u64 = at;
    return true;
  }
  return false;
}
extern "C" bool pes6_team_role(PPCContext &ctx, uint8_t *base) {
  const auto value = pes6::teams::EditedRole(base, ctx.r4.u32, ctx.r5.u32);
  if (value < 0)
    return false;
  ctx.r3.u64 = unsigned(value);
  return true;
}
extern "C" void pes6_team_configuration(uint8_t *base, uint32_t record, uint32_t team) {
  pes6::teams::Configuration(base, record, team);
}

extern "C" void pes6_team_edit_menu(PPCContext &ctx, uint8_t *base) {
  const auto menu = ctx.r3.u32, keys = ctx.r4.u32;
  if (!edit_menu_strings)
    edit_menu_strings = Allocate(ctx, base, 64);
  if (!edit_menu_strings) {
    sub_827317F8(ctx, base);
    return;
  }
  const unsigned restore_team_selection = transfer_entry_mode ? 2 : team_entry_mode ? 1 : 0;
  team_entry_mode = false;
  transfer_entry_mode = false;
  constexpr char label[] = "Editar equipo";
  constexpr char transfers[] = "Traspasos";
  std::memcpy(base + edit_menu_strings + 24, label, sizeof(label));
  std::memcpy(base + edit_menu_strings + 40, transfers, sizeof(transfers));
  REX_STORE_U32(edit_menu_strings, Native(ctx, base, sub_827320A0, REX_LOAD_U32(keys)));
  REX_STORE_U32(edit_menu_strings + 4, edit_menu_strings + 24);
  REX_STORE_U32(edit_menu_strings + 8, edit_menu_strings + 40);
  REX_STORE_U32(edit_menu_strings + 12, Native(ctx, base, sub_827320A0, REX_LOAD_U32(keys + 4)));
  REX_STORE_U32(edit_menu_strings + 16, Native(ctx, base, sub_827320A0, REX_LOAD_U32(keys + 8)));
  ctx.r3.u64 = Native(ctx, base, sub_82731648, menu, edit_menu_strings, 5);
  if (restore_team_selection) {
    const auto root = REX_LOAD_U32(menu + 152);
    const auto form = Native(ctx, base, sub_8272F650, root, REX_LOAD_U32(root + 448));
    Native(ctx, base, sub_8272E980, form, 0, restore_team_selection);
  }
}
extern "C" void pes6_team_edit_choice(PPCContext &ctx) {
  if (!edit_menu_strings) {
    team_entry_mode = false;
    transfer_entry_mode = false;
    return;
  }
  const unsigned row = ctx.r3.u32;
  if (row >= 5) {
    team_entry_mode = false;
    transfer_entry_mode = false;
    return;
  }
  team_entry_mode = row == 1 || row == 2;
  transfer_entry_mode = row == 2;
  if (row == 0) BeginPlayerSession();
  if (team_entry_mode) BeginCoachSession();
  ctx.r3.u64 = row < 3 ? 0 : row - 2;
}
extern "C" void pes6_team_selector_header(PPCContext &ctx, uint8_t *base) {
  if (!team_editor.owner && !player_roster_ui.active) EditArtworkDraw(ctx, base, transfer_entry_mode ? transfer_coach_index : team_entry_mode ? coach_artwork_request : selector_player_portrait ? PlayerSessionArtwork() : -1, true);
  if (!team_entry_mode || !edit_menu_strings)
    return;
  const auto header = Native(ctx, base, sub_82730A50);
  if (team_editor.management) {
    const auto label = Valid(base, header, 20) ? REX_LOAD_U32(header + 16) : 0;
    if (Valid(base, label, 54)) base[label + 53] = 0;
    return;
  }
  Native(ctx, base, sub_82730828, header, edit_menu_strings + (transfer_entry_mode ? 40 : 24));
  if (team_editor.owner)
    TeamFooter(ctx, base);
}
extern "C" void pes6_edit_menu_portrait(PPCContext &ctx, uint8_t *base, uint32_t owner) {
  const auto menu = REX_LOAD_U32(owner + 168);
  if (!Valid(base, menu, 156)) { EditArtworkDraw(ctx, base, -1); return; }
  const auto root = REX_LOAD_U32(menu + 152);
  if (!Valid(base, root, 452)) { EditArtworkDraw(ctx, base, -1); return; }
  const auto selected = Native(ctx, base, sub_82730FC0, menu);
  menu_art_foreground = root;
  EditArtworkDraw(ctx, base, selected == 0 ? 5 : selected == 1 ? 6 : selected == 2 ? transfer_artwork_index : -1);
  if (selected >= 3) pes6_edit_effects(ctx, base, root, 0x4ff);
}
extern "C" void pes6_edit_menu_artwork_leave(PPCContext &ctx, uint8_t *base) {
  EditArtworkClose(ctx, base);
  pes6_edit_effects_close(ctx, base);
}
extern "C" bool pes6_team_comparison_label(uint8_t *base, uint32_t node) {
  pes6_edit_text_foreground(base, node);
  if (!team_editor.management || !Valid(base, node, 104)) return false;
  // Runs after native text transforms: includes copies re-enabled by their
  // controllers later than TeamManagementPresentation. Never hide left labels.
  if (ReadFloat(base, node + 16) <= 112 || ReadFloat(base, node + 20) >= -140) return false;
  const auto string = REX_LOAD_U32(node + 100);
  if (!Valid(base, string, 32)) return false;
  unsigned digits = 0, separators = 0;
  for (unsigned i = 0; i < 31; i++) {
    const auto c = base[string + i];
    if (!c) return digits >= 3 && separators >= 2;
    if (c >= '0' && c <= '9') ++digits;
    else if (c == '-') ++separators;
    else if (i + 2 < 31 && c == 0xe2 &&
             ((base[string + i + 1] == 0x80 && base[string + i + 2] >= 0x90 && base[string + i + 2] <= 0x95) ||
              (base[string + i + 1] == 0x88 && base[string + i + 2] == 0x92))) { ++separators; i += 2; }
    else if (c != ' ') return false;
  }
  return false;
}
