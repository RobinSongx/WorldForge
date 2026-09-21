// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_DOMAIN_IMPORTED_SCENE_H_
#define WORLDFORGE_SRC_DOMAIN_IMPORTED_SCENE_H_
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "domain/mesh_asset.h"

namespace wf {
struct ImportedNode {
  std::string name;
  int mesh = -1;
  std::vector<int> children;
  glm::mat4 local{1};
  glm::mat4 world{1};
  bool active = false;
};

// 保留源节点的局部矩阵，world 为选中 scene 内的预计算祖先链。
// Preserves source local matrices; world caches the selected scene's chain.
struct ImportedScene {
  std::vector<std::shared_ptr<const MeshAsset>> meshes;
  std::vector<ImportedNode> nodes;
  std::vector<int> roots;
  std::vector<std::string> warnings;
  Bounds bounds;
  std::size_t vertices = 0;
  std::size_t triangles = 0;
  std::size_t upload_bytes = 0;
  std::size_t image_bytes = 0;
};

std::shared_ptr<const ImportedScene> MakeCubeScene();
}  // namespace wf
#endif  // WORLDFORGE_SRC_DOMAIN_IMPORTED_SCENE_H_
