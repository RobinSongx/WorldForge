// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "platform/glb_files.h"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "SDL3/SDL.h"

#include "io/glb_importer.h"

namespace wf {
bool ReadGlbFile(const std::string& path, std::vector<std::uint8_t>* bytes,
                 std::string* error) {
  auto extension = path.size() >= 4 ? path.substr(path.size() - 4) : "";
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 [](unsigned char value) {
                   return static_cast<char>(std::tolower(value));
                 });
  if (extension != ".glb") {
    *error = "Choose a self-contained .glb file";
    return false;
  }
  // SDL 的 UTF-8 文件 API 保留中文路径；句柄在所有返回路径自动关闭。
  // SDL UTF-8 I/O preserves Chinese paths; RAII closes every return path.
  const std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> file(
      SDL_IOFromFile(path.c_str(), "rb"), SDL_CloseIO);
  if (!file) {
    *error = SDL_GetError();
    return false;
  }
  const auto size = SDL_GetIOSize(file.get());
  if (size < 20 || static_cast<std::uint64_t>(size) > kMaxGlbBytes) {
    *error = "GLB must be between 20 bytes and 256 MiB";
    return false;
  }
  std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
  if (SDL_ReadIO(file.get(), data.data(), data.size()) != data.size()) {
    *error = "Incomplete GLB read: " + std::string(SDL_GetError());
    return false;
  }
  *bytes = std::move(data);
  return true;
}

struct GlbFilePicker::Mailbox {
  std::mutex mutex;
  bool active = true;
  bool busy = false;
  std::uint64_t generation = 0;
  std::optional<FileChoice> choice;
};

GlbFilePicker::GlbFilePicker(DialogProvider provider)
    : mailbox_(std::make_shared<Mailbox>()), provider_(std::move(provider)) {}

GlbFilePicker::~GlbFilePicker() {
  const std::lock_guard lock(mailbox_->mutex);
  mailbox_->active = false;
}

bool GlbFilePicker::Open(std::uint64_t generation, bool scene, bool save) {
  {
    const std::lock_guard lock(mailbox_->mutex);
    if (mailbox_->busy) return false;
    mailbox_->busy = true;
    mailbox_->generation = generation;
  }
  if (provider_) {
    auto choice = provider_(scene, save);
    choice.generation = generation;
    const std::lock_guard lock(mailbox_->mutex);
    mailbox_->choice = std::move(choice);
    return true;
  }
  static constexpr SDL_DialogFileFilter kFilter{"Binary glTF (*.glb)", "glb"};
  static constexpr SDL_DialogFileFilter kSceneFilter{"WorldForge (*.wfscene)",
                                                     "wfscene"};
  // userdata 的所有权交给回调；窗口关闭后邮箱仍有效。
  // Transfers userdata ownership to the callback; mailbox outlives the window.
  auto* context = new std::shared_ptr<Mailbox>(mailbox_);
  if (save)
    SDL_ShowSaveFileDialog(Complete, context, nullptr, &kSceneFilter, 1,
                           nullptr);
  else
    SDL_ShowOpenFileDialog(Complete, context, nullptr,
                           scene ? &kSceneFilter : &kFilter, 1, nullptr, false);
  return true;
}

void SDLCALL GlbFilePicker::Complete(void* userdata, const char* const* files,
                                     int /*filter*/) {
  const std::unique_ptr<std::shared_ptr<Mailbox>> context(
      static_cast<std::shared_ptr<Mailbox>*>(userdata));
  const auto& mailbox = *context;
  const std::lock_guard lock(mailbox->mutex);
  if (!mailbox->active) return;
  FileChoice choice;
  choice.generation = mailbox->generation;
  if (!files)
    choice.error = SDL_GetError();
  else if (files[0])
    choice.path = files[0];
  mailbox->choice = std::move(choice);
}

std::optional<FileChoice> GlbFilePicker::TakeChoice() {
  const std::lock_guard lock(mailbox_->mutex);
  if (!mailbox_->choice) return std::nullopt;
  auto result = std::move(mailbox_->choice);
  mailbox_->choice.reset();
  mailbox_->busy = false;
  return result;
}

bool GlbFilePicker::busy() const {
  const std::lock_guard lock(mailbox_->mutex);
  return mailbox_->busy;
}
}  // namespace wf
