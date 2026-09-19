// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_UI_STUDIO_PANELS_H_
#define WORLDFORGE_SRC_UI_STUDIO_PANELS_H_

#include <array>
#include <string>

#include "editor/editor_state.h"
#include "render/viewport_renderer.h"
namespace wf {
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
void ApplyTheme(float scale);
bool DrawStudio(EditorState& editor, ViewportRenderer& renderer,
                PanelState& panels, const std::string& gpu, std::string* error);
}  // namespace wf

#endif  // WORLDFORGE_SRC_UI_STUDIO_PANELS_H_
