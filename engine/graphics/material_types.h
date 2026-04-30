#pragma once

#include <string>
namespace Rodan {

enum class AlphaMode {
  Opaque,
  Mask,
  Blend,
};

inline AlphaMode ToAlphaMode(std::string s) {
  if (s == "OPAQUE")
    return AlphaMode::Opaque;
  if (s == "MASK")
    return AlphaMode::Mask;
  if (s == "BLEND")
    return AlphaMode::Blend;

  return AlphaMode::Opaque;
}

} // namespace Rodan
