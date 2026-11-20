#pragma once
#include <atomic>
#include <cstdint>
#include <array>

// Counts guest requests to present a frame, not repeated host UI paints.
inline std::atomic<uint64_t> pes6_frame_submissions{0};
inline std::atomic<uint32_t> pes6_frontbuffer_width{0};
inline std::atomic<uint32_t> pes6_frontbuffer_height{0};
inline std::atomic<uint32_t> pes6_render_scale{1};
inline std::atomic<uint32_t> pes6_target_fps{60};
inline std::atomic<uint32_t> pes6_requested_interval{1};
inline std::atomic<uint32_t> pes6_scene_flags{0};
inline std::atomic<uint64_t> pes6_pacing_generation{0};
// Rolling submission intervals (not GPU timestamps or monitor refreshes).
inline std::array<std::atomic<uint32_t>, 240> pes6_frame_intervals_us{};
inline std::atomic<uint32_t> pes6_last_work_us{0};
inline std::atomic<uint32_t> pes6_last_limit_wait_us{0};
void pes6_start_timing();
void pes6_stop_timing();

