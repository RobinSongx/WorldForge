// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "domain/mesh_asset.h"

#include <atomic>
#include <cstdint>

#include "glm/glm.hpp"

namespace wf {
void Bounds::Include(const glm::vec3& point) {
  if (!valid) {
    min = max = point;
    valid = true;
  } else {
    min = glm::min(min, point);
    max = glm::max(max, point);
  }
}

void Bounds::Include(const Bounds& bounds) {
  if (!bounds.valid) return;
  Include(bounds.min);
  Include(bounds.max);
}

Bounds Bounds::Transformed(const glm::mat4& matrix) const {
  Bounds result;
  if (!valid) return result;
  // 变换八个角，正确处理旋转、负缩放和非均匀缩放。
  // Transforms all eight corners for rotation, negative and nonuniform scale.
  for (int i = 0; i < 8; ++i) {
    const glm::vec3 corner{(i & 1) ? max.x : min.x, (i & 2) ? max.y : min.y,
                           (i & 4) ? max.z : min.z};
    result.Include(glm::vec3(matrix * glm::vec4(corner, 1)));
  }
  return result;
}

std::uint64_t NextAssetId() {
  static std::atomic<std::uint64_t> next{1};
  return next.fetch_add(1, std::memory_order_relaxed);
}
}  // namespace wf
