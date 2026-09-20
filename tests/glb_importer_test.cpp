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
                           "bin_first"}) {
    error.clear();
    Require(!wf::ImportGlb(Read(std::string("reject_") + name), &scene, &error),
            std::string("Must reject: ") + name);
    Require(scene == original && !error.empty(),
            "Failure must preserve output");
  }
  std::cout << "GLB parser: valid fixtures and 31 rejection cases passed\n";
  return 0;
}
