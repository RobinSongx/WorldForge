// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "platform/sdl_host.h"

#include <algorithm>
#include <string>

#include "SDL3/SDL.h"
#include "glad/gl.h"

namespace wf {

bool SdlHost::Initialize(bool hidden, std::string* error) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    *error = SDL_GetError();
    return false;
  }
  // 记录每一步已取得的资源，使初始化中途失败也能安全清理。
  // Tracks acquired resources so partial initialization can be cleaned up.
  initialized_ = true;
  if (!SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4) ||
      !SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5) ||
      !SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                           SDL_GL_CONTEXT_PROFILE_CORE) ||
      !SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1) ||
      !SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24)) {
    *error = SDL_GetError();
    return false;
  }
  const auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                     SDL_WINDOW_HIGH_PIXEL_DENSITY |
                     (hidden ? SDL_WINDOW_HIDDEN : 0);
  window_ = SDL_CreateWindow("WorldForge | Studio", 1440, 900, flags);
  if (window_ == nullptr) {
    *error = SDL_GetError();
    return false;
  }
  SDL_SetWindowMinimumSize(window_, 900, 600);
  context_ = SDL_GL_CreateContext(window_);
  if (context_ == nullptr || !SDL_GL_MakeCurrent(window_, context_)) {
    *error = SDL_GetError();
    return false;
  }
  // 必须先使上下文当前有效，再加载对应驱动的 GL 函数。
  // Makes the context current before loading driver-specific GL functions.
  if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)) ||
      !GLAD_GL_VERSION_4_5) {
    *error = "OpenGL 4.5 is required. Update your graphics driver.";
    return false;
  }
  SDL_GL_SetSwapInterval(hidden ? 0 : 1);
  return true;
}

SdlHost::~SdlHost() {
  // 按依赖的逆序释放：上下文、窗口，最后退出 SDL。
  // Releases dependencies in reverse order: context, window, then SDL.
  if (context_ != nullptr) SDL_GL_DestroyContext(context_);
  if (window_ != nullptr) SDL_DestroyWindow(window_);
  if (initialized_) SDL_Quit();
}

float SdlHost::scale() const {
  return std::max(SDL_GetWindowDisplayScale(window_), 1.0f);
}

void SdlHost::Swap() { SDL_GL_SwapWindow(window_); }

std::string SdlHost::GpuDescription() const {
  return std::string(reinterpret_cast<const char*>(glGetString(GL_RENDERER))) +
         " | GL " + reinterpret_cast<const char*>(glGetString(GL_VERSION));
}

}  // namespace wf
