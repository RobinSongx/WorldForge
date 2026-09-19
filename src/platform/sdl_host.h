// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_PLATFORM_SDL_HOST_H_
#define WORLDFORGE_SRC_PLATFORM_SDL_HOST_H_

#include <string>

#include "SDL3/SDL.h"

namespace wf {

// Owns the process SDL video session and its current OpenGL context.
// Initialize once before using the accessors; destroy after all GL resources.
class SdlHost {
 public:
  SdlHost() = default;
  SdlHost(const SdlHost&) = delete;
  SdlHost& operator=(const SdlHost&) = delete;
  ~SdlHost();

  // Returns false and fills error if the window or GL context cannot be
  // created.
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
