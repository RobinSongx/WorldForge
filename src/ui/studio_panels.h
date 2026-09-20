// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_UI_STUDIO_PANELS_H_
#define WORLDFORGE_SRC_UI_STUDIO_PANELS_H_

#include <array>
#include <string>

#include "editor/editor_state.h"
#include "render/viewport_renderer.h"

namespace wf {
// 仅保存面板开关、布局和瞬时统计；不持有场景资产。
// Stores panel visibility, layout and transient metrics, not scene assets.
struct PanelState {
  bool hierarchy = true;
  bool inspector = true;
  bool console = true;
  bool performance = true;
  bool assets = true;
  bool reset_layout = false;
  bool quit_requested = false;
  std::array<float, 120> frame_times{};
  unsigned int frame_index{};
  unsigned int panels_drawn{};
  int viewport_width = 1;
  int viewport_height = 1;
  float image_x = 0;
  float image_y = 0;
};

// 从基础主题重新计算 DPI 样式，可重复调用而不累积缩放。
// Rebuilds DPI styling from the base theme without cumulative scaling.
void ApplyTheme(float scale);

// 绘制所有面板并提交编辑操作；需要当前 ImGui 帧和 GL 上下文。
// 渲染失败时返回 false，并写入非空的 error。
// Draws panels and applies edits within an active ImGui frame and GL context.
// Returns false and fills the non-null error on rendering failure.
bool DrawStudio(EditorState& editor, ViewportRenderer& renderer,
                PanelState& panels, const std::string& gpu, std::string* error);

}  // namespace wf

#endif  // WORLDFORGE_SRC_UI_STUDIO_PANELS_H_
