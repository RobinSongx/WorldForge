// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_STUDIO_DOCUMENT_CONTROLLER_H_
#define WORLDFORGE_SRC_STUDIO_DOCUMENT_CONTROLLER_H_
#include <cstdint>
#include <memory>
#include <string>

#include "application/import_service.h"
#include "application/scene_io.h"
#include "editor/editor_state.h"
#include "platform/glb_files.h"
#include "render/viewport_renderer.h"
#include "ui/studio_panels.h"

namespace wf {
// 主线程协调器；后台闭包只捕获独立结果，不捕获窗口或 this。
// Main-thread coordinator; worker closures own results, never UI or this.
class DocumentController {
 public:
  explicit DocumentController(GlbFilePicker::DialogProvider provider = {});

  void Update(EditorState& editor, ViewportRenderer& renderer,
              ImportService& worker, PanelState& panels, bool import_dialog);
  void DrawPrompts(EditorState& editor, ImportService& worker);
  bool busy() const;

  bool finished() const { return finished_; }

  const std::string& path() const { return path_; }

 private:
  enum class Action { kNone, kNew, kOpen, kQuit };
  enum class Stage {
    kIdle,
    kUnsaved,
    kChooseOpen,
    kChooseSave,
    kPrepareSave,
    kOverwrite,
    kSaving,
    kOpening,
    kError
  };
  void Continue(EditorState& editor);
  void Save(EditorState& editor, ImportService& worker, bool save_as);
  void PrepareSave(EditorState& editor, ImportService& worker,
                   const std::string& path);
  void StartWrite(EditorState& editor, ImportService& worker);
  void Error(EditorState& editor, const std::string& message);
  Stage stage_ = Stage::kIdle;
  Action pending_ = Action::kNone;
  bool exit_queued_ = false;
  bool finished_ = false;
  std::uint64_t generation_ = 0;
  std::string path_;
  std::string baseline_;
  std::string message_;
  std::shared_ptr<SceneLease> lease_;
  std::shared_ptr<SceneIoResult> result_;
  GlbFilePicker picker_;
};
}  // namespace wf
#endif  // WORLDFORGE_SRC_STUDIO_DOCUMENT_CONTROLLER_H_
