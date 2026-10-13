// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include <locale.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "application/import_service.h"
#include "application/scene_io.h"
#include "editor/editor_state.h"
#include "io/glb_importer.h"
#include "io/scene_codec.h"
#include "platform/scene_identity.h"
#include "platform/scene_storage.h"

namespace {
void Require(bool value, const std::string& message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

void Put(const std::string& path, const std::string& text) {
  Require(SDL_SaveFile(path.c_str(), text.data(), text.size()), SDL_GetError());
}

std::string Digest(const std::string& path) {
  std::string hash, error;
  Require(wf::FileDigest(path, false, &hash, &error), error);
  return hash;
}

std::string Read(const std::string& path) {
  std::vector<std::uint8_t> bytes;
  std::string error;
  Require(wf::ReadBoundedFile(path, 16 * 1024 * 1024, &bytes, &error), error);
  return {bytes.begin(), bytes.end()};
}

std::string Replace(std::string text, const std::string& old,
                    const std::string& replacement) {
  const auto at = text.find(old);
  Require(at != std::string::npos, "Test replacement missing");
  text.replace(at, old.size(), replacement);
  return text;
}

void Codec() {
  wf::EditorState editor;
  auto s = editor.CaptureSnapshot();
  std::string json, error;
  s.entities[0].name = "中文 \"quoted\" \n \t";
  s.entities[0].transform.position = {.123456789f, -123456.789f, 0};
  s.revision = UINT64_MAX;
  Require(wf::EncodeScene(s, &json, &error), error);
  wf::SceneSnapshot result;
  Require(wf::DecodeScene(json, &result, &error), error);
  Require(result.revision == UINT64_MAX &&
              result.entities[0].name == s.entities[0].name &&
              result.entities[0].transform.position ==
                  s.entities[0].transform.position,
          "Lossy JSON round trip");
  std::string again;
  Require(wf::EncodeScene(result, &again, &error) && json == again,
          "Unstable JSON output");
  const std::string locale = setlocale(LC_NUMERIC, nullptr);
  Require(setlocale(LC_NUMERIC, "German_Germany.1252") != nullptr,
          "Locale test unavailable");
  Require(wf::EncodeScene(s, &again, &error) && json == again &&
              wf::DecodeScene(json, &result, &error),
          "Locale changed codec");
  setlocale(LC_NUMERIC, locale.c_str());
  std::vector<std::string> bad{
      json + "garbage",
      "\xef\xbb\xbf" + json,
      Replace(json, "\"format_version\": 1", "\"format_version\": 2"),
      Replace(json, "\"format_version\": 1", "\"format_version\": true"),
      Replace(json, "\"format_version\": 1",
              "\"format_version\": 1, \"format_version\": 1"),
      Replace(json, "\"format_version\": 1",
              "\"format_version\": 1, \"extra\": 0"),
      Replace(json, "\"visible\": true", "\"visible\": 1"),
      Replace(json, "\"scale\": [1, 1, 1]", "\"scale\": [0, 1, 1]"),
      Replace(json, "\"scale\": [1, 1, 1]", "\"scale\": [1e999, 1, 1]"),
      Replace(json, "\"grid\": true", "\"grid\": null"),
      Replace(json, "\"distance\": 7", "\"distance\": 0"),
      Replace(json, "\"scene_revision\": \"18446744073709551615\"",
              "\"scene_revision\": \"18446744073709551616\""),
      Replace(json, "\"builtin\"", "\"unknown\""),
      Replace(json, "\"cube\"", "\"cube\\u0000\""),
      Replace(json, "\"cube\"", "\"\\ud800\""),
      Replace(json, "\"cube\"", std::string("\"") + '\xff' + "\"")};
  for (const auto& text : bad) {
    const auto old = result.scene_id;
    Require(!wf::DecodeScene(text, &result, &error) && result.scene_id == old,
            "Malformed scene accepted or output mutated");
  }
  auto invalid = s;
  invalid.entities[0].id = invalid.scene_id;
  Require(!wf::ValidateSnapshot(invalid, &error), "Global ID collision");
  invalid = s;
  invalid.entities[0].asset_id = wf::GenerateUuid();
  Require(!wf::ValidateSnapshot(invalid, &error), "Dangling reference");
  invalid = s;
  invalid.entities.clear();
  Require(!wf::ValidateSnapshot(invalid, &error), "Unused asset");
  invalid.assets.clear();
  Require(wf::EncodeScene(invalid, &json, &error) &&
              wf::DecodeScene(json, &result, &error) && result.entities.empty(),
          "Empty scene round trip");
  auto viewless = json.substr(0, json.find(",\n  \"editor_state\"")) + "\n}\n";
  Require(
      wf::DecodeScene(viewless, &result, &error) && result.camera.distance == 7,
      "Optional editor state defaults");
  Require(
      !wf::DecodeScene(std::string(16 * 1024 * 1024 + 1, ' '), &result, &error),
      "JSON byte budget");
  invalid.entities.resize(10001);
  Require(!wf::ValidateSnapshot(invalid, &error), "Entity budget");
}

void Dirty() {
  wf::EditorState editor;
  Require(editor.dirty(), "New scene must be unsaved");
  editor.MarkSaved();
  const auto before = editor.revision();
  const auto id = editor.selected()->id;
  editor.SetTransform(id, editor.selected()->transform);
  editor.SetAppearance(id, editor.selected()->color, true);
  editor.camera().Orbit(10, 4);
  editor.display_options().grid = false;
  Require(!editor.dirty() && editor.revision() == before, "Spurious dirty");
  editor.SetAppearance(id, {.1f, .2f, .3f}, false);
  Require(editor.dirty() && editor.revision() == before + 1,
          "Appearance dirty");
  editor.MarkSaved();
  editor.RemoveSelected();
  Require(editor.dirty(), "Deletion dirty");
}

int ProbeLock(const std::string& path) {
  const auto exe = std::string(SDL_GetBasePath()) + "wf_scene_tests.exe";
  const char* args[] = {exe.c_str(), "--probe-lock", path.c_str(), nullptr};
  const std::unique_ptr<SDL_Process, decltype(&SDL_DestroyProcess)> process(
      SDL_CreateProcess(args, false), SDL_DestroyProcess);
  Require(process != nullptr, "Cannot start second process");
  int code = -1;
  Require(SDL_WaitProcess(process.get(), true, &code),
          "Cannot wait for process");
  return code;
}

void Files(const std::string& root) {
  wf::EditorState editor;
  std::string error;
  std::vector<std::uint8_t> bytes;
  Require(wf::ReadBoundedFile(
              std::string(WF_FIXTURE_DIR) + "/texture_quadrants.glb",
              1024 * 1024, &bytes, &error),
          error);
  const auto asset_path = root + "/模型 空格.glb";
  Require(SDL_SaveFile(asset_path.c_str(), bytes.data(), bytes.size()),
          SDL_GetError());
  std::shared_ptr<const wf::ImportedScene> asset;
  Require(wf::ImportGlb(bytes, &asset, &error), error);
  wf::AssetSource source;
  source.path = asset_path;
  source.sha256 = wf::Sha256(bytes);
  Require(editor.AddImported("纹理", asset, source) != 0, "Import publish");
  auto snapshot = editor.CaptureSnapshot();
  auto duplicate = snapshot.entities.back();
  duplicate.id = wf::GenerateUuid();
  duplicate.visible = false;
  snapshot.entities.push_back(duplicate);
  const auto path = root + "/world.wfscene";
  auto save = wf::PrepareSceneSave(snapshot, path, {}, "");
  Require(save.error.empty(), save.error);
  Require(save.snapshot.assets.back().source.path_base == "scene",
          "Relative asset path expected");
  wf::WriteScene(&save);
  Require(save.outcome == wf::SaveOutcome::kCommitted, save.error);
  Require(!wf::SceneLease::Acquire(path, &error), "Second lease accepted");
  auto opened = wf::ReadScene(path, save.lease);
  Require(opened.error.empty(), opened.error);
  wf::SceneDocument candidate;
  Require(
      editor.BuildDocument(opened.snapshot, opened.assets, &candidate, &error),
      error);
  Require(candidate.entities[1].asset == candidate.entities[2].asset &&
              candidate.entities[1].persistent_id == snapshot.entities[1].id,
          "Shared identity/payload lost");
  const auto generation = editor.generation();
  editor.PublishDocument(std::move(candidate), opened.snapshot);
  Require(!editor.dirty() && !editor.selected() &&
              editor.generation() == generation + 1,
          "Open baseline/selection");
  Require(editor.AddCube() != 0 &&
              editor.entities().back().asset == editor.entities().front().asset,
          "Opened builtin must remain shared");
  editor.Select(editor.entities().back().id);
  editor.RemoveSelected();
  const auto probe = root + "/probe.wfscene";
  auto lock = wf::SceneLease::Acquire(probe, &error);
  Require(lock && ProbeLock(probe) == 7, "Cross-process exclusion failed");
  lock.reset();
  Require(ProbeLock(probe) == 0, "Stale lock file blocked reacquisition");
  const auto old_hash = Digest(path);
  for (auto fault : {wf::SaveFault::kCreate, wf::SaveFault::kWrite,
                     wf::SaveFault::kFlush, wf::SaveFault::kReplace}) {
    auto failed = wf::PrepareSceneSave(editor.CaptureSnapshot(), path,
                                       save.lease, old_hash);
    wf::WriteScene(&failed, fault);
    Require(failed.outcome == wf::SaveOutcome::kNotCommitted &&
                Digest(path) == old_hash && editor.dirty(),
            "Failed save damaged original/dirty");
  }
  const auto sub = root + "/other";
  Require(SDL_CreateDirectory(sub.c_str()), SDL_GetError());
  auto other = wf::PrepareSceneSave(
      editor.CaptureSnapshot(), sub + "/另存.wfscene", save.lease, old_hash);
  Require(other.error.empty() &&
              other.snapshot.assets.back().source.path == "../模型 空格.glb",
          "Save As did not rebase");
  wf::WriteScene(&other);
  Require(other.outcome == wf::SaveOutcome::kCommitted, other.error);
  auto reopened = wf::ReadScene(other.path, other.lease);
  Require(
      reopened.error.empty() && reopened.snapshot.scene_id == snapshot.scene_id,
      "Save As logical identity");
  Require(wf::RelativeAssetPath("C:/a/world.wfscene", "D:/model.glb") ==
              "D:/model.glb",
          "Cross-drive fallback");
  std::string resolved;
  Require(!wf::ResolveAssetPath(path, "scene", "C:evil", &resolved, &error) &&
              !wf::CanonicalScenePath("//server/share/file", false, &resolved,
                                      &error),
          "Invalid path accepted");
  // 覆盖确认之后目标被外部修改，提交必须拒绝。
  // External edits after overwrite confirmation must reject commit.
  auto overwrite = wf::PrepareSceneSave(editor.CaptureSnapshot(), path,
                                        save.lease, old_hash);
  Put(path, Read(path) + " ");
  wf::WriteScene(&overwrite);
  Require(overwrite.outcome == wf::SaveOutcome::kNotCommitted &&
              Digest(path) != old_hash,
          "External scene edit overwritten");
  // 资产改变/缺失不清空当前场景。
  // Changed/missing assets must leave the active scene untouched.
  Put(asset_path, "changed");
  const auto count = editor.entities().size();
  auto broken = wf::ReadScene(other.path, other.lease);
  Require(!broken.error.empty() && editor.entities().size() == count &&
              editor.dirty(),
          "Changed source not isolated");
  Require(SDL_RemovePath(asset_path.c_str()), SDL_GetError());
  broken = wf::ReadScene(other.path, other.lease);
  Require(!broken.error.empty(), "Missing source accepted");
  Require(SDL_SaveFile(asset_path.c_str(), bytes.data(), bytes.size()),
          SDL_GetError());
  auto recovery = wf::PrepareSceneSave(editor.CaptureSnapshot(), other.path,
                                       other.lease, Digest(other.path));
  wf::WriteScene(&recovery, wf::SaveFault::kVerify);
  Require(recovery.outcome == wf::SaveOutcome::kRecoveryRequired &&
              !recovery.error.empty(),
          "Uncertain commit called success");
  // 单 worker 的发布信号也是结果数据的线程同步边界。
  // Worker completion publishes its result through the synchronization gate.
  auto async = std::make_shared<wf::SceneIoResult>();
  wf::ImportService service([](const auto&, auto*, auto*) { return false; });
  Require(service.StartTask([async, path = other.path, lease = other.lease] {
    *async = wf::ReadScene(path, lease);
  }),
          "Start scene worker");
  Require(!service.Start("ignored", 1), "Concurrent import allowed");
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (!service.TakeTaskDone()) {
    Require(std::chrono::steady_clock::now() < deadline, "Worker timeout");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(async->error.empty() && !service.busy(), "Worker publication");
}
}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string(argv[1]) == "--probe-lock") {
    std::string error;
    return wf::SceneLease::Acquire(argv[2], &error) ? 0 : 7;
  }
  Codec();
  Dirty();
  const auto root =
      std::string(WF_FIXTURE_DIR) + "/scene-" + wf::GenerateUuid();
  Require(SDL_CreateDirectory(root.c_str()), SDL_GetError());
  Files(root);
  // 所有租约释放后移动整个文件夹，确保旧路径消失后仍能重开。
  // Move the whole folder after leases release; reopen without the old paths.
  const auto moved = root + "-moved";
  Require(SDL_RenamePath(root.c_str(), moved.c_str()), SDL_GetError());
  auto relocated = wf::ReadScene(moved + "/other/另存.wfscene", {});
  Require(relocated.error.empty(), relocated.error);
  std::cout
      << "Scene codec, dirty, identity, storage, failures and worker passed\n";
  return 0;
}
