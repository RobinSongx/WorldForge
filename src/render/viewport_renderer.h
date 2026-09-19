// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_
#define WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_

#include <cstdint>
#include <string>

#include "editor/editor_state.h"
namespace wf {
// Owns GL objects. A current GL context must outlive this object.
// Initialize once before rendering; failed initialization is safe to destroy.
class ViewportRenderer {
 public:
  ViewportRenderer() = default;
  ViewportRenderer(const ViewportRenderer&) = delete;
  ViewportRenderer& operator=(const ViewportRenderer&) = delete;
  ~ViewportRenderer();

  bool Initialize(std::string* error);
  bool Render(const EditorState& state, int width, int height,
              std::string* error);
  std::uintptr_t texture_token() const { return color_; }
  int width() const { return width_; }
  int height() const { return height_; }
  unsigned int draws() const { return draws_; }
  bool HasRenderedContent() const;

 private:
  bool Resize(int width, int height, std::string* error);
  unsigned int program_ = 0;
  unsigned int vao_ = 0;
  unsigned int vbo_ = 0;
  unsigned int grid_vao_ = 0;
  unsigned int grid_vbo_ = 0;
  unsigned int fbo_ = 0;
  unsigned int color_ = 0;
  unsigned int depth_ = 0;
  int width_ = 0;
  int height_ = 0;
  int grid_count_ = 0;
  unsigned int draws_{};
};
}  // namespace wf

#endif  // WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_
