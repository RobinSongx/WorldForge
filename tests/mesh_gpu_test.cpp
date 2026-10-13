// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "glad/gl.h"
#include "glm/glm.hpp"

#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "editor/editor_state.h"
#include "io/glb_importer.h"
#include "platform/glb_files.h"
#include "platform/sdl_host.h"
#include "render/viewport_renderer.h"

namespace {
void Require(bool value, const std::string& message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}
}  // namespace

int main(int argc, char** argv) {
  wf::SdlHost host;
  std::string error;
  Require(host.Initialize(true, &error), error);
  wf::ViewportRenderer renderer;
  Require(renderer.Initialize(&error), error);
  wf::EditorState editor;
  editor.RemoveSelected();
  editor.display_options().grid = false;
  const std::string path =
      argc > 1 ? argv[1]
               : std::string(WF_FIXTURE_DIR) + "/multiple_primitives.glb";
  std::vector<std::uint8_t> bytes;
  Require(wf::ReadGlbFile(path, &bytes, &error), error);
  std::shared_ptr<const wf::ImportedScene> scene;
  Require(wf::ImportGlb(bytes, &scene, &error), error);
  const auto mesh_count = scene->meshes.size();
  const auto upload_bytes = scene->upload_bytes;
  std::weak_ptr<const wf::MeshAsset> observed = scene->meshes[0];
  Require(renderer.Prepare(*scene, &error), error);
  Require(renderer.gpu_bytes() == upload_bytes &&
              renderer.cached_meshes() == mesh_count,
          "Unique mesh cache");
  const auto first = editor.AddImported("First import", scene);
  const auto second = editor.AddImported("Shared import", scene);
  scene.reset();
  Require(renderer.Render(editor.BuildRenderScene(1.5f), 900, 600, &error),
          error);
  Require(renderer.HasRenderedContent(),
          "Imported geometry must produce pixels");
  Require(renderer.cached_meshes() == mesh_count,
          "Instances duplicated GPU data");
  auto transform = editor.selected()->transform;
  transform.rotation_degrees.y = 35;
  transform.scale = {2, 1, .5f};
  transform.position.x = 1;
  Require(editor.SetTransform(second, transform), "Root transform");
  editor.camera().Frame(*editor.selected());
  Require(renderer.Render(editor.BuildRenderScene(1.25f), 800, 640, &error),
          error);
  Require(renderer.width() == 800 && renderer.height() == 640 &&
              renderer.HasRenderedContent(),
          "Resize and transformed mesh");
  editor.RemoveSelected();
  renderer.CollectUnused();
  Require(renderer.cached_meshes() == mesh_count && !observed.expired(),
          "Deleting one instance freed shared geometry");
  editor.Select(first);
  editor.SetAppearance(editor.selected()->id, editor.selected()->color, false);
  Require(renderer.Render(editor.BuildRenderScene(1.25f), 800, 640, &error),
          error);
  Require(!renderer.HasRenderedContent() && renderer.draws() == 0,
          "Hidden import still drawn");
  editor.SetAppearance(editor.selected()->id, editor.selected()->color, true);
  std::shared_ptr<const wf::ImportedScene> invalid;
  Require(!wf::ImportGlb({bytes.data(), 12}, &invalid, &error),
          "Truncated input accepted");
  Require(editor.entities().size() == 1, "Failed parse altered scene");
  Require(renderer.Render(editor.BuildRenderScene(1.25f), 800, 640, &error),
          error);
  Require(renderer.HasRenderedContent(), "Scene unusable after failure");
  editor.RemoveSelected();
  renderer.CollectUnused();
  Require(observed.expired() && renderer.cached_meshes() == 0 &&
              renderer.gpu_bytes() == 0,
          "Last reference did not reclaim CPU/GPU data");
  editor.NewScene();
  Require(renderer.Render(editor.BuildRenderScene(1.25f), 800, 640, &error),
          error);
  Require(renderer.HasRenderedContent(), "Cube generic mesh regression");
  Require(wf::ImportGlb(bytes, &scene, &error), error);
  editor.AddImported("Reimport", scene);
  scene.reset();
  Require(renderer.Render(editor.BuildRenderScene(1.25f), 800, 640, &error),
          error);
  Require(renderer.cached_meshes() == mesh_count + 1,
          "Cube/import coexistence");
  editor.NewScene();
  renderer.CollectUnused();
  Require(renderer.cached_meshes() == 1, "New scene leaked imported geometry");
  Require(glGetError() == GL_NO_ERROR, "GL error during mesh lifecycle");
  std::cout << "Mesh GPU: pixels, instances, editing, resize, visibility, "
               "failure isolation, deletion, reset and reimport passed\n";
  return 0;
}
