#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <glm/glm.hpp>
namespace Rodan::Debug {
class GraphRenderer {
public:
  explicit GraphRenderer(const char *name, size_t maxGraphPoints = 256)
      : name_(name), maxGraphPoints_(maxGraphPoints) {}

  void addPoint(float point);

  void renderGraph(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                   const glm::vec4 &color = glm::vec4(1.0)) const;

private:
  const char *name_ = nullptr;
  const size_t maxGraphPoints_;
  std::deque<float> graph_;
};
} // namespace Rodan::Debug
