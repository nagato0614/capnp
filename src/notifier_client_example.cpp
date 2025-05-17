// notifier_client_example.cpp
//
// Cap’n Proto RPC クライアント: Notifier.subscribe() を呼び出して
// 通知を受け取り、5 秒後にキャンセルするサンプル実装。

#include <capnp/ez-rpc.h>
#include <kj/async.h>
#include <kj/debug.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <utility.hpp>

#include "notification.capnp.h"  // schema/notification.capnp から生成

/**
 * @brief タスク失敗時にログを出力するエラーハンドラクラス
 */
class SimpleErrorHandler final : public kj::TaskSet::ErrorHandler {
 public:
  /**
   * @brief タスク中に発生した未処理の例外をログに記録
   *
   * @param e 発生した例外
   */
  void taskFailed(kj::Exception&& e) override {
    LOG_COUT << "Task failed: " << e.getDescription().cStr() << std::endl;
  }
};

// ── ヘルパ関数: 通知の表示 ───────────────────────────────
void printNotification(::Notification::Reader n) {
  LOG_COUT << "[Notification] id=" << n.getId()
           << ", kind=" << n.getKind().cStr()
           << ", timestamp=" << n.getTimestamp() << std::endl;
}

int main() {
  try {
    LOG_COUT << "Starting Notifier client..." << std::endl;
    capnp::EzRpcClient client("localhost", 5923);
    auto& ws = client.getWaitScope();
    auto& timer = client.getIoProvider().getTimer();
    SimpleErrorHandler errorHandler;
    kj::TaskSet task_set(errorHandler);

    // ストリームが登録されていない段階でのアクセス
    try {
      auto stream = client.getMain<NotificationStream>();
      auto nResp = stream.readRequest().send().wait(ws);
      if (nResp.hasResult()) {
        printNotification(nResp.getResult());
      } else {
        std::cout << "[Client] Stream ended." << std::endl;
        return 0;
      }
    } catch (kj::Exception& e) {
      LOG_COUT << "Failed to read from stream: " << e.getDescription().cStr()
               << std::endl;
    }

    auto notifier = client.getMain<Notifier>();

    // ── Subscribe リクエスト送信 ──
    LOG_COUT << "Sending Subscribe request..." << std::endl;
    auto req = notifier.subscribeRequest();
    req.getParams().setFilter("Notifier");

    auto resp = req.send().wait(ws);
    LOG_COUT << "Subscribe request sent." << std::endl;
    auto stream = resp.getStream();
    auto session = resp.getSubscription();

    LOG_COUT << "Subscribe response received." << std::endl;

    // ── 5秒後にキャンセルを送信（Timer使用） ──
    auto timer_promise =
        timer.afterDelay(5 * kj::SECONDS).then([session]() mutable {
          LOG_COUT << "[Client] Cancelling subscription..." << std::endl;
          session.cancelRequest().send().ignoreResult();
        });
    task_set.add(kj::mv(timer_promise));

    LOG_COUT << "Waiting for notifications..." << std::endl;

    while (true) {
      auto nResp = stream.readRequest().send().wait(ws);
      if (nResp.hasResult()) {
        printNotification(nResp.getResult());
      } else {
        std::cout << "[Client] Stream ended." << std::endl;
        break;
      }
    }

    task_set.onEmpty().wait(ws);
  } catch (kj::Exception& e) {
    LOG_COUT << "Client exception: " << e.getDescription().cStr() << std::endl;
  }

  return 0;
}
