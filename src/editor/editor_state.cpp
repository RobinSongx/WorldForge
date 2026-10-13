// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "editor/editor_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "domain/render_scene.h"
#include "platform/scene_identity.h"

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

EditorState::EditorState(IdGenerator ids)
    : ids_(ids ? std::move(ids) : GenerateUuid), cube_asset_(MakeCubeScene()) {
  NewScene();
}

EntityId EditorState::AddCube() {
  // ID 与容器下标分离，删除或重建场景不会复用旧 ID。
  // Monotonic IDs stay independent of vector indices across scene resets.
  if (document_.entities.size() >= 10000 ||
      document_.revision == std::numeric_limits<std::uint64_t>::max())
    return 0;
  Entity e{};
  e.persistent_id = ids_();
  if (!ValidUuid(e.persistent_id)) return 0;
  auto asset =
      std::find_if(document_.assets.begin(), document_.assets.end(),
                   [](const AssetRecord& a) { return a.source.builtin; });
  if (asset == document_.assets.end()) {
    if (document_.assets.size() >= 256 || !CanAddAsset(*cube_asset_)) return 0;
    const auto id = ids_();
    if (!ValidUuid(id)) return 0;
    AssetSource source;
    source.builtin = true;
    document_.assets.push_back({id, source, cube_asset_});
    e.asset_id = id;
    e.asset = cube_asset_;
  } else {
    e.asset_id = asset->id;
    e.asset = asset->payload;
  }
  e.id = next_id_++;
  e.name = "Cube";
  e.name += " " + std::to_string(e.id);
  e.transform.position = {static_cast<float>(document_.entities.size()) * 1.4f,
                          0.5f, 0.0f};
  selection_ = e.id;
  document_.entities.push_back(e);
  ++document_.revision;
  Log("INFO", "Created " + e.name);
  return e.id;
}

EntityId EditorState::AddImported(std::string name,
                                  std::shared_ptr<const ImportedScene> asset,
                                  AssetSource source) {
  if (!asset || !CanAddAsset(*asset) || document_.assets.size() >= 256 ||
      document_.entities.size() >= 10000 ||
      document_.revision == std::numeric_limits<std::uint64_t>::max())
    return 0;
  Entity entity{};
  entity.persistent_id = ids_();
  entity.asset_id = ids_();
  if (!ValidUuid(entity.persistent_id) || !ValidUuid(entity.asset_id)) return 0;
  document_.assets.push_back({entity.asset_id, std::move(source), asset});
  ++document_.revision;
  entity.id = next_id_++;
  entity.name = std::move(name);
  entity.asset = std::move(asset);
  entity.color = glm::vec3(1);
  selection_ = entity.id;
  document_.entities.push_back(std::move(entity));
  camera_.Frame(document_.entities.back());
  Log("INFO", "Imported " + document_.entities.back().name);
  return document_.entities.back().id;
}

RenderScene EditorState::BuildRenderScene(float aspect) const {
  RenderScene scene;
  scene.view_projection = camera_.Projection(aspect) * camera_.View();
  scene.grid = display_options_.grid;
  scene.wireframe = display_options_.wireframe;
  scene.selection_overlay = display_options_.selection_overlay;
  for (const auto& entity : document_.entities) {
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
  if (!selection_ ||
      document_.revision == std::numeric_limits<std::uint64_t>::max())
    return false;
  const auto id = *selection_;
  auto it = std::find_if(document_.entities.begin(), document_.entities.end(),
                         [id](const Entity& e) { return e.id == id; });
  if (it == document_.entities.end()) return false;
  Log("INFO", "Deleted " + it->name);
  document_.entities.erase(it);
  std::erase_if(document_.assets, [this](const AssetRecord& a) {
    return std::none_of(document_.entities.begin(), document_.entities.end(),
                        [&a](const Entity& e) { return e.asset_id == a.id; });
  });
  ++document_.revision;
  selection_.reset();
  return true;
}

bool EditorState::Select(EntityId id) {
  if (std::none_of(document_.entities.begin(), document_.entities.end(),
                   [id](const Entity& e) { return e.id == id; }))
    return false;
  selection_ = id;
  return true;
}

void EditorState::ClearSelection() { selection_.reset(); }

const Entity* EditorState::selected() const {
  if (!selection_) return nullptr;
  auto it = std::find_if(
      document_.entities.begin(), document_.entities.end(),
      [this](const Entity& entity) { return entity.id == *selection_; });
  return it == document_.entities.end() ? nullptr : &*it;
}

bool EditorState::SetTransform(EntityId id, const Transform& value) {
  if (!ValidTransform(value)) return false;
  for (auto& e : document_.entities)
    if (e.id == id) {
      if (e.transform.position == value.position &&
          e.transform.rotation_degrees == value.rotation_degrees &&
          e.transform.scale == value.scale)
        return true;
      if (document_.revision == std::numeric_limits<std::uint64_t>::max())
        return false;
      e.transform = value;
      ++document_.revision;
      return true;
    }
  return false;
}

void EditorState::NewScene() {
  const auto id = ids_();
  if (!ValidUuid(id)) {
    Log("ERROR", "UUID generation failed");
    return;
  }
  ++generation_;
  document_ = {};
  document_.id = id;
  saved_ = false;
  selection_.reset();
  camera_ = OrbitCamera{};
  AddCube();
  Log("INFO", "New scene. Import a static GLB from File > Import GLB.");
}

CameraState OrbitCamera::CaptureState() const {
  return {target_, yaw_, pitch_, distance_, radius_};
}

bool OrbitCamera::RestoreState(const CameraState& state) {
  if (!ValidCamera(state)) return false;
  target_ = state.target;
  yaw_ = state.yaw_radians;
  pitch_ = state.pitch_radians;
  distance_ = state.distance;
  radius_ = state.framing_radius;
  return true;
}

bool EditorState::CanAddAsset(const ImportedScene& asset) const {
  std::size_t geometry = asset.upload_bytes;
  std::size_t images = asset.image_bytes;
  for (const auto& a : document_.assets) {
    geometry += a.payload->upload_bytes;
    images += a.payload->image_bytes;
  }
  return geometry <= 256 * 1024 * 1024 && images <= 128 * 1024 * 1024;
}

bool EditorState::SetAppearance(EntityId id, const glm::vec3& color,
                                bool visible) {
  for (int i = 0; i < 3; ++i)
    if (!std::isfinite(color[i]) || color[i] < 0 || color[i] > 1) return false;
  for (auto& e : document_.entities) {
    if (e.id != id) continue;
    if (e.color == color && e.visible == visible) return true;
    if (document_.revision == std::numeric_limits<std::uint64_t>::max())
      return false;
    e.color = color;
    e.visible = visible;
    ++document_.revision;
    return true;
  }
  return false;
}

SceneSnapshot EditorState::CaptureSnapshot() const {
  SceneSnapshot s;
  s.scene_id = document_.id;
  s.revision = document_.revision;
  s.camera = camera_.CaptureState();
  s.display = display_options_;
  for (const auto& a : document_.assets) s.assets.push_back({a.id, a.source});
  for (const auto& e : document_.entities)
    s.entities.push_back(
        {e.persistent_id, e.name, e.asset_id, e.transform, e.color, e.visible});
  return s;
}

bool EditorState::BuildDocument(const SceneSnapshot& s,
                                const std::vector<AssetRecord>& assets,
                                SceneDocument* document,
                                std::string* error) const {
  if (!ValidateSnapshot(s, error) || assets.size() != s.assets.size())
    return false;
  SceneDocument candidate;
  candidate.id = s.scene_id;
  candidate.revision = s.revision;
  candidate.assets = assets;
  EntityId next = next_id_;
  for (const auto& e : s.entities) {
    auto a = std::find_if(
        assets.begin(), assets.end(),
        [&e](const AssetRecord& record) { return record.id == e.asset_id; });
    if (a == assets.end() || !a->payload) {
      *error = "Prepared scene has an unbound asset";
      return false;
    }
    candidate.entities.push_back({next++, e.name, e.transform, e.tint_srgb,
                                  e.visible, a->payload, e.id, e.asset_id});
  }
  *document = std::move(candidate);
  return true;
}

void EditorState::PublishDocument(SceneDocument document,
                                  const SceneSnapshot& snapshot) {
  // 调用者已完成 CPU 校验和整批 GPU 准备；此处只移动权威文档。
  // Caller validated CPU data and staged all GPU resources; move authority.
  for (const auto& e : document.entities)
    next_id_ = std::max(next_id_, e.id + 1);
  document_ = std::move(document);
  camera_.RestoreState(snapshot.camera);
  display_options_ = snapshot.display;
  selection_.reset();
  ++generation_;
  MarkSaved();
}

void EditorState::Log(std::string level, std::string message) {
  // 仅保留最近 500 条记录，避免长时间编辑时日志无限增长。
  // Retains the newest 500 entries to bound memory during long sessions.
  if (logs_.size() >= 500) logs_.erase(logs_.begin());
  logs_.push_back({std::move(level), std::move(message)});
}
}  // namespace wf
