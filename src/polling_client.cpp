// polling_client.cpp
// ポーリング方式の通知クライアント実装
// Context経由で通知を受信

#include <capnp/ez-rpc.h>
#include <kj/async.h>
#include <kj/debug.h>

#include <chrono>
#include <iostream>
#include <thread>
#include <utility.hpp>

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

/**
 * @brief PollingNotificationReceiver の実装
 * @details サーバーからContext経由で通知を受信し、再帰的に通知処理を継続する
 */
class NotificationReceiverImpl final
    : public PollingNotificationReceiver::Server {
 public:
  /**
   * @brief 通知受信時に呼び出されるメソッド
   * @details
   * サーバーから送信された通知を受信し、初回通知時に再帰的な処理を開始する
   * @param context 通知コンテキスト（通知データを含��）
   * @return kj::Promise<void> 処理完了を示すプロミス
   */
  kj::Promise<void> onNotification(OnNotificationContext context) override {
    const auto notification = context.getParams().getNotification();

    LOG_COUT << "[Context Notification] id=" << notification.getId()
             << ", kind=" << notification.getKind().cStr()
             << ", timestamp=" << notification.getTimestamp() << std::endl;

    if (!is_start_) {
      is_start_ = true;
      recursivePrint(notification);
    }
    return kj::READY_NOW;
  }

  /**
   * @brief 通知データを再帰的に処理する
   * @details 100ミリ秒間隔で同じ通知データを継続的に出力し続ける
   * @param reader 処理対象の通知データリーダー
   */
  void recursivePrint(const ::Notification::Reader& reader) {
    LOG_COUT << "[recursivePrint] id=" << reader.getId() << std::endl;
    auto promise =
        timer->afterDelay(100 * kj::MILLISECONDS).then([this, reader]() {
          recursivePrint(reader);
        });
    taskSet->add(kj::mv(promise));
  }

  /**
   * @brief タイマーオブジェクトを設定する
   * @param t 使用するタイマーオブジェクトのポインタ
   */
  void setTimer(kj::Timer* t) { timer = t; }

  /**
   * @brief タスクセットオブジェクトを設定する
   * @param ts 使用するタスクセットオブジェクトのポインタ
   */
  void setTaskSet(kj::TaskSet* ts) { taskSet = ts; }

  bool is_start_ = false;  ///< 再帰処理が開始されたかを示すフラグ
  kj::Timer* timer;        ///< 遅延処理用のタイマーオブジェクト
  kj::TaskSet* taskSet;    ///< 非同期タスク管理用のタスクセット
};

/**
 * @brief メイン関数
 * @details ポーリング通知クライアントを起動し、サーバーからの通知を受信する
 * @return int プログラムの終了コード��0: 正常終了、その他: エラー）
 */
int main() {
  try {
    LOG_COUT << "Starting Polling Notifier client..." << std::endl;

    // ポーリングサーバーに接続（ポート5924）
    capnp::EzRpcClient client("localhost", 5924);
    auto& ws = client.getWaitScope();
    auto& timer = client.getIoProvider().getTimer();  // 参照で取得
    SimpleErrorHandler errorHandler;
    kj::TaskSet task_set(errorHandler);

    // NotificationReceiver実装を作成
    auto receiverImpl = kj::heap<NotificationReceiverImpl>();
    receiverImpl->setTimer(&timer);
    receiverImpl->setTaskSet(&task_set);
    PollingNotificationReceiver::Client receiver(kj::mv(receiverImpl));

    // PollingNotifierに接続
    auto pollingNotifier = client.getMain<PollingNotifier>();

    // Subscribe リクエスト送信
    LOG_COUT << "Sending Polling Subscribe request..." << std::endl;
    auto req = pollingNotifier.subscribeRequest();
    req.setFilter("PollingNotifier");
    req.setReceiver(receiver);

    auto resp = req.send().wait(ws);
    LOG_COUT << "Polling Subscribe request sent." << std::endl;
    auto subscription = resp.getSubscription();

    LOG_COUT << "Polling Subscribe response received." << std::endl;

    // 10秒後にキャンセルを送信
    auto timer_promise =
        timer.afterDelay(10 * kj::SECONDS).then([subscription]() mutable {
          LOG_COUT << "[Client] Cancelling polling subscription..."
                   << std::endl;
          (void)subscription.cancelRequest().send().ignoreResult();
        });
    task_set.add(kj::mv(timer_promise));

    LOG_COUT << "[Client] Polling client finished." << std::endl;
    task_set.onEmpty().wait(ws);
  } catch (kj::Exception& e) {
    LOG_COUT << "Client exception: " << e.getDescription().cStr() << std::endl;
  } catch (std::exception& e) {
    LOG_COUT << "Client std::exception: " << e.what() << std::endl;
  } catch (...) {
    LOG_COUT << "Client unknown exception occurred." << std::endl;
  }

  return 0;
}
