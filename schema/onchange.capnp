@0xecc8_d31c_9134_9b82;

using Import = import "/capnp/capability.capnp";

#========================================================
# 基本データ型
#========================================================

struct Notification {
  # 通知 1 件分
  id        @0 :UInt64;        # 一意 ID
  timestamp @1 :Int64;         # epoch millis
  kind      @2 :Text;          # 種別（"message" など）
  payload   @3 :Data;          # 任意バイナリ
}

#========================================================
# ストリーム用のリクエスト / ストップ信号
#========================================================

struct SubscribeParams {
  filter @0 :Text;             # 例: 通知フィルタ
}

interface Subscription {
  # サーバ↔クライアントの長寿命セッション。
  # クライアントが cancel() するとストリーム終了。
  cancel @0 () -> ();
}

#========================================================
# パブリッシャーサービス（クライアントが最初に呼び出す）
#========================================================

interface Notifier {
  # -- Subscribe --------------------------------------------------------------
  #
  #  呼び出し順序:
  #   1) client: notifier.subscribe(params)   ---> returns (subscription, stream)
  #   2) server: push notifications via stream.send()
  #   3) client: subscription.cancel()       ---> stream closes
  #
  subscribe @0 (params :SubscribeParams)
      -> (subscription :Subscription,
          stream       :Import.Stream(Notification));
}
