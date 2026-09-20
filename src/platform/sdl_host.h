// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_PLATFORM_SDL_HOST_H_
#define WORLDFORGE_SRC_PLATFORM_SDL_HOST_H_

#include <string>

#include "SDL3/SDL.h"

namespace wf {

// 持有 SDL 视频会话与当前 GL 上下文；只初始化一次，晚于 GL 资源销毁。
// Owns SDL video and the current GL context. Initialize once; destroy only
// after all dependent GL resources have been released.
class SdlHost {
 public:
  SdlHost() = default;

  SdlHost(const SdlHost&) = delete;

  SdlHost& operator=(const SdlHost&) = delete;

  ~SdlHost();

  // 窗口或上下文创建失败时返回 false；error 必须非空，析构负责清理。
  // Returns false with a diagnostic on failure. Requires a non-null error;
  // destruction releases resources from partial initialization.
  bool Initialize(bool hidden, std::string* error);

  SDL_Window* window() const { return window_; }

  SDL_GLContext context() const { return context_; }

  float scale() const;

  void Swap();

  std::string GpuDescription() const;

 private:
  bool initialized_ = false;
  SDL_Window* window_ = nullptr;
  SDL_GLContext context_ = nullptr;
};

}  // namespace wf

#endif  // WORLDFORGE_SRC_PLATFORM_SDL_HOST_H_
