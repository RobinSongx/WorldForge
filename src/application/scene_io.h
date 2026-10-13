// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_APPLICATION_SCENE_IO_H_
#define WORLDFORGE_SRC_APPLICATION_SCENE_IO_H_
#include <memory>
#include <string>
#include <vector>

#include "domain/scene_document.h"
#include "platform/scene_storage.h"

namespace wf {
struct SceneIoResult {
  SceneSnapshot snapshot;
  std::vector<AssetRecord> assets;
  std::shared_ptr<SceneLease> lease;
  std::string path;
  std::string baseline;
  std::string json;
  std::string error;
  bool overwrite = false;
  bool absolute_references = false;
  SaveOutcome outcome = SaveOutcome::kNotCommitted;
};

// worker 输出独立候选；主线程完成 GPU 准备后才能发布。
// Worker produces isolated candidates; publish only after main-thread GPU prep.
SceneIoResult ReadScene(const std::string& path,
                        const std::shared_ptr<SceneLease>& current);
SceneIoResult PrepareSceneSave(SceneSnapshot snapshot, const std::string& path,
                               const std::shared_ptr<SceneLease>& current,
                               const std::string& baseline);
void WriteScene(SceneIoResult* result, SaveFault fault = SaveFault::kNone);
}  // namespace wf
#endif  // WORLDFORGE_SRC_APPLICATION_SCENE_IO_H_
