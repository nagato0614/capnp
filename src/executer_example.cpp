#include <kj/async-io.h>
#include <kj/debug.h>

#include <iostream>
#include <thread>
#include <utility.hpp>

// TaskSet用エラーハンドラ
class SimpleErrorHandler final : public kj::TaskSet::ErrorHandler {
 public:
  void taskFailed(kj::Exception&& e) override {
    LOG_COUT << "[TaskSet] Task failed: " << e.getDescription().cStr()
             << std::endl;
  }
};

int main() {
  auto io = kj::setupAsyncIo();
  auto& ws = io.waitScope;
  auto& timer = io.provider->getTimer();

  SimpleErrorHandler errorHandler;
  kj::TaskSet taskSet(errorHandler);
  auto& executor = kj::getCurrentThreadExecutor();

  // 別スレッドからメインスレッドにタスクを送信
  std::thread worker([&]() {
    try {
      executor.executeSync([]() -> kj::Promise<void> {
        // ここは Executor スレッドで動く（EventLoop がある）
        return kj::evalLater([] {
          const auto tid = std::this_thread::get_id();
          LOG_COUT << "[evalLater] Scheduled in Executor thread: " << tid
                   << std::endl;
        });
      });
    } catch (const kj::Exception& e) {
      LOG_COUT << "[Worker] Exception: " << e.getDescription().cStr()
               << std::endl;
    }

    // ここはエラーになる
    // taskSet.add(kj::evalLater([] {
    //   // スレッドのIDを取得
    //   const std::thread::id tid = std::this_thread::get_id();
    //   LOG_COUT << "[Worker] Hello from task ID: " << tid << std::endl;
    // }));
  });

  {
    try {
      auto exe = executor.executeAsync([&]() -> kj::Promise<void> {
        // ここは Executor スレッドで動く（EventLoop がある）
        const auto tid = std::this_thread::get_id();
        LOG_COUT << "[executeAsync] Scheduled in Executor thread: " << tid
                 << std::endl;
        return kj::READY_NOW;
      });
      exe.wait(ws);  // Promise の完了を待機

    } catch (const kj::Exception& e) {
      LOG_COUT << "[Worker] Exception: " << e.getDescription().cStr()
               << std::endl;
    }
  }

  // evalLater でメインスレッドのタスク追加
  taskSet.add(kj::evalLater([] {
    // スレッドのIDを取得
    const std::thread::id tid = std::this_thread::get_id();
    LOG_COUT << "[Worker] Hello from task ID: " << tid << std::endl;
  }));

  // 少しイベントループを回して完了を待つ
  io.provider->getTimer().afterDelay(300 * kj::MILLISECONDS).wait(ws);

  LOG_COUT << "[Main] Done. Exiting..." << std::endl;
  worker.join();
  taskSet.onEmpty().wait(ws);  ///< TaskSet が空になるまで待機
  return 0;
}
