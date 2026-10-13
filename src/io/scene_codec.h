// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_IO_SCENE_CODEC_H_
#define WORLDFORGE_SRC_IO_SCENE_CODEC_H_
#include <string>

#include "domain/scene_document.h"

namespace wf {
// 失败不修改输出；只处理值数据，不接触文件、窗口或 GPU。
// Failure preserves output; handles values only, without files, UI or GPU.
bool DecodeScene(const std::string& json, SceneSnapshot* scene,
                 std::string* error);
bool EncodeScene(const SceneSnapshot& scene, std::string* json,
                 std::string* error);
}  // namespace wf
#endif  // WORLDFORGE_SRC_IO_SCENE_CODEC_H_
