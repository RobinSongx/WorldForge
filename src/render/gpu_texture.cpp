// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "render/gpu_texture.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "glad/gl.h"

#include "domain/mesh_asset.h"

namespace wf {
namespace {
// 一次失败可能设置多个 GL 错误位；排空后旧场景才能继续绘制。
// One failure can set multiple GL errors; drain them so the old scene can draw.
bool CheckUpload(const char* operation, std::string* error) {
  const auto code = glGetError();
  if (code == GL_NO_ERROR) return true;
  while (glGetError() != GL_NO_ERROR) {
  }
  *error = std::string(operation) + ", GL error " + std::to_string(code);
  return false;
}
}  // namespace

GpuImage::GpuImage(const std::shared_ptr<const TextureImage>& image)
    : owner_(image) {}

GpuImage::~GpuImage() { glDeleteTextures(1, &id_); }

bool GpuImage::Upload(const TextureImage& image, std::string* error) {
  int maximum = 0;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
  if (!image.width || !image.height || image.width > 4096 ||
      image.height > 4096 || image.width > static_cast<unsigned int>(maximum) ||
      image.height > static_cast<unsigned int>(maximum) ||
      image.rgba.size() !=
          static_cast<std::uint64_t>(image.width) * image.height * 4) {
    *error = "Invalid texture dimensions/data or GPU texture limit";
    return false;
  }
  // 始终构建完整 mip 链，任意 sampler 可共享。统计为逻辑字节，不含驱动开销。
  // Full mip chains allow all samplers to share; bytes exclude driver overhead.
  int levels = 0;
  auto width = image.width;
  auto height = image.height;
  do {
    bytes_ += static_cast<std::size_t>(width) * height * 4;
    ++levels;
    if (width == 1 && height == 1) break;
    width = std::max(1u, width / 2);
    height = std::max(1u, height / 2);
  } while (true);
  glCreateTextures(GL_TEXTURE_2D, 1, &id_);
  if (!CheckUpload("Texture creation failed", error)) return false;
  glTextureStorage2D(id_, levels, GL_SRGB8_ALPHA8,
                     static_cast<int>(image.width),
                     static_cast<int>(image.height));
  if (!CheckUpload("Texture storage failed", error)) return false;
  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
  glTextureSubImage2D(id_, 0, 0, 0, static_cast<int>(image.width),
                      static_cast<int>(image.height), GL_RGBA, GL_UNSIGNED_BYTE,
                      image.rgba.data());
  glGenerateTextureMipmap(id_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
  return CheckUpload("Texture upload failed", error);
}

GpuSampler::GpuSampler(const std::shared_ptr<const BaseColorTexture>& texture)
    : owner_(texture) {}

GpuSampler::~GpuSampler() { glDeleteSamplers(1, &id_); }

bool GpuSampler::Upload(const TextureSampler& sampler, std::string* error) {
  glCreateSamplers(1, &id_);
  glSamplerParameteri(id_, GL_TEXTURE_WRAP_S, sampler.wrap_s);
  glSamplerParameteri(id_, GL_TEXTURE_WRAP_T, sampler.wrap_t);
  glSamplerParameteri(id_, GL_TEXTURE_MIN_FILTER, sampler.min_filter);
  glSamplerParameteri(id_, GL_TEXTURE_MAG_FILTER, sampler.mag_filter);
  return CheckUpload("Sampler upload failed", error);
}
}  // namespace wf
