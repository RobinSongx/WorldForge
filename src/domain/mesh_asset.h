// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_DOMAIN_MESH_ASSET_H_
#define WORLDFORGE_SRC_DOMAIN_MESH_ASSET_H_

#include <cstdint>
#include <vector>

#include "glm/glm.hpp"

namespace wf {
struct Bounds {
  glm::vec3 min{0};
  glm::vec3 max{0};
  bool valid = false;
  void Include(const glm::vec3& point);

  void Include(const Bounds& bounds);

  Bounds Transformed(const glm::mat4& matrix) const;
};

struct MeshVertex {
  glm::vec3 position{0};
  glm::vec3 normal{0, 1, 0};
  glm::vec2 uv{0};
};

struct MeshPrimitive {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
  glm::vec3 base_color{1};
  Bounds bounds;
};

// 发布后通过 const shared_ptr 共享；ID 只在当前进程内有效。
// Shared through const shared_ptr after publication; IDs are process-local.
struct MeshAsset {
  std::uint64_t id = 0;
  std::vector<MeshPrimitive> primitives;
  Bounds bounds;
};

std::uint64_t NextAssetId();
}  // namespace wf
#endif  // WORLDFORGE_SRC_DOMAIN_MESH_ASSET_H_
