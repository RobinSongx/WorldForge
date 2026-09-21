// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_DOMAIN_MESH_ASSET_H_
#define WORLDFORGE_SRC_DOMAIN_MESH_ASSET_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "glm/glm.hpp"

namespace wf {
struct Bounds {
  glm::vec3 min{0};
  glm::vec3 max{0};
  bool valid = false;
  void Include(const glm::vec3& point);

  void Include(const Bounds& bounds);

  Bounds Transformed(const glm::mat4& matrix) const;
};

struct MeshVertex {
  glm::vec3 position{0};
  glm::vec3 normal{0, 1, 0};
  glm::vec2 uv{0};
};

// 图片保持 PNG 的从上到下行顺序；UV (0,0) 对应第一行，不翻转。
// Images retain PNG top-to-bottom rows; UV (0,0) addresses row zero.
// rgba 为 RGBA8，RGB 使用 sRGB 编码；发布后不可变。
// rgba is RGBA8 with sRGB-encoded RGB; immutable after publication.
struct TextureImage {
  std::uint64_t id = 0;
  unsigned int width = 0;
  unsigned int height = 0;
  std::vector<std::uint8_t> rgba;
};

// 数值沿用 glTF 枚举，不依赖 GL 头；默认线性及三线性 mip 过滤。
// Values follow glTF enums without GL headers; defaults use linear/trilinear.
struct TextureSampler {
  int wrap_s = 10497;
  int wrap_t = 10497;
  int min_filter = 9987;
  int mag_filter = 9729;
};

struct BaseColorTexture {
  std::uint64_t id = 0;
  std::shared_ptr<const TextureImage> image;
  TextureSampler sampler;
};

struct MeshPrimitive {
  std::vector<MeshVertex> vertices;
  std::vector<std::uint32_t> indices;
  glm::vec3 base_color{1};
  std::shared_ptr<const BaseColorTexture> base_color_texture;
  Bounds bounds;
};

// 发布后通过 const shared_ptr 共享；ID 只在当前进程内有效。
// Shared through const shared_ptr after publication; IDs are process-local.
struct MeshAsset {
  std::uint64_t id = 0;
  std::vector<MeshPrimitive> primitives;
  Bounds bounds;
};

std::uint64_t NextAssetId();
}  // namespace wf
#endif  // WORLDFORGE_SRC_DOMAIN_MESH_ASSET_H_
