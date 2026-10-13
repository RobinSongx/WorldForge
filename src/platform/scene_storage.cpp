// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "platform/scene_storage.h"

#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "domain/scene_document.h"
#include "platform/scene_identity.h"

namespace wf {
namespace {
class Handle {
 public:
  explicit Handle(HANDLE value) : value_(value) {}

  Handle(const Handle&) = delete;

  Handle& operator=(const Handle&) = delete;

  ~Handle() {
    if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_);
  }

  HANDLE get() const { return value_; }

  bool valid() const { return value_ != INVALID_HANDLE_VALUE; }

 private:
  HANDLE value_;
};

std::wstring Wide(const std::string& text) {
  const int count =
      MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                          static_cast<int>(text.size()), nullptr, 0);
  std::wstring result(count, 0);
  if (count)
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), count);
  return result;
}

std::string Utf8(const std::wstring& text) {
  const int count = WideCharToMultiByte(
      CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
      nullptr, 0, nullptr, nullptr);
  std::string result(count, 0);
  if (count)
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                        static_cast<int>(text.size()), result.data(), count,
                        nullptr, nullptr);
  std::replace(result.begin(), result.end(), '\\', '/');
  return result;
}

bool Fail(const std::string& message, std::string* error) {
  *error = message + " (Win32 " + std::to_string(GetLastError()) + ")";
  return false;
}

bool LocalPath(const std::string& path) {
  return path.size() >= 3 &&
         ((path[0] >= 'A' && path[0] <= 'Z') ||
          (path[0] >= 'a' && path[0] <= 'z')) &&
         path[1] == ':' && (path[2] == '/' || path[2] == '\\') &&
         path.find(':', 2) == std::string::npos &&
         path.find_first_of("*?\"<>|") == std::string::npos && ValidUtf8(path);
}

std::string Parent(const std::string& path) {
  return path.substr(0, path.find_last_of("/\\") + 1);
}

bool Missing(DWORD error) {
  return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

std::vector<std::string> Parts(const std::string& path) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < path.size()) {
    const auto end = path.find('/', start);
    parts.push_back(path.substr(
        start, end == std::string::npos ? std::string::npos : end - start));
    if (end == std::string::npos) break;
    start = end + 1;
  }
  return parts;
}
}  // namespace

bool CanonicalScenePath(const std::string& path, bool allow_missing,
                        std::string* canonical, std::string* error) {
  if (!LocalPath(path) || path.size() > 4096) {
    *error = "Only absolute local-drive paths are supported: " + path;
    return false;
  }
  const auto wide = Wide(path);
  Handle file(
      CreateFileW(wide.c_str(), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
  if (!file.valid()) {
    if (!allow_missing || !Missing(GetLastError()))
      return Fail("Cannot resolve " + path, error);
    const auto parent = Parent(path);
    const auto name = path.substr(parent.size());
    if (name.empty() || name == "." || name == ".." || name.back() == '.' ||
        name.back() == ' ') {
      *error = "Invalid target filename";
      return false;
    }
    std::string canonical_parent;
    if (!CanonicalScenePath(parent, false, &canonical_parent, error))
      return false;
    *canonical =
        canonical_parent + (canonical_parent.back() == '/' ? "" : "/") + name;
    return true;
  }
  std::wstring buffer(32768, 0);
  const DWORD length = GetFinalPathNameByHandleW(
      file.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
      FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!length || length >= buffer.size()) return Fail("Path resolution", error);
  buffer.resize(length);
  if (buffer.starts_with(L"\\\\?\\")) buffer.erase(0, 4);
  auto result = Utf8(buffer);
  if (!LocalPath(result)) {
    *error = "Resolved path is not a local drive";
    return false;
  }
  *canonical = std::move(result);
  return true;
}

bool ReadBoundedFile(const std::string& path, std::size_t limit,
                     std::vector<std::uint8_t>* bytes, std::string* error) {
  Handle file(CreateFileW(Wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                          nullptr));
  LARGE_INTEGER size{};
  if (!file.valid() || !GetFileSizeEx(file.get(), &size))
    return Fail("Cannot read " + path, error);
  if (size.QuadPart < 0 || static_cast<std::uint64_t>(size.QuadPart) > limit) {
    *error = "File budget exceeded: " + path;
    return false;
  }
  std::vector<std::uint8_t> data(static_cast<std::size_t>(size.QuadPart));
  DWORD read = 0;
  if (!ReadFile(file.get(), data.data(), static_cast<DWORD>(data.size()), &read,
                nullptr) ||
      read != data.size())
    return Fail("Incomplete read: " + path, error);
  *bytes = std::move(data);
  return true;
}

bool FileDigest(const std::string& path, bool allow_missing,
                std::string* digest, std::string* error) {
  const auto attributes = GetFileAttributesW(Wide(path).c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES && allow_missing &&
      Missing(GetLastError())) {
    digest->clear();
    return true;
  }
  std::vector<std::uint8_t> bytes;
  if (!ReadBoundedFile(path, 256 * 1024 * 1024, &bytes, error)) return false;
  *digest = Sha256(bytes);
  if (digest->empty()) {
    *error = "SHA256 failed";
    return false;
  }
  return true;
}

bool SamePath(const std::string& a, const std::string& b) {
  const auto left = Wide(a);
  const auto right = Wide(b);
  return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) ==
         CSTR_EQUAL;
}

bool SameFile(const std::string& a, const std::string& b) {
  if (SamePath(a, b)) return true;
  Handle left(
      CreateFileW(Wide(a).c_str(), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr));
  Handle right(
      CreateFileW(Wide(b).c_str(), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr));
  BY_HANDLE_FILE_INFORMATION x{}, y{};
  return left.valid() && right.valid() &&
         GetFileInformationByHandle(left.get(), &x) &&
         GetFileInformationByHandle(right.get(), &y) &&
         x.dwVolumeSerialNumber == y.dwVolumeSerialNumber &&
         x.nFileIndexHigh == y.nFileIndexHigh &&
         x.nFileIndexLow == y.nFileIndexLow;
}

bool ResolveAssetPath(const std::string& scene_path, const std::string& base,
                      const std::string& value, std::string* resolved,
                      std::string* error) {
  if (base == "absolute")
    return CanonicalScenePath(value, false, resolved, error);
  if (base != "scene" || value.empty() || value[0] == '/' || value[0] == '\\' ||
      value.find(':') != std::string::npos) {
    *error = "Invalid relative asset path";
    return false;
  }
  return CanonicalScenePath(Parent(scene_path) + value, false, resolved, error);
}

std::string RelativeAssetPath(const std::string& scene_path,
                              const std::string& absolute) {
  const auto folder = Parts(Parent(scene_path));
  const auto target = Parts(absolute);
  if (folder.empty() || target.empty() || !SamePath(folder[0], target[0]))
    return absolute;
  std::size_t shared = 0;
  while (shared < folder.size() && shared < target.size() &&
         SamePath(folder[shared], target[shared]))
    ++shared;
  std::string result;
  for (std::size_t i = shared; i < folder.size(); ++i) result += "../";
  for (std::size_t i = shared; i < target.size(); ++i) {
    if (i != shared) result += '/';
    result += target[i];
  }
  return result;
}

SceneLease::SceneLease(std::string path, void* handle)
    : path_(std::move(path)), handle_(handle) {}

SceneLease::~SceneLease() { CloseHandle(handle_); }

std::shared_ptr<SceneLease> SceneLease::Acquire(const std::string& path,
                                                std::string* error) {
  Handle target(
      CreateFileW(Wide(path).c_str(), FILE_READ_ATTRIBUTES,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, 0, nullptr));
  if (target.valid()) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(target.get(), &info) ||
        info.nNumberOfLinks > 1 ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      *error = "Scene target must be a regular file without hard-link aliases";
      return {};
    }
  } else if (!Missing(GetLastError())) {
    Fail("Cannot inspect target", error);
    return {};
  }
  HANDLE lock =
      CreateFileW(Wide(path + ".lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (lock == INVALID_HANDLE_VALUE) {
    Fail("Scene is busy or lock directory is not writable: " + path, error);
    return {};
  }
  return std::shared_ptr<SceneLease>(new SceneLease(path, lock));
}

SaveOutcome CommitSceneFile(const std::string& path, const std::string& text,
                            const std::string& baseline, std::string* error,
                            SaveFault fault) {
  std::string current;
  if (!FileDigest(path, true, &current, error))
    return SaveOutcome::kNotCommitted;
  if (current != baseline) {
    *error = "Conflict: scene changed outside Studio";
    return SaveOutcome::kNotCommitted;
  }
  const auto id = GenerateUuid();
  if (id.empty()) {
    *error = "UUID failed";
    return SaveOutcome::kNotCommitted;
  }
  const auto temporary = path + "." + id + ".tmp";
  const auto backup = path + "." + id + ".bak";
  bool ready = false;
  {
    Handle file(fault == SaveFault::kCreate
                    ? INVALID_HANDLE_VALUE
                    : CreateFileW(Wide(temporary).c_str(), GENERIC_WRITE, 0,
                                  nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
    if (!file.valid()) {
      Fail("Temporary file creation failed", error);
      return SaveOutcome::kNotCommitted;
    }
    DWORD written = 0;
    ready = fault != SaveFault::kWrite &&
            WriteFile(file.get(), text.data(), static_cast<DWORD>(text.size()),
                      &written, nullptr) &&
            written == text.size() && fault != SaveFault::kFlush &&
            FlushFileBuffers(file.get());
  }
  if (!ready || !FileDigest(path, true, &current, error) ||
      current != baseline || fault == SaveFault::kReplace) {
    DeleteFileW(Wide(temporary).c_str());
    *error = "Save not committed: write/flush failure or external conflict";
    return SaveOutcome::kNotCommitted;
  }
  const bool replaced =
      baseline.empty()
          ? MoveFileExW(Wide(temporary).c_str(), Wide(path).c_str(),
                        MOVEFILE_WRITE_THROUGH)
          : ReplaceFileW(Wide(path).c_str(), Wide(temporary).c_str(),
                         Wide(backup).c_str(), 0, nullptr, nullptr);
  const auto expected = Sha256(std::span(
      reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
  if (replaced && fault != SaveFault::kVerify &&
      FileDigest(path, false, &current, error) && current == expected) {
    // 保留唯一命名的备份，不覆盖用户可能已有的同名旁路文件。
    // Keep uniquely named backups; never overwrite an unrelated sidecar.
    return SaveOutcome::kCommitted;
  }
  std::string old;
  std::string ignored;
  if (!replaced && FileDigest(path, true, &current, &ignored) &&
      current == baseline) {
    DeleteFileW(Wide(temporary).c_str());
    *error = "Replacement failed; original content retained";
    return SaveOutcome::kNotCommitted;
  }
  // 仅目标缺失时恢复备份，绝不覆盖不明来源的新文件。
  // Restore backup only if target is absent; never overwrite unknown content.
  if (!replaced && current.empty() && !baseline.empty() &&
      FileDigest(backup, false, &old, &ignored) && old == baseline &&
      MoveFileExW(Wide(backup).c_str(), Wide(path).c_str(),
                  MOVEFILE_WRITE_THROUGH)) {
    DeleteFileW(Wide(temporary).c_str());
    *error = "Replacement failed; original restored";
    return SaveOutcome::kNotCommitted;
  }
  *error =
      "RecoveryRequired: inspect " + path + ", " + backup + ", " + temporary;
  return SaveOutcome::kRecoveryRequired;
}
}  // namespace wf
