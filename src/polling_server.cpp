// polling_server.cpp
// ポーリング方式の通知サーバー実装
// Context経由で通知を送信

#include <capnp/ez-rpc.h>
#include <kj/debug.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <utility.hpp>
#include <vector>

#include "schema/notification.capnp.h"

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

//------------------------------------------------------------
// ポーリング購読状態
//------------------------------------------------------------
struct PollingSubscriptionState {
  std::atomic<bool> cancelled{false};
  PollingNotificationReceiver::Client receiver;
  std::string filter;

  PollingSubscriptionState(PollingNotificationReceiver::Client r,
                           const std::string& f)
      : receiver(kj::mv(r)), filter(f) {}
};

//------------------------------------------------------------
// PollingSubscription実装
//------------------------------------------------------------
class PollingSubscriptionImpl final : public PollingSubscription::Server {
 public:
  explicit PollingSubscriptionImpl(std::shared_ptr<PollingSubscriptionState> s)
      : state(s) {}

  kj::Promise<void> cancel(CancelContext context) override {
    if (state->cancelled.load()) {
      LOG_COUT << "[PollingSubscription] already cancelled\n";
    } else {
      LOG_COUT << "[PollingSubscription] cancel()\n";
      state->cancelled.store(true);
    }
    return kj::READY_NOW;
  }

 private:
  std::shared_ptr<PollingSubscriptionState> state;
};

//------------------------------------------------------------
// PollingNotifier実装
//------------------------------------------------------------
class PollingNotifierImpl final : public PollingNotifier::Server {
 public:
  PollingNotifierImpl() = default;

  void setTimer(kj::Timer& t) {
    timer_ptr_ = &t;
    startNotificationLoop();
  }

  void setTaskSet(kj::TaskSet& t) { task_set_ = &t; }

  kj::Promise<void> subscribe(SubscribeContext ctx) override {
    const auto params = ctx.getParams();
    const auto filter = params.getFilter();
    auto receiver = params.getReceiver();

    LOG_COUT << "[PollingNotifier] subscribe: filter=" << filter.cStr()
             << std::endl;

    // 新しい購読状態を作成
    auto state = std::make_shared<PollingSubscriptionState>(kj::mv(receiver),
                                                            filter.cStr());
    subscriptions_.push_back(state);

    // Subscriptionオブジェクトを返す
    ctx.getResults().setSubscription(kj::heap<PollingSubscriptionImpl>(state));

    LOG_COUT << "[PollingNotifier] new polling subscription created\n";
    return kj::READY_NOW;
  }

 private:
  void startNotificationLoop() {
    if (!timer_ptr_) return;

    // 定期的に通知を送信するループを開始
    auto promise =
        sendNotifications()
            .then([this]() { return timer_ptr_->afterDelay(1 * kj::SECONDS); })
            .then([this]() {
              startNotificationLoop();  // 再帰的に継続
            })
            .catch_([](kj::Exception&& e) {
              LOG_COUT << "Notification loop error: "
                       << e.getDescription().cStr() << std::endl;
            });

    task_set_->add(kj::mv(promise));
  }

  kj::Promise<void> sendNotifications() {
    LOG_COUT << "[Server] Starting sendNotifications..." << std::endl;

    // アクティブな購読をクリーンアップ
    subscriptions_.erase(
        std::remove_if(
            subscriptions_.begin(), subscriptions_.end(),
            [](const std::weak_ptr<PollingSubscriptionState>& weak_state) {
              auto state = weak_state.lock();
              return !state || state->cancelled.load();
            }),
        subscriptions_.end());

    LOG_COUT << "[Server] Active subscriptions: " << subscriptions_.size()
             << std::endl;

    // 各購読者に通知を送信
    kj::Vector<kj::Promise<void>> promises;

    for (auto& weak_state : subscriptions_) {
      auto state = weak_state.lock();
      if (!state || state->cancelled.load()) continue;

      /**
       * @brief 購読者への通知送信ログを出力
       */
      LOG_COUT << "[Server] Sending notification to a subscriber..."
               << std::endl;

      /**
       * @brief 通知データを作成
       * @details サーバーからクライアントへ送信する通知メッセージを構築
       * - 一意のID（カウンター）を設定
       * - 現在のタイムスタンプを設定
       * - 通知の種類を設定
       */
      auto req = state->receiver.onNotificationRequest();
      auto notification = req.initNotification();
      notification.setId(notification_counter_++);

      auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
      notification.setTimestamp(ts);
      notification.setKind("polling_demo");

      /**
       * @brief 作成された通知データの詳細をログ出力
       */
      LOG_COUT << "[Server] Notification created: id=" << notification.getId()
               << ", timestamp=" << notification.getTimestamp() << std::endl;

      /**
       * @brief 非同期で通知を送信
       * @details
       * - req.send()で非同期送信を開始
       * - then()で送信成功時の処理を定義
       * - catch_()で送信失敗時のエラーハンドリングを定義
       * @return kj::Promise<void> 送信完了を示すプロミス
       */
      auto promise = req.send()
                         .then([](auto&&) {
                           LOG_COUT
                               << "[Server] Notification sent successfully."
                               << std::endl;
                         })
                         .catch_([](kj::Exception&& e) {
                           LOG_COUT << "[Server] Failed to send notification: "
                                    << e.getDescription().cStr() << std::endl;
                         });

      promises.add(kj::mv(promise));
    }

    /**
     * @brief アクティブな購読が存在しない場合の処理
     * @details 送信対象となる購読者がいない場合は即座に完了を返す
     */
    if (promises.size() == 0) {
      LOG_COUT << "[Server] No active subscriptions to send notifications to."
               << std::endl;
      return kj::READY_NOW;
    }

    /**
     * @brief 全ての通知送信プロミスを結合して完了を待機
     * @details
     * - joinPromises()で全ての送信プロミスを並行実行
     * - then()で全送信完了後の処理を定義
     * @return kj::Promise<void> 全通知送信完了を示すプロミス
     */
    return kj::joinPromises(promises.releaseAsArray())
        .then([]() -> kj::Promise<void> {
          LOG_COUT << "[Server] All notifications sent successfully."
                   << std::endl;
          return kj::READY_NOW;
        });
  }

  kj::Timer* timer_ptr_ =
      nullptr;  ///< 定期実行用タイマーオブジェクトへのポインタ
  kj::TaskSet* task_set_ = nullptr;
  std::vector<std::weak_ptr<PollingSubscriptionState>> subscriptions_;
  uint64_t notification_counter_ = 0;
};

//------------------------------------------------------------
// main
//------------------------------------------------------------
int main() {
  try {
    // PollingNotifierImpl を heap で生成してClientに変換
    auto notifierImpl = kj::heap<PollingNotifierImpl>();
    auto* notifierRaw = notifierImpl.get();
    auto notifierClient = PollingNotifier::Client(kj::mv(notifierImpl));

    // EzRpcServer を起動
    capnp::EzRpcServer server(kj::mv(notifierClient), "localhost", 5924);

    // Timer を取得し、NotifierImpl に注入
    auto& timer = server.getIoProvider().getTimer();
    auto& ws = server.getWaitScope();
    SimpleErrorHandler errorHandler;
    kj::TaskSet taskSet(errorHandler);
    notifierRaw->setTaskSet(taskSet);
    notifierRaw->setTimer(timer);

    // ログ & イベントループ
    auto port = server.getPort().wait(ws);
    LOG_COUT << "Polling Notifier server started on port " << port << '\n';

    kj::NEVER_DONE.wait(ws);
  } catch (kj::Exception& e) {
    LOG_COUT << "Server exception: " << e.getDescription().cStr() << '\n';
  }

  return 0;
}
