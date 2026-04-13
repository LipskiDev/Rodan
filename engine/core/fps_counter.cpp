#include <assert.h>

#ifndef assert
#error direct assert.h still missing assert
#endif

#include <cassert>

#ifndef assert
#error cassert still missing assert
#endif

#include <core/fps_counter.h>

#ifndef assert
#error assert disappeared after fps_counter.h
#endif

#include <cstdio>

#ifndef assert
#error assert disappeared after cstdio
#endif

namespace Rodan {

FramePerSecondCounter::FramePerSecondCounter(float avgInterval)
    : avgInterval_(avgInterval) {
  assert(avgInterval > 0.0f);
}

bool FramePerSecondCounter::tick(float deltaSeconds, bool frameRendered) {
  if (frameRendered)
    numFrames_++;

  accumulatedTime_ += deltaSeconds;

  if (accumulatedTime_ > avgInterval_) {
    currentFPS_ = static_cast<float>(numFrames_ / accumulatedTime_);
    if (printFPS_)
      printf("FPS: %.1f\n", currentFPS_);
    numFrames_ = 0;
    accumulatedTime_ = 0;
    return true;
  }

  return false;
}

} // namespace Rodan
