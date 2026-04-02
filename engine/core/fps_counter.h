#pragma once

namespace Rodan {

class FramePerSecondCounter {
public:
  explicit FramePerSecondCounter(float avgInterval = 0.5f);

  bool tick(float deltaSeconds, bool frameRendered = true);

  inline float getFPS() const { return currentFPS_; }

public:
  float avgInterval_ = 0.5f;
  unsigned int numFrames_ = 0;
  double accumulatedTime_ = 0;
  float currentFPS_ = 0.0f;

  bool printFPS_ = true;
};

} // namespace Rodan
