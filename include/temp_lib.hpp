//
// Created by toru on 2025/05/18.
//

#ifndef TEMP_LIB_HPP
#define TEMP_LIB_HPP

#include <kj/common.h>
#include "repeating_timer_with_cancel.hpp"

namespace nagato {

class TempLibBase {
 protected:
  explicit TempLibBase(int value = 0) : value_(value) {}
  virtual ~TempLibBase() = default;

 public:
  virtual int ReadValue() = 0;
  virtual void WriteValue(int value) = 0;
  virtual void Start() = 0;
  virtual void Stop() = 0;

  int value_;
};

class TempLib : public TempLibBase {
 public:
  static TempLibBase *GetInstance() {
    static TempLib instance;
    return dynamic_cast<TempLibBase *>(&instance);
  }

  int ReadValue() override { return value_; }
  void WriteValue(int value) override { value_ = value; }
  void Start() override { value_ += 1; }
  void Stop() override { value_ -= 1; }

 private:
  explicit TempLib(const int value = 0) : TempLibBase(value) {}
  ~TempLib() override = default;
};

}  // namespace nagato

#endif  // TEMP_LIB_HPP
