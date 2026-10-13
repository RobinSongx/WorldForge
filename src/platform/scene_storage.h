// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_PLATFORM_SCENE_STORAGE_H_
#define WORLDFORGE_SRC_PLATFORM_SCENE_STORAGE_H_
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace wf {
bool CanonicalScenePath(const std::string& path, bool allow_missing,
                        std::string* canonical, std::string* error);
bool ReadBoundedFile(const std::string& path, std::size_t limit,
                     std::vector<std::uint8_t>* bytes, std::string* error);
bool FileDigest(const std::string& path, bool allow_missing,
                std::string* digest, std::string* error);
bool SamePath(const std::string& left, const std::string& right);
bool SameFile(const std::string& left, const std::string& right);
bool ResolveAssetPath(const std::string& scene_path, const std::string& base,
                      const std::string& value, std::string* resolved,
                      std::string* error);
std::string RelativeAssetPath(const std::string& scene_path,
                              const std::string& absolute);

// 锁文件存在不代表占用；共享租约只由活句柄决定。
// Lock-file existence is not ownership; a live handle owns the shared lease.
class SceneLease {
 public:
  SceneLease(const SceneLease&) = delete;
  SceneLease& operator=(const SceneLease&) = delete;
  ~SceneLease();
  static std::shared_ptr<SceneLease> Acquire(const std::string& canonical,
                                             std::string* error);

  const std::string& path() const { return path_; }

 private:
  SceneLease(std::string path, void* handle);
  std::string path_;
  void* handle_;
};
enum class SaveOutcome { kNotCommitted, kCommitted, kRecoveryRequired };
enum class SaveFault { kNone, kCreate, kWrite, kFlush, kReplace, kVerify };
// 故障点仅用于临时目录测试；默认不注入故障。
// Faults are for temporary-directory tests; production defaults to none.
SaveOutcome CommitSceneFile(const std::string& path, const std::string& text,
                            const std::string& baseline, std::string* error,
                            SaveFault fault = SaveFault::kNone);
}  // namespace wf
#endif  // WORLDFORGE_SRC_PLATFORM_SCENE_STORAGE_H_
