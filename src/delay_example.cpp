// delay_example.cpp ----------------------------------------------------------
#include <kj/async-io.h>
#include <kj/debug.h>

#include <chrono>
#include <iostream>
#include <utility.hpp>

// -----------------------------------------------------------------------------
// 再利用可能な「100 ms刻みスリープ」タスク
class ReusableTask {
 public:
  ReusableTask(kj::Timer& timer, uint32_t totalMs)
      : timer(timer), remaining(totalMs) {}

  kj::Promise<void> start() { return run(); }

 private:
  kj::Timer& timer;
  uint32_t remaining;

  kj::Promise<void> run() {
    if (remaining == 0) {
      LOG_COUT << "[ReusableTask] Task complete.\n";
      return kj::READY_NOW;
    }
    LOG_COUT << "[ReusableTask] Waiting... Remaining = " << remaining
             << " ms\n";
    remaining -= 100;
    return timer.afterDelay(100 * kj::MILLISECONDS).then([this] {
      return run();
    });
  }
};

// -----------------------------------------------------------------------------
// タイムアウト付き実行（失敗時は false・例外を投げない）
kj::Promise<bool> timeoutSafe(kj::Promise<void>&& task, kj::Timer& timer,
                              kj::Duration timeout) {
  // キャンセラとフラグをヒープに確保して attach() で保持する
  auto cancelerOwn = kj::heap<kj::Canceler>();
  auto doneOwn = kj::heap<bool>(false);
  auto* canceler = cancelerOwn.get();
  auto* doneFlag = doneOwn.get();

  auto cancelableTask = canceler->wrap(kj::mv(task));

  // ① タスク完了時
  auto guardedTask = kj::mv(cancelableTask)
                         .then([doneFlag] {
                           *doneFlag = true;
                           LOG_COUT << "[timeoutSafe] Task completed.\n";
                           return true;
                         })
                         .catch_([doneFlag](kj::Exception&& e) {
                           if (!*doneFlag) {  // キャンセル or 失敗
                             LOG_COUT
                                 << "[timeoutSafe] Task cancelled / failed: "
                                 << e.getDescription().cStr() << '\n';
                           }
                           return false;
                         });

  // ② タイムアウト時
  auto timeoutP = timer.afterDelay(timeout).then([doneFlag, canceler] {
    if (!*doneFlag) {
      LOG_COUT << "[timeoutSafe] Timeout -> cancelling task …\n";
      canceler->cancel("timeout");  // 強制キャンセル
      return false;
    }
    return true;  // 既に完了
  });

  // ③ どちらか早い方
  return guardedTask.exclusiveJoin(kj::mv(timeoutP))
      .attach(kj::mv(cancelerOwn), kj::mv(doneOwn));
}

// -----------------------------------------------------------------------------
// メイン
int main() {
  try {
    auto io = kj::setupAsyncIo();
    auto& timer = io.provider->getTimer();
    auto& ws = io.waitScope;

    // ---------- Task1: 5 s 仕事, timeout 1 s ----------
    LOG_COUT << "[Main] Task1 (5 s) / timeout 1 s …\n";
    ReusableTask t1(timer, 5000);
    auto s1 = std::chrono::steady_clock::now();
    bool r1 = timeoutSafe(t1.start(), timer, 1 * kj::SECONDS).wait(ws);
    auto e1 = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - s1)
                  .count();
    LOG_COUT << "[Main] Task1 done (result=" << r1 << ") elapsed=" << e1
             << " ms\n";

    // ---------- Task2: 1 s 仕事, timeout 0.5 s ----------
    LOG_COUT << "[Main] Task2 (1 s) / timeout 0.5 s …\n";
    ReusableTask t2(timer, 1000);
    auto s2 = std::chrono::steady_clock::now();
    bool r2 = timeoutSafe(t2.start(), timer, 500 * kj::MILLISECONDS).wait(ws);
    auto e2 = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - s2)
                  .count();
    LOG_COUT << "[Main] Task2 done (result=" << r2 << ") elapsed=" << e2
             << " ms\n";

    // ---------- Task3: 無限ループ, timeout 2 s ----------
    LOG_COUT << "[Main] Task3 (∞) / timeout 2 s …\n";
    auto task3 = []() -> kj::Promise<void> {
      return kj::evalLater([]-> kj::Promise<void> {
        auto start = std::chrono::steady_clock::now();
        auto last = start;
        while (true) {
          auto now = std::chrono::steady_clock::now();
          if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last)
                  .count() >= 100) {
            LOG_COUT << "[Task3] Elapsed "
                     << std::chrono::duration_cast<std::chrono::milliseconds>(
                            now - start)
                            .count()
                     << " ms\n";
            last = now;
          }
          for (volatile int i = 0; i < 1000; ++i);  // busy wait 抑制

          // 10秒後に強制終了
          if (std::chrono::duration_cast<std::chrono::seconds>(now - start)
                  .count() >= 10) {
            LOG_COUT << "[Task3] Force exit after 10 seconds.\n";
            break;
          }
        }

        return kj::READY_NOW;  // 実際にはここには到達しない
      });
    };

    auto s3 = std::chrono::steady_clock::now();
    bool r3 = timeoutSafe(task3(), timer, 2 * kj::SECONDS).wait(ws);
    auto e3 = std::chrono::duration_cast<std::chrono::milliseconds>(
                  std::chrono::steady_clock::now() - s3)
                  .count();
    LOG_COUT << "[Main] Task3 done (result=" << r3 << ") elapsed=" << e3
             << " ms\n";

  } catch (kj::Exception& e) {
    LOG_COUT << "[Main] Exception: " << e.getDescription().cStr() << '\n';
  }
  return 0;
}
