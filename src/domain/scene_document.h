// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_DOMAIN_SCENE_DOCUMENT_H_
#define WORLDFORGE_SRC_DOMAIN_SCENE_DOCUMENT_H_
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "domain/imported_scene.h"

namespace wf {
using EntityId = std::uint64_t;
using IdGenerator = std::function<std::string()>;

struct Transform {
  glm::vec3 position{0};
  glm::vec3 rotation_degrees{0};
  glm::vec3 scale{1};
  glm::mat4 Matrix() const;
};

struct CameraState {
  glm::vec3 target{0, .5f, 0};
  float yaw_radians = .7f;
  float pitch_radians = .42f;
  float distance = 7;
  float framing_radius = 1;
};

struct DisplayOptions {
  bool grid = true;
  bool wireframe = false;
  bool selection_overlay = false;
};

// 文件快照没有运行时句柄；来源在内存中使用规范绝对路径。
// File snapshots have no runtime handles; live sources use canonical paths.
struct AssetSource {
  bool builtin = false;
  std::string path_base = "absolute";
  std::string path;
  std::string sha256;
};

struct AssetRecord {
  std::string id;
  AssetSource source;
  std::shared_ptr<const ImportedScene> payload;
};

struct EntitySnapshot {
  std::string id;
  std::string name;
  std::string asset_id;
  Transform transform;
  glm::vec3 tint_srgb{1};
  bool visible = true;
};

struct AssetSnapshot {
  std::string id;
  AssetSource source;
};

struct SceneSnapshot {
  std::string scene_id;
  std::uint64_t revision = 0;
  std::vector<AssetSnapshot> assets;
  std::vector<EntitySnapshot> entities;
  CameraState camera;
  DisplayOptions display;
};

// asset 是资产表的不可变别名，发布时统一绑定，不能独立替换。
// asset aliases immutable table payloads; publication binds it consistently.
struct Entity {
  EntityId id = 0;
  std::string name;
  Transform transform;
  glm::vec3 color{.77f, .57f, .29f};
  bool visible = true;
  std::shared_ptr<const ImportedScene> asset;
  std::string persistent_id;
  std::string asset_id;
};

struct SceneDocument {
  std::string id;
  std::uint64_t revision = 0;
  std::vector<AssetRecord> assets;
  std::vector<Entity> entities;
};

bool ValidUtf8(const std::string& text);
bool ValidUuid(const std::string& text);
bool ValidTransform(const Transform& transform);
bool ValidCamera(const CameraState& camera);
bool ValidateSnapshot(const SceneSnapshot& scene, std::string* error);
}  // namespace wf
#endif  // WORLDFORGE_SRC_DOMAIN_SCENE_DOCUMENT_H_
