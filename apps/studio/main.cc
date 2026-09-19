// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include <string>
#include <string_view>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

#include "studio/studio_app.h"

int main(int argc, char** argv) {
  const bool smoke = argc > 1 && std::string_view(argv[1]) == "--smoke-test";
  const std::string output = smoke && argc > 2 ? argv[2] : "artifacts/smoke";
  std::string error;
  const int result = wf::RunStudio(smoke, output, &error);
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
