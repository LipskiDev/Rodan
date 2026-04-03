#include <scene/camera.h>

namespace Rodan {

const glm::mat4 &Camera::GetView() const { return view_; }

const glm::mat4 &Camera::GetProjection() const { return projection_; }

} // namespace Rodan
