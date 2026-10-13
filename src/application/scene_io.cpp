// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "application/scene_io.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "domain/imported_scene.h"
#include "io/glb_importer.h"
#include "io/scene_codec.h"
#include "platform/scene_identity.h"
#include "platform/scene_storage.h"

namespace wf {
namespace {
bool Acquire(const std::string& path, bool missing,
             const std::shared_ptr<SceneLease>& current, SceneIoResult* r) {
  if (!CanonicalScenePath(path, missing, &r->path, &r->error)) return false;
  if (r->path.size() < 8 ||
      !SamePath(r->path.substr(r->path.size() - 8), ".wfscene")) {
    r->error = "Choose a .wfscene file";
    return false;
  }
  r->lease = current && SamePath(current->path(), r->path)
                 ? current
                 : SceneLease::Acquire(r->path, &r->error);
  return r->lease != nullptr;
}

bool LoadAsset(const std::string& scene_path, AssetSnapshot* a,
               std::shared_ptr<const ImportedScene>* payload,
               std::string* error) {
  if (a->source.builtin) {
    *payload = MakeCubeScene();
    return true;
  }
  std::string resolved;
  if (!ResolveAssetPath(scene_path, a->source.path_base, a->source.path,
                        &resolved, error))
    return false;
  std::vector<std::uint8_t> bytes;
  if (!ReadBoundedFile(resolved, kMaxGlbBytes, &bytes, error)) return false;
  if (Sha256(bytes) != a->source.sha256) {
    *error = "SourceChanged: " + resolved;
    return false;
  }
  if (!ImportGlb(bytes, payload, error)) {
    *error = resolved + ": " + *error;
    return false;
  }
  a->source.path = std::move(resolved);
  a->source.path_base = "absolute";
  return true;
}
}  // namespace

SceneIoResult ReadScene(const std::string& path,
                        const std::shared_ptr<SceneLease>& current) {
  SceneIoResult r;
  if (!Acquire(path, false, current, &r)) return r;
  std::vector<std::uint8_t> bytes;
  if (!ReadBoundedFile(r.path, 16 * 1024 * 1024, &bytes, &r.error)) return r;
  r.baseline = Sha256(bytes);
  if (r.baseline.empty()) {
    r.error = "SHA256 failed";
    return r;
  }
  const std::string json(bytes.begin(), bytes.end());
  if (!DecodeScene(json, &r.snapshot, &r.error)) return r;
  std::size_t geometry = 0;
  std::size_t images = 0;
  for (auto& a : r.snapshot.assets) {
    std::shared_ptr<const ImportedScene> payload;
    if (!LoadAsset(r.path, &a, &payload, &r.error)) return r;
    geometry += payload->upload_bytes;
    images += payload->image_bytes;
    if (geometry > 256 * 1024 * 1024 || images > 128 * 1024 * 1024) {
      r.error = "Candidate scene geometry/image budget exceeded";
      return r;
    }
    r.assets.push_back({a.id, a.source, std::move(payload)});
  }
  return r;
}

SceneIoResult PrepareSceneSave(SceneSnapshot snapshot, const std::string& path,
                               const std::shared_ptr<SceneLease>& current,
                               const std::string& baseline) {
  SceneIoResult r;
  r.snapshot = std::move(snapshot);
  if (!Acquire(path, true, current, &r)) return r;
  if (!FileDigest(r.path, true, &r.baseline, &r.error)) return r;
  const bool same = current && SamePath(current->path(), r.path);
  if (same && baseline != r.baseline) {
    r.error = "Conflict: scene changed outside Studio";
    return r;
  }
  r.overwrite = !same && !r.baseline.empty();
  for (auto& a : r.snapshot.assets) {
    if (a.source.builtin) continue;
    std::string canonical;
    if (a.source.path_base != "absolute" ||
        !CanonicalScenePath(a.source.path, false, &canonical, &r.error)) {
      if (r.error.empty()) r.error = "Live asset must have absolute provenance";
      return r;
    }
    if (SameFile(canonical, r.path)) {
      r.error = "Refusing to overwrite a source asset";
      return r;
    }
    std::string digest;
    if (!FileDigest(canonical, false, &digest, &r.error)) return r;
    if (digest != a.source.sha256) {
      r.error = "SourceChanged: " + canonical;
      return r;
    }
    a.source.path = RelativeAssetPath(r.path, canonical);
    a.source.path_base =
        a.source.path.find(':') == std::string::npos ? "scene" : "absolute";
    r.absolute_references |= a.source.path_base == "absolute";
  }
  EncodeScene(r.snapshot, &r.json, &r.error);
  return r;
}

void WriteScene(SceneIoResult* r, SaveFault fault) {
  if (!r->error.empty() || !r->lease) return;
  // 确认覆盖期间来源可能改变，提交前再次检查资产版本。
  // Sources may change during overwrite confirmation; recheck before commit.
  for (const auto& a : r->snapshot.assets) {
    if (a.source.builtin) continue;
    std::string path;
    std::string digest;
    if (!ResolveAssetPath(r->path, a.source.path_base, a.source.path, &path,
                          &r->error) ||
        !FileDigest(path, false, &digest, &r->error))
      return;
    if (digest != a.source.sha256) {
      r->error = "SourceChanged: " + path;
      return;
    }
  }
  r->outcome = CommitSceneFile(r->path, r->json, r->baseline, &r->error, fault);
  if (r->outcome == SaveOutcome::kCommitted)
    r->baseline = Sha256(std::span(
        reinterpret_cast<const std::uint8_t*>(r->json.data()), r->json.size()));
}
}  // namespace wf
