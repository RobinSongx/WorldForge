// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_EDITOR_EDITOR_STATE_H_
#define WORLDFORGE_SRC_EDITOR_EDITOR_STATE_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "glm/glm.hpp"

#include "domain/imported_scene.h"
#include "domain/render_scene.h"
#include "domain/scene_document.h"

namespace wf {

// 绕目标点旋转的相机；内部角度为弧度，拖动增量为逻辑像素。
// Orbits a target; internal angles are radians, drag deltas logical pixels.
class OrbitCamera {
 public:
  CameraState CaptureState() const;

  bool RestoreState(const CameraState& state);

  glm::vec3 Eye() const;

  glm::mat4 View() const;

  glm::mat4 Projection(float aspect) const;

  void Orbit(float dx, float dy);

  void Pan(float dx, float dy);

  void Zoom(float amount);

  void Frame(const Entity& entity, float aspect = 1.0f);

  const glm::vec3& target() const { return target_; }

  float yaw() const { return yaw_; }

  float pitch() const { return pitch_; }

  float distance() const { return distance_; }

 private:
  glm::vec3 target_{0.0f, 0.5f, 0.0f};
  float yaw_ = 0.7f;
  float pitch_ = 0.42f;
  float distance_ = 7.0f;
  float radius_ = 1.0f;
};

struct LogEntry {
  std::string level;
  std::string message;
};

// 管理内存中的预览场景。增删或重建场景后，必须重新获取实体指针。
// Owns the in-memory scene. Reacquire entity pointers after add, remove,
// or new-scene operations; do not retain them across these edits.
class EditorState {
 public:
  explicit EditorState(IdGenerator ids = {});

  SceneSnapshot CaptureSnapshot() const;

  bool BuildDocument(const SceneSnapshot& snapshot,
                     const std::vector<AssetRecord>& assets,
                     SceneDocument* document, std::string* error) const;

  void PublishDocument(SceneDocument document, const SceneSnapshot& snapshot);

  bool CanAddAsset(const ImportedScene& asset) const;

  bool SetAppearance(EntityId id, const glm::vec3& color, bool visible);

  bool dirty() const {
    return !saved_ || document_.revision != saved_revision_;
  }

  void MarkUnsaved() { saved_ = false; }

  void MarkSaved() {
    saved_ = true;
    saved_revision_ = document_.revision;
  }

  std::uint64_t revision() const { return document_.revision; }

  EntityId AddCube();

  // asset 必须是已校验且已完成 GPU 准备的不可变结果；仅主线程调用。
  // Main thread only; asset must be validated, immutable and GPU-prepared.
  EntityId AddImported(std::string name,
                       std::shared_ptr<const ImportedScene> asset,
                       AssetSource source = {});

  RenderScene BuildRenderScene(float aspect) const;

  std::uint64_t generation() const { return generation_; }

  bool RemoveSelected();

  bool Select(EntityId id);

  void ClearSelection();

  const Entity* selected() const;

  // 先校验再更新；ID 无效或变换非法时返回 false，保留原值。
  // Validates before updating; invalid IDs or transforms leave state intact.
  bool SetTransform(EntityId id, const Transform& value);

  // 重建示例场景和相机；保留日志与 ID 计数器。
  // Resets the sample scene and camera, retaining logs and the ID counter.
  void NewScene();

  void Log(std::string level, std::string message);

  const std::vector<Entity>& entities() const { return document_.entities; }

  const std::vector<LogEntry>& logs() const { return logs_; }

  std::optional<EntityId> selection() const { return selection_; }

  OrbitCamera& camera() { return camera_; }

  const OrbitCamera& camera() const { return camera_; }

  DisplayOptions& display_options() { return display_options_; }

  const DisplayOptions& display_options() const { return display_options_; }

 private:
  SceneDocument document_;
  IdGenerator ids_;
  std::uint64_t saved_revision_ = 0;
  bool saved_ = false;
  std::vector<LogEntry> logs_;
  std::optional<EntityId> selection_;
  EntityId next_id_ = 1;
  std::uint64_t generation_ = 0;
  std::shared_ptr<const ImportedScene> cube_asset_;
  OrbitCamera camera_;
  DisplayOptions display_options_;
};

}  // namespace wf

#endif  // WORLDFORGE_SRC_EDITOR_EDITOR_STATE_H_
