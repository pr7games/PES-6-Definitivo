#pragma once
#include <array>
#include <cstdint>
#include "team_coach_fade.h"
namespace pes6::team_presentation {
inline constexpr std::array<unsigned,4> kit_variants{1,3,0,2};
struct Player {
  uint16_t id=0;
  bool keeper=false;
  std::array<unsigned,26> ability{};
};
// The game has no single overall rating: rank field players by the mean of
// their 25 field attributes; keepers by goalkeeping, then useful keeper skills.
inline unsigned Score(const Player& p) {
  if(p.keeper)return p.ability[25]*10000+p.ability[6]+p.ability[7]+p.ability[20];
  unsigned score=0;for(unsigned i=0;i<25;i++)score+=p.ability[i];return score;
}
inline std::array<uint16_t,4> PreviewPlayers(const std::array<Player,32>& players,
                                           const std::array<uint8_t,358>& configuration) {
  uint16_t starter=0,backup=0,first=0,second=0,fallback=0;
  unsigned backup_score=0,first_score=0,second_score=0;
  for(const auto& p:players)if(p.id){fallback=p.id;break;}
  for(unsigned i=0;i<11;i++){
    const auto slot=configuration[i];
    if(slot<32&&players[slot].id&&players[slot].keeper){starter=players[slot].id;break;}
  }
  // Stable ties follow roster order; no arbitrary player gets selected twice.
  for(const auto& p:players)if(p.id){
    const auto score=Score(p);
    if(p.keeper){if(p.id!=starter&&(!backup||score>backup_score)){backup=p.id;backup_score=score;}}
    else if(!first||score>first_score){second=first;second_score=first_score;first=p.id;first_score=score;}
    else if(p.id!=first&&(!second||score>second_score)){second=p.id;second_score=score;}
  }
  if(!starter){starter=backup;backup=0;backup_score=0;
    for(const auto& p:players)if(p.id&&p.keeper&&p.id!=starter&&(!backup||Score(p)>backup_score)){
      backup=p.id;backup_score=Score(p);
    }
  }
  if(!first)first=fallback;if(!second)second=first;
  if(!starter)starter=fallback;if(!backup)backup=starter;
  // Native storage order: goalkeeper home, field home, goalkeeper away, field away.
  return {starter,first,backup,second};
}
struct CoachClock {
  uint64_t entered=0;
  unsigned index=0;
  uint32_t screen=0;
  bool visible=false,seen=false;
  bool Tick(uint64_t now,bool show,uint32_t next_screen=0){
    if(!show){visible=false;if(next_screen)screen=next_screen;return false;}
    const bool changed=!seen||screen!=next_screen;
    if(changed){if(seen)index=(index+1)%5;entered=now;seen=true;screen=next_screen;}
    visible=true;return changed;
  }
  unsigned Alpha(uint64_t now) const {
    const auto frame=now>=entered?(now-entered)*60/1000:0;
    return 255-team_coach_fade_mask[frame<40?unsigned(frame):39];
  }
};
}
