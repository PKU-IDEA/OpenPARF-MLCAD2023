/*
 * @file          : stopwatch.h
 * @project       : util
 * @author        : Jing Mai <jingmai@pku.edu.cn>
 * @created date  : July 07 2023, 15:05:41, Friday
 * @brief         :
 * -----
 * Last Modified: July 07 2023, 15:18:30, Friday
 * Modified By: Jing Mai <jingmai@pku.edu.cn>
 * -----
 * @history :
 * ====================================================================================
 * Date         	By     	(version)	Comments
 * -------------	-------	---------	--------------------------------------------------
 * ====================================================================================
 * Copyright (c) 2020 - 2023 All Right Reserved, PKU-IDEA Group
 * -----
 * This header is generated by VSCode extension psi-header.
 */
#pragma once

// C++ library headers
#include <chrono>   // NOLINT [build/c++11]
#include <string>
#include <utility>
#include <vector>

#include "macros.h"
#include "util/namespace.h"

OPENPARF_BEGIN_NAMESPACE

class Stopwatch {
 public:
  enum class TimeFormat { NANOSECONDS, MICROSECONDS, MILLISECONDS, SECONDS };

  Stopwatch() : start_time_(), laps_({}) {}

  void start() {
    start_time_ = std::chrono::high_resolution_clock::now();
    laps_       = {start_time_};
  }

  template<TimeFormat fmt = TimeFormat::MILLISECONDS>
  time_t elapsed() {
    const auto en = std::chrono::high_resolution_clock::now();
    return ticks<fmt>(start_time_, en);
  }

  template<TimeFormat fmt = TimeFormat::MILLISECONDS>
  time_t lap() {
    const auto t      = std::chrono::high_resolution_clock::now();
    const auto last_r = laps_.back();
    laps_.push_back(t);
    return ticks<fmt>(last_r, t);
  }

  template<TimeFormat fmt_total = TimeFormat::MILLISECONDS, TimeFormat fmt_lap = fmt_total>
  std::pair<time_t, std::vector<time_t>> elapsed_lap() {
    std::vector<time_t> lap_times;
    lap_times.reserve(laps_.size() - 1);
    for (size_t i = 0; i <= laps_.size() - 2; i++) {
      const auto lap_end   = laps_[i + 1];
      const auto lap_start = laps_[i];
      lap_times.push_back(ticks<fmt_lap>(lap_start, lap_end));
    }
    return {ticks<fmt_total>(start_time_, laps_.back()), lap_times};
  }

 private:
  typedef std::chrono::time_point<std::chrono::high_resolution_clock> TimePoint;
  TimePoint                                                           start_time_;
  std::vector<TimePoint>                                              laps_;

  template<TimeFormat fmt = TimeFormat::MILLISECONDS>
  static std::time_t ticks(const TimePoint &st, const TimePoint &en) {
    const auto   duration = en - st;
    const time_t ns_count = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();
    switch (fmt) {
      case TimeFormat::NANOSECONDS:
        return ns_count;
      case TimeFormat::MICROSECONDS: {
        time_t up = ((ns_count / 100) % 10 >= 5) ? 1 : 0;
        return (ns_count / 1000) + up;
      }
      case TimeFormat::MILLISECONDS: {
        time_t up = ((ns_count / 100000) % 10 >= 5) ? 1 : 0;
        return (ns_count / 1000000) + up;
      }
      case TimeFormat::SECONDS: {
        time_t up = ((ns_count / 100000000) % 10 >= 5) ? 1 : 0;
        return (ns_count / 1000000000) + up;
      }
    }
  }
  DISALLOW_COPY_AND_ASSIGN(Stopwatch);
};

constexpr Stopwatch::TimeFormat ns           = Stopwatch::TimeFormat::NANOSECONDS;
constexpr Stopwatch::TimeFormat mus          = Stopwatch::TimeFormat::MICROSECONDS;
constexpr Stopwatch::TimeFormat ms           = Stopwatch::TimeFormat::MILLISECONDS;
constexpr Stopwatch::TimeFormat s            = Stopwatch::TimeFormat::SECONDS;

constexpr Stopwatch::TimeFormat nanoseconds  = Stopwatch::TimeFormat::NANOSECONDS;
constexpr Stopwatch::TimeFormat microseconds = Stopwatch::TimeFormat::MICROSECONDS;
constexpr Stopwatch::TimeFormat milliseconds = Stopwatch::TimeFormat::MILLISECONDS;
constexpr Stopwatch::TimeFormat seconds      = Stopwatch::TimeFormat::SECONDS;

OPENPARF_END_NAMESPACE
