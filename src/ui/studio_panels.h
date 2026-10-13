// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_UI_STUDIO_PANELS_H_
#define WORLDFORGE_SRC_UI_STUDIO_PANELS_H_

#include <array>
#include <string>

#include "editor/editor_state.h"
#include "render/viewport_renderer.h"
#include "ui/editor_theme.h"

namespace wf {
// 仅保存面板开关、布局和瞬时统计；不持有场景资产。
// Stores panel visibility, layout and transient metrics, not scene assets.
struct PanelState {
  bool hierarchy = true;
  bool inspector = true;
  bool console = false;
  bool performance = false;
  bool assets = false;
  bool reset_layout = false;
  bool quit_requested = false;
  bool new_requested = false;
  bool open_requested = false;
  bool save_requested = false;
  bool save_as_requested = false;
  bool document_busy = false;
  std::string document_name = "Untitled *";
  bool import_requested = false;
  bool import_busy = false;
  std::string import_status = "Ready to import a static GLB";
  std::string import_metrics;
  std::array<float, 120> frame_times{};
  unsigned int frame_index{};
  unsigned int panels_drawn{};
  int viewport_width = 1;
  int viewport_height = 1;
  float image_x = 0;
  float image_y = 0;
};

// 绘制所有面板并提交编辑操作；需要当前 ImGui 帧和 GL 上下文。
// 渲染失败时返回 false，并写入非空的 error。
// Draws panels and applies edits within an active ImGui frame and GL context.
// Returns false and fills the non-null error on rendering failure.
bool DrawStudio(EditorState& editor, ViewportRenderer& renderer,
                PanelState& panels, const std::string& gpu, std::string* error);

}  // namespace wf

#endif  // WORLDFORGE_SRC_UI_STUDIO_PANELS_H_
