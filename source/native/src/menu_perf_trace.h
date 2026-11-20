#pragma once
#include <windows.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace pes6_menu_perf {
struct Counters {
  std::atomic<uint64_t> vdswap_us{0};
  std::atomic<uint64_t> pace_us{0};
  std::atomic<uint64_t> wait_us{0};
  std::atomic<uint64_t> ui_update_us{0};
  std::atomic<uint64_t> ui_draw_us{0};
  std::atomic<uint64_t> preview_draw_us{0};
  std::atomic<uint64_t> editor_frame_us{0};
  std::atomic<uint64_t> ui_ctrl_us{0};
  std::atomic<uint64_t> ui_gc_us{0};
  std::atomic<uint64_t> ui_roots_us{0};
  std::atomic<uint64_t> top_root_us{0};
  std::atomic<uint32_t> top_root_vtable{0};
  std::atomic<uint64_t> top_child_us{0};
  std::atomic<uint32_t> top_child_vtable{0};
  std::atomic<uint32_t> cab0_calls{0};
  std::atomic<uint32_t> b270_calls{0};
  std::atomic<uint32_t> frames{0};
};
inline Counters counters;
inline std::atomic<uint32_t> fx_nodes_count{0}, fx_geom_count{0}, fx_frames_count{0}, fx_retired_count{0};
inline std::atomic<uint32_t> preview_state_bits{0};

inline uint64_t NowUs() {
  return uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count());
}

inline unsigned CountUiList(uint8_t* base, uint32_t offset) {
  auto readable = [&](uint32_t at, size_t len) {
    if (at < 0x10000 || uint64_t(at) + len >= 0xe0000000ull) return false;
    MEMORY_BASIC_INFORMATION info{};
    return VirtualQuery(base + at, &info, sizeof(info)) && info.State == MEM_COMMIT &&
           !(info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
           base + at + len <= static_cast<uint8_t*>(info.BaseAddress) + info.RegionSize;
  };
  const uint32_t list = REX_LOAD_U32(0x82d90000u - 21328u);
  if (!readable(list, 20)) return 0;
  const uint32_t sentinel = REX_LOAD_U32(list + offset);
  if (!readable(sentinel, 12)) return 0;
  unsigned count = 0;
  for (uint32_t link = REX_LOAD_U32(sentinel);
       count < 4096 && link != sentinel && readable(link, 12);
       link = REX_LOAD_U32(link)) {
    ++count;
  }
  return count;
}

inline void FlushIfDue(uint8_t* base, uint32_t req_interval) {
  static const std::filesystem::path log_dir = []() {
    wchar_t file[MAX_PATH]{};
    GetModuleFileNameW(nullptr, file, MAX_PATH);
    auto root = std::filesystem::path(file).parent_path();
    for (unsigned i = 0; i < 6 && !std::filesystem::exists(root / "game/cv.img"); ++i)
      root = root.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(root / "data/logs", ec);
    return root / "data/logs";
  }();
  static const std::filesystem::path log_path = log_dir / "menu-perf-trace.tsv";
  static const std::filesystem::path spike_path = log_dir / "menu-perf-spikes.tsv";

  // Per-frame spike detector (> 19 ms frame delta) with minimal overhead on 60fps frames.
  static std::atomic<uint64_t> last_frame_us{0};
  static std::atomic<uint64_t> prev_up{0}, prev_dr{0}, prev_pv{0}, prev_ef{0}, prev_ur{0};
  static std::atomic<uint32_t> prev_c0{0}, prev_b2{0}, spike_count{0};
  const uint64_t now_us = NowUs();
  const uint64_t prev_us = last_frame_us.exchange(now_us, std::memory_order_relaxed);
  const uint64_t cur_up = counters.ui_update_us.load(std::memory_order_relaxed);
  const uint64_t cur_dr = counters.ui_draw_us.load(std::memory_order_relaxed);
  const uint64_t cur_pv = counters.preview_draw_us.load(std::memory_order_relaxed);
  const uint64_t cur_ef = counters.editor_frame_us.load(std::memory_order_relaxed);
  const uint64_t cur_ur = counters.ui_roots_us.load(std::memory_order_relaxed);
  const uint32_t cur_c0 = counters.cab0_calls.load(std::memory_order_relaxed);
  const uint32_t cur_b2 = counters.b270_calls.load(std::memory_order_relaxed);
  const uint64_t p_up = prev_up.exchange(cur_up, std::memory_order_relaxed);
  const uint64_t p_dr = prev_dr.exchange(cur_dr, std::memory_order_relaxed);
  const uint64_t p_pv = prev_pv.exchange(cur_pv, std::memory_order_relaxed);
  const uint64_t p_ef = prev_ef.exchange(cur_ef, std::memory_order_relaxed);
  const uint64_t p_ur = prev_ur.exchange(cur_ur, std::memory_order_relaxed);
  const uint32_t p_c0 = prev_c0.exchange(cur_c0, std::memory_order_relaxed);
  const uint32_t p_b2 = prev_b2.exchange(cur_b2, std::memory_order_relaxed);
  if (prev_us && now_us - prev_us > 19000 && spike_count.load(std::memory_order_relaxed) < 2000) {
    static std::mutex spike_mutex;
    static std::ofstream sout(spike_path, std::ios::trunc);
    static bool spike_hdr = false;
    std::lock_guard slock(spike_mutex);
    const uint32_t idx = spike_count.fetch_add(1, std::memory_order_relaxed);
    if (sout) {
      if (!spike_hdr) {
        sout << "tick_ms\tframe_dt_ms\tui_update_ms\tui_roots_ms\ttop_root_ms\ttop_vtable\ttop_child_ms\ttop_child_vtable\tui_draw_ms\tpreview_draw_ms\teditor_frame_ms\tcab0_delta\tb270_delta\tui_roots\tui_ctrls\tfx_nodes\tfx_frames\tpreview_bits\n";
        spike_hdr = true;
      }
      sout << GetTickCount64() << '\t' << ((now_us - prev_us) / 1000.0) << '\t'
           << ((cur_up >= p_up ? cur_up - p_up : cur_up) / 1000.0) << '\t'
           << ((cur_ur >= p_ur ? cur_ur - p_ur : cur_ur) / 1000.0) << '\t'
           << (counters.top_root_us.load(std::memory_order_relaxed) / 1000.0) << '\t'
           << std::hex << counters.top_root_vtable.load(std::memory_order_relaxed) << std::dec << '\t'
           << (counters.top_child_us.load(std::memory_order_relaxed) / 1000.0) << '\t'
           << std::hex << counters.top_child_vtable.load(std::memory_order_relaxed) << std::dec << '\t'
           << ((cur_dr >= p_dr ? cur_dr - p_dr : cur_dr) / 1000.0) << '\t'
           << ((cur_pv >= p_pv ? cur_pv - p_pv : cur_pv) / 1000.0) << '\t'
           << ((cur_ef >= p_ef ? cur_ef - p_ef : cur_ef) / 1000.0) << '\t'
           << (cur_c0 >= p_c0 ? cur_c0 - p_c0 : cur_c0) << '\t'
           << (cur_b2 >= p_b2 ? cur_b2 - p_b2 : cur_b2) << '\t'
           << CountUiList(base, 4) << '\t' << CountUiList(base, 16) << '\t'
           << fx_nodes_count.load(std::memory_order_relaxed) << '\t'
           << fx_frames_count.load(std::memory_order_relaxed) << '\t'
           << preview_state_bits.load(std::memory_order_relaxed) << '\n';
      if ((idx & 31u) == 31u) sout.flush();
    }
  }

  counters.frames.fetch_add(1, std::memory_order_relaxed);
  static std::mutex mutex;
  static ULONGLONG last_flush = 0;
  const auto now = GetTickCount64();
  if (last_flush && now - last_flush < 1000) return;
  std::lock_guard lock(mutex);
  if (last_flush && now - last_flush < 1000) return;
  const bool first = (last_flush == 0);
  last_flush = now;
  const uint32_t fps = counters.frames.exchange(0, std::memory_order_relaxed);
  if (first) return;
  const uint64_t vd = counters.vdswap_us.exchange(0, std::memory_order_relaxed);
  const uint64_t pa = counters.pace_us.exchange(0, std::memory_order_relaxed);
  const uint64_t wa = counters.wait_us.exchange(0, std::memory_order_relaxed);
  const uint64_t up = counters.ui_update_us.exchange(0, std::memory_order_relaxed);
  const uint64_t dr = counters.ui_draw_us.exchange(0, std::memory_order_relaxed);
  const uint64_t pv = counters.preview_draw_us.exchange(0, std::memory_order_relaxed);
  const uint64_t ef = counters.editor_frame_us.exchange(0, std::memory_order_relaxed);
  const uint64_t uc = counters.ui_ctrl_us.exchange(0, std::memory_order_relaxed);
  const uint64_t ug = counters.ui_gc_us.exchange(0, std::memory_order_relaxed);
  const uint64_t ur = counters.ui_roots_us.exchange(0, std::memory_order_relaxed);
  const uint64_t tr = counters.top_root_us.exchange(0, std::memory_order_relaxed);
  const uint32_t tv = counters.top_root_vtable.exchange(0, std::memory_order_relaxed);
  const uint64_t tc = counters.top_child_us.exchange(0, std::memory_order_relaxed);
  const uint32_t tcv = counters.top_child_vtable.exchange(0, std::memory_order_relaxed);
  const uint32_t c0 = counters.cab0_calls.exchange(0, std::memory_order_relaxed);
  const uint32_t b2 = counters.b270_calls.exchange(0, std::memory_order_relaxed);
  prev_up.store(0, std::memory_order_relaxed);
  prev_dr.store(0, std::memory_order_relaxed);
  prev_pv.store(0, std::memory_order_relaxed);
  prev_ef.store(0, std::memory_order_relaxed);
  prev_ur.store(0, std::memory_order_relaxed);
  prev_c0.store(0, std::memory_order_relaxed);
  prev_b2.store(0, std::memory_order_relaxed);
  static std::ofstream out(log_path, std::ios::trunc);
  static bool header_written = false;
  if (!out) return;
  if (!header_written) {
    out << "tick_ms\tfps\tvdswap_ms\tpace_ms\twait_ms\tui_update_ms\tui_ctrl_ms\tui_gc_ms\tui_roots_ms\ttop_root_ms\ttop_vtable\ttop_child_ms\ttop_child_vtable\tcab0_calls\tb270_calls\tui_draw_ms\tpreview_draw_ms\teditor_frame_ms\tui_roots\tui_ctrls\tfx_nodes\tfx_geom\tfx_frames\tfx_retired\treq_interval\tpreview_bits\n";
    header_written = true;
  }
  out << now << '\t' << fps << '\t'
      << (vd / 1000.0) << '\t' << (pa / 1000.0) << '\t' << (wa / 1000.0) << '\t' << (up / 1000.0) << '\t'
      << (uc / 1000.0) << '\t' << (ug / 1000.0) << '\t' << (ur / 1000.0) << '\t'
      << (tr / 1000.0) << '\t' << std::hex << tv << '\t'
      << std::dec << (tc / 1000.0) << '\t' << std::hex << tcv << std::dec << '\t'
      << c0 << '\t' << b2 << '\t'
      << (dr / 1000.0) << '\t' << (pv / 1000.0) << '\t'
      << (ef / 1000.0) << '\t' << CountUiList(base, 4) << '\t' << CountUiList(base, 16) << '\t'
      << fx_nodes_count.load(std::memory_order_relaxed) << '\t'
      << fx_geom_count.load(std::memory_order_relaxed) << '\t'
      << fx_frames_count.load(std::memory_order_relaxed) << '\t'
      << fx_retired_count.load(std::memory_order_relaxed) << '\t'
      << req_interval << '\t' << preview_state_bits.load(std::memory_order_relaxed) << '\n';
  out.flush();
}
}  // namespace pes6_menu_perf
