#include <kj/async-io.h>
#include <kj/debug.h>

#include <iostream>
#include <utility.hpp>

/**
 * @class ReusableTask
 * @brief 指定された時間分だけ非同期に待機する再利用可能なタスク。
 */
class ReusableTask {
 public:
  /**
   * @brief コンストラクタ。
   * @param timer 非同期タイマーオブジェクトへの参照。
   * @param totalMs タスクが完了するまでの総待機時間（ミリ秒）。
   */
  ReusableTask(kj::Timer& timer, uint32_t totalMs)
      : timer(timer), remaining(totalMs) {}

  /**
   * @brief タスクの実行を開始する。
   * @return タスクの完了を表す Promise。
   */
  kj::Promise<void> start() { return run(); }

 private:
  kj::Timer& timer;    ///< 使用する kj::Timer の参照
  uint32_t remaining;  ///< 残りの待機時間（ミリ秒）

  /**
   * @brief タスク内部処理。100ms ごとに繰り返し待機。
   * @return タスク完了を表す Promise。
   */
  kj::Promise<void> run() {
    if (remaining == 0) {
      LOG_COUT << "[ReusableTask] Task complete.\n";
      return kj::READY_NOW;
    }

    LOG_COUT << "[ReusableTask] Waiting... Remaining = " << remaining << "ms\n";
    remaining -= 100;

    return timer.afterDelay(100 * kj::MILLISECONDS).then([this]() {
      return run();
    });
  }
};

/**
 * @brief タスクをタイムアウト付きで安全に実行する。
 *
 * タスクが完了すれば true、タイムアウトに達した場合は false を返す。
 * 例外はスローされない。
 *
 * @param task 実行する非同期タスク。
 * @param timer タイマーオブジェクト。
 * @param timeout タイムアウトの時間。
 * @return 完了フラグ（true = 成功, false = タイムアウト）。
 */
kj::Promise<bool> timeoutSafe(kj::Promise<void>&& task, kj::Timer& timer,
                              kj::Duration timeout) {
  auto timeoutPromise = timer.afterDelay(timeout).then([]() {
    LOG_COUT << "[ReusableTask] Timeout : Task timed out.\n";
    return false;
  });

  /*
   * exclusiveJoin は 2つの Promise を同時に待機し、
   * どちらかが完了した時点で結果を返す。
   *
   * ここでは task が完了した場合は true を返し、
   * timeoutPromise が完了した場合は false を返す。
   */
  return kj::mv(task)
      .then([]() {
        LOG_COUT << "[ReusableTask] Timeout : Task completed successfully.\n";
        return true;
      })
      .exclusiveJoin(kj::mv(timeoutPromise));
}

/**
 * @brief メイン関数。
 *
 * ReusableTask を 5 秒間実行するが、1 秒のタイムアウトで制御する。
 * タイムアウトしても例外をスローせずに終了する。
 */
int main() {
  try {
    auto io = kj::setupAsyncIo();
    kj::Timer& timer = io.provider->getTimer();
    kj::WaitScope& ws = io.waitScope;

    LOG_COUT << "[Main] Starting ReusableTask with timeout...\n";

    ReusableTask task(timer, 5000);
    auto promise = task.start();

    auto timed = timeoutSafe(kj::mv(promise), timer, 1 * kj::SECONDS);
    timed.wait(ws);

    LOG_COUT << "[Main] Task completed without timeout.\n";

    LOG_COUT << "[Main] Task2 Start \n";

    ReusableTask task2(timer, 1000);
    auto promise2 = task2.start();
    auto timeout2 =
        timeoutSafe(kj::mv(promise2), timer, 500 * kj::MILLISECONDS);

    LOG_COUT << "[Main] Task2 completed without timeout.\n";

  } catch (const kj::Exception& e) {
    LOG_COUT << "[Main] Task timeout or error: " << e.getDescription().cStr()
             << std::endl;
  }

  return 0;
}
