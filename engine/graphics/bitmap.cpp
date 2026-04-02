#include "glm/fwd.hpp"
#include <graphics/bitmap.h>

namespace Rodan::Graphics {

glm::vec3 faceCoordsToXYZ(int i, int j, int faceID, int faceSize) {
  const float A = 2.0f * float(i) / faceSize;
  const float B = 2.0f * float(j) / faceSize;

  if (faceID == 0)
    return glm::vec3(-1.0f, A - 1.0f, B - 1.0f);
  if (faceID == 1)
    return glm::vec3(A - 1.0f, -1.0f, 1.0f - B);
  if (faceID == 2)
    return glm::vec3(1.0f, A - 1.0f, 1.0f - B);
  if (faceID == 3)
    return glm::vec3(1.0f - A, 1.0f, 1.0f - B);
  if (faceID == 4)
    return glm::vec3(B - 1.0f, A - 1.0f, 1.0f);
  if (faceID == 5)
    return glm::vec3(1.0f - B, A - 1.0f, -1.0f);

  return glm::vec3();
}

Bitmap convertEquirectangularMapToVerticalCross(const Bitmap &input) {
  if (input.type_ != BitmapType::Bitmap2D)
    return Bitmap();

  const int faceSize = input.w_ / 4;

  const int w = faceSize * 3;
  const int h = faceSize * 4;

  Bitmap result(w, h, input.comp_, input.fmt_);

  const glm::ivec2 kFaceOffsets[] = {glm::ivec2(faceSize, faceSize * 3),
                                     glm::ivec2(0, faceSize),
                                     glm::ivec2(faceSize, faceSize),
                                     glm::ivec2(faceSize * 2, faceSize),
                                     glm::ivec2(faceSize, 0),
                                     glm::ivec2(faceSize, faceSize * 2)};

  const int clampW = input.w_ - 1;
  const int clampH = input.h_ - 1;

  for (int face = 0; face != 6; face++) {
    for (int i = 0; i != faceSize; i++) {
      for (int j = 0; j != faceSize; j++) {
        const glm::vec3 P = faceCoordsToXYZ(i, j, face, faceSize);
        const float R = hypot(P.x, P.y);
        const float theta = atan2(P.y, P.x);
        const float phi = atan2(P.z, R);
        //	float point source coordinates
        const float Uf = float(2.0f * faceSize * (theta + M_PI) / M_PI);
        const float Vf = float(2.0f * faceSize * (M_PI / 2.0f - phi) / M_PI);
        // 4-samples for bilinear interpolation
        const int U1 = glm::clamp(int(floor(Uf)), 0, clampW);
        const int V1 = glm::clamp(int(floor(Vf)), 0, clampH);
        const int U2 = glm::clamp(U1 + 1, 0, clampW);
        const int V2 = glm::clamp(V1 + 1, 0, clampH);
        // fractional part
        const float s = Uf - U1;
        const float t = Vf - V1;
        // fetch 4-samples
        const glm::vec4 A = input.getPixel(U1, V1);
        const glm::vec4 B = input.getPixel(U2, V1);
        const glm::vec4 C = input.getPixel(U1, V2);
        const glm::vec4 D = input.getPixel(U2, V2);
        // bilinear interpolation
        const glm::vec4 color = A * (1 - s) * (1 - t) + B * (s) * (1 - t) +
                                C * (1 - s) * t + D * (s) * (t);
        result.setPixel(i + kFaceOffsets[face].x, j + kFaceOffsets[face].y,
                        color);
      }
    };
  }

  return result;
}

Bitmap convertVerticalCrossToCubeMapFaces(const Bitmap &input) {
  const int faceWidth = input.w_ / 3;
  const int faceHeight = input.h_ / 4;

  Bitmap cubemap(faceWidth, faceHeight, 6, input.comp_, input.fmt_);
  cubemap.type_ = BitmapType::Cube;

  const uint8_t *src = input.data_.data();
  uint8_t *dst = cubemap.data_.data();

  /*
      ------
      | +Y |
   ----------------
   | -X | -Z | +X |
   ----------------
      | -Y |
      ------
      | +Z |
      ------
  */

  const int pixelSize =
      cubemap.comp_ * Bitmap::getBytesPerComponent(cubemap.fmt_);

  for (int face = 0; face != 6; ++face) {
    for (int j = 0; j != faceHeight; ++j) {
      for (int i = 0; i != faceWidth; ++i) {
        int x = 0;
        int y = 0;

        switch (face) {
          // CUBE_MAP_POSITIVE_X
        case 0:
          x = 2 * faceWidth + i;
          y = 1 * faceHeight + j;
          break;

          // CUBE_MAP_NEGATIVE_X
        case 1:
          x = i;
          y = faceHeight + j;
          break;

          // CUBE_MAP_POSITIVE_Y
        case 2:
          x = 1 * faceWidth + i;
          y = j;
          break;

          // CUBE_MAP_NEGATIVE_Y
        case 3:
          x = 1 * faceWidth + i;
          y = 2 * faceHeight + j;
          break;

          // CUBE_MAP_POSITIVE_Z
        case 4:
          x = faceWidth + i;
          y = faceHeight + j;
          break;

          // CUBE_MAP_NEGATIVE_Z
        case 5:
          x = 2 * faceWidth - (i + 1);
          y = input.h_ - (j + 1);
          break;
        }

        memcpy(dst, src + (y * input.w_ + x) * pixelSize, pixelSize);

        dst += pixelSize;
      }
    }
  }

  return cubemap;
}

} // namespace Rodan::Graphics
