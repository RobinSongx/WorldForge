// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_IO_GLB_IMPORTER_H_
#define WORLDFORGE_SRC_IO_GLB_IMPORTER_H_
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

#include "domain/imported_scene.h"

namespace wf {
inline constexpr std::size_t kMaxGlbBytes = 256 * 1024 * 1024;

// 只读取内存；失败不修改 output。第三方类型不越过此接口。
// Reads memory only; leaves output intact on failure. Hides parser types.
bool ImportGlb(std::span<const std::uint8_t> bytes,
               std::shared_ptr<const ImportedScene>* output,
               std::string* error);
}  // namespace wf
#endif  // WORLDFORGE_SRC_IO_GLB_IMPORTER_H_
