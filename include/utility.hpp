//
// Created by toru on 2025/05/17.
//
#define KJ_LOG_LEVEL KJ_LOG_SEVERITY_INFO
#include <kj/async-io.h>
#include <kj/debug.h>
#include <kj/std/iostream.h>

#include <chrono>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#ifndef UTILITY_HPP
#define UTILITY_HPP

inline std::string get_current_time_string() {
  using namespace std::chrono;

  const auto now = system_clock::now();
  const auto in_time_t = system_clock::to_time_t(now);
  const auto fractional =
      duration_cast<milliseconds>(now.time_since_epoch()) % 1000;

  const std::tm local_time = *std::localtime(&in_time_t);

  std::ostringstream oss;
  oss << "[" << std::put_time(&local_time, "%H:%M:%S") << '.' << std::setw(3)
      << std::setfill('0') << fractional.count() << "]";

  return oss.str();
}

#define LOG_COUT                                                               \
  std::cout << get_current_time_string() << "[" << __FILE__ << ":" << __LINE__ \
            << "]"

#endif  // UTILITY_HPP
