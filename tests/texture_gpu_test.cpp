// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include <array>
#include <cmath>
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
#include "domain/render_scene.h"
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

std::shared_ptr<const wf::ImportedScene> Load(const char* name) {
  std::vector<std::uint8_t> bytes;
  std::string error;
  Require(wf::ReadGlbFile(std::string(WF_FIXTURE_DIR) + "/" + name + ".glb",
                          &bytes, &error),
          error);
  std::shared_ptr<const wf::ImportedScene> scene;
  Require(wf::ImportGlb(bytes, &scene, &error), error);
  return scene;
}

std::array<int, 3> Pixel(const wf::ViewportRenderer& renderer, int x, int y) {
  std::array<unsigned char, 4> pixel{};
  glGetTextureSubImage(static_cast<unsigned int>(renderer.texture_token()), 0,
                       x, y, 0, 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 4,
                       pixel.data());
  return {pixel[0], pixel[1], pixel[2]};
}

void ExpectPixel(const wf::ViewportRenderer& renderer, int x, int y,
                 std::array<int, 3> expected, const char* reason) {
  const auto actual = Pixel(renderer, x, y);
  for (int i = 0; i < 3; ++i) {
    Require(std::abs(actual[i] - expected[i]) <= 3,
            std::string(reason) + ": channel " + std::to_string(i) +
                " expected " + std::to_string(expected[i]) + " got " +
                std::to_string(actual[i]));
  }
}

int Encoded(float linear) {
  const float value = linear <= .0031308f
                          ? linear * 12.92f
                          : 1.055f * std::pow(linear, 1.0f / 2.4f) - .055f;
  return static_cast<int>(std::round(value * 255));
}

float Linear(float encoded) {
  return encoded <= .04045f ? encoded / 12.92f
                            : std::pow((encoded + .055f) / 1.055f, 2.4f);
}

void TestPixels() {
  wf::ViewportRenderer renderer;
  std::string error;
  Require(renderer.Initialize(&error), error);
  auto asset = Load("texture_quadrants");
  wf::RenderScene scene;
  scene.grid = false;
  scene.meshes.push_back({asset->meshes[0], glm::mat4(1), glm::vec3(1), true});
  // 法线与固定光源平行，四象限 UV 和色值均有独立的精确期望。
  // Normals face the light, giving independent exact UV/color expectations.
  glEnable(GL_FRAMEBUFFER_SRGB);
  Require(renderer.Render(scene, 256, 256, &error), error);
  Require(!glIsEnabled(GL_FRAMEBUFFER_SRGB), "Double sRGB encoding state");
  ExpectPixel(renderer, 64, 192, {128, 0, 0}, "Top-left / opaque alpha");
  ExpectPixel(renderer, 192, 192, {0, 128, 0}, "Top-right");
  ExpectPixel(renderer, 64, 64, {0, 0, 128}, "Bottom-left");
  ExpectPixel(renderer, 192, 64, {128, 128, 128}, "Bottom-right");
  Require(renderer.draws() == 1 && renderer.cached_images() == 1 &&
              renderer.texture_bytes() == 20,
          "One image including mip level");

  scene.selection_overlay = true;
  Require(renderer.Render(scene, 256, 256, &error), error);
  Require(renderer.draws() == 2, "Selection overlay draw");
  scene.wireframe = true;
  Require(renderer.Render(scene, 256, 256, &error), error);
  Require(renderer.draws() == 1, "Wireframe must not duplicate overlay");
  scene.grid = true;
  Require(renderer.Render(scene, 256, 256, &error), error);
  Require(renderer.draws() == 2, "Grid independent of wireframe");
  scene.grid = scene.wireframe = scene.selection_overlay = false;

  auto mesh = std::make_shared<wf::MeshAsset>(*asset->meshes[0]);
  mesh->id = wf::NextAssetId();
  mesh->primitives[0].base_color = {.5f, .25f, .75f};
  scene.meshes[0].mesh = mesh;
  scene.meshes[0].tint = {.5f, .75f, 1};
  Require(renderer.Render(scene, 256, 256, &error), error);
  const float source = Linear(128.0f / 255);
  ExpectPixel(renderer, 192, 64,
              {Encoded(source * .5f * Linear(.5f)),
               Encoded(source * .25f * Linear(.75f)), Encoded(source * .75f)},
              "Linear factor and sRGB tint");
  // 无纹理材质同样需要线性光照和一次显示编码。
  // Untextured materials also require linear lighting and one output encoding.
  auto plain = std::make_shared<wf::MeshAsset>(*mesh);
  plain->id = wf::NextAssetId();
  plain->primitives[0].base_color_texture.reset();
  scene.meshes[0].mesh = plain;
  scene.meshes[0].tint = glm::vec3(1);
  Require(renderer.Render(scene, 256, 256, &error), error);
  ExpectPixel(renderer, 192, 64, {Encoded(.5f), Encoded(.25f), Encoded(.75f)},
              "Untextured factor");
}

void TestSampling() {
  wf::ViewportRenderer renderer;
  std::string error;
  Require(renderer.Initialize(&error), error);
  auto asset = Load("texture_quadrants");
  for (int wrap : {10497, 33071, 33648}) {
    for (bool vertical : {false, true}) {
      auto mesh = std::make_shared<wf::MeshAsset>(*asset->meshes[0]);
      mesh->id = wf::NextAssetId();
      auto texture = std::make_shared<wf::BaseColorTexture>(
          *mesh->primitives[0].base_color_texture);
      texture->id = wf::NextAssetId();
      texture->sampler.wrap_s = texture->sampler.wrap_t = wrap;
      mesh->primitives[0].base_color_texture = texture;
      for (auto& vertex : mesh->primitives[0].vertices)
        vertex.uv[vertical ? 1 : 0] += 1;
      wf::RenderScene scene;
      scene.grid = false;
      scene.meshes.push_back({mesh});
      Require(renderer.Render(scene, 256, 256, &error), error);
      const std::array<int, 3> expected =
          wrap == 10497 ? std::array<int, 3>{128, 0, 0}
                        : (vertical ? std::array<int, 3>{0, 0, 128}
                                    : std::array<int, 3>{0, 128, 0});
      ExpectPixel(renderer, 64, 192, expected, "Repeat/clamp/mirror U and V");
    }
  }
  auto mesh = std::make_shared<wf::MeshAsset>(*asset->meshes[0]);
  mesh->id = wf::NextAssetId();
  auto texture = std::make_shared<wf::BaseColorTexture>(
      *mesh->primitives[0].base_color_texture);
  texture->id = wf::NextAssetId();
  texture->sampler.mag_filter = 9729;
  mesh->primitives[0].base_color_texture = texture;
  wf::RenderScene scene;
  scene.grid = false;
  scene.meshes.push_back({mesh});
  Require(renderer.Render(scene, 256, 256, &error), error);
  ExpectPixel(renderer, 127, 192, {92, 92, 0},
              "Linear magnification interpolates decoded sRGB");
  for (int filter : {9984, 9985, 9986, 9987}) {
    auto mip_mesh = std::make_shared<wf::MeshAsset>(*asset->meshes[0]);
    mip_mesh->id = wf::NextAssetId();
    auto mip_binding = std::make_shared<wf::BaseColorTexture>(
        *mip_mesh->primitives[0].base_color_texture);
    mip_binding->id = wf::NextAssetId();
    mip_binding->sampler.min_filter = filter;
    mip_mesh->primitives[0].base_color_texture = mip_binding;
    scene.meshes[0].mesh = mip_mesh;
    Require(renderer.Render(scene, 1, 1, &error), error);
    ExpectPixel(renderer, 0, 0, {92, 92, 92}, "All four mipmap min filters");
  }
  const auto mip = Load("texture_mipmap");
  scene.meshes[0].mesh = mip->meshes[0];
  Require(renderer.Render(scene, 256, 256, &error), error);
  ExpectPixel(renderer, 127, 192, {188, 188, 188},
              "Minification averages linear light, not encoded bytes");
}

void TestLifecycle() {
  wf::ViewportRenderer renderer;
  std::string error;
  Require(renderer.Initialize(&error), error);
  wf::EditorState editor;
  editor.RemoveSelected();
  auto asset = Load("texture_shared");
  std::weak_ptr<const wf::TextureImage> image =
      asset->meshes[0]->primitives[0].base_color_texture->image;
  Require(renderer.Prepare(*asset, &error), error);
  const auto first = editor.AddImported("First", asset);
  editor.AddImported("Second", asset);
  asset.reset();
  Require(renderer.cached_images() == 1 && renderer.cached_samplers() == 2,
          "Different samplers must share one GPU image");
  editor.RemoveSelected();
  renderer.CollectUnused();
  Require(renderer.cached_images() == 1 && !image.expired(),
          "Deleting one instance must retain texture");
  editor.Select(first);
  editor.selected()->visible = false;
  Require(renderer.Render(editor.BuildRenderScene(1), 256, 256, &error), error);
  Require(renderer.cached_images() == 1, "Hiding must retain texture");

  // 故意在第二个图片上传处失败，前面暂存的图片和几何也不能发布。
  // Fail the second image upload; earlier staged images/geometry cannot
  // publish.
  auto broken =
      std::make_shared<wf::MeshAsset>(*editor.selected()->asset->meshes[0]);
  broken->id = wf::NextAssetId();
  for (int i = 0; i < 2; ++i) {
    auto binding = std::make_shared<wf::BaseColorTexture>(
        *broken->primitives[i].base_color_texture);
    binding->id = wf::NextAssetId();
    auto pixels = std::make_shared<wf::TextureImage>(*binding->image);
    pixels->id = wf::NextAssetId();
    if (i == 1) pixels->width = 0;
    binding->image = pixels;
    broken->primitives[i].base_color_texture = binding;
  }
  wf::ImportedScene failed;
  auto staged_good =
      std::make_shared<wf::MeshAsset>(*editor.selected()->asset->meshes[0]);
  staged_good->id = wf::NextAssetId();
  failed.meshes = {staged_good, broken};
  staged_good.reset();
  Require(!renderer.Prepare(failed, &error), "Invalid GPU data accepted");
  Require(renderer.cached_images() == 1 && renderer.cached_samplers() == 2 &&
              renderer.cached_meshes() == 1 && editor.entities().size() == 1,
          "Failed staging changed published scene/cache");
  auto invalid_sampler = std::make_shared<wf::BaseColorTexture>(
      *broken->primitives[0].base_color_texture);
  invalid_sampler->id = wf::NextAssetId();
  invalid_sampler->sampler.mag_filter = 9987;
  broken->primitives[0].base_color_texture = invalid_sampler;
  Require(!renderer.Prepare(failed, &error) && renderer.cached_images() == 1 &&
              renderer.cached_meshes() == 1 && glGetError() == GL_NO_ERROR,
          "GL sampler failure must roll back and leave no stale GL errors");
  invalid_sampler.reset();
  editor.selected()->visible = true;
  Require(renderer.Render(editor.BuildRenderScene(1), 256, 256, &error), error);
  Require(renderer.HasRenderedContent(), "Old scene lost after upload failure");
  broken.reset();
  failed.meshes.clear();
  editor.RemoveSelected();
  renderer.CollectUnused();
  Require(image.expired() && renderer.cached_images() == 0 &&
              renderer.cached_samplers() == 0 && renderer.texture_bytes() == 0,
          "Last reference must reclaim image/sampler storage");
  asset = Load("texture_shared");
  editor.AddImported("Again", asset);
  asset.reset();
  Require(renderer.Render(editor.BuildRenderScene(1), 256, 256, &error), error);
  Require(renderer.cached_images() == 1, "Reimport texture");
  editor.NewScene();
  renderer.CollectUnused();
  Require(renderer.cached_images() == 0 && renderer.cached_samplers() == 0,
          "New scene leaked textures");
}
}  // namespace

int main(int, char**) {
  wf::SdlHost host;
  std::string error;
  Require(host.Initialize(true, &error), error);
  TestPixels();
  TestSampling();
  TestLifecycle();
  Require(glGetError() == GL_NO_ERROR, "GL error during texture tests");
  std::cout << "Texture GPU: UV, sRGB, tint, filters, mipmaps, overlays, "
               "sharing, rollback and lifetime passed\n";
  return 0;
}
