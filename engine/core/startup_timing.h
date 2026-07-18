#pragma once

#include <chrono>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace Rodan::StartupTiming {

using Clock = std::chrono::steady_clock;

inline std::optional<Clock::time_point> startedAt;
inline Clock::time_point lastCheckpointAt;
inline std::vector<std::pair<std::string, double>> stages;
inline bool firstFrameReported = false;

inline void Begin() {
  startedAt = Clock::now();
  lastCheckpointAt = *startedAt;
  stages.clear();
  firstFrameReported = false;
}

inline void MarkCheckpoint(const char *name) {
  if (!startedAt || firstFrameReported) {
    return;
  }

  const Clock::time_point now = Clock::now();
  stages.emplace_back(
      name, std::chrono::duration<double, std::milli>(now - lastCheckpointAt)
                .count());
  lastCheckpointAt = now;
}

inline void ReportFirstFramePresented() {
  if (!startedAt || firstFrameReported) {
    return;
  }

  const Clock::time_point presentedAt = Clock::now();
  MarkCheckpoint("Submit + present first frame");
  firstFrameReported = true;
  const auto milliseconds = [](Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
  };

  std::ostringstream message;
  message << std::fixed << std::setprecision(2)
          << "\n[Startup] First-frame startup breakdown\n";

  double cumulativeMs = 0.0;
  for (const auto &[name, durationMs] : stages) {
    cumulativeMs += durationMs;
    message << "  " << std::setw(42) << std::left << name << std::setw(10)
            << std::right << durationMs << " ms  (" << cumulativeMs
            << " ms total)\n";
  }

  message << "  " << std::string(70, '-') << "\n"
          << "  " << std::setw(42) << std::left << "TOTAL"
          << std::setw(10) << std::right << milliseconds(*startedAt, presentedAt)
          << " ms\n";

  std::cout << message.str() << std::flush;
}

} // namespace Rodan::StartupTiming
