@0xecc8d31c91349b82;

# 通知のデータ構造
struct Notification {
  id        @0 :UInt64;
  timestamp @1 :Int64;
  kind      @2 :Text;
  payload   @3 :Data;
}

# 通知受信用インターフェース（クライアントが read() を呼ぶ）
interface NotificationStream {
  read @0 () -> (result :Notification);
}

# Subscribe 呼び出し用パラメータ
struct SubscribeParams {
  filter @0 :Text;
}

# 通知購読セッション。キャンセル可能。
interface Subscription {
  cancel @0 () -> ();
}

# 通知を送る側（Notifier）
interface Notifier {
  subscribe @0 (params :SubscribeParams)
      -> (subscription :Subscription,
          stream :NotificationStream);
}

# ポーリング用の通知受信インターフェース
interface PollingNotificationReceiver {
  # クライアントがこのメソッドを実装し、サーバーがコンテキスト経由で通知を送信
  onNotification @0 (notification :Notification) -> ();
}

# ポーリング購読セッション
interface PollingSubscription {
  cancel @0 () -> ();
}

# ポーリング用のNotifier
interface PollingNotifier {
  # クライアントがreceiverを渡し、サーバーがそのreceiverに通知を送信
  subscribe @0 (filter :Text, receiver :PollingNotificationReceiver)
      -> (subscription :PollingSubscription);
}
