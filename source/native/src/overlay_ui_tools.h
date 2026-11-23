#pragma once
#include <atomic>
#include <mutex>
#include <string>
#include <cstdint>

namespace pes6::ui_tools {

struct KeyboardState {
  std::mutex mutex;
  std::atomic<bool> active{false};
  std::atomic<bool> finished{false};
  bool accepted = false;
  std::string text;
  uint32_t max_chars = 63;
  int row = 1, col = 1;
  bool shift = false;
  bool symbols = false;
  bool accents = false;
  size_t cursor = 0;
  bool left_trigger = false, right_trigger = false;
  uint64_t last_move = 0;
  uint16_t prev_buttons = 0;
  uint8_t* guest_base = nullptr;
  uint32_t out_ptr = 0;
  uint32_t ov_ptr = 0;
};

struct ColorPickerState {
  std::mutex mutex;
  std::atomic<bool> active{false};
  std::atomic<bool> finished{false};
  bool accepted = false;
  float rgba[4]{1.0f, 1.0f, 1.0f, 1.0f};
  std::string label;
};

inline KeyboardState g_keyboard;
inline ColorPickerState g_color_picker;

} // namespace pes6::ui_tools
