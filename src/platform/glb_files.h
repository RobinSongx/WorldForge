// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_PLATFORM_GLB_FILES_H_
#define WORLDFORGE_SRC_PLATFORM_GLB_FILES_H_
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "SDL3/SDL.h"

namespace wf {
bool ReadGlbFile(const std::string& path, std::vector<std::uint8_t>* bytes,
                 std::string* error);

struct FileChoice {
  std::uint64_t generation = 0;
  std::string path;
  std::string error;
};

// 回调仅持有共享邮箱，不引用窗口、Editor、ImGui 或 GL。
// Callbacks retain only a mailbox, never a window, editor, ImGui or GL state.
class GlbFilePicker {
 public:
  GlbFilePicker();

  GlbFilePicker(const GlbFilePicker&) = delete;

  GlbFilePicker& operator=(const GlbFilePicker&) = delete;

  ~GlbFilePicker();

  bool Open(std::uint64_t generation);

  std::optional<FileChoice> TakeChoice();

  bool busy() const;

 private:
  struct Mailbox;
  static void SDLCALL Complete(void* userdata, const char* const* files,
                               int filter);

  std::shared_ptr<Mailbox> mailbox_;
};
}  // namespace wf
#endif  // WORLDFORGE_SRC_PLATFORM_GLB_FILES_H_
