// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_
#define WORLDFORGE_SRC_RENDER_VIEWPORT_RENDERER_H_

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "domain/imported_scene.h"
#include "domain/render_scene.h"

namespace wf {
// 所有 GL 对象只能在当前上下文的主线程创建或销毁。
// All GL objects are created and destroyed on the main context thread.
class ViewportRenderer {
 public:
  ViewportRenderer();

  ViewportRenderer(const ViewportRenderer&) = delete;

  ViewportRenderer& operator=(const ViewportRenderer&) = delete;

  ~ViewportRenderer();

  bool Initialize(std::string* error);

  // 先上传全部新 Mesh，再发布缓存；失败释放本次临时资源。
  // Stages all new meshes before cache publication; failure releases staging.
  bool Prepare(const ImportedScene& scene, std::string* error);

  void CollectUnused();

  bool Render(const RenderScene& scene, int width, int height,
              std::string* error);

  std::uintptr_t texture_token() const { return color_; }

  int width() const { return width_; }

  int height() const { return height_; }

  unsigned int draws() const { return draws_; }

  std::size_t cached_meshes() const { return meshes_.size(); }

  std::size_t gpu_bytes() const;

  // 读取像素供测试判断非空画面；不用于每帧性能统计。
  // Reads pixels for nonempty-frame tests, not per-frame profiling.
  bool HasRenderedContent() const;

 private:
  class GpuMesh;
  bool Resize(int width, int height, std::string* error);

  std::map<std::uint64_t, std::unique_ptr<GpuMesh>> meshes_;
  unsigned int program_ = 0;
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
