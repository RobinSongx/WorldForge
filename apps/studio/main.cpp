// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include <string>
#include <string_view>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "studio/studio_app.h"

int main(int argc, char** argv) {
  // 这是开发验证入口；正常运行通过对话框报告错误，测试使用文件和日志。
  // This is a developer smoke entry; normal errors use a dialog, tests use
  // logs.
  const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
  const std::string output = smoke && argc > 2 ? argv[2] : "artifacts/smoke";
  std::string error;
  const std::string mesh_path = smoke && argc > 3 ? argv[3] : "";
  const int result = wf::RunStudio(smoke, output, &error, mesh_path);
  if (result != 0) {
    if (smoke) {
      if (SDL_CreateDirectory(output.c_str())) {
        if (!SDL_SaveFile((output + "/error.txt").c_str(), error.data(),
                          error.size())) {
          SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                       "Cannot write error report: %s", SDL_GetError());
        }
      }
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", error.c_str());
    } else {
      SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "WorldForge error",
                               error.c_str(), nullptr);
    }
  }
  return result;
}
