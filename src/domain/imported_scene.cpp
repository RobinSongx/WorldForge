// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "domain/imported_scene.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "glm/glm.hpp"

#include "domain/mesh_asset.h"

namespace wf {
std::shared_ptr<const ImportedScene> MakeCubeScene() {
  auto scene = std::make_shared<ImportedScene>();
  auto mesh = std::make_shared<MeshAsset>();
  mesh->id = NextAssetId();
  MeshPrimitive primitive;
  // 各面独立顶点保留硬边；Cube 与导入几何使用同一索引绘制路径。
  // Separate face vertices retain hard edges; cubes use the import draw path.
  const glm::vec3 normals[] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                               {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  const glm::vec3 corners[][4] = {
      {{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}},
      {{-.5f, -.5f, .5f},
       {-.5f, .5f, .5f},
       {-.5f, .5f, -.5f},
       {-.5f, -.5f, -.5f}},
      {{-.5f, .5f, -.5f}, {-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}},
      {{-.5f, -.5f, .5f},
       {-.5f, -.5f, -.5f},
       {.5f, -.5f, -.5f},
       {.5f, -.5f, .5f}},
      {{-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}},
      {{.5f, -.5f, -.5f},
       {-.5f, -.5f, -.5f},
       {-.5f, .5f, -.5f},
       {.5f, .5f, -.5f}}};
  for (int face = 0; face < 6; ++face) {
    for (int index : {0, 1, 2, 0, 2, 3}) {
      MeshVertex vertex;
      vertex.position = corners[face][index];
      vertex.normal = normals[face];
      primitive.bounds.Include(vertex.position);
      primitive.indices.push_back(
          static_cast<std::uint32_t>(primitive.vertices.size()));
      primitive.vertices.push_back(vertex);
    }
  }
  mesh->bounds = primitive.bounds;
  mesh->primitives.push_back(std::move(primitive));
  scene->meshes.push_back(mesh);
  ImportedNode node;
  node.name = "Cube geometry";
  node.mesh = 0;
  node.active = true;
  scene->nodes.push_back(node);
  scene->roots.push_back(0);
  scene->bounds = mesh->bounds;
  scene->vertices = 36;
  scene->triangles = 12;
  scene->upload_bytes = 36 * (sizeof(MeshVertex) + sizeof(std::uint32_t));
  return scene;
}
}  // namespace wf
