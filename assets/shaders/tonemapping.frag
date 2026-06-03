#version 450

layout (location=0) in vec2 texcoords;

layout(set = 0, binding = 0) uniform sampler2D u_Scene;
layout(location = 0) out vec4 outColor;

vec3 ACESFilm(vec3 x) {
  float a = 2.51f;
  float b = 0.03f;
  float c = 2.43f;
  float d = 0.59f;
  float e = 0.14f;

  return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
  vec2 uv = texcoords;

  vec3 baseColor = texture(u_Scene, uv).rgb;

  vec3 mapped = ACESFilm(baseColor);

  outColor = vec4(mapped, 1.0);
}
