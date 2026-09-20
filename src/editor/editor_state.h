// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_EDITOR_EDITOR_STATE_H_
#define WORLDFORGE_SRC_EDITOR_EDITOR_STATE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "glm/glm.hpp"

namespace wf {

using EntityId = std::uint64_t;

// 局部变换：平移采用世界单位，欧拉角以度为单位。
// Local transform: translation uses world units; Euler angles use degrees.
struct Transform {
  glm::vec3 position{0.0f};
  glm::vec3 rotation_degrees{0.0f};
  glm::vec3 scale{1.0f};
  glm::mat4 Matrix() const;
};

struct Entity {
  EntityId id;
  std::string name;
  Transform transform;
  glm::vec3 color{0.77f, 0.57f, 0.29f};
  bool visible = true;
};

// 绕目标点旋转的相机；内部角度为弧度，拖动增量为逻辑像素。
// Orbits a target; internal angles are radians, drag deltas logical pixels.
class OrbitCamera {
 public:
  glm::vec3 Eye() const;

  glm::mat4 View() const;

  glm::mat4 Projection(float aspect) const;

  void Orbit(float dx, float dy);

  void Pan(float dx, float dy);

  void Zoom(float amount);

  void Frame(const Entity& entity);

  const glm::vec3& target() const { return target_; }

  float yaw() const { return yaw_; }

  float pitch() const { return pitch_; }

  float distance() const { return distance_; }

 private:
  glm::vec3 target_{0.0f, 0.5f, 0.0f};
  float yaw_ = 0.7f;
  float pitch_ = 0.42f;
  float distance_ = 7.0f;
};

struct LogEntry {
  std::string level;
  std::string message;
};

struct DisplayOptions {
  bool grid = true;
  bool wireframe = false;
};

// 管理内存中的预览场景。增删或重建场景后，必须重新获取实体指针。
// Owns the in-memory scene. Reacquire entity pointers after add, remove,
// or new-scene operations; do not retain them across these edits.
class EditorState {
 public:
  EditorState();

  EntityId AddCube();

  bool RemoveSelected();

  bool Select(EntityId id);

  Entity* selected();

  const Entity* selected() const;

  // 先校验再更新；ID 无效或变换非法时返回 false，保留原值。
  // Validates before updating; invalid IDs or transforms leave state intact.
  bool SetTransform(EntityId id, const Transform& value);

  // 重建示例场景和相机；保留日志与 ID 计数器。
  // Resets the sample scene and camera, retaining logs and the ID counter.
  void NewScene();

  void Log(std::string level, std::string message);

  const std::vector<Entity>& entities() const { return entities_; }

  const std::vector<LogEntry>& logs() const { return logs_; }

  std::optional<EntityId> selection() const { return selection_; }

  OrbitCamera& camera() { return camera_; }

  const OrbitCamera& camera() const { return camera_; }

  DisplayOptions& display_options() { return display_options_; }

  const DisplayOptions& display_options() const { return display_options_; }

 private:
  std::vector<Entity> entities_;
  std::vector<LogEntry> logs_;
  std::optional<EntityId> selection_;
  EntityId next_id_ = 1;
  OrbitCamera camera_;
  DisplayOptions display_options_;
};

}  // namespace wf

#endif  // WORLDFORGE_SRC_EDITOR_EDITOR_STATE_H_
