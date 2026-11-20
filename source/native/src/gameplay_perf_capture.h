#pragma once
#include <windows.h>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>

// Bounded producer/consumer: disk access stays off the presentation thread.
namespace pes6_gameplay_perf {
inline uint64_t Now() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
struct Row { uint64_t time, dt, work, pace, swap; uint32_t interval, width, height; };
class Capture {
  std::array<Row, 8192> queue_{};
  size_t head_ = 0, tail_ = 0;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::atomic<bool> stop_{false};
  std::atomic<uint64_t> dropped_{0};
  std::thread writer_;
  uint64_t previous_start_ = 0, previous_end_ = 0, origin_ = 0;
  bool enabled_ = false;
public:
  Capture() noexcept {
    try {
      wchar_t file[32768]{};
      if (!GetModuleFileNameW(nullptr, file, 32768)) return;
      auto root = std::filesystem::path(file).parent_path();
      for (int i = 0; i < 8 && !std::filesystem::exists(root / "game/cv.img"); ++i) {
        auto parent = root.parent_path();
        if (parent == root) return;
        root = parent;
      }
      if (!std::filesystem::exists(root / "data/logs/gameplay-perf.enabled")) return;
      auto dir = root / "data/logs/gameplay-performance";
      std::filesystem::create_directories(dir);
      SYSTEMTIME t{}; GetSystemTime(&t);
      wchar_t name[160]{};
      swprintf_s(name, L"session-%04u%02u%02u-%02u%02u%02u-%lu-%llu.csv",
          t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,
          GetCurrentProcessId(), static_cast<unsigned long long>(GetTickCount64()));
      const auto path = dir / name;
      writer_ = std::thread([this, path] { Write(path); });
      enabled_ = true;
    } catch (...) { enabled_ = false; }
  }
  ~Capture() { stop_.store(true); wake_.notify_one(); if (writer_.joinable()) writer_.join(); }
  bool Enabled() const { return enabled_; }
  void Record(uint64_t start, uint64_t before_pace, uint64_t before_swap,
              uint64_t end, uint32_t interval, uint32_t width, uint32_t height) {
    if (!origin_) origin_ = start;
    const Row r{start-origin_, previous_start_ ? start-previous_start_ : 0,
      previous_end_ ? start-previous_end_ : 0, before_swap-before_pace,
      end-before_swap, interval, width, height};
    previous_start_ = start; previous_end_ = end;
    std::unique_lock<std::mutex> lock(mutex_, std::try_to_lock);
    if (!lock.owns_lock() || (head_+1)%queue_.size() == tail_) { ++dropped_; return; }
    queue_[head_] = r; head_ = (head_+1)%queue_.size();
  }
private:
  void Write(const std::filesystem::path& path) noexcept {
    try {
      std::ofstream out(path);
      if (!out) return;
      out << "elapsed_ms,frame_start_interval_ms,outside_swap_wrapper_ms,pace_ms,backend_swap_ms,requested_interval,width,height,dropped_records\n";
      size_t written = 0;
      for (;;) {
        std::array<Row,8192> batch;
        size_t count = 0;
        {
          std::unique_lock<std::mutex> lock(mutex_);
          wake_.wait_for(lock, std::chrono::milliseconds(250), [this]{return stop_.load();});
          while (tail_ != head_) { batch[count++] = queue_[tail_]; tail_ = (tail_+1)%queue_.size(); }
        }
        for (size_t i=0; i<count; ++i) {
          const auto& r=batch[i];
          if (!r.dt) continue;
          out << r.time/1000.0 << ',' << r.dt/1000.0 << ',' << r.work/1000.0 << ','
              << r.pace/1000.0 << ',' << r.swap/1000.0 << ',' << r.interval << ','
              << r.width << ',' << r.height << ',' << dropped_.load() << '\n';
          if (++written >= 500000) { out << "# capture_limit_reached\n"; out.flush(); return; }
        }
        out.flush();
        if (stop_.load()) break;
      }
    } catch (...) { /* Diagnostic failure must not stop gameplay. */ }
  }
};
inline Capture& Get() { static Capture capture; return capture; }
}
