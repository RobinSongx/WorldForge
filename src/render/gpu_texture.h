// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_RENDER_GPU_TEXTURE_H_
#define WORLDFORGE_SRC_RENDER_GPU_TEXTURE_H_
#include <cstddef>
#include <memory>
#include <string>

#include "domain/mesh_asset.h"

namespace wf {
// 必须在 GL 主上下文内使用；弱引用允许最后一个资产消失时回收缓存。
// Requires the main GL context; weak ownership enables last-asset collection.
class GpuImage {
 public:
  explicit GpuImage(const std::shared_ptr<const TextureImage>& image);

  GpuImage(const GpuImage&) = delete;

  GpuImage& operator=(const GpuImage&) = delete;

  ~GpuImage();

  bool Upload(const TextureImage& image, std::string* error);

  bool expired() const { return owner_.expired(); }

  unsigned int id() const { return id_; }

  std::size_t bytes() const { return bytes_; }

 private:
  std::weak_ptr<const TextureImage> owner_;
  unsigned int id_ = 0;
  std::size_t bytes_ = 0;
};

// sampler 与图片分离；同一图片的不同寻址方式不产生重复像素存储。
// Samplers are separate: different addressing never duplicates image storage.
class GpuSampler {
 public:
  explicit GpuSampler(const std::shared_ptr<const BaseColorTexture>& texture);

  GpuSampler(const GpuSampler&) = delete;

  GpuSampler& operator=(const GpuSampler&) = delete;

  ~GpuSampler();

  bool Upload(const TextureSampler& sampler, std::string* error);

  bool expired() const { return owner_.expired(); }

  unsigned int id() const { return id_; }

 private:
  std::weak_ptr<const BaseColorTexture> owner_;
  unsigned int id_ = 0;
};
}  // namespace wf
#endif  // WORLDFORGE_SRC_RENDER_GPU_TEXTURE_H_
