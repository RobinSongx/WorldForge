// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "domain/scene_document.h"

#include <cmath>
#include <cstddef>
#include <set>
#include <string>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

namespace wf {
bool ValidUtf8(const std::string& text) {
  for (std::size_t i = 0; i < text.size();) {
    const auto c = static_cast<unsigned char>(text[i++]);
    if (c == 0) return false;
    if (c < 128) continue;
    int count = 0;
    unsigned int value = 0;
    unsigned int minimum = 0;
    if (c >= 0xc2 && c <= 0xdf) {
      count = 1;
      value = c & 31;
      minimum = 128;
    } else if (c >= 0xe0 && c <= 0xef) {
      count = 2;
      value = c & 15;
      minimum = 2048;
    } else if (c >= 0xf0 && c <= 0xf4) {
      count = 3;
      value = c & 7;
      minimum = 65536;
    } else {
      return false;
    }
    for (int j = 0; j < count; ++j) {
      if (i == text.size()) return false;
      const auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80) return false;
      value = (value << 6) | (next & 63);
    }
    if (value < minimum || value > 0x10ffff ||
        (value >= 0xd800 && value <= 0xdfff))
      return false;
  }
  return true;
}

bool ValidUuid(const std::string& text) {
  if (text.size() != 36) return false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (text[i] != '-') return false;
    } else if (!((text[i] >= '0' && text[i] <= '9') ||
                 (text[i] >= 'a' && text[i] <= 'f'))) {
      return false;
    }
  }
  return text != "00000000-0000-0000-0000-000000000000";
}

bool ValidTransform(const Transform& t) {
  for (int i = 0; i < 3; ++i)
    if (!std::isfinite(t.position[i]) ||
        !std::isfinite(t.rotation_degrees[i]) || !std::isfinite(t.scale[i]) ||
        std::abs(t.position[i]) > 1e12f ||
        std::abs(t.rotation_degrees[i]) > 1e6f || t.scale[i] < .01f ||
        t.scale[i] > 100)
      return false;
  return true;
}

bool ValidCamera(const CameraState& c) {
  for (int i = 0; i < 3; ++i)
    if (!std::isfinite(c.target[i]) || std::abs(c.target[i]) > 1e12f)
      return false;
  if (!std::isfinite(c.yaw_radians) || std::abs(c.yaw_radians) > 1e6f ||
      !std::isfinite(c.pitch_radians) || std::abs(c.pitch_radians) > 1.45f ||
      !std::isfinite(c.distance) || c.distance < .0001f || c.distance > 1e15f ||
      !std::isfinite(c.framing_radius) || c.framing_radius < .0001f ||
      c.framing_radius > 1e15f)
    return false;
  const auto eye =
      c.target +
      c.distance *
          glm::vec3(std::cos(c.pitch_radians) * std::sin(c.yaw_radians),
                    std::sin(c.pitch_radians),
                    std::cos(c.pitch_radians) * std::cos(c.yaw_radians));
  const auto view = glm::lookAt(eye, c.target, glm::vec3(0, 1, 0));
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      if (!std::isfinite(view[col][row])) return false;
  return true;
}

bool ValidateSnapshot(const SceneSnapshot& s, std::string* error) {
  auto fail = [error](const std::string& field) {
    *error = "Invalid scene field: " + field;
    return false;
  };
  if (!ValidUuid(s.scene_id)) return fail("scene_id");
  if (s.entities.size() > 10000 || s.assets.size() > 256)
    return fail("document budget");
  std::set<std::string> ids{s.scene_id};
  std::set<std::string> assets;
  std::set<std::string> used;
  for (const auto& a : s.assets) {
    if (!ValidUuid(a.id) || !ids.insert(a.id).second) return fail("assets.id");
    assets.insert(a.id);
    if (!a.source.builtin) {
      if ((a.source.path_base != "scene" && a.source.path_base != "absolute") ||
          a.source.path.empty() || a.source.path.size() > 4096 ||
          !ValidUtf8(a.source.path))
        return fail("assets.source.path");
      if (a.source.sha256.size() != 64 ||
          a.source.sha256.find_first_not_of("0123456789abcdef") !=
              std::string::npos)
        return fail("assets.source.sha256");
    }
  }
  for (std::size_t i = 0; i < s.entities.size(); ++i) {
    const auto& e = s.entities[i];
    const auto field = "entities[" + std::to_string(i) + "]";
    if (!ValidUuid(e.id) || !ids.insert(e.id).second)
      return fail(field + ".id");
    if (!assets.contains(e.asset_id)) return fail(field + ".asset_id");
    used.insert(e.asset_id);
    if (!ValidUtf8(e.name) || e.name.size() > 1024)
      return fail(field + ".name");
    if (!ValidTransform(e.transform)) return fail(field + ".transform");
    for (int j = 0; j < 3; ++j)
      if (!std::isfinite(e.tint_srgb[j]) || e.tint_srgb[j] < 0 ||
          e.tint_srgb[j] > 1)
        return fail(field + ".tint_srgb");
  }
  if (assets != used) return fail("unreferenced asset");
  if (!ValidCamera(s.camera)) return fail("editor_state.camera");
  return true;
}
}  // namespace wf
