#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace kchess::persistence {

struct SqliteWritePrioritySnapshot {
  std::size_t foreground_waiters{0};
  std::size_t foreground_sessions{0};
  std::size_t background_writers{0};
  std::uint64_t foreground_wait_count{0};
  std::uint64_t foreground_wait_total_ms{0};
  std::uint64_t background_wait_count{0};
  std::uint64_t background_wait_total_ms{0};
};

// Coordinates SQLite writers that use separate connections to the shared
// kchess.sqlite3 file. Foreground analysis owns priority for its entire active
// session. Knowledge/diagnostic writers are serialized and may only begin a new
// write while no foreground analysis is waiting or active. Existing short
// background transactions are allowed to finish before foreground takes over.
class SqliteWritePriorityGate {
 public:
  void begin_foreground() {
    const auto started = std::chrono::steady_clock::now();
    std::unique_lock lock(mutex_);
    ++foreground_waiters_;
    const bool blocked = background_writers_ != 0;
    if (blocked) ++foreground_wait_count_;
    cv_.wait(lock, [this] { return background_writers_ == 0; });
    if (blocked) {
      foreground_wait_total_ms_ += elapsed_ms(started);
    }
    --foreground_waiters_;
    ++foreground_sessions_;
    cv_.notify_all();
  }

  void end_foreground() noexcept {
    std::lock_guard lock(mutex_);
    if (foreground_sessions_ > 0) --foreground_sessions_;
    cv_.notify_all();
  }

  void begin_background_write() {
    const auto started = std::chrono::steady_clock::now();
    std::unique_lock lock(mutex_);
    const bool blocked = foreground_waiters_ != 0 || foreground_sessions_ != 0 ||
                         background_writers_ != 0;
    if (blocked) ++background_wait_count_;
    cv_.wait(lock, [this] {
      return foreground_waiters_ == 0 && foreground_sessions_ == 0 &&
             background_writers_ == 0;
    });
    if (blocked) {
      background_wait_total_ms_ += elapsed_ms(started);
    }
    ++background_writers_;
  }

  void end_background_write() noexcept {
    std::lock_guard lock(mutex_);
    if (background_writers_ > 0) --background_writers_;
    cv_.notify_all();
  }

  bool try_begin_background_write() {
    std::lock_guard lock(mutex_);
    if (foreground_waiters_ != 0 || foreground_sessions_ != 0 || background_writers_ != 0) return false;
    ++background_writers_;
    return true;
  }

  [[nodiscard]] SqliteWritePrioritySnapshot snapshot() {
    std::lock_guard lock(mutex_);
    return {
        .foreground_waiters = foreground_waiters_,
        .foreground_sessions = foreground_sessions_,
        .background_writers = background_writers_,
        .foreground_wait_count = foreground_wait_count_,
        .foreground_wait_total_ms = foreground_wait_total_ms_,
        .background_wait_count = background_wait_count_,
        .background_wait_total_ms = background_wait_total_ms_,
    };
  }

 private:
  static std::uint64_t elapsed_ms(
      const std::chrono::steady_clock::time_point started) noexcept {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
  }

  std::mutex mutex_;
  std::condition_variable cv_;
  std::size_t foreground_waiters_{0};
  std::size_t foreground_sessions_{0};
  std::size_t background_writers_{0};
  std::uint64_t foreground_wait_count_{0};
  std::uint64_t foreground_wait_total_ms_{0};
  std::uint64_t background_wait_count_{0};
  std::uint64_t background_wait_total_ms_{0};
};

inline SqliteWritePriorityGate& sqlite_write_priority_gate() {
  static SqliteWritePriorityGate gate;
  return gate;
}

class ForegroundSqlitePriorityLease {
 public:
  ForegroundSqlitePriorityLease() {
    sqlite_write_priority_gate().begin_foreground();
    active_ = true;
  }

  ~ForegroundSqlitePriorityLease() { reset(); }

  ForegroundSqlitePriorityLease(const ForegroundSqlitePriorityLease&) = delete;
  ForegroundSqlitePriorityLease& operator=(const ForegroundSqlitePriorityLease&) = delete;

  void reset() noexcept {
    if (!active_) return;
    active_ = false;
    sqlite_write_priority_gate().end_foreground();
  }

 private:
  bool active_{false};
};

class BackgroundSqliteWriteGuard {
 public:
  BackgroundSqliteWriteGuard() {
    sqlite_write_priority_gate().begin_background_write();
    active_ = true;
  }
  explicit BackgroundSqliteWriteGuard(std::try_to_lock_t)
      : active_(sqlite_write_priority_gate().try_begin_background_write()) {}
  [[nodiscard]] bool owns_lock() const noexcept { return active_; }

  ~BackgroundSqliteWriteGuard() { reset(); }

  BackgroundSqliteWriteGuard(const BackgroundSqliteWriteGuard&) = delete;
  BackgroundSqliteWriteGuard& operator=(const BackgroundSqliteWriteGuard&) = delete;

  void reset() noexcept {
    if (!active_) return;
    active_ = false;
    sqlite_write_priority_gate().end_background_write();
  }

 private:
  bool active_{false};
};

}  // namespace kchess::persistence
