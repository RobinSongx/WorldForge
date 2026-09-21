// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_IO_GLB_TEXTURES_H_
#define WORLDFORGE_SRC_IO_GLB_TEXTURES_H_
#include <map>
#include <memory>
#include <string>

// 内部适配器使用上游 C 类型；格式库不会进入领域模型。
// This internal adapter uses upstream C types, never exposed in domain data.
#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "tiny_gltf_v3.h"  // NOLINT(build/include_subdir)

namespace wf {
// 单次导入内按源索引缓存，图片独立于 sampler 共享；失败随导入整体丢弃。
// Caches per import by source index; shares images independently of samplers.
class GlbTextures {
 public:
  bool Read(const tg3_model& model, const tg3_texture_info& info,
            ImportedScene* scene,
            std::shared_ptr<const BaseColorTexture>* output,
            std::string* error);

 private:
  bool ReadImage(const tg3_model& model, int index, ImportedScene* scene,
                 std::string* error);

  std::map<int, std::shared_ptr<const TextureImage>> images_;
  std::map<int, std::shared_ptr<const BaseColorTexture>> textures_;
};
}  // namespace wf
#endif  // WORLDFORGE_SRC_IO_GLB_TEXTURES_H_
