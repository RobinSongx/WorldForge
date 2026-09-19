// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "editor/editor_state.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace wf {
glm::mat4 Transform::Matrix() const {
  auto m = glm::translate(glm::mat4(1), position);
  m = glm::rotate(m, glm::radians(rotation_degrees.z), {0, 0, 1});
  m = glm::rotate(m, glm::radians(rotation_degrees.y), {0, 1, 0});
  m = glm::rotate(m, glm::radians(rotation_degrees.x), {1, 0, 0});
  return glm::scale(m, scale);
}
glm::vec3 OrbitCamera::Eye() const {
  return target_ + distance_ * glm::vec3(std::cos(pitch_) * std::sin(yaw_),
                                         std::sin(pitch_),
                                         std::cos(pitch_) * std::cos(yaw_));
}
glm::mat4 OrbitCamera::View() const {
  return glm::lookAt(Eye(), target_, glm::vec3(0, 1, 0));
}
glm::mat4 OrbitCamera::Projection(float aspect) const {
  return glm::perspective(glm::radians(45.0f), std::max(aspect, 0.01f), 0.05f,
                          500.0f);
}
void OrbitCamera::Orbit(float dx, float dy) {
  yaw_ -= dx * 0.008f;
  pitch_ = std::clamp(pitch_ + dy * 0.008f, -1.45f, 1.45f);
}
void OrbitCamera::Pan(float dx, float dy) {
  const auto forward = glm::normalize(target_ - Eye());
  const auto right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
  const auto up = glm::cross(right, forward);
  target_ += (-dx * right + dy * up) * distance_ * 0.0015f;
}
void OrbitCamera::Zoom(float amount) {
  distance_ = std::clamp(distance_ * std::exp(-amount * 0.15f), 0.2f, 200.0f);
}
void OrbitCamera::Frame(const Entity& entity) {
  target_ = entity.transform.position;
  distance_ =
      std::clamp(glm::length(entity.transform.scale) * 2.0f, 1.5f, 200.0f);
}
EditorState::EditorState() { NewScene(); }
EntityId EditorState::AddCube() {
  Entity e{next_id_++, "Cube"};
  e.name += " " + std::to_string(e.id);
  e.transform.position = {static_cast<float>(entities_.size()) * 1.4f, 0.5f,
                          0.0f};
  selection_ = e.id;
  entities_.push_back(e);
  Log("INFO", "Created " + e.name);
  return e.id;
}
bool EditorState::RemoveSelected() {
  if (!selection_) return false;
  const auto id = *selection_;
  auto it = std::find_if(entities_.begin(), entities_.end(),
                         [id](const Entity& e) { return e.id == id; });
  if (it == entities_.end()) return false;
  Log("INFO", "Deleted " + it->name);
  entities_.erase(it);
  selection_.reset();
  return true;
}
bool EditorState::Select(EntityId id) {
  if (std::none_of(entities_.begin(), entities_.end(),
                   [id](const Entity& e) { return e.id == id; }))
    return false;
  selection_ = id;
  return true;
}
Entity* EditorState::selected() {
  if (!selection_) return nullptr;
  auto it =
      std::find_if(entities_.begin(), entities_.end(),
                   [this](const Entity& e) { return e.id == *selection_; });
  return it == entities_.end() ? nullptr : &*it;
}
const Entity* EditorState::selected() const {
  if (!selection_) return nullptr;
  auto it = std::find_if(
      entities_.begin(), entities_.end(),
      [this](const Entity& entity) { return entity.id == *selection_; });
  return it == entities_.end() ? nullptr : &*it;
}
bool EditorState::SetTransform(EntityId id, const Transform& value) {
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(value.position[i]) ||
        !std::isfinite(value.rotation_degrees[i]) ||
        !std::isfinite(value.scale[i]) || value.scale[i] < 0.01f ||
        value.scale[i] > 100.0f)
      return false;
  }
  for (auto& e : entities_)
    if (e.id == id) {
      e.transform = value;
      return true;
    }
  return false;
}
void EditorState::NewScene() {
  entities_.clear();
  selection_.reset();
  camera_ = OrbitCamera{};
  AddCube();
  Log("INFO",
      "New procedural scene. File import arrives in the Mesh milestone.");
}
void EditorState::Log(std::string level, std::string message) {
  if (logs_.size() >= 500) logs_.erase(logs_.begin());
  logs_.push_back({std::move(level), std::move(message)});
}
}  // namespace wf
