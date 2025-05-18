// notifier_server_example.cpp

#include <capnp/ez-rpc.h>
#include <kj/debug.h>

#include <atomic>
#include <chrono>
#include <utility.hpp>

#include "notification.capnp.h"

//------------------------------------------------------------
// 共有状態
//------------------------------------------------------------
struct SharedState {
  std::atomic<bool> cancelled{false};
};

//------------------------------------------------------------
// Subscription
//------------------------------------------------------------
class SubscriptionImpl final : public Subscription::Server {
 public:
  explicit SubscriptionImpl(SharedState &s) : state(s) {}

  kj::Promise<void> cancel(CancelContext context) override {
    if (state.cancelled.load()) {
      LOG_COUT << "[Subscription] already cancelled\n";
    } else {
      LOG_COUT << "[Subscription] cancel()\n";
      state.cancelled.store(true);
    }
    return kj::READY_NOW;
  }

 private:
  SharedState &state;
};

//------------------------------------------------------------
// NotificationStream
//------------------------------------------------------------
class StreamImpl final : public NotificationStream::Server {
 public:
  StreamImpl(SharedState &s, kj::Timer &t) : state(s), timer(t) {}
  kj::Promise<void> read(ReadContext ctx) override {
    if (state.cancelled.load()) {
      LOG_COUT << "[Stream] stream closed\n";
      KJ_FAIL_REQUIRE("stream closed");
    }
    return timer.afterDelay(200 * kj::MILLISECONDS)
        .then([this, ctx = kj::mv(ctx)]() mutable {
          auto n = ctx.getResults().initResult();
          n.setId(counter++);
          auto ts = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
          n.setTimestamp(ts);
          n.setKind("demo");
        });
  }

 private:
  SharedState &state;
  kj::Timer &timer;
  uint64_t counter = 0;
};

//------------------------------------------------------------
// Notifier
//------------------------------------------------------------
class NotifierImpl final : public Notifier::Server {
 public:
  NotifierImpl() = default;
  void setTimer(kj::Timer &t) { timer_ptr_ = &t; }

  kj::Promise<void> subscribe(SubscribeContext ctx) override {
    KJ_REQUIRE(timer_ptr_ != nullptr, "Timer not set!");
    // params を表示
    const auto params = ctx.getParams();
    LOG_COUT << "[Notifier] subscribe: filter="
             << params.getParams().getFilter().cStr() << std::endl;
    state_ = kj::heap<SharedState>();

    ctx.getResults().setStream(kj::heap<StreamImpl>(*state_, *timer_ptr_));
    ctx.getResults().setSubscription(kj::heap<SubscriptionImpl>(*state_));

    LOG_COUT << "[Notifier] new subscription\n";
    return kj::READY_NOW;
  }

 private:
  kj::Timer *timer_ptr_ = nullptr;
  kj::Own<SharedState> state_;
};

//------------------------------------------------------------
// main
//------------------------------------------------------------
int main() {
  try {
    // 1) NotifierImpl を heap で生成し、生ポインタを控える
    auto notifierOwn = kj::heap<NotifierImpl>();
    auto *notifierRaw = notifierOwn.get();  // Timer 注入用

    // 2) EzRpcServer を起動 (mainInterface, bindAddress, defaultPort)
    capnp::EzRpcServer server(kj::mv(notifierOwn), "localhost", 5923);

    // 3) サーバ作成後に Timer を取得し、NotifierImpl に注入
    auto &timer = server.getIoProvider().getTimer();
    notifierRaw->setTimer(timer);

    // 4) ログ & イベントループ
    auto &ws = server.getWaitScope();
    auto port = server.getPort().wait(ws);
    LOG_COUT << "Notifier server started on port " << port << '\n';

    kj::NEVER_DONE.wait(ws);
  } catch (kj::Exception &e) {
    LOG_COUT << "Server exception: " << e.getDescription().cStr() << '\n';
  }
}
