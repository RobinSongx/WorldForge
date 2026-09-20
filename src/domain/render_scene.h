// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_DOMAIN_RENDER_SCENE_H_
#define WORLDFORGE_SRC_DOMAIN_RENDER_SCENE_H_
#include <memory>
#include <vector>

#include "glm/glm.hpp"

#include "domain/mesh_asset.h"

namespace wf {
// 每帧只读快照；持有 Mesh 引用直到本帧绘制结束，不借用可变实体。
// Read-only frame snapshot; retains meshes without borrowing mutable entities.
struct MeshDraw {
  std::shared_ptr<const MeshAsset> mesh;
  glm::mat4 model{1};
  glm::vec3 tint{1};
  bool selected = false;
};

struct RenderScene {
  glm::mat4 view_projection{1};
  std::vector<MeshDraw> meshes;
  bool grid = true;
  bool wireframe = false;
};
}  // namespace wf
#endif  // WORLDFORGE_SRC_DOMAIN_RENDER_SCENE_H_
