// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "io/glb_textures.h"

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>

#include "domain/mesh_asset.h"
#include "io/png_decoder.h"

namespace wf {
namespace {
bool ValidIndex(int index, std::uint32_t count) {
  return index >= 0 && static_cast<std::uint32_t>(index) < count;
}

bool Fail(const char* message, std::string* error) {
  *error = message;
  return false;
}

bool ValidWrap(int value) {
  return value == 33071 || value == 33648 || value == 10497;
}

bool ReadSampler(const tg3_model& model, int index, TextureSampler* output,
                 std::string* error) {
  if (index == -1) return true;
  if (!ValidIndex(index, model.samplers_count))
    return Fail("Invalid texture sampler index", error);
  const auto& source = model.samplers[index];
  if (source.ext.extensions_count || !ValidWrap(source.wrap_s) ||
      !ValidWrap(source.wrap_t) ||
      (source.mag_filter != -1 && source.mag_filter != 9728 &&
       source.mag_filter != 9729) ||
      (source.min_filter != -1 && source.min_filter != 9728 &&
       source.min_filter != 9729 &&
       (source.min_filter < 9984 || source.min_filter > 9987)))
    return Fail("Unsupported sampler extension or invalid filtering/wrapping",
                error);
  output->wrap_s = source.wrap_s;
  output->wrap_t = source.wrap_t;
  if (source.min_filter != -1) output->min_filter = source.min_filter;
  if (source.mag_filter != -1) output->mag_filter = source.mag_filter;
  return true;
}
}  // namespace

bool GlbTextures::ReadImage(const tg3_model& model, int index,
                            ImportedScene* scene, std::string* error) {
  if (images_.contains(index)) return true;
  if (!ValidIndex(index, model.images_count))
    return Fail("Invalid texture image index", error);
  const auto& source = model.images[index];
  if (source.uri.len || source.ext.extensions_count || !source.mime_type.data ||
      std::string(source.mime_type.data, source.mime_type.len) != "image/png")
    return Fail("Base-color images must be embedded PNG (JPEG unsupported)",
                error);
  if (!ValidIndex(source.buffer_view, model.buffer_views_count))
    return Fail("Invalid image bufferView", error);
  const auto& view = model.buffer_views[source.buffer_view];
  if (view.byte_stride || !ValidIndex(view.buffer, model.buffers_count))
    return Fail("Invalid image buffer or stride", error);
  const auto& buffer = model.buffers[view.buffer];
  if (!buffer.data.data || buffer.byte_length > buffer.data.count ||
      view.byte_offset > buffer.byte_length ||
      view.byte_length > buffer.byte_length - view.byte_offset ||
      !view.byte_length)
    return Fail("Image bufferView exceeds buffer range", error);
  auto image = std::make_shared<TextureImage>();
  const std::span<const std::uint8_t> bytes(
      buffer.data.data + view.byte_offset,
      static_cast<std::size_t>(view.byte_length));
  if (!DecodePng(bytes, kMaxSceneImageBytes - scene->image_bytes, image.get(),
                 error))
    return false;
  scene->image_bytes += image->rgba.size();
  images_.emplace(index, std::move(image));
  return true;
}

bool GlbTextures::Read(const tg3_model& model, const tg3_texture_info& info,
                       ImportedScene* scene,
                       std::shared_ptr<const BaseColorTexture>* output,
                       std::string* error) {
  if (info.index == -1) return true;
  if (info.tex_coord != 0 || info.ext.extensions_count)
    return Fail("Base-color texture requires TEXCOORD_0 without transforms",
                error);
  if (!ValidIndex(info.index, model.textures_count))
    return Fail("Invalid base-color texture index", error);
  if (textures_.contains(info.index)) {
    *output = textures_.at(info.index);
    return true;
  }
  const auto& source = model.textures[info.index];
  if (source.ext.extensions_count)
    return Fail("Texture extensions/compression are not supported", error);
  auto texture = std::make_shared<BaseColorTexture>();
  texture->id = NextAssetId();
  if (!ReadSampler(model, source.sampler, &texture->sampler, error) ||
      !ReadImage(model, source.source, scene, error))
    return false;
  texture->image = images_.at(source.source);
  textures_.emplace(info.index, texture);
  *output = std::move(texture);
  return true;
}
}  // namespace wf
