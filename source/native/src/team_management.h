#pragma once
#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <windows.h>

namespace pes6::teams {
constexpr unsigned count = 189;
struct Squad {
  std::array<uint16_t, 32> players{};
  std::array<uint8_t, 32> numbers{};
  std::array<uint8_t, 52> formation{};
  std::array<uint8_t, 6> roles{};
  std::array<uint8_t, 358> configuration{};
  std::array<uint8_t, 4060> saved{};
  bool operator==(const Squad &) const = default;
};
inline std::map<std::pair<std::string, unsigned>, std::vector<uint8_t>> overrides;
inline bool loaded = false;
inline uint32_t applied_db = 0, applied_tactics = 0;
inline bool dirty = true;
inline unsigned revision = 1;
inline unsigned indexed_revision = 0;
inline std::array<bool, count> has_formation{}, has_roles{}, has_configuration{};
inline bool any_formation = false, any_roles = false, any_configuration = false;
inline std::array<uint32_t, count> configured_records{};
inline std::array<unsigned, count> configured_revisions{};
inline void IndexOverrides() {
  if (indexed_revision == revision)
    return;
  has_formation.fill(false);
  has_roles.fill(false);
  has_configuration.fill(false);
  any_formation = any_roles = any_configuration = false;
  for (const auto &[key, bytes] : overrides) {
    if (key.second >= count)
      continue;
    if (key.first == "FORM") {
      has_formation[key.second] = true;
      any_formation = true;
    }
    if (key.first == "ROLES") {
      has_roles[key.second] = true;
      any_roles = true;
    }
    if (key.first == "CONFIG") {
      has_configuration[key.second] = true;
      any_configuration = true;
    }
  }
  indexed_revision = revision;
}
inline std::filesystem::path store;
inline unsigned Size(const std::string &kind) {
  if (kind == "SAVED")
    return 4060;
  if (kind == "CONFIG")
    return 358;
  if (kind == "ROSTER")
    return 64;
  if (kind == "NUMBERS")
    return 32;
  if (kind == "FORM")
    return 52;
  if (kind == "ROLES")
    return 6;
  return 0;
}
inline bool Readable(uint8_t *base, uint32_t at, size_t length) {
  if (at < 0x10000 || uint64_t(at) + length >= 0xe0000000ull)
    return false;
  const auto *p = base + at;
  while (length) {
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
      return false;
    const auto step = std::min(
        length, size_t(static_cast<const uint8_t *>(info.BaseAddress) + info.RegionSize - p));
    if (!step)
      return false;
    p += step;
    length -= step;
  }
  return true;
}
inline uint32_t Word(uint8_t *base, uint32_t at) {
  return uint32_t(base[at]) << 24 | uint32_t(base[at + 1]) << 16 | uint32_t(base[at + 2]) << 8 |
         base[at + 3];
}
inline bool AtomicWrite(const std::filesystem::path &file, const std::string &bytes) {
  auto temporary = file;
  temporary += L".tmp";
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  out.write(bytes.data(), std::streamsize(bytes.size()));
  out.flush();
  if (!out)
    return false;
  out.close();
  return MoveFileExW(temporary.c_str(), file.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}
inline std::filesystem::path Journal() {
  auto file = store;
  file += L".transaction";
  return file;
}
inline bool Recover() {
  std::ifstream in(Journal(), std::ios::binary);
  if (!in)
    return true;
  std::string header;
  std::getline(in, header);
  if (header != "PES6_TEAM_TRANSACTION_V1")
    return false;
  std::array<bool, 2> exists{};
  std::array<std::string, 2> contents;
  for (unsigned i = 0; i < 2; i++) {
    unsigned present;
    std::string hex;
    if (!(in >> present >> hex) || present > 1 || hex.size() > 4000000)
      return false;
    exists[i] = present != 0;
    if (hex == "-")
      continue;
    if (hex.size() % 2 || hex.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
      return false;
    for (size_t at = 0; at < hex.size(); at += 2)
      contents[i].push_back(char(std::stoul(hex.substr(at, 2), nullptr, 16)));
  }
  std::string committed;
  in >> committed;
  in.close();
  if (committed != "COMMIT") {
    const std::array<std::filesystem::path, 2> files{store, store.parent_path() / "team-info.txt"};
    for (unsigned i = 0; i < 2; i++) {
      if (exists[i]) {
        if (!AtomicWrite(files[i], contents[i]))
          return false;
      } else if (std::filesystem::exists(files[i]) && !DeleteFileW(files[i].c_str()))
        return false;
    }
  }
  return DeleteFileW(Journal().c_str()) != 0;
}
inline bool BeginTransaction() {
  if (!Recover())
    return false;
  std::error_code error;
  std::filesystem::create_directories(store.parent_path(), error);
  if (error)
    return false;
  std::ostringstream out;
  out << "PES6_TEAM_TRANSACTION_V1\n";
  for (const auto &file : {store, store.parent_path() / "team-info.txt"}) {
    const bool present = std::filesystem::exists(file);
    std::ifstream input(file, std::ios::binary);
    if (present && !input.is_open())
      return false;
    std::string bytes((std::istreambuf_iterator<char>(input)), {});
    if (present && input.bad())
      return false;
    out << (present ? 1 : 0) << ' ';
    if (bytes.empty())
      out << '-';
    for (unsigned char value : bytes)
      out << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
    out << '\n';
  }
  return AtomicWrite(Journal(), out.str());
}
inline bool CommitTransaction() {
  std::ofstream out(Journal(), std::ios::binary | std::ios::app);
  out << "COMMIT\n";
  out.flush();
  if (!out)
    return false;
  out.close();
  DeleteFileW(Journal().c_str());
  return true;
}
inline void Load() {
  if (loaded)
    return;
  loaded = true;
  wchar_t exe[MAX_PATH]{};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  auto root = std::filesystem::path(exe).parent_path();
  for (unsigned i = 0; i < 6 && !std::filesystem::exists(root / "game/cv.img"); i++)
    root = root.parent_path();
  store = root / "data/editor/team-management.txt";
  if (!Recover()) {
    loaded = false;
    return;
  }
  std::ifstream input(store, std::ios::binary);
  std::string line;
  std::getline(input, line);
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
  if (line != "PES6_TEAM_MANAGEMENT_V1")
    return;
  decltype(overrides) parsed;
  while (std::getline(input, line)) {
    if (line.find_first_not_of(" \t\r") == std::string::npos)
      continue;
    std::istringstream row(line);
    std::string kind, hex, extra;
    unsigned id;
    if (!(row >> kind >> id >> hex) || id >= count || !Size(kind) || hex.size() != Size(kind) * 2 ||
        (row >> extra))
      return;
    std::vector<uint8_t> bytes;
    for (unsigned i = 0; i < hex.size(); i += 2) {
      const auto pair = hex.substr(i, 2);
      if (pair.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos)
        return;
      bytes.push_back(uint8_t(std::stoul(pair, nullptr, 16)));
    }
    if (kind == "ROSTER") {
      std::array<bool, 4210> seen{};
      for (unsigned slot = 0; slot < 32; slot++) {
        const unsigned player = unsigned(bytes[slot * 2]) << 8 | bytes[slot * 2 + 1];
        if (player >= seen.size() || (player && seen[player]))
          return;
        seen[player] = true;
      }
    }
    if (kind == "ROLES" &&
        std::any_of(bytes.begin(), bytes.end(), [](auto value) { return value > 31; }))
      return;
    if (kind == "FORM")
      for (unsigned i = 0; i < 52; i++)
        if (bytes[i] > (i < 10 ? 40 : i < 20 ? 52 : i < 30 ? 104 : i < 41 ? 255 : 2))
          return;
    parsed[{kind, id}] = std::move(bytes);
  }
  overrides = std::move(parsed);
}
inline unsigned ResolveTeam(uint8_t *base, unsigned id) {
  id &= 0xffff;
  if (id == 214 || id == 215) {
    const auto match = Word(base, 0x83139448);
    if (!Readable(base, match, 12))
      return count;
    const auto team = Word(base, match + (id == 214 ? 4 : 8));
    if (!Readable(base, team, 8))
      return count;
    id = unsigned(base[team + 6]) << 8 | base[team + 7];
  }
  return id;
}
inline uint32_t Address(uint8_t *base, const std::string &kind, unsigned id) {
  if (id >= count)
    return 0;
  const auto db = Word(base, 0x83139440), tactics = Word(base, 0x83139458);
  uint32_t at = 0;
  // Native getters 821784C8/82178470 and 8217B3E8/8217B360.
  if (kind == "ROSTER" && db)
    at = db + 760004 + id * 64;
  if (kind == "NUMBERS" && db)
    at = db + 772100 + id * 32;
  if (kind == "FORM" && tactics)
    at = tactics + 1134 + id * 52;
  if (kind == "ROLES" && tactics)
    at = tactics + id * 6;
  return at && Readable(base, at, Size(kind)) ? at : 0;
}
inline uint32_t EditedFormation(uint8_t *base, unsigned id) {
  Load();
  IndexOverrides();
  if (!any_formation)
    return 0;
  id = ResolveTeam(base, id);
  if (id >= count || !has_formation[id])
    return 0;
  if (!dirty && applied_tactics && Word(base, 0x83139458) == applied_tactics)
    return applied_tactics + 1134 + id * 52;
  const auto found = overrides.find({"FORM", id});
  if (found == overrides.end())
    return 0;
  const auto at = Address(base, "FORM", id);
  if (at)
    std::memcpy(base + at, found->second.data(), 52);
  return at;
}
inline int EditedRole(uint8_t *base, unsigned id, unsigned slot) {
  Load();
  IndexOverrides();
  if (!any_roles)
    return -1;
  id = ResolveTeam(base, id);
  if (slot >= 6 || id >= count || !has_roles[id])
    return -1;
  const auto found = overrides.find({"ROLES", id});
  return found == overrides.end() ? -1 : int(found->second[slot]);
}
inline void Configuration(uint8_t *base, uint32_t record, unsigned id) {
  Load();
  IndexOverrides();
  if (!any_configuration && !any_roles && !any_formation)
    return;
  id = ResolveTeam(base, id);
  if (id >= count || (!has_configuration[id] && !has_roles[id] && !has_formation[id]))
    return;
  // A getter must not continually reset live pre-match/in-match changes.
  if (configured_records[id] == record && configured_revisions[id] == revision)
    return;
  if (!Readable(base, record, 406))
    return;
  configured_records[id] = record;
  configured_revisions[id] = revision;
  if (const auto config = overrides.find({"CONFIG", id}); config != overrides.end())
    std::memcpy(base + record + 48, config->second.data(), 358);
  if (const auto roles = overrides.find({"ROLES", id}); roles != overrides.end())
    std::memcpy(base + record + 48 + 105, roles->second.data(), 6);
  const auto form = overrides.find({"FORM", id});
  if (has_configuration[id])
    return; // Full configuration retains its three distinct plans.
  if (form == overrides.end())
    return;
  const auto &bytes = form->second;
  // 8217DDD8 decodes a 52-byte database formation into each native 82-byte plan.
  for (unsigned plan = 0; plan < 3; plan++) {
    const auto at = record + 48 + 111 + plan * 82;
    base[at] = 1;
    base[at + 21] = 0;
    for (unsigned i = 0; i < 10; i++) {
      base[at + 1 + i * 2] = bytes[10 + i];
      base[at + 2 + i * 2] = bytes[20 + i];
      base[at + 22 + i] = bytes[i];
    }
    for (unsigned i = 0; i < 11; i++) {
      base[at + 32 + i] = bytes[30 + i];
      base[at + 43 + i] = bytes[41 + i];
      base[at + 54 + i] = i == 0 || bytes[i - 1] != 4;
    }
  }
}
inline void Apply(uint8_t *base) {
  Load();
  if (!loaded)
    return;
  const auto db = Word(base, 0x83139440), tactics = Word(base, 0x83139458);
  if (!dirty && db == applied_db && tactics == applied_tactics)
    return;
  if (!Readable(base, db + 156, 46) || !base[db + 156] || !Readable(base, tactics + 1144, 20) ||
      !base[tactics + 1144] || !base[tactics + 1154])
    return;
  bool complete = true;
  for (const auto &[key, bytes] : overrides) {
    if (key.first == "CONFIG" || key.first == "SAVED")
      continue; // Applied by the native configuration getter.
    const auto at = Address(base, key.first, key.second);
    if (at)
      std::memcpy(base + at, bytes.data(), bytes.size());
    else
      complete = false;
  }
  if (complete) {
    dirty = false;
    applied_db = db;
    applied_tactics = tactics;
  }
}
inline bool Read(uint8_t *base, unsigned id, Squad &squad) {
  Apply(base);
  if (!loaded)
    return false;
  const auto roster = Address(base, "ROSTER", id), numbers = Address(base, "NUMBERS", id),
             formation = Address(base, "FORM", id), roles = Address(base, "ROLES", id);
  if (!roster || !numbers || !formation || !roles)
    return false;
  for (unsigned i = 0; i < 32; i++)
    squad.players[i] = uint16_t(unsigned(base[roster + i * 2]) << 8 | base[roster + i * 2 + 1]);
  std::memcpy(squad.numbers.data(), base + numbers, 32);
  std::memcpy(squad.formation.data(), base + formation, 52);
  std::memcpy(squad.roles.data(), base + roles, 6);
  if (const auto saved = overrides.find({"SAVED", id}); saved != overrides.end())
    std::memcpy(squad.saved.data(), saved->second.data(), squad.saved.size());
  const auto record = Word(base, 0x83139440) + 915740 + id * 406;
  if (Readable(base, record, 406)) {
    Configuration(base, record, id);
    std::memcpy(squad.configuration.data(), base + record + 48, 358);
  }
  return true;
}
inline void Set(unsigned id, const Squad &squad, const Squad *original = nullptr) {
  std::vector<uint8_t> roster;
  for (auto player : squad.players) {
    roster.push_back(uint8_t(player >> 8));
    roster.push_back(uint8_t(player));
  }
  if (!original || squad.players != original->players)
    overrides[{"ROSTER", id}] = std::move(roster);
  if (!original || squad.numbers != original->numbers)
    overrides[{"NUMBERS", id}] = {squad.numbers.begin(), squad.numbers.end()};
  if (!original || squad.formation != original->formation)
    overrides[{"FORM", id}] = {squad.formation.begin(), squad.formation.end()};
  if (!original || squad.roles != original->roles)
    overrides[{"ROLES", id}] = {squad.roles.begin(), squad.roles.end()};
  if ((!original || squad.configuration != original->configuration) &&
      std::any_of(squad.configuration.begin(), squad.configuration.end(),
                  [](uint8_t v) { return v != 0; }))
    overrides[{"CONFIG", id}] = {squad.configuration.begin(), squad.configuration.end()};
  if ((!original || squad.saved != original->saved) &&
      std::any_of(squad.saved.begin(), squad.saved.end(), [](uint8_t v) { return v != 0; }))
    overrides[{"SAVED", id}] = {squad.saved.begin(), squad.saved.end()};
  ++revision;
}
inline bool Save() {
  std::error_code error;
  std::filesystem::create_directories(store.parent_path(), error);
  if (error)
    return false;
  auto temporary = store;
  temporary += L".tmp";
  std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
  out << "PES6_TEAM_MANAGEMENT_V1\n";
  for (const auto &[key, bytes] : overrides) {
    out << key.first << ' ' << std::dec << key.second << ' ';
    for (auto value : bytes)
      out << std::hex << std::setw(2) << std::setfill('0') << unsigned(value);
    out << '\n';
  }
  out.flush();
  if (!out)
    return false;
  out.close();
  if (!MoveFileExW(temporary.c_str(), store.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
    return false;
  dirty = true;
  ++revision;
  return true;
}
// Persist the native editor's completed tactical setup without committing
// unrelated roster, shirt-number or identity drafts from the outer editor.
inline bool SaveTactics(unsigned id, const Squad &draft, Squad &original) {
  Squad persisted = original;
  persisted.configuration = draft.configuration;
  persisted.formation = draft.formation;
  persisted.roles = draft.roles;
  persisted.saved = draft.saved;
  if (persisted == original)
    return true;
  if (!BeginTransaction())
    return false;
  const auto previous = overrides;
  Set(id, persisted, &original);
  if (!Save() || !CommitTransaction()) {
    overrides = previous;
    dirty = true;
    ++revision;
    Recover();
    return false;
  }
  original = std::move(persisted);
  return true;
}
} // namespace pes6::teams
