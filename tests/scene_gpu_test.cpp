// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "backends/imgui_impl_opengl3.h"
#include "glad/gl.h"
// 测试通过真实 ImGui 控件激活离开保护，不新增产品测试入口。
// Tests activate actual ImGui controls without adding product test hooks.
#include "imgui.h"           // NOLINT(build/include_subdir)
#include "imgui_internal.h"  // NOLINT(build/include_subdir)

#include "application/import_service.h"
#include "application/scene_io.h"
#include "domain/mesh_asset.h"
#include "editor/editor_state.h"
#include "io/glb_importer.h"
#include "platform/glb_files.h"
#include "platform/scene_identity.h"
#include "platform/scene_storage.h"
#include "platform/sdl_host.h"
#include "render/viewport_renderer.h"
#include "studio/document_controller.h"
#include "ui/studio_panels.h"

namespace {
void Require(bool value, const std::string& message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

std::vector<std::uint8_t> Pixels(const wf::ViewportRenderer& renderer) {
  std::vector<std::uint8_t> pixels(256 * 256 * 4);
  glGetTextureImage(static_cast<GLuint>(renderer.texture_token()), 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, static_cast<GLsizei>(pixels.size()),
                    pixels.data());
  return pixels;
}

void RoundTrip(const std::string& model) {
  std::string error;
  wf::EditorState editor;
  std::vector<std::uint8_t> bytes;
  Require(wf::ReadGlbFile(model, &bytes, &error), error);
  std::shared_ptr<const wf::ImportedScene> asset;
  Require(wf::ImportGlb(bytes, &asset, &error), error);
  wf::AssetSource source;
  Require(wf::CanonicalScenePath(model, false, &source.path, &error), error);
  source.sha256 = wf::Sha256(bytes);
  Require(editor.AddImported("纹理模型", asset, source) != 0, "Add asset");
  editor.display_options().grid = false;
  editor.camera().Frame(*editor.selected());
  editor.camera().Orbit(12, 6);
  auto transform = editor.selected()->transform;
  transform.rotation_degrees = {12, 23, 9};
  transform.scale = {.9f, 1.1f, 1};
  editor.SetTransform(editor.selected()->id, transform);
  editor.SetAppearance(editor.selected()->id, {.8f, .9f, 1}, true);
  wf::ViewportRenderer renderer;
  Require(renderer.Initialize(&error), error);
  Require(renderer.Render(editor.BuildRenderScene(1), 256, 256, &error), error);
  const auto before = Pixels(renderer);
  const auto root = std::string(WF_FIXTURE_DIR) + "/gpu-" + wf::GenerateUuid();
  Require(SDL_CreateDirectory(root.c_str()), SDL_GetError());
  auto save = wf::PrepareSceneSave(editor.CaptureSnapshot(),
                                   root + "/scene.wfscene", {}, "");
  Require(save.error.empty(), save.error);
  wf::WriteScene(&save);
  Require(save.outcome == wf::SaveOutcome::kCommitted, save.error);
  auto opened = wf::ReadScene(save.path, save.lease);
  Require(opened.error.empty(), opened.error);
  wf::SceneDocument candidate;
  Require(
      editor.BuildDocument(opened.snapshot, opened.assets, &candidate, &error),
      error);
  wf::ImportedScene batch;
  for (const auto& a : opened.assets)
    batch.meshes.insert(batch.meshes.end(), a.payload->meshes.begin(),
                        a.payload->meshes.end());
  const auto mesh_count = renderer.cached_meshes();
  const auto image_count = renderer.cached_images();
  const auto sampler_count = renderer.cached_samplers();
  // 第一个候选已暂存后，让最后一个资源失败，验证整批回滚。
  // Fail the last resource after earlier staging to verify batch rollback.
  auto bad_mesh = std::make_shared<wf::MeshAsset>(*batch.meshes.back());
  bad_mesh->id = wf::NextAssetId();
  auto bad_texture = std::make_shared<wf::BaseColorTexture>();
  bad_texture->id = wf::NextAssetId();
  bad_mesh->primitives[0].base_color_texture = bad_texture;
  batch.meshes.push_back(bad_mesh);
  const auto old_id = editor.CaptureSnapshot().scene_id;
  Require(!renderer.Prepare(batch, &error) &&
              renderer.cached_meshes() == mesh_count &&
              renderer.cached_images() == image_count &&
              renderer.cached_samplers() == sampler_count &&
              editor.CaptureSnapshot().scene_id == old_id,
          "Candidate failure mutated caches or scene");
  Require(renderer.Render(editor.BuildRenderScene(1), 256, 256, &error) &&
              Pixels(renderer) == before,
          "Old frame damaged by failed open");
  batch.meshes.pop_back();
  Require(renderer.Prepare(batch, &error), error);
  editor.PublishDocument(std::move(candidate), opened.snapshot);
  Require(renderer.Render(editor.BuildRenderScene(1), 256, 256, &error) &&
              Pixels(renderer) == before && !editor.dirty() &&
              !editor.selected(),
          "Save/reopen changed texture, transform, tint or camera pixels");
  std::cout << "GPU scene round-trip pixel equality: " << model << '\n';
}

class Ui {
 public:
  Ui() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = {1440, 900};
    io.DeltaTime = 1.0f / 60;
    wf::ApplyTheme(1);
    Require(ImGui_ImplOpenGL3_Init("#version 450 core"), "UI backend");
  }

  ~Ui() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
  }

  void Frame(wf::EditorState& editor, wf::ViewportRenderer& renderer,
             wf::ImportService& worker, wf::PanelState& panels,
             wf::DocumentController& documents) {
    documents.Update(editor, renderer, worker, panels, false);
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    std::string error;
    Require(wf::DrawStudio(editor, renderer, panels, "test", &error), error);
    documents.DrawPrompts(editor, worker);
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 1440, 900);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }
};

void Click(const char* window, const char* button) {
  auto* w = ImGui::FindWindowByName(window);
  Require(w != nullptr, std::string("Missing window: ") + window);
  ImGui::ActivateItemByID(w->GetID(button));
}

void Prompts() {
  Ui ui;
  wf::EditorState editor;
  wf::ViewportRenderer renderer;
  std::string error;
  Require(renderer.Initialize(&error), error);
  wf::ImportService worker(wf::ReadGlbFile);
  wf::PanelState panels;
  std::string next_choice;
  wf::DocumentController documents([&next_choice](bool, bool) {
    wf::FileChoice choice;
    choice.path = next_choice;
    return choice;
  });
  auto frame = [&] { ui.Frame(editor, renderer, worker, panels, documents); };
  for (int i = 0; i < 4; ++i) frame();
  auto id = editor.CaptureSnapshot().scene_id;
  Click("Scene", "New");
  frame();
  frame();
  Require(documents.busy(), "New must prompt for unsaved scene");
  Click("Unsaved scene", "Cancel");
  frame();
  frame();
  Require(!documents.busy() && editor.CaptureSnapshot().scene_id == id,
          "Cancel mutated document");
  Click("Scene", "New");
  frame();
  frame();
  Click("Unsaved scene", "Discard");
  frame();
  frame();
  Require(editor.CaptureSnapshot().scene_id != id && editor.dirty(),
          "Discard/New did not replace with unsaved scene");
  panels.quit_requested = true;
  frame();
  frame();
  Click("Unsaved scene", "Cancel");
  frame();
  frame();
  Require(!documents.finished(), "Cancelled close terminated session");
  editor.MarkSaved();
  const auto revision = editor.revision();
  editor.camera().Orbit(1, 1);
  frame();
  Require(!editor.dirty() && editor.revision() == revision,
          "Camera should not prompt on exit");
  auto& io = ImGui::GetIO();
  io.AddKeyEvent(ImGuiMod_Ctrl, true);
  io.AddKeyEvent(ImGuiKey_S, true);
  // 只检查快捷键请求；下一帧不消费它，避免触发原生选框。
  // Inspect shortcut request without consuming it into a native picker.
  ImGui_ImplOpenGL3_NewFrame();
  ImGui::NewFrame();
  Require(wf::DrawStudio(editor, renderer, panels, "test", &error), error);
  Require(panels.save_requested, "Ctrl+S did not request save");
  ImGui::Render();
  panels.save_requested = false;
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  io.AddKeyEvent(ImGuiKey_S, false);
  auto drain = [&] {
    for (int i = 0; i < 2000; ++i) {
      frame();
      if (!documents.busy() && !worker.busy()) return;
      SDL_Delay(1);
    }
    Require(false, "Document UI operation timed out");
  };
  const auto root = std::string(WF_FIXTURE_DIR) + "/ui-" + wf::GenerateUuid();
  Require(SDL_CreateDirectory(root.c_str()), SDL_GetError());
  next_choice = root + "/saved.wfscene";
  panels.save_requested = true;
  drain();
  const auto saved_id = editor.CaptureSnapshot().scene_id;
  Require(!editor.dirty() && !documents.path().empty(),
          "Save UI did not establish baseline");
  editor.SetAppearance(editor.entities()[0].id, {.3f, .4f, .5f}, false);
  panels.new_requested = true;
  frame();
  frame();
  Click("Unsaved scene", "Save");
  frame();
  drain();
  Require(editor.CaptureSnapshot().scene_id != saved_id && editor.dirty() &&
              documents.path().empty(),
          "Save then New failed");
  const auto new_id = editor.CaptureSnapshot().scene_id;
  const auto saved_path = next_choice;
  next_choice.clear();
  panels.new_requested = true;
  frame();
  frame();
  Click("Unsaved scene", "Save");
  frame();
  drain();
  Require(editor.CaptureSnapshot().scene_id == new_id && editor.dirty(),
          "Cancelled pending Save must stop New");
  next_choice = saved_path;
  panels.open_requested = true;
  frame();
  frame();
  Click("Unsaved scene", "Discard");
  frame();
  drain();
  Require(editor.CaptureSnapshot().scene_id == saved_id && !editor.dirty() &&
              !editor.entities()[0].visible,
          "Open UI did not restore saved edits");
  next_choice.clear();
  panels.open_requested = true;
  drain();
  Require(editor.CaptureSnapshot().scene_id == saved_id && !editor.dirty(),
          "Cancelled Open mutated world");
  editor.SetAppearance(editor.entities()[0].id, {.4f, .5f, .6f}, true);
  const auto old_path = documents.path();
  panels.save_as_requested = true;
  drain();
  Require(editor.dirty() && documents.path() == old_path,
          "Cancelled Save As changed baseline/path");
  // 保存期间系统关闭：等待提交完成，随后走 clean 退出。
  // System close during Save waits for commit, then exits the clean document.
  panels.save_requested = true;
  frame();
  panels.quit_requested = true;
  drain();
  Require(documents.finished() && !editor.dirty(),
          "Close during save did not wait for successful commit");
}
}  // namespace

int main(int argc, char** argv) {
  wf::SdlHost host;
  std::string error;
  Require(host.Initialize(true, &error), error);
  RoundTrip(argc > 1 ? argv[1]
                     : std::string(WF_FIXTURE_DIR) + "/texture_quadrants.glb");
  Prompts();
  Require(glGetError() == GL_NO_ERROR, "Scene test GL errors");
  std::cout << "Scene GPU rollback and unsaved UI passed\n";
  return 0;
}
