#include <scene/camera.h>
#include <stdexcept>

namespace Rodan {

const glm::mat4 &Camera::GetView() const { return view_; }

const glm::mat4 &Camera::GetProjection() const { return projections_.front(); }

const glm::mat4 &Camera::GetProjectionFromCascade(uint32_t cascade) const {
  if (cascade >= cascadeProjections_.size()) {
    throw std::out_of_range(
        "Camera::GetProjectionFromCascade: cascade requested out of bounds");
  }
  return cascadeProjections_[cascade];
}

float Camera::GetCascadeFarDistance(uint32_t cascade) const {
  if (cascade >= cascadeFarDistances_.size()) {
    throw std::out_of_range(
        "Camera::GetCascadeFarDistance: cascade requested out of bounds");
  }
  return cascadeFarDistances_[cascade];
}

void Camera::SetCascades(uint32_t cascadeCount, std::vector<float> cascades,
                         float maxDistance) {
  const size_t expectedSplitCount = cascadeCount > 0 ? cascadeCount - 1 : 0;
  if (cascades.size() != expectedSplitCount) {
    throw std::invalid_argument(
        "Camera::SetCascades: cascadeCount and cascades mismatch");
  }
  if (cascadeCount == 0) {
    throw std::invalid_argument(
        "Camera::SetCascades: cascadeCount must be greater than zero");
  }
  if (maxDistance <= 0.0f) {
    throw std::invalid_argument(
        "Camera::SetCascades: maxDistance must be greater than zero");
  }
  for (size_t i = 0; i < cascades.size(); ++i) {
    if (cascades[i] <= 0.0f || cascades[i] >= 1.0f ||
        (i > 0 && cascades[i] <= cascades[i - 1])) {
      throw std::invalid_argument(
          "Camera::SetCascades: splits must be strictly increasing in (0, 1)");
    }
  }

  cascadeCount_ = cascadeCount;
  cascades_ = std::move(cascades);
  cascadeMaxDistance_ = maxDistance;
  UpdateProjectionMatrix();

  if (fakeProjs_.empty()) {
    SetFakeAsCurrent();
  }
}

void Camera::SetFakeAsCurrent() {
  fakeView_ = view_;
  fakeProjs_ = cascadeProjections_;
}

} // namespace Rodan
