// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "io/glb_importer.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "domain/imported_scene.h"
#include "io/png_decoder.h"

namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

std::vector<std::uint8_t> Read(const std::string& name) {
  std::ifstream file(std::string(WF_FIXTURE_DIR) + "/" + name + ".glb",
                     std::ios::binary);
  Require(file.good(), "Fixture missing: " + name);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}
}  // namespace

int main() {
  std::shared_ptr<const wf::ImportedScene> scene;
  std::string error;
  for (const char* name :
       {"indices_5121", "indices_5123", "indices_5125", "first_scene"}) {
    Require(wf::ImportGlb(Read(name), &scene, &error),
            std::string(name) + ": " + error);
    Require(scene->meshes.size() == 1 && scene->nodes.size() == 3,
            "Shared geometry and hierarchy must be preserved");
    Require(scene->nodes[1].world[3].x == 2 && scene->nodes[1].world[3].y == 1,
            "TRS ancestor chain");
    Require(scene->bounds.min.x == -3 && scene->bounds.max.x == 3 &&
                scene->bounds.max.y == 3,
            "Matrix instance bounds");
    const auto& primitive = scene->meshes[0]->primitives[0];
    Require(primitive.indices[2] == 2 && primitive.vertices[1].uv.x == 1,
            "Index conversion and normalized UV");
  }
  Require(wf::ImportGlb(Read("nonindexed_flat"), &scene, &error), error);
  Require(
      scene->triangles == 1 && scene->vertices == 3 && !scene->warnings.empty(),
      "Generated flat normals");
  Require(wf::ImportGlb(Read("multiple_primitives"), &scene, &error), error);
  Require(scene->meshes[0]->primitives.size() == 2 && scene->triangles == 2,
          "Multiple primitives");
  Require(wf::ImportGlb(Read("texture_shared"), &scene, &error), error);
  const auto& first_texture =
      scene->meshes[0]->primitives[0].base_color_texture;
  const auto& second_texture =
      scene->meshes[0]->primitives[1].base_color_texture;
  Require(first_texture && second_texture &&
              first_texture->image == second_texture->image &&
              first_texture != second_texture && scene->image_bytes == 16,
          "Images must be shared independently of samplers");
  Require(first_texture->image->width == 2 &&
              first_texture->image->rgba[0] == 128 &&
              first_texture->image->rgba[3] == 0 &&
              second_texture->sampler.wrap_s == 33071 &&
              second_texture->sampler.wrap_t == 33648,
          "PNG orientation/alpha and source sampler must survive import");
  Require(wf::ImportGlb(Read("texture_defaults"), &scene, &error), error);
  Require(
      scene->meshes[0]->primitives[0].base_color_texture->sampler.min_filter ==
          9987,
      "Default mipmap sampler");
  for (const char* name : {"texture_rgb", "texture_gray", "texture_palette"}) {
    Require(wf::ImportGlb(Read(name), &scene, &error), error);
    const auto& pixels =
        scene->meshes[0]->primitives[0].base_color_texture->image->rgba;
    Require(pixels.size() == 4 && pixels[0] == 128 && pixels[3] == 255,
            "PNG color modes must convert to RGBA8");
  }
  const auto original = scene;
  for (const char* name : {"cycle",
                           "multiple_parents",
                           "bad_child",
                           "bad_scene",
                           "no_scenes",
                           "empty_scene",
                           "singular",
                           "bad_quaternion",
                           "required_extension",
                           "compressed",
                           "lines",
                           "morph",
                           "animation",
                           "skin",
                           "sparse",
                           "transparent",
                           "external_buffer",
                           "external_image",
                           "bad_offset",
                           "bad_stride",
                           "huge_count",
                           "quantized",
                           "bad_normal_count",
                           "duplicate_root",
                           "index",
                           "nan",
                           "degenerate",
                           "truncated",
                           "duplicate_json",
                           "trailing_bytes",
                           "bin_first",
                           "texture_index",
                           "texture_source",
                           "texture_sampler",
                           "texture_filter",
                           "texture_mag",
                           "texture_wrap",
                           "texture_uv1",
                           "texture_uv_missing",
                           "texture_transform",
                           "texture_extension",
                           "texture_jpeg",
                           "texture_data_uri",
                           "texture_view",
                           "texture_range",
                           "texture_stride",
                           "png_signature",
                           "png_crc",
                           "png_truncated",
                           "png_trailing",
                           "png_dimension",
                           "png_zero",
                           "png_zlib",
                           "png_metadata_crc",
                           "png_inflate_limit"}) {
    error.clear();
    Require(!wf::ImportGlb(Read(std::string("reject_") + name), &scene, &error),
            std::string("Must reject: ") + name);
    Require(scene == original && !error.empty(),
            "Failure must preserve output");
  }
  std::ifstream png_file(std::string(WF_FIXTURE_DIR) + "/quadrants.png",
                         std::ios::binary);
  std::vector<std::uint8_t> png{std::istreambuf_iterator<char>(png_file),
                                std::istreambuf_iterator<char>()};
  wf::TextureImage image;
  Require(wf::DecodePng(png, 16, &image, &error), error);
  const auto image_id = image.id;
  Require(!wf::DecodePng(png, 15, &image, &error) && image.id == image_id,
          "Remaining aggregate image budget must preserve prior output");
  std::cout << "GLB parser: geometry, textures, sharing and 55 rejection cases "
               "passed\\n";
  return 0;
}
