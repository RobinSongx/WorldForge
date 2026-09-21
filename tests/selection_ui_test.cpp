// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include <cstdlib>
#include <iostream>
#include <string>

#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "backends/imgui_impl_opengl3.h"
#include "glad/gl.h"
// 测试需要查询停靠窗口的实际位置，使用上游根目录公共头。
// Tests query docked window positions through upstream root-level headers.
#include "imgui.h"  // NOLINT(build/include_subdir)
// 仅测试使用窗口查询与聚焦，不向产品增加测试入口。
// Window lookup/focus are test-only; no product test hooks are added.
#include "imgui_internal.h"  // NOLINT(build/include_subdir)

#include "editor/editor_state.h"
#include "platform/sdl_host.h"
#include "render/viewport_renderer.h"
#include "ui/studio_panels.h"

namespace {
void Require(bool value, const char* message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

class UiSession {
 public:
  UiSession() = default;

  UiSession(const UiSession&) = delete;

  UiSession& operator=(const UiSession&) = delete;

  ~UiSession() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui::DestroyContext();
  }

  void Initialize() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.DisplaySize = {1440, 900};
    io.DeltaTime = 1.0f / 60;
    wf::ApplyTheme(1);
    Require(ImGui_ImplOpenGL3_Init("#version 450 core"), "ImGui GL init");
  }

  void Frame(wf::EditorState& editor, wf::ViewportRenderer& renderer,
             wf::PanelState& panels, const char* focus = nullptr,
             bool text = false, bool activate = false, bool popup = false) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui::NewFrame();
    if (focus) ImGui::SetWindowFocus(focus);
    if (text || popup) {
      ImGui::Begin("Scene");
      if (text) {
        if (activate) ImGui::SetKeyboardFocusHere();
        ImGui::InputText("Test text field", buffer_, sizeof(buffer_));
        text_active_ = ImGui::IsItemActive();
      }
      if (popup) {
        ImGui::OpenPopup("Test modal");
        if (ImGui::BeginPopupModal("Test modal")) {
          ImGui::TextUnformatted("Modal input must retain selection");
          ImGui::EndPopup();
        }
      }
      ImGui::End();
    }
    std::string error;
    Require(wf::DrawStudio(editor, renderer, panels, "Test GPU", &error),
            error.c_str());
    ImGui::Render();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 1440, 900);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
  }

  bool text_active() const { return text_active_; }

 private:
  char buffer_[32] = "editable";
  bool text_active_ = false;
};

void TestSelection(wf::ViewportRenderer& renderer) {
  UiSession ui;
  ui.Initialize();
  wf::EditorState editor;
  wf::PanelState panels;
  for (int i = 0; i < 4; ++i) ui.Frame(editor, renderer, panels);
  const auto selected = editor.selected()->id;
  ImGui::ActivateItemByID(
      ImGui::FindWindowByName("Scene")->GetID("Clear selection"));
  ui.Frame(editor, renderer, panels);
  Require(!editor.selected(), "Clear selection button");
  editor.Select(selected);
  auto* viewport = ImGui::FindWindowByName("Viewport");
  for (const char* label : {"Ground grid", "Wireframe", "Selection overlay"}) {
    const bool grid = editor.display_options().grid;
    const bool wireframe = editor.display_options().wireframe;
    const bool overlay = editor.display_options().selection_overlay;
    ImGui::ActivateItemByID(viewport->GetID(label));
    ui.Frame(editor, renderer, panels);
    const auto& options = editor.display_options();
    Require((options.grid != grid) + (options.wireframe != wireframe) +
                    (options.selection_overlay != overlay) ==
                1,
            "Each checkbox must change only one display option");
    Require(editor.selection() == selected, "Display toggle cleared selection");
  }
  editor.display_options() = {};
  auto& io = ImGui::GetIO();

  for (const char* focus : {"Scene", "Viewport"}) {
    io.AddKeyEvent(ImGuiKey_Escape, false);
    ui.Frame(editor, renderer, panels, focus);
    editor.Select(selected);
    io.AddKeyEvent(ImGuiKey_Escape, true);
    ui.Frame(editor, renderer, panels, focus);
    Require(!editor.selected() && editor.entities().size() == 1,
            "Focused Scene/Viewport Escape must only clear selection");
  }
  io.AddKeyEvent(ImGuiKey_Escape, false);
  ui.Frame(editor, renderer, panels, "Inspector");
  editor.Select(selected);
  io.AddKeyEvent(ImGuiKey_Escape, true);
  ui.Frame(editor, renderer, panels, "Inspector");
  Require(editor.selected(), "Inspector Escape must not clear selection");

  io.AddKeyEvent(ImGuiKey_Escape, false);
  panels.import_busy = true;
  ui.Frame(editor, renderer, panels, "Scene");
  io.AddKeyEvent(ImGuiKey_Escape, true);
  ui.Frame(editor, renderer, panels, "Scene");
  Require(editor.selected(), "Native dialog/import busy Escape protection");
  panels.import_busy = false;

  io.AddKeyEvent(ImGuiKey_Escape, false);
  ui.Frame(editor, renderer, panels, "Scene", true, true);
  ui.Frame(editor, renderer, panels, nullptr, true);
  Require(ui.text_active(), "Text test must activate a real input field");
  io.AddKeyEvent(ImGuiKey_Escape, true);
  ui.Frame(editor, renderer, panels, nullptr, true);
  Require(editor.selected(), "Text editing Escape must preserve selection");

  io.AddKeyEvent(ImGuiKey_Escape, false);
  ui.Frame(editor, renderer, panels, "Scene", false, false, true);
  io.AddKeyEvent(ImGuiKey_Escape, true);
  ui.Frame(editor, renderer, panels, nullptr, false, false, true);
  Require(editor.selected(), "Popup Escape must preserve selection");
  ImGui::ClosePopupToLevel(0, true);
  io.AddKeyEvent(ImGuiKey_Escape, false);
  ui.Frame(editor, renderer, panels, "Scene");

  const auto* window = ImGui::FindWindowByName("Scene");
  Require(window != nullptr, "Scene window");
  io.AddMousePosEvent(window->Pos.x + window->Size.x / 2,
                      window->Pos.y + window->Size.y - 40);
  ui.Frame(editor, renderer, panels);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  ui.Frame(editor, renderer, panels);
  Require(!editor.selected(), "Scene blank click must clear selection");
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  ui.Frame(editor, renderer, panels);
  editor.Select(selected);
  io.AddMousePosEvent(panels.image_x + panels.viewport_width / 2.0f,
                      panels.image_y + panels.viewport_height / 2.0f);
  ui.Frame(editor, renderer, panels);
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  ui.Frame(editor, renderer, panels);
  Require(editor.selected(), "Viewport click must not pretend to pick empty");
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
  ui.Frame(editor, renderer, panels);
  editor.ClearSelection();
  const auto target = editor.camera().target();
  const auto distance = editor.camera().distance();
  io.AddKeyEvent(ImGuiKey_F, true);
  ui.Frame(editor, renderer, panels, "Viewport");
  Require(!editor.selected() && editor.camera().target() == target &&
              editor.camera().distance() == distance,
          "Frame without selection must be a safe no-op");
  Require(glGetError() == GL_NO_ERROR, "Selection UI GL error");
}
}  // namespace

int main(int, char**) {
  wf::SdlHost host;
  std::string error;
  Require(host.Initialize(true, &error), error.c_str());
  wf::ViewportRenderer renderer;
  Require(renderer.Initialize(&error), error.c_str());
  TestSelection(renderer);
  std::cout << "Selection UI: focused Escape, input/dialog guards, Scene "
               "background, viewport click and empty framing passed\n";
  return 0;
}
