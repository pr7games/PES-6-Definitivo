#pragma once
#include <array>
namespace pes6::editor::preview {
// Xbox native selector -> stage -> package AND clip, not package alone.
// Sources: 82538BF8,82538F90,8254BD18/BD80,8254B590, and dedicated
// families 82552A18,8254D160,8254D4B8,8254C578,82553A40,8254CA60.
struct CelebrationClip { unsigned stage,file,clip; };
inline constexpr std::array<CelebrationClip,79> celebration_clips{{
  {63,2633,1634}, // selector 1
  {55,2635,1432}, // selector 2
  {46,2621,1634}, // selector 3
  {30,2629,1636}, // selector 4
  {31,2629,1637}, // selector 5
  {33,2631,1634}, // selector 6
  {41,2632,1638}, // selector 7
  {36,2631,1636}, // selector 8
  {28,2629,1635}, // selector 9
  {56,2617,1634}, // selector 10
  {51,2624,1638}, // selector 11
  {50,2624,1636}, // selector 12
  {43,2637,1634}, // selector 13
  {44,2637,1635}, // selector 14
  {66,2628,1635}, // selector 15
  {48,2624,1634}, // selector 16
  {49,2624,1635}, // selector 17
  {47,2617,1636}, // selector 18
  {57,2629,1634}, // selector 19
  {45,2633,1636}, // selector 20
  {42,2633,1635}, // selector 21
  {37,2631,1637}, // selector 22
  {52,2628,1636}, // selector 23
  {58,2626,1635}, // selector 24
  {65,2628,1634}, // selector 25
  {20,2624,1637}, // selector 26
  {67,2641,1634}, // selector 27
  {59,2626,1634}, // selector 28
  {64,2626,1638}, // selector 29
  {27,2628,1637}, // selector 30
  {39,2632,1636}, // selector 31
  {32,2660,1637}, // selector 32
  {35,2630,1634}, // selector 33
  {53,2637,1636}, // selector 34
  {34,2631,1635}, // selector 35
  {22,2626,1636}, // selector 36
  {11,0,1404}, // selector 37
  {15,2617,1635}, // selector 38
  {16,2627,1634}, // selector 39
  {62,2632,1635}, // selector 40
  {71,2641,1636}, // selector 41
  {80,2641,1637}, // selector 42
  {72,2641,1638}, // selector 43
  {82,2642,1634}, // selector 44
  {79,2642,1635}, // selector 45
  {76,2642,1636}, // selector 46
  {74,2642,1637}, // selector 47
  {77,2643,1634}, // selector 48
  {81,2643,1635}, // selector 49
  {75,2643,1636}, // selector 50
  {73,2644,1634}, // selector 51
  {78,2644,1635}, // selector 52
  {83,2644,1636}, // selector 53
  {84,2649,1636}, // selector 54
  {85,2649,1635}, // selector 55
  {86,2649,1637}, // selector 56
  {88,2649,1634}, // selector 57
  {90,2649,1638}, // selector 58
  {91,2651,1634}, // selector 59
  {89,2621,1637}, // selector 60
  {87,2645,1634}, // selector 61
  {101,2658,1635}, // selector 62
  {100,2658,1636}, // selector 63
  {103,2658,1634}, // selector 64
  {104,2658,1637}, // selector 65
  {105,2658,1638}, // selector 66
  {108,2659,1634}, // selector 67
  {109,2661,1636}, // selector 68
  {110,2659,1635}, // selector 69
  {111,2659,1636}, // selector 70
  {112,2659,1637}, // selector 71
  {113,2659,1638}, // selector 72
  {114,2660,1634}, // selector 73
  {115,2660,1635}, // selector 74
  {116,2660,1636}, // selector 75
  {117,2660,1638}, // selector 76
  {118,2661,1634}, // selector 77
  {119,2661,1635}, // selector 78
  {120,2617,1638}, // selector 79
}};
}
