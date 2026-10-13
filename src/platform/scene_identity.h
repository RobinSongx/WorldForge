// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_PLATFORM_SCENE_IDENTITY_H_
#define WORLDFORGE_SRC_PLATFORM_SCENE_IDENTITY_H_
#include <cstdint>
#include <span>
#include <string>

namespace wf {
// 平台失败返回空字符串，由调用者阻止发布；不生成假身份。
// Returns empty on platform failure; callers must not publish fake IDs.
std::string GenerateUuid();
std::string Sha256(std::span<const std::uint8_t> bytes);
}  // namespace wf
#endif  // WORLDFORGE_SRC_PLATFORM_SCENE_IDENTITY_H_
