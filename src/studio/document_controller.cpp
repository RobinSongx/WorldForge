// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "studio/document_controller.h"

#include <memory>
#include <string>
#include <utility>
// 上游根目录公共头文件。
// Upstream public header at the include root.
#include "imgui.h"  // NOLINT(build/include_subdir)

#include "application/scene_io.h"
#include "domain/imported_scene.h"
#include "platform/scene_storage.h"

namespace wf {
DocumentController::DocumentController(GlbFilePicker::DialogProvider provider)
    : picker_(std::move(provider)) {}

bool DocumentController::busy() const { return stage_ != Stage::kIdle; }

void DocumentController::Error(EditorState& editor,
                               const std::string& message) {
  message_ = message;
  editor.Log("ERROR", message);
  pending_ = Action::kNone;
  exit_queued_ = false;
  stage_ = Stage::kError;
}

void DocumentController::Continue(EditorState& editor) {
  const auto action = pending_;
  pending_ = Action::kNone;
  stage_ = Stage::kIdle;
  if (action == Action::kNew) {
    const auto generation = editor.generation();
    editor.NewScene();
    if (generation == editor.generation()) {
      Error(editor, "Could not create scene identity");
      return;
    }
    lease_.reset();
    path_.clear();
    baseline_.clear();
  } else if (action == Action::kQuit) {
    finished_ = true;
  } else if (action == Action::kOpen) {
    generation_ = editor.generation();
    picker_.Open(generation_, true);
    stage_ = Stage::kChooseOpen;
  }
}

void DocumentController::PrepareSave(EditorState& editor, ImportService& worker,
                                     const std::string& path) {
  generation_ = editor.generation();
  result_ = std::make_shared<SceneIoResult>();
  const auto result = result_;
  const auto lease = lease_;
  const auto baseline = baseline_;
  auto snapshot = editor.CaptureSnapshot();
  if (!worker.StartTask(
          [result, lease, baseline, path, snapshot = std::move(snapshot)] {
            *result = PrepareSceneSave(snapshot, path, lease, baseline);
          })) {
    Error(editor, "I/O worker is busy");
    return;
  }
  stage_ = Stage::kPrepareSave;
}

void DocumentController::Save(EditorState& editor, ImportService& worker,
                              bool save_as) {
  if (save_as || path_.empty()) {
    generation_ = editor.generation();
    picker_.Open(generation_, true, true);
    stage_ = Stage::kChooseSave;
  } else {
    PrepareSave(editor, worker, path_);
  }
}

void DocumentController::StartWrite(EditorState& editor,
                                    ImportService& worker) {
  const auto result = result_;
  if (!worker.StartTask([result] { WriteScene(result.get()); })) {
    Error(editor, "I/O worker is busy");
    return;
  }
  stage_ = Stage::kSaving;
}

void DocumentController::Update(EditorState& editor, ViewportRenderer& renderer,
                                ImportService& worker, PanelState& panels,
                                bool import_dialog) {
  if (panels.quit_requested) {
    exit_queued_ = true;
    panels.quit_requested = false;
  }
  if (auto choice = picker_.TakeChoice()) {
    const auto stage = stage_;
    if (!choice->error.empty()) {
      Error(editor, choice->error);
    } else if (choice->path.empty() ||
               choice->generation != editor.generation()) {
      pending_ = Action::kNone;
      stage_ = Stage::kIdle;
    } else if (stage == Stage::kChooseSave) {
      auto path = choice->path;
      if (path.size() < 8 ||
          !SamePath(path.substr(path.size() - 8), ".wfscene"))
        path += ".wfscene";
      PrepareSave(editor, worker, path);
    } else if (stage == Stage::kChooseOpen) {
      result_ = std::make_shared<SceneIoResult>();
      const auto result = result_;
      const auto lease = lease_;
      const auto path = choice->path;
      if (worker.StartTask(
              [result, lease, path] { *result = ReadScene(path, lease); }))
        stage_ = Stage::kOpening;
      else
        Error(editor, "I/O worker is busy");
    }
  }
  if (worker.TakeTaskDone()) {
    if (generation_ != editor.generation()) {
      Error(editor, "Discarded stale document operation");
    } else if (!result_->error.empty()) {
      if (result_->outcome == SaveOutcome::kRecoveryRequired)
        editor.MarkUnsaved();
      Error(editor, result_->error);
    } else if (stage_ == Stage::kPrepareSave) {
      if (result_->overwrite)
        stage_ = Stage::kOverwrite;
      else
        StartWrite(editor, worker);
    } else if (stage_ == Stage::kSaving) {
      if (result_->outcome != SaveOutcome::kCommitted) {
        Error(editor, "Save did not commit");
      } else {
        path_ = result_->path;
        baseline_ = result_->baseline;
        lease_ = result_->lease;
        editor.MarkSaved();
        editor.Log("INFO", "Saved " + path_);
        panels.import_status = "Saved " + path_;
        if (result_->absolute_references)
          editor.Log("WARN",
                     "Scene contains absolute external asset references");
        result_.reset();
        Continue(editor);
      }
    } else if (stage_ == Stage::kOpening) {
      SceneDocument candidate;
      std::string error;
      ImportedScene aggregate;
      for (const auto& a : result_->assets)
        aggregate.meshes.insert(aggregate.meshes.end(),
                                a.payload->meshes.begin(),
                                a.payload->meshes.end());
      if (!editor.BuildDocument(result_->snapshot, result_->assets, &candidate,
                                &error) ||
          !renderer.Prepare(aggregate, &error)) {
        Error(editor, error);
      } else {
        editor.PublishDocument(std::move(candidate), result_->snapshot);
        path_ = result_->path;
        baseline_ = result_->baseline;
        lease_ = result_->lease;
        editor.Log("INFO", "Opened " + path_);
        panels.import_status = "Opened " + path_;
        for (const auto& a : result_->assets)
          for (const auto& warning : a.payload->warnings)
            editor.Log("WARN", warning);
        result_.reset();
        stage_ = Stage::kIdle;
      }
      renderer.CollectUnused();
    }
  }
  if (!busy() && !worker.busy() && !import_dialog) {
    Action action = Action::kNone;
    if (exit_queued_) {
      action = Action::kQuit;
      exit_queued_ = false;
    } else if (panels.new_requested) {
      action = Action::kNew;
    } else if (panels.open_requested) {
      action = Action::kOpen;
    }
    if (action != Action::kNone) {
      pending_ = action;
      if (editor.dirty())
        stage_ = Stage::kUnsaved;
      else
        Continue(editor);
    } else if (panels.save_requested || panels.save_as_requested) {
      Save(editor, worker, panels.save_as_requested);
    }
  }
  panels.new_requested = panels.open_requested = false;
  panels.save_requested = panels.save_as_requested = false;
  panels.document_busy = busy() || worker.busy() || import_dialog;
  panels.document_name =
      path_.empty() ? "Untitled" : path_.substr(path_.find_last_of("/\\") + 1);
  if (editor.dirty()) panels.document_name += " *";
  if (stage_ == Stage::kError) panels.import_status = message_;
  if (stage_ == Stage::kOpening) panels.import_status = "Opening scene...";
  if (stage_ == Stage::kPrepareSave)
    panels.import_status = "Validating scene...";
  if (stage_ == Stage::kSaving) panels.import_status = "Saving scene...";
}

void DocumentController::DrawPrompts(EditorState& editor,
                                     ImportService& worker) {
  if (stage_ == Stage::kUnsaved) {
    ImGui::OpenPopup("Unsaved scene");
    if (ImGui::BeginPopupModal("Unsaved scene", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextUnformatted("Save changes before continuing?");
      if (ImGui::Button("Save")) {
        ImGui::CloseCurrentPopup();
        Save(editor, worker, false);
      }
      ImGui::SameLine();
      if (ImGui::Button("Discard")) {
        ImGui::CloseCurrentPopup();
        Continue(editor);
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        pending_ = Action::kNone;
        exit_queued_ = false;
        stage_ = Stage::kIdle;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  } else if (stage_ == Stage::kOverwrite) {
    ImGui::OpenPopup("Overwrite scene");
    if (ImGui::BeginPopupModal("Overwrite scene", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextWrapped("Replace existing scene?\n%s", result_->path.c_str());
      if (ImGui::Button("Replace")) {
        ImGui::CloseCurrentPopup();
        StartWrite(editor, worker);
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        result_.reset();
        pending_ = Action::kNone;
        stage_ = Stage::kIdle;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  } else if (stage_ == Stage::kError) {
    ImGui::OpenPopup("Document operation failed");
    if (ImGui::BeginPopupModal("Document operation failed", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::TextWrapped("%s", message_.c_str());
      if (ImGui::Button("OK")) {
        result_.reset();
        stage_ = Stage::kIdle;
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
}
}  // namespace wf
