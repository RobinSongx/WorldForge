// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "io/scene_codec.h"

#include <locale.h>

#include <charconv>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <utility>
// 固定依赖的根目录公共 C 头。
// Public C header at the pinned dependency's include root.
#include "domain/scene_document.h"
#include "tinygltf_json_c.h"  // NOLINT(build/include_subdir)

namespace wf {
namespace {
using Json = tg3json_value;

// 解析器在 MSVC 使用 strtod；线程局部 C locale 不改变其他线程。
// MSVC parsing uses strtod; a thread-local C locale leaves other threads alone.
class JsonTree {
 public:
  JsonTree() {
    locale_mode_ = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    old_locale_ = setlocale(LC_NUMERIC, nullptr);
    setlocale(LC_NUMERIC, "C");
  }

  JsonTree(const JsonTree&) = delete;

  JsonTree& operator=(const JsonTree&) = delete;

  ~JsonTree() {
    tg3json_value_free(&value_);
    setlocale(LC_NUMERIC, old_locale_.c_str());
    _configthreadlocale(locale_mode_);
  }

  Json* value() { return &value_; }

 private:
  Json value_{};
  std::string old_locale_;
  int locale_mode_ = 0;
};

const Json* Get(const Json* v, const char* key) {
  return v ? tg3json_object_get(v, key) : nullptr;
}

bool Fields(const Json* v, std::initializer_list<const char*> required,
            std::initializer_list<const char*> optional = {}) {
  if (!v || v->type != TG3JSON_OBJECT) return false;
  std::set<std::string> allowed;
  for (const auto* key : required) {
    if (!Get(v, key)) return false;
    allowed.insert(key);
  }
  for (const auto* key : optional) allowed.insert(key);
  std::set<std::string> seen;
  for (std::size_t i = 0; i < v->u.object.count; ++i) {
    const auto& entry = v->u.object.items[i];
    const std::string key(entry.key, entry.key_len);
    if (!allowed.contains(key) || !seen.insert(key).second) return false;
  }
  return true;
}

bool Text(const Json* v, std::string* out) {
  if (!v || v->type != TG3JSON_STRING) return false;
  out->assign(v->u.string.ptr, v->u.string.len);
  return ValidUtf8(*out);
}

bool IsText(const Json* v, const char* expected) {
  std::string text;
  return Text(v, &text) && text == expected;
}

bool Integer(const Json* v, std::int64_t expected) {
  return v && v->type == TG3JSON_INT && v->u.integer == expected;
}

bool Boolean(const Json* v, bool* out) {
  if (!v || v->type != TG3JSON_BOOL) return false;
  *out = v->u.boolean != 0;
  return true;
}

bool Number(const Json* v, float* out) {
  if (!v || (v->type != TG3JSON_INT && v->type != TG3JSON_REAL)) return false;
  const double number =
      v->type == TG3JSON_INT ? static_cast<double>(v->u.integer) : v->u.real;
  if (!std::isfinite(number) ||
      std::abs(number) > std::numeric_limits<float>::max())
    return false;
  *out = static_cast<float>(number);
  return std::isfinite(*out);
}

bool Vector(const Json* v, glm::vec3* out) {
  if (!v || v->type != TG3JSON_ARRAY || v->u.array.count != 3) return false;
  for (int i = 0; i < 3; ++i)
    if (!Number(&v->u.array.items[i], &(*out)[i])) return false;
  return true;
}

bool ReadAsset(const Json* v, AssetSnapshot* a) {
  if (!Fields(v, {"id", "source"}) || !Text(Get(v, "id"), &a->id)) return false;
  const auto* source = Get(v, "source");
  if (IsText(Get(source, "kind"), "builtin")) {
    a->source.builtin = true;
    return Fields(source, {"kind", "name", "version"}) &&
           IsText(Get(source, "name"), "cube") &&
           Integer(Get(source, "version"), 1);
  }
  if (!Fields(source, {"kind", "path", "sha256", "import_profile"}) ||
      !IsText(Get(source, "kind"), "glb") ||
      !IsText(Get(source, "import_profile"), "static-glb-png-v1"))
    return false;
  const auto* path = Get(source, "path");
  return Fields(path, {"base", "value"}) &&
         Text(Get(path, "base"), &a->source.path_base) &&
         Text(Get(path, "value"), &a->source.path) &&
         Text(Get(source, "sha256"), &a->source.sha256);
}

bool ReadEntity(const Json* v, EntitySnapshot* e) {
  if (!Fields(v,
              {"id", "name", "asset_id", "transform", "tint_srgb", "visible"}))
    return false;
  const auto* t = Get(v, "transform");
  return Text(Get(v, "id"), &e->id) && Text(Get(v, "name"), &e->name) &&
         Text(Get(v, "asset_id"), &e->asset_id) &&
         Fields(t, {"position", "rotation_degrees", "scale"}) &&
         Vector(Get(t, "position"), &e->transform.position) &&
         Vector(Get(t, "rotation_degrees"), &e->transform.rotation_degrees) &&
         Vector(Get(t, "scale"), &e->transform.scale) &&
         Vector(Get(v, "tint_srgb"), &e->tint_srgb) &&
         Boolean(Get(v, "visible"), &e->visible);
}

bool ReadView(const Json* v, SceneSnapshot* s) {
  if (!v) return true;
  if (!Fields(v, {"camera", "display"})) return false;
  const auto* c = Get(v, "camera");
  const auto* d = Get(v, "display");
  return Fields(c, {"target", "yaw_radians", "pitch_radians", "distance",
                    "framing_radius"}) &&
         Fields(d, {"grid", "wireframe", "selection_overlay"}) &&
         Vector(Get(c, "target"), &s->camera.target) &&
         Number(Get(c, "yaw_radians"), &s->camera.yaw_radians) &&
         Number(Get(c, "pitch_radians"), &s->camera.pitch_radians) &&
         Number(Get(c, "distance"), &s->camera.distance) &&
         Number(Get(c, "framing_radius"), &s->camera.framing_radius) &&
         Boolean(Get(d, "grid"), &s->display.grid) &&
         Boolean(Get(d, "wireframe"), &s->display.wireframe) &&
         Boolean(Get(d, "selection_overlay"), &s->display.selection_overlay);
}

std::string Quote(const std::string& text) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string out = "\"";
  for (unsigned char c : text) {
    if (c == '"' || c == '\\') {
      out += '\\';
      out += static_cast<char>(c);
    } else if (c < 32) {
      out += "\\u00";
      out += kHex[c >> 4];
      out += kHex[c & 15];
    } else {
      out += static_cast<char>(c);
    }
  }
  return out + '"';
}

void WriteVector(std::ostream& out, const glm::vec3& v) {
  out << '[' << v.x << ", " << v.y << ", " << v.z << ']';
}
}  // namespace

bool DecodeScene(const std::string& json, SceneSnapshot* scene,
                 std::string* error) {
  *error = "Invalid JSON/schema";
  if (json.size() > 16 * 1024 * 1024 || !ValidUtf8(json)) return false;
  JsonTree tree;
  const tg3json_parse_options options{32, 64 * 1024 * 1024, 64 * 1024 * 1024,
                                      4096, 0};
  const char* position = nullptr;
  if (!tg3json_parse_n_opts(json.data(), json.size(), &options, tree.value(),
                            &position))
    return false;
  const auto* root = tree.value();
  SceneSnapshot result;
  std::string revision;
  if (!Fields(root,
              {"format", "format_version", "scene_id", "scene_revision",
               "assets", "entities"},
              {"editor_state"}) ||
      !IsText(Get(root, "format"), "worldforge.scene") ||
      !Integer(Get(root, "format_version"), 1) ||
      !Text(Get(root, "scene_id"), &result.scene_id) ||
      !Text(Get(root, "scene_revision"), &revision) || revision.empty() ||
      revision.find_first_not_of("0123456789") != std::string::npos)
    return false;
  const auto converted = std::from_chars(
      revision.data(), revision.data() + revision.size(), result.revision);
  if (converted.ec != std::errc() ||
      converted.ptr != revision.data() + revision.size())
    return false;
  const auto* assets = Get(root, "assets");
  const auto* entities = Get(root, "entities");
  if (assets->type != TG3JSON_ARRAY || assets->u.array.count > 256 ||
      entities->type != TG3JSON_ARRAY || entities->u.array.count > 10000)
    return false;
  for (std::size_t i = 0; i < assets->u.array.count; ++i) {
    AssetSnapshot a;
    if (!ReadAsset(&assets->u.array.items[i], &a)) {
      *error = "Invalid schema at assets[" + std::to_string(i) + "]";
      return false;
    }
    result.assets.push_back(std::move(a));
  }
  for (std::size_t i = 0; i < entities->u.array.count; ++i) {
    EntitySnapshot e;
    if (!ReadEntity(&entities->u.array.items[i], &e)) {
      *error = "Invalid schema at entities[" + std::to_string(i) + "]";
      return false;
    }
    result.entities.push_back(std::move(e));
  }
  if (!ReadView(Get(root, "editor_state"), &result) ||
      !ValidateSnapshot(result, error))
    return false;
  *scene = std::move(result);
  error->clear();
  return true;
}

bool EncodeScene(const SceneSnapshot& s, std::string* json,
                 std::string* error) {
  if (!ValidateSnapshot(s, error)) return false;
  // classic locale 与 max_digits10 保证浮点往返，避免平台 stringify
  // 的区域差异。 Classic locale and max_digits10 avoid locale-dependent float
  // loss.
  std::ostringstream out;
  out.imbue(std::locale::classic());
  out << std::setprecision(std::numeric_limits<float>::max_digits10)
      << std::boolalpha;
  out << "{\n  \"format\": \"worldforge.scene\",\n  \"format_version\": 1,\n"
      << "  \"scene_id\": " << Quote(s.scene_id)
      << ",\n  \"scene_revision\": " << Quote(std::to_string(s.revision))
      << ",\n  \"assets\": [";
  for (std::size_t i = 0; i < s.assets.size(); ++i) {
    const auto& a = s.assets[i];
    out << (i ? "," : "") << "\n    {\"id\": " << Quote(a.id)
        << ", \"source\": ";
    if (a.source.builtin) {
      out << "{\"kind\": \"builtin\", \"name\": \"cube\", \"version\": 1}";
    } else {
      out << "{\"kind\": \"glb\", \"path\": {\"base\": "
          << Quote(a.source.path_base)
          << ", \"value\": " << Quote(a.source.path)
          << "}, \"sha256\": " << Quote(a.source.sha256)
          << ", \"import_profile\": \"static-glb-png-v1\"}";
    }
    out << '}';
  }
  out << "\n  ],\n  \"entities\": [";
  for (std::size_t i = 0; i < s.entities.size(); ++i) {
    const auto& e = s.entities[i];
    out << (i ? "," : "") << "\n    {\"id\": " << Quote(e.id)
        << ", \"name\": " << Quote(e.name)
        << ", \"asset_id\": " << Quote(e.asset_id)
        << ",\n      \"transform\": {\"position\": ";
    WriteVector(out, e.transform.position);
    out << ", \"rotation_degrees\": ";
    WriteVector(out, e.transform.rotation_degrees);
    out << ", \"scale\": ";
    WriteVector(out, e.transform.scale);
    out << "},\n      \"tint_srgb\": ";
    WriteVector(out, e.tint_srgb);
    out << ", \"visible\": " << e.visible << '}';
  }
  out << "\n  ],\n  \"editor_state\": {\n    \"camera\": {\"target\": ";
  WriteVector(out, s.camera.target);
  out << ", \"yaw_radians\": " << s.camera.yaw_radians
      << ", \"pitch_radians\": " << s.camera.pitch_radians
      << ", \"distance\": " << s.camera.distance
      << ", \"framing_radius\": " << s.camera.framing_radius
      << "},\n    \"display\": {\"grid\": " << s.display.grid
      << ", \"wireframe\": " << s.display.wireframe
      << ", \"selection_overlay\": " << s.display.selection_overlay
      << "}\n  }\n}\n";
  auto text = out.str();
  if (text.size() > 16 * 1024 * 1024) {
    *error = "Scene JSON exceeds 16 MiB";
    return false;
  }
  *json = std::move(text);
  return true;
}
}  // namespace wf
