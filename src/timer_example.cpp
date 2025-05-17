#define KJ_LOG_LEVEL KJ_LOG_SEVERITY_INFO
#include <kj/async-io.h>
#include <kj/debug.h>

#include <functional>
#include <iostream>
#include <string>

#include "utility.hpp"  // get_current_time_string, LOG_COUT などの補助関数定義

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
 * @brief 一定間隔でコールバックを呼び出し、キャンセル可能な非同期タイマー
 *
 * `kj::Timer` + `kj::TaskSet` + `kj::Canceler` を組み合わせて構成される。
 * `start()` で繰り返し処理を開始し、`cancel()` によって安全に停止できる。
 */
class RepeatingTimerWithCancel {
 public:
  /**
   * @brief コンストラクタ
   *
   * @param timer       KJ タイマーインスタンス（外部から供給される）
   * @param taskSet     登録されたタスクを管理する TaskSet
   * @param canceler    キャンセル機構（複数のタスクをまとめて停止）
   */
  RepeatingTimerWithCancel(kj::Timer& timer, kj::TaskSet& taskSet,
                           kj::Canceler& canceler)
      : timer(timer), taskSet(taskSet), canceler(canceler) {}

  /**
   * @brief タイマーの起動（指定間隔で繰り返しコールバックを呼び出す）
   *
   * @param interval 実行間隔（kj::Duration）
   * @param callback 実行する処理（std::function<void()>）
   */
  void start(const kj::Duration interval, std::function<void()> &&callback) {
    this->interval = interval;
    this->callback = callback;
    scheduleNext();  ///< 最初の1回目のスケジュールを行う
  }

  /**
   * @brief 実行中のタイマーをキャンセルする
   *
   * @param reason キャンセル理由（ログ出力用）
   */
  void cancel(const char* reason) const {
    canceler.cancel(reason);  ///< すべての未完了タスクにキャンセル通知
  }

 private:
  kj::Timer& timer;
  kj::TaskSet& taskSet;
  kj::Canceler& canceler;
  kj::Duration interval;
  std::function<void()> callback;

  /**
   * @brief 次回のタイマー処理をスケジュールする（内部再帰）
   */
  void scheduleNext() {
    auto promise = canceler.wrap(timer.afterDelay(interval))
                       .then([this]() {
                         if (callback)
                           callback();  ///< タイマー満了時にコールバック実行
                       })
                       .then([this]() {
                         scheduleNext();  ///< 再帰的に次回スケジュール
                       })
                       .catch_([](kj::Exception&& e) {
                         LOG_COUT
                             << "Timer canceled: " << e.getDescription().cStr()
                             << std::endl;
                       });

    taskSet.add(kj::mv(promise));  ///< TaskSet に登録して管理
  }
};

/**
 * @brief 実行エントリポイント
 */
int main() {
  // 非同期 I/O セットアップ（KJ の基本構造）
  auto io = kj::setupAsyncIo();
  kj::Timer& timer = io.provider->getTimer();
  kj::WaitScope& ws = io.waitScope;
  kj::Canceler canceler;
  SimpleErrorHandler errorHandler;
  kj::TaskSet taskSet(errorHandler);

  // タイマーのインスタンス作成
  RepeatingTimerWithCancel repeatingTimer(timer, taskSet, canceler);

  /**
   * @brief テスト用タイマー処理：1秒後にキャンセル
   */
  auto timer_test = [&]() {
    // 100ミリ秒間隔でタイマー発火させる
    repeatingTimer.start(100 * kj::MILLISECONDS,
                         []() { LOG_COUT << "Timer fired!" << std::endl; });

    // 1秒後にキャンセルを実行
    timer.afterDelay(1 * kj::SECONDS)
        .then([&]() {
          LOG_COUT << "Stopping timer" << std::endl;
          repeatingTimer.cancel("Manual cancel after 1 second");
        })
        .wait(ws);  ///< Promise の完了を待機

    // すべてのタスクが終了するのを待つ
    taskSet.onEmpty().wait(ws);
  };

  timer_test();  ///< 1回目のテスト
  timer_test();  ///< 2回目のテスト（再利用可能性の確認）

  // タイマーが未起動の状態で cancel() しても安全に処理される
  repeatingTimer.cancel("Manual cancel before starting");

  return 0;
}
