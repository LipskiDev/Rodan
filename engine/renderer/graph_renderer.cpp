#include "imgui.h"
#include <limits>
#include <renderer/graph_renderer.h>

#include <implot.h>

namespace Rodan::Debug {

void GraphRenderer::addPoint(float point) {
  graph_.push_back(point);
  if (graph_.size() > maxGraphPoints_) {
    graph_.erase(graph_.begin());
  }
}

void GraphRenderer::renderGraph(uint32_t x, uint32_t y, uint32_t width,
                                uint32_t height, const glm::vec4 &color) const {
  float minVal = std::numeric_limits<float>::max();
  float maxVal = std::numeric_limits<float>::min();

  for (float f : graph_) {
    if (f < minVal)
      minVal = f;
    if (f > maxVal)
      maxVal = f;
  }

  const float range = maxVal - minVal;

  float valX = 0.0;

  std::vector<float> dataX_;
  std::vector<float> dataY_;
  dataX_.reserve(graph_.size());
  dataY_.reserve(graph_.size());

  for (float f : graph_) {
    const float valY = (f - minVal) / range;
    valX += 1.0f / maxGraphPoints_;
    dataX_.push_back(valX);
    dataY_.push_back(valY);
  }

  ImGui::SetNextWindowPos(ImVec2(x, y));
  ImGui::SetNextWindowSize(ImVec2(width, height));

  ImGui::Begin(
      name_, nullptr,
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
          ImGuiWindowFlags_NoSavedSettings |
          ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav |
          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);

  if (ImPlot::BeginPlot(name_, ImVec2(width, height),
                        ImPlotFlags_CanvasOnly | ImPlotFlags_NoFrame |
                            ImPlotFlags_NoInputs)) {

    ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations,
                      ImPlotAxisFlags_NoDecorations);

    ImPlot::PushStyleColor(ImPlotCol_PlotBg, ImVec4(0, 0, 0, 0));

    ImPlot::PlotLine(
        "#line", dataX_.data(), dataY_.data(), (int)graph_.size(),
        {ImPlotProp_LineColor, ImVec4(color.r, color.g, color.b, color.a)});

    ImPlot::PopStyleColor();
    ImPlot::EndPlot();
  }

  ImGui::End();
}
} // namespace Rodan::Debug
