#pragma once
#include <cstdint>
#include <string>
namespace pes6::editor {
void InitializeSetPieceGameplay(const std::string& root);
bool SetPieceGameplayReady();
bool SetPieceFKPenaltyReady();
bool SetPiecePenaltyReady(unsigned style);
void ClearExtendedPenalties();
void RegisterPlayerSetPieces(unsigned id,int freeKick,int penalty);
}
class PES6SetPieceStyleAlias {
  uint8_t* base_;uint32_t player_=0,saved_=0,alias_=0;
public:
  PES6SetPieceStyleAlias(uint8_t*,uint32_t,bool wrapped,bool select_bank=true);
  ~PES6SetPieceStyleAlias();
};
extern "C" uint32_t pes6_setpiece_load_file(uint32_t bank,uint32_t file);
extern "C" uint32_t pes6_setpiece_query_file(uint32_t bank,uint32_t file);
extern "C" uint32_t pes6_setpiece_lookup_file(uint32_t file);
extern "C" uint32_t pes6_setpiece_penalty_shot(uint8_t*,uint32_t clip);
extern "C" uint32_t pes6_setpiece_reverse_clip(uint8_t*,uint32_t clip);
extern "C" uint32_t pes6_setpiece_dispatch_style(uint8_t*,uint32_t style);
extern "C" uint32_t pes6_setpiece_native_style(uint8_t*,uint32_t player,uint32_t style);
struct PPCContext;
extern "C" bool pes6_setpiece_resource(PPCContext&,uint8_t*);
extern "C" uint32_t pes6_converted_sampler_frames(uint8_t*,uint32_t header,uint32_t frames);
extern "C" void pes6_penalty_initial_placement(uint8_t*,uint32_t player);
extern "C" bool pes6_penalty_hold_finish(uint8_t*,uint32_t object,uint32_t requested,bool wrapper);
class PES6PenaltyPlacementScope {
  uint8_t* base_;uint32_t player_;
public:
  PES6PenaltyPlacementScope(uint8_t*,uint32_t);
  ~PES6PenaltyPlacementScope();
};

extern "C" uint32_t pes6_openplay_motion(PPCContext&,uint8_t*,uint32_t controller,uint32_t clip,uint32_t original);

class PES6OpenPlayActorScope { uint32_t previous_; public:explicit PES6OpenPlayActorScope(uint32_t);~PES6OpenPlayActorScope(); };
extern "C" void pes6_openplay_replay_pose(PPCContext&,uint8_t*,uint32_t controller);
extern "C" void pes6_openplay_record(uint8_t*,uint32_t controller,uint32_t payload);
extern "C" void pes6_openplay_record_compact(uint8_t*,uint32_t controller,uint32_t payload);
extern "C" uint32_t pes6_openplay_compact_motion(PPCContext&,uint8_t*,uint32_t controller,uint32_t payload,uint32_t original);
extern "C" uint32_t pes6_openplay_decode_id(uint32_t);
extern "C" double pes6_replay_wrap_clock(double,double);
extern "C" void pes6_cr7_chop_arm(uint8_t*,uint32_t actor);
extern "C" void pes6_openplay_probe(PPCContext&,uint8_t*,uint32_t stage,uint32_t object,uint32_t payload);
class PES6OpenPlayReplayScope { int previous_[2]; public:PES6OpenPlayReplayScope(uint8_t*,uint32_t);~PES6OpenPlayReplayScope(); };
