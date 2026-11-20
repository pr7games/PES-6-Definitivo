#pragma once
#include <array>
#include <cstdint>
// Updated only by the native main-menu controller. Leaving is explicit:
// a slow frame, GPU stall, or pause is not a menu navigation event.
struct Pes6MenuCarouselClock {
  static constexpr unsigned portrait_count=10;
  // Messi (index 10 / 10.we) hidden pending a better cutout. Keep its assets.
  std::array<unsigned,portrait_count> order{0,1,2,3,4,5,6,7,8,9 /*,10*/};
  uint64_t entered=0,last_seen=0;unsigned slot=0,index=0;bool seen=false,timed_change=false,away=false;
  void Leave(){if(seen)away=true;}
  unsigned Visit(uint64_t now,unsigned initial=0){
    if(!seen){seen=true;entered=now;slot=initial%portrait_count;index=order[slot];}
    else if(away){slot=(slot+1)%portrait_count;index=order[slot];entered=now;timed_change=false;away=false;}
    else if(now>=entered&&now-entered>=3000){slot=(slot+1)%portrait_count;index=order[slot];entered=now;timed_change=true;}
    last_seen=now;return index;
  }
};
