// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_
#define WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_

#include <cstdint>
#include <string>

#include "editor/editor_state.h"

namespace wf {
// 持有 GL 对象，所有调用和析构都要求所属 GL 上下文当前有效。
// 只初始化一次；初始化失败后仍可安全析构。
// Owns GL objects; all calls and destruction require their current context.
// Initialize once before rendering; failed initialization is safe to destroy.
class ViewportRenderer {
 public:
  ViewportRenderer() = default;

  ViewportRenderer(const ViewportRenderer&) = delete;

  ViewportRenderer& operator=(const ViewportRenderer&) = delete;

  ~ViewportRenderer();

  bool Initialize(std::string* error);

  // 按像素尺寸渲染离屏视口；失败时将诊断写入非空的 error。
  // Renders offscreen in pixels; writes failures to the non-null error.
  bool Render(const EditorState& state, int width, int height,
              std::string* error);

  // 借用的 GL 纹理句柄；调整尺寸或析构会使旧句柄失效。
  // Returns a borrowed GL texture handle, invalidated by resize or destruction.
  std::uintptr_t texture_token() const { return color_; }

  int width() const { return width_; }

  int height() const { return height_; }

  unsigned int draws() const { return draws_; }

  // 仅供固定 Cube 冒烟测试使用，会读回像素；不用于每帧统计。
  // Reads pixels for the fixed cube smoke test, not per-frame profiling.
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
