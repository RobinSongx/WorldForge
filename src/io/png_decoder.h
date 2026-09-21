// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_IO_PNG_DECODER_H_
#define WORLDFORGE_SRC_IO_PNG_DECODER_H_
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "domain/mesh_asset.h"

namespace wf {
inline constexpr std::size_t kMaxImageBytes = 64 * 1024 * 1024;
inline constexpr std::size_t kMaxSceneImageBytes = 128 * 1024 * 1024;

// 在分配像素前检查尺寸和剩余预算；失败不修改输出。
// Checks dimensions and remaining budget before allocation; failure is atomic.
bool DecodePng(std::span<const std::uint8_t> bytes, std::size_t remaining,
               TextureImage* output, std::string* error);
}  // namespace wf
#endif  // WORLDFORGE_SRC_IO_PNG_DECODER_H_
