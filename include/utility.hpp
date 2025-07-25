#ifndef UTILITY_HPP
#define UTILITY_HPP
#define KJ_LOG_LEVEL KJ_LOG_SEVERITY_INFO
#include <kj/async-io.h>
#include <kj/debug.h>
#include <kj/std/iostream.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>

inline std::string get_current_time_string() {
  using namespace std::chrono;

  const auto now = system_clock::now();
  const auto in_time_t = system_clock::to_time_t(now);
  const auto fractional =
      duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  const std::tm local_time = *std::localtime(&in_time_t);

  std::ostringstream oss;
  oss << "[" << std::put_time(&local_time, "%H:%M:%S") << '.' << std::setw(3)
      << std::setfill('0') << fractional.count() << "]";

  return oss.str();
}

class AsyncLogQueue {
 private:
  static std::queue<std::string> log_queue_;
  static std::mutex queue_mutex_;
  static std::condition_variable cv_;
  static std::atomic<bool> stop_flag_;
  static std::thread worker_thread_;
  static std::atomic<bool> initialized_;

  static void worker() {
    while (true) {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      cv_.wait(lock, [] { return !log_queue_.empty() || stop_flag_; });

      while (!log_queue_.empty()) {
        std::cout << log_queue_.front() << std::flush;
        log_queue_.pop();
      }

      if (stop_flag_ && log_queue_.empty()) {
        break;
      }
    }
  }

  static void ensureInitialized() {
    static std::once_flag init_flag;
    std::call_once(init_flag, []() { start(); });
  }

 public:
  static void start() {
    if (!initialized_.exchange(true)) {
      worker_thread_ = std::thread(worker);
    }
  }

  static void stop() {
    if (initialized_) {
      stop_flag_ = true;
      cv_.notify_all();
      if (worker_thread_.joinable()) {
        worker_thread_.join();
      }
    }
  }

  class LogStream {
   private:
    std::ostringstream buffer_;

   public:
    // デフォルトコンストラクタ
    LogStream() = default;

    // ムーブコンストラクタ
    LogStream(LogStream&& other) noexcept : buffer_(std::move(other.buffer_)) {}

    // ムーブ代入演算子
    LogStream& operator=(LogStream&& other) noexcept {
      if (this != &other) {
        buffer_ = std::move(other.buffer_);
      }
      return *this;
    }

    // コピーコンストラクタとコピー代入演算子を削除
    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

    template <typename T>
    LogStream& operator<<(T&& value) {
      buffer_ << std::forward<T>(value);
      return *this;
    }

    LogStream& operator<<(std::ostream& (*manip)(std::ostream&)) {
      buffer_ << manip;
      return *this;
    }

    LogStream& operator<<(std::ios_base& (*manip)(std::ios_base&)) {
      buffer_ << manip;
      return *this;
    }

    ~LogStream() {
      ensureInitialized();
      std::lock_guard<std::mutex> lock(queue_mutex_);
      log_queue_.push(buffer_.str());
      cv_.notify_one();
    }
  };

  static LogStream createStream(const char* file, int line) {
    LogStream stream;
    stream << get_current_time_string() << "[" << file << ":" << line << "]";
    return stream;
  }
};

// 静的メンバの定義
std::queue<std::string> AsyncLogQueue::log_queue_;
std::mutex AsyncLogQueue::queue_mutex_;
std::condition_variable AsyncLogQueue::cv_;
std::atomic<bool> AsyncLogQueue::stop_flag_{false};
std::thread AsyncLogQueue::worker_thread_;
std::atomic<bool> AsyncLogQueue::initialized_{false};

#define LOG_COUT AsyncLogQueue::createStream(__FILE__, __LINE__)

// プログラム終了時のクリーンアップ用クラス
class LogCleanup {
 public:
  ~LogCleanup() { AsyncLogQueue::stop(); }
};

// グローバルインスタンスでプログラム終了時にクリーンアップ
static LogCleanup log_cleanup;

#endif  // UTILITY_HPP