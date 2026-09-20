// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "editor/editor_state.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

namespace {
// 不用 assert，以保证 Release 构建也执行检查并返回非零退出码。
// Avoids assert so checks remain active and fail the process in Release.
void Require(bool value, const char* message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
  }
}
}  // namespace

int main() {
  wf::EditorState state;
  Require(state.entities().size() == 1, "Initial cube missing");
  const auto first = state.selected()->id;
  const auto second = state.AddCube();
  Require(second != first && state.selection() == second,
          "Entity identity or selection failed");
  wf::Transform t;
  t.position = {3, 2, 1};
  t.scale = {2, 3, 4};
  Require(state.SetTransform(second, t), "Valid transform rejected");
  const auto center = t.Matrix() * glm::vec4(0, 0, 0, 1);
  Require(glm::length(glm::vec3(center) - t.position) < .0001f,
          "Transform composition incorrect");
  // 非法变换必须被拒绝，而且不能修改原有状态。
  // Invalid transforms must be rejected without mutating existing state.
  auto bad = t;
  bad.scale.y = 0;
  Require(!state.SetTransform(second, bad), "Singular scale accepted");
  bad = t;
  bad.position.x = std::numeric_limits<float>::quiet_NaN();
  Require(!state.SetTransform(second, bad), "NaN accepted");
  Require(state.selected()->transform.position == t.position,
          "Rejected transform mutated state");
  state.camera().Frame(*state.selected());
  Require(state.camera().target() == t.position,
          "Frame selection target wrong");
  // 窄视口取景必须包含旋转、非均匀缩放后的全部八角。
  // Narrow framing must contain all eight rotated/nonuniformly scaled corners.
  t.rotation_degrees = {20, 35, 15};
  Require(state.SetTransform(second, t), "Rotated transform rejected");
  state.camera().Frame(*state.selected(), 0.2f);
  const auto framed = state.selected()->asset->bounds.Transformed(t.Matrix());
  const auto projection =
      state.camera().Projection(0.2f) * state.camera().View();
  for (int i = 0; i < 8; ++i) {
    const glm::vec3 point{(i & 1) ? framed.max.x : framed.min.x,
                          (i & 2) ? framed.max.y : framed.min.y,
                          (i & 4) ? framed.max.z : framed.min.z};
    const auto clip = projection * glm::vec4(point, 1);
    Require(clip.w > 0 && std::abs(clip.x / clip.w) <= 1 &&
                std::abs(clip.y / clip.w) <= 1,
            "Frame cropped world bounds");
  }
  // 极端输入仍需保持距离与俯仰范围，生成有限的相机矩阵。
  // Extreme inputs must preserve bounds and finite camera matrices.
  state.camera().Zoom(10000);
  Require(state.camera().distance() >= .0001f, "Zoom underflow");
  state.camera().Zoom(-10000);
  Require(state.camera().distance() <= 1.0e15f, "Zoom overflow");
  state.camera().Orbit(0, 100000);
  Require(state.camera().pitch() <= 1.45f, "Orbit pole singularity");
  state.camera().Pan(5, 3);
  auto view = state.camera().View();
  for (int i = 0; i < 16; ++i)
    Require(std::isfinite(glm::value_ptr(view)[i]), "Nonfinite camera");
  Require(!state.Select(99999), "Invalid selection accepted");
  Require(state.RemoveSelected() && !state.selected(),
          "Delete did not clear selection");
  Require(state.entities().size() == 1, "Delete removed wrong count");
  state.NewScene();
  Require(state.selected()->id > second, "IDs reused after new scene");
  for (int i = 0; i < 600; ++i) state.Log("INFO", "bounded");
  Require(state.logs().size() == 500, "Log buffer unbounded");
  std::cout << "Editor state and camera checks passed\n";
  return 0;
}
