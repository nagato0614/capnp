//
// Created by toru on 2025/05/18.
//

#ifndef TIMER_WITH_CANCEL_HPP
#define TIMER_WITH_CANCEL_HPP
#include <kj/async-io.h>
#include <kj/debug.h>

#include <functional>
#include <iostream>
#include <string>

#include "utility.hpp"  // get_current_time_string, LOG_COUT などの補助関数定義

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
  void start(const kj::Duration interval, std::function<void()>&& callback) {
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

#endif  // TIMER_WITH_CANCEL_HPP
