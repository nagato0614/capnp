//
// Created by toru on 2025/05/13.
//

#ifndef ONCHANGE_IMPL_HPP
#define ONCHANGE_IMPL_HPP

#include <capnp/ez-rpc.h>
#include "onchange.capnp.h"
#include <vector>
#include <memory>

class ChangeServiceImpl final : public onchange::ChangeService::Server {
  public:
  kj::Promise<void> subscribe(SubscribeContext context) override {
    auto listener = context.getParams().getListener();
    listeners.push_back(kj::heap<capnp::CapabilityClient>(kj::mv(listener)));
    return kj::READY_NOW;
  }

  kj::Promise<void> triggerChange(TriggerChangeContext context) override {
    for (auto& listener : listeners) {
      auto client = listener->cast<onchange::IOnChange>();
      auto req = client.onChangedRequest();
      req.setMessage("State has changed!");
      req.send();  // Fire and forget
    }
    return kj::READY_NOW;
  }

  private:
  std::vector<std::unique_ptr<capnp::CapabilityClient>> listeners;
};

#endif //ONCHANGE_IMPL_HPP
