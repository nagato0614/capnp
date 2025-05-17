#define KJ_LOG_LEVEL KJ_LOG_SEVERITY_INFO
#include <kj/async-io.h>
#include <kj/debug.h>

#include <functional>
#include <iostream>
#include <string>

#include "utility.hpp"

class SimpleErrorHandler final : public kj::TaskSet::ErrorHandler {
 public:
  void taskFailed(kj::Exception&& e) override {
    LOG_COUT << "Task failed: " << e.getDescription().cStr() << std::endl;
  }
};

class RepeatingTimerWithCancel {
 public:
  RepeatingTimerWithCancel(kj::Timer& timer, kj::TaskSet& taskSet,
                           kj::Canceler& canceler)
      : timer(timer), taskSet(taskSet), canceler(canceler) {}

  void start(kj::Duration interval, std::function<void()> callback) {
    this->interval = interval;
    this->callback = callback;
    scheduleNext();  // 最初の1回
  }

  void cancel(const char* reason) { canceler.cancel(reason); }

 private:
  kj::Timer& timer;
  kj::TaskSet& taskSet;
  kj::Canceler& canceler;

  kj::Duration interval;
  std::function<void()> callback;

  void scheduleNext() {
    auto promise = canceler.wrap(timer.afterDelay(interval))
                       .then([this]() {
                         if (callback) callback();
                       })
                       .then([this]() {
                         scheduleNext();  // 再帰的に次をスケジュール
                       })
                       .catch_([](kj::Exception&& e) {
                         LOG_COUT
                             << "Timer canceled: " << e.getDescription().cStr()
                             << std::endl;
                       });

    taskSet.add(kj::mv(promise));
  }
};

int main() {
  auto io = kj::setupAsyncIo();
  kj::Timer& timer = io.provider->getTimer();
  kj::WaitScope& ws = io.waitScope;
  kj::Canceler canceler;
  SimpleErrorHandler errorHandler;
  kj::TaskSet taskSet(errorHandler);

  RepeatingTimerWithCancel repeatingTimer(timer, taskSet, canceler);

  auto timer_test = [&]() {
    repeatingTimer.start(100 * kj::MILLISECONDS,
                         []() { LOG_COUT << "Timer fired!" << std::endl; });

    timer.afterDelay(1 * kj::SECONDS)
        .then([&]() {
          LOG_COUT << "Stopping timer" << std::endl;
          repeatingTimer.cancel("Manual cancel after 6 seconds");
        })
        .wait(ws);

    taskSet.onEmpty().wait(ws);
  };

  timer_test();
  timer_test();

  // timer is not set and try to cancel
  repeatingTimer.cancel("Manual cancel before starting");

  return 0;
}