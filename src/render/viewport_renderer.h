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
class GpuImage;
class GpuSampler;

// 所有 GL 对象只能在当前上下文的主线程创建或销毁。
// All GL objects are created and destroyed on the main context thread.
class ViewportRenderer {
 public:
  ViewportRenderer();

  ViewportRenderer(const ViewportRenderer&) = delete;

  ViewportRenderer& operator=(const ViewportRenderer&) = delete;

  ~ViewportRenderer();

  bool Initialize(std::string* error);

  // 先上传全部几何、图片和采样器；全部成功才发布，失败释放临时资源。
  // Stages geometry, images and samplers; publishes only after all succeed.
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

  std::size_t cached_images() const { return images_.size(); }

  std::size_t cached_samplers() const { return samplers_.size(); }

  std::size_t texture_bytes() const;

  // 读取像素供测试判断非空画面；不用于每帧性能统计。
  // Reads pixels for nonempty-frame tests, not per-frame profiling.
  bool HasRenderedContent() const;

 private:
  class GpuMesh;
  bool Resize(int width, int height, std::string* error);

  std::map<std::uint64_t, std::unique_ptr<GpuMesh>> meshes_;
  std::map<std::uint64_t, std::unique_ptr<GpuImage>> images_;
  std::map<std::uint64_t, std::unique_ptr<GpuSampler>> samplers_;
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
