// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "studio/studio_app.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "SDL3/SDL.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl3.h"
#include "glad/gl.h"
#include "imgui.h"  // NOLINT(build/include_subdir): upstream public header.

#include "editor/editor_state.h"
#include "platform/sdl_host.h"
#include "render/viewport_renderer.h"
#include "ui/studio_panels.h"

namespace wf {
namespace {
class ImGuiLifetime {
 public:
  ImGuiLifetime() = default;
  ImGuiLifetime(const ImGuiLifetime&) = delete;
  ImGuiLifetime& operator=(const ImGuiLifetime&) = delete;
  ~ImGuiLifetime() {
    if (renderer_) ImGui_ImplOpenGL3_Shutdown();
    if (platform_) ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
  }
  bool Initialize(const SdlHost& host, std::string* error) {
    platform_ = ImGui_ImplSDL3_InitForOpenGL(host.window(), host.context());
    if (!platform_) {
      *error = "ImGui SDL initialization failed";
      return false;
    }
    renderer_ = ImGui_ImplOpenGL3_Init("#version 450 core");
    if (!renderer_) {
      *error = "ImGui OpenGL initialization failed";
      return false;
    }
    return true;
  }

 private:
  bool platform_ = false;
  bool renderer_ = false;
};
bool SaveScreenshot(SDL_Window* window, const std::string& path,
                    std::string* error) {
  int w = 0;
  int h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  std::vector<unsigned char> pixels(static_cast<size_t>(w) * h * 4);
  std::vector<unsigned char> flipped(pixels.size());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  glReadBuffer(GL_BACK);
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  for (int y = 0; y < h; ++y)
    std::copy_n(pixels.data() + static_cast<size_t>(h - 1 - y) * w * 4,
                static_cast<size_t>(w) * 4,
                flipped.data() + static_cast<size_t>(y) * w * 4);
  auto* surface = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBA32,
                                        flipped.data(), w * 4);
  if (!surface) {
    *error = SDL_GetError();
    return false;
  }
  const bool saved = SDL_SaveBMP(surface, path.c_str());
  SDL_DestroySurface(surface);
  if (!saved) {
    *error = SDL_GetError();
    return false;
  }
  return true;
}
}  // namespace
int RunStudio(bool smoke_test, const std::string& output, std::string* error) {
  SdlHost host;
  if (!host.Initialize(smoke_test, error)) return 1;
  std::string settings_path;
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiLifetime ui;
  auto& io = ImGui::GetIO();
  io.ConfigFlags |=
      ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
  if (smoke_test) {
    io.IniFilename = nullptr;
    if (!SDL_CreateDirectory(output.c_str())) {
      *error = SDL_GetError();
      return 1;
    }
  } else {
    char* pref = SDL_GetPrefPath("WorldForge", "Studio");
    if (pref) {
      settings_path = std::string(pref) + "layout.ini";
      SDL_free(pref);
      io.IniFilename = settings_path.c_str();
    } else {
      io.IniFilename = nullptr;
    }
  }
  char* windows_dir{};
  size_t windows_dir_size{};
  _dupenv_s(&windows_dir, &windows_dir_size, "WINDIR");
  if (windows_dir) {
    const std::string font = std::string(windows_dir) + "/Fonts/segoeui.ttf";
    if (SDL_GetPathInfo(font.c_str(), nullptr))
      io.FontDefault = io.Fonts->AddFontFromFileTTF(font.c_str(), 14.0f);
  }
  std::free(windows_dir);
  ApplyTheme(host.scale());
  if (!ui.Initialize(host, error)) return 1;
  ViewportRenderer renderer;
  if (!renderer.Initialize(error)) return 1;
  EditorState editor;
  PanelState panels;
  const auto gpu = host.GpuDescription();
  editor.Log("INFO", "WorldForge Studio " WF_VERSION " / Windows framework");
  editor.Log("INFO", gpu);
  editor.Log("INFO",
             "OpenGL viewport ready. Right-drag to orbit; middle-drag to pan.");
  unsigned int frame{};
  bool running = true;
  bool resize_verified = false;
  bool transform_verified = false;
  bool content_verified = false;
  bool camera_input_verified = false;
  int initial_width{};
  float yaw_before_input{};
  while (running && !panels.quit_requested) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);
      if (event.type == SDL_EVENT_QUIT ||
          (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
           event.window.windowID == SDL_GetWindowID(host.window())))
        running = false;
      if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED)
        ApplyTheme(host.scale());
    }
    if (!running) break;
    if (!smoke_test &&
        (SDL_GetWindowFlags(host.window()) & SDL_WINDOW_MINIMIZED)) {
      SDL_Delay(30);
      continue;
    }
    if (smoke_test && frame == 8) {
      initial_width = renderer.width();
      SDL_SetWindowSize(host.window(), 1200, 780);
    }
    if (smoke_test && frame == 15) {
      auto id = editor.AddCube();
      Transform t;
      t.position = {1.6f, .5f, 0};
      t.rotation_degrees.y = 25;
      transform_verified = editor.SetTransform(id, t);
      editor.camera().Orbit(20, 5);
      editor.camera().Zoom(.3f);
    }
    if (smoke_test && frame == 22) {
      resize_verified = renderer.width() != initial_width;
      SDL_SetWindowSize(host.window(), 1440, 900);
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    if (smoke_test && frame >= 25 && frame <= 28) {
      io.AddFocusEvent(true);
      io.AddMousePosEvent(panels.image_x + 200 + (frame >= 26 ? 30.0f : 0.0f),
                          panels.image_y + 150);
      if (frame == 25) {
        yaw_before_input = editor.camera().yaw();
        io.AddMouseButtonEvent(1, true);
      }
      if (frame == 27) io.AddMouseButtonEvent(1, false);
    }
    ImGui::NewFrame();
    if (!DrawStudio(editor, renderer, panels, gpu, error)) return 1;
    if (smoke_test && frame == 28)
      camera_input_verified = editor.camera().yaw() != yaw_before_input;
    ImGui::Render();
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(host.window(), &width, &height);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, width, height);
    glClearColor(.04f, .05f, .06f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (smoke_test && frame == 35) {
      content_verified = renderer.HasRenderedContent();
      if (!SaveScreenshot(host.window(), output + "/studio.bmp", error))
        return 1;
      const auto gl_error = glGetError();
      const bool success = content_verified && resize_verified &&
                           transform_verified && camera_input_verified &&
                           panels.panels_drawn == 6 && gl_error == GL_NO_ERROR;
      std::ostringstream report;
      report << "{\n  \"success\": " << (success ? "true" : "false")
             << ",\n  \"frames\": " << frame + 1 << ",\n  \"viewportContent\": "
             << (content_verified ? "true" : "false")
             << ",\n  \"resize\": " << (resize_verified ? "true" : "false")
             << ",\n  \"transform\": "
             << (transform_verified ? "true" : "false")
             << ",\n  \"cameraInput\": "
             << (camera_input_verified ? "true" : "false")
             << ",\n  \"panels\": " << panels.panels_drawn
             << ",\n  \"glError\": " << gl_error << "\n}\n";
      const auto report_text = report.str();
      if (!SDL_SaveFile((output + "/report.json").c_str(), report_text.data(),
                        report_text.size())) {
        *error = SDL_GetError();
        return 1;
      }
      if (!success) *error = "GPU smoke checks failed; see report.json";
      return success ? 0 : 2;
    }
    host.Swap();
    ++frame;
  }
  if (smoke_test) {
    *error = "Smoke run interrupted before checks completed";
    return 2;
  }
  return 0;
}
}  // namespace wf
