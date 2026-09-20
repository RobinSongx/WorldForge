// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "editor/editor_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "domain/render_scene.h"

namespace wf {
glm::mat4 Transform::Matrix() const {
  // 使用列向量，矩阵为 T * Rz * Ry * Rx * S，作用顺序从右向左。
  // Uses column vectors: T * Rz * Ry * Rx * S, applied right to left.
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
  return glm::perspective(glm::radians(45.0f), std::max(aspect, 0.01f),
                          std::max(0.0001f, distance_ * 0.0001f),
                          std::max(500.0f, distance_ + radius_ * 4));
}

void OrbitCamera::Orbit(float dx, float dy) {
  // 俯仰角不触及极点，避免视线与向上方向共线。
  // Keeps pitch away from poles so view and up directions stay independent.
  yaw_ -= dx * 0.008f;
  pitch_ = std::clamp(pitch_ + dy * 0.008f, -1.45f, 1.45f);
}

void OrbitCamera::Pan(float dx, float dy) {
  // 沿相机的右方和上方平移；距离越远，相同拖动覆盖的世界范围越大。
  // Pans in the camera plane; world-space motion scales with distance.
  const auto forward = glm::normalize(target_ - Eye());
  const auto right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));
  const auto up = glm::cross(right, forward);
  target_ += (-dx * right + dy * up) * distance_ * 0.0015f;
}

void OrbitCamera::Zoom(float amount) {
  // 指数缩放使滚轮改变距离比例，限制范围以保持可用视距。
  // Exponential zoom changes distance proportionally within usable limits.
  distance_ =
      std::clamp(distance_ * std::exp(-amount * 0.15f), 0.0001f, 1.0e15f);
}

void OrbitCamera::Frame(const Entity& entity, float aspect) {
  // 包装根变换后合并八角包围盒；保持源资产的原点和尺寸不变。
  // Frames transformed bounds without changing the source origin or scale.
  if (!entity.asset) return;
  const auto bounds =
      entity.asset->bounds.Transformed(entity.transform.Matrix());
  if (!bounds.valid) return;
  target_ = bounds.min * 0.5f + bounds.max * 0.5f;
  radius_ = std::max(glm::length(bounds.max - bounds.min) * 0.5f, 0.0001f);
  // 窄视口使用水平视角，确保整个包围球落入画面。
  // Uses the horizontal angle in narrow viewports to fit the bounding sphere.
  const float angle = std::atan(std::tan(glm::radians(22.5f)) *
                                std::clamp(aspect, 0.01f, 1.0f));
  distance_ = radius_ * 1.1f / std::sin(angle);
}

EditorState::EditorState() : cube_asset_(MakeCubeScene()) { NewScene(); }

EntityId EditorState::AddCube() {
  // ID 与容器下标分离，删除或重建场景不会复用旧 ID。
  // Monotonic IDs stay independent of vector indices across scene resets.
  Entity e{};
  e.id = next_id_++;
  e.name = "Cube";
  e.asset = cube_asset_;
  e.name += " " + std::to_string(e.id);
  e.transform.position = {static_cast<float>(entities_.size()) * 1.4f, 0.5f,
                          0.0f};
  selection_ = e.id;
  entities_.push_back(e);
  Log("INFO", "Created " + e.name);
  return e.id;
}

EntityId EditorState::AddImported(std::string name,
                                  std::shared_ptr<const ImportedScene> asset) {
  Entity entity{};
  entity.id = next_id_++;
  entity.name = std::move(name);
  entity.asset = std::move(asset);
  entity.color = glm::vec3(1);
  selection_ = entity.id;
  entities_.push_back(std::move(entity));
  camera_.Frame(entities_.back());
  Log("INFO", "Imported " + entities_.back().name);
  return entities_.back().id;
}

RenderScene EditorState::BuildRenderScene(float aspect) const {
  RenderScene scene;
  scene.view_projection = camera_.Projection(aspect) * camera_.View();
  scene.grid = display_options_.grid;
  scene.wireframe = display_options_.wireframe;
  for (const auto& entity : entities_) {
    if (!entity.visible || !entity.asset) continue;
    for (const auto& node : entity.asset->nodes) {
      if (!node.active || node.mesh < 0) continue;
      scene.meshes.push_back({entity.asset->meshes[node.mesh],
                              entity.transform.Matrix() * node.world,
                              entity.color, selection_ == entity.id});
    }
  }
  return scene;
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
  // 先校验所有分量，避免部分更新；非零缩放保证法线矩阵可求逆。
  // Validates all components first; nonzero scale keeps normals invertible.
  for (int i = 0; i < 3; ++i) {
    if (!std::isfinite(value.position[i]) ||
        !std::isfinite(value.rotation_degrees[i]) ||
        !std::isfinite(value.scale[i]) || value.scale[i] < 0.01f ||
        value.scale[i] > 100.0f || std::abs(value.position[i]) > 1.0e12f ||
        std::abs(value.rotation_degrees[i]) > 1.0e6f)
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
  ++generation_;
  entities_.clear();
  selection_.reset();
  camera_ = OrbitCamera{};
  AddCube();
  Log("INFO", "New scene. Import a static GLB from File > Import GLB.");
}

void EditorState::Log(std::string level, std::string message) {
  // 仅保留最近 500 条记录，避免长时间编辑时日志无限增长。
  // Retains the newest 500 entries to bound memory during long sessions.
  if (logs_.size() >= 500) logs_.erase(logs_.begin());
  logs_.push_back({std::move(level), std::move(message)});
}
}  // namespace wf
