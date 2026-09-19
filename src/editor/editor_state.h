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

// Orbit angles are radians; input deltas are logical screen pixels.
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

// Owns the in-memory preview scene. Entity pointers are invalidated by add,
// remove and new-scene operations; callers must not retain them across edits.
class EditorState {
 public:
  EditorState();
  EntityId AddCube();
  bool RemoveSelected();
  bool Select(EntityId id);
  Entity* selected();
  const Entity* selected() const;
  bool SetTransform(EntityId id, const Transform& value);
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
