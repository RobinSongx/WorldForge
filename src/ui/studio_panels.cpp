// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "ui/studio_panels.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "glm/gtc/type_ptr.hpp"
// 上游根目录公共头文件；仅豁免这一处路径检查。
// Upstream public header at the include root; only its path check is waived.
#include "imgui.h"  // NOLINT(build/include_subdir)
// 上游 DockBuilder API 所在头文件；路径由上游提供。
// Upstream header for DockBuilder APIs; its path is defined upstream.
#include "imgui_internal.h"  // NOLINT(build/include_subdir)

#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "editor/editor_state.h"
#include "render/viewport_renderer.h"

namespace wf {
namespace {
constexpr ImVec4 kAccent{0.87f, 0.65f, 0.34f, 1.0f};
constexpr ImVec4 kMuted{0.53f, 0.60f, 0.65f, 1.0f};

void DrawLabel(const char* text) { ImGui::TextColored(kMuted, "%s", text); }

// 只在首次运行或用户重置时重建布局，保留平时手动调整的停靠位置。
// Rebuilds only on first use or reset, preserving user docking changes.
void ResetDock(ImGuiID id, const ImVec2& size) {
  ImGui::DockBuilderRemoveNode(id);
  ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(id, size);
  auto center = id;
  ImGuiID left = 0;
  ImGuiID right = 0;
  ImGuiID bottom = 0;
  ImGuiID bottom_right = 0;
  ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, .18f, &left, &center);
  ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, .25f, &right, &center);
  ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, .25f, &bottom, &center);
  ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, .40f, &bottom_right,
                              &bottom);
  ImGui::DockBuilderDockWindow("Scene", left);
  ImGui::DockBuilderDockWindow("Inspector", right);
  ImGui::DockBuilderDockWindow("Viewport", center);
  ImGui::DockBuilderDockWindow("Console", bottom);
  ImGui::DockBuilderDockWindow("Assets", bottom);
  ImGui::DockBuilderDockWindow("Performance", bottom_right);
  ImGui::DockBuilderFinish(id);
}

void DrawHeader(EditorState& editor, PanelState& panels) {
  if (!ImGui::BeginMainMenuBar()) return;
  ImGui::TextColored(kAccent, "W O R L D F O R G E");
  ImGui::Dummy({14, 0});
  if (ImGui::BeginMenu("File")) {
    if (ImGui::MenuItem("Import GLB...", nullptr, false, !panels.import_busy))
      panels.import_requested = true;
    if (ImGui::MenuItem("Exit")) panels.quit_requested = true;
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("Scene")) {
    if (ImGui::MenuItem("Add cube")) editor.AddCube();
    if (ImGui::MenuItem("Clear selection", nullptr, false, editor.selected()))
      editor.ClearSelection();
    if (ImGui::MenuItem("Delete selected", nullptr, false,
                        editor.selected() != nullptr))
      editor.RemoveSelected();
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("View")) {
    ImGui::MenuItem("Scene", nullptr, &panels.hierarchy);
    ImGui::MenuItem("Inspector", nullptr, &panels.inspector);
    ImGui::MenuItem("Console", nullptr, &panels.console);
    ImGui::MenuItem("Assets", nullptr, &panels.assets);
    ImGui::MenuItem("Performance", nullptr, &panels.performance);
    if (ImGui::MenuItem("Reset layout")) panels.reset_layout = true;
    ImGui::EndMenu();
  }
  const char* status = "WINDOWS / MESH PREVIEW";
  const float status_width = ImGui::CalcTextSize(status).x;
  if (ImGui::GetWindowWidth() > ImGui::GetCursorPosX() + status_width + 30) {
    ImGui::SameLine(ImGui::GetWindowWidth() - status_width - 40);
    DrawLabel(status);
  }
  ImGui::EndMainMenuBar();
}
}  // namespace

void ApplyTheme(float scale) {
  // 先恢复基准，再按当前 DPI 缩放；避免切换显示器时尺寸反复累乘。
  // Resets the base style before DPI scaling to avoid repeated multiplication.
  ImGui::GetStyle() = ImGuiStyle{};
  ImGui::StyleColorsDark();
  auto& style = ImGui::GetStyle();
  style.FontSizeBase = 14.0f;
  style.WindowRounding = 4;
  style.FrameRounding = 3;
  style.GrabRounding = 3;
  style.TabRounding = 3;
  style.WindowBorderSize = 1;
  style.FrameBorderSize = 0;
  style.WindowPadding = {14, 12};
  style.FramePadding = {8, 5};
  style.ItemSpacing = {8, 8};
  auto* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = {.068f, .079f, .092f, 1};
  colors[ImGuiCol_ChildBg] = colors[ImGuiCol_WindowBg];
  colors[ImGuiCol_PopupBg] = {.09f, .105f, .12f, 1};
  colors[ImGuiCol_Border] = {.16f, .19f, .22f, 1};
  colors[ImGuiCol_TitleBg] = {.075f, .09f, .10f, 1};
  colors[ImGuiCol_TitleBgActive] = {.12f, .14f, .16f, 1};
  colors[ImGuiCol_MenuBarBg] = {.048f, .059f, .07f, 1};
  colors[ImGuiCol_Header] = {.18f, .22f, .25f, 1};
  colors[ImGuiCol_HeaderHovered] = {.25f, .29f, .31f, 1};
  colors[ImGuiCol_HeaderActive] = {.32f, .28f, .21f, 1};
  colors[ImGuiCol_FrameBg] = {.115f, .14f, .16f, 1};
  colors[ImGuiCol_FrameBgHovered] = {.19f, .23f, .26f, 1};
  colors[ImGuiCol_Button] = {.19f, .23f, .26f, 1};
  colors[ImGuiCol_ButtonHovered] = {.32f, .29f, .23f, 1};
  colors[ImGuiCol_ButtonActive] = {.43f, .33f, .20f, 1};
  colors[ImGuiCol_CheckMark] = kAccent;
  colors[ImGuiCol_SliderGrab] = kAccent;
  colors[ImGuiCol_SliderGrabActive] = kAccent;
  colors[ImGuiCol_Tab] = {.09f, .11f, .13f, 1};
  colors[ImGuiCol_TabSelected] = {.17f, .20f, .23f, 1};
  colors[ImGuiCol_TabHovered] = {.30f, .26f, .20f, 1};
  colors[ImGuiCol_DockingPreview] = {.87f, .65f, .34f, .3f};
  colors[ImGuiCol_Text] = {.88f, .91f, .93f, 1};
  colors[ImGuiCol_TextDisabled] = kMuted;
  style.ScaleAllSizes(scale);
  style.FontScaleDpi = scale;
}

namespace {
void DrawDock(PanelState& panels) {
  auto* main = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(main->WorkPos);
  ImGui::SetNextWindowSize(main->WorkSize);
  ImGui::SetNextWindowViewport(main->ID);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("StudioDock", nullptr,
               ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoNavFocus);
  ImGui::PopStyleVar(3);
  const auto dock_id = ImGui::GetID("WorldForgeDock");
  if (!ImGui::DockBuilderGetNode(dock_id) || panels.reset_layout) {
    ResetDock(dock_id, main->WorkSize);
    panels.reset_layout = false;
    panels.hierarchy = panels.inspector = panels.console = panels.performance =
        panels.assets = true;
  }
  ImGui::DockSpace(dock_id, ImVec2(0, 0), ImGuiDockNodeFlags_None);
  ImGui::End();
  panels.panels_drawn = 0;
}

// 仅展开源节点；所有可编辑变换都位于模型包装根上。
// Expands source nodes read-only; editable transforms belong to the wrapper.
void DrawImportedNodes(const ImportedScene& asset) {
  std::vector<int> pending(asset.roots.rbegin(), asset.roots.rend());
  while (!pending.empty()) {
    const int index = pending.back();
    pending.pop_back();
    if (index == -1) {
      ImGui::TreePop();
      continue;
    }
    const auto& node = asset.nodes[index];
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_None;
    if (node.children.empty()) flags |= ImGuiTreeNodeFlags_Leaf;
    if (ImGui::TreeNodeEx(&node, flags, "%s%s", node.name.c_str(),
                          node.mesh >= 0 ? " [mesh]" : "")) {
      pending.push_back(-1);
      pending.insert(pending.end(), node.children.rbegin(),
                     node.children.rend());
    }
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "Source node (read-only). Edit the model root in Inspector.");
  }
}

// Esc 仅作用于当前编辑窗口，不抢文本、弹窗及原生文件对话框的输入。
// Esc applies only to the focused editor window, never text or dialog input.
void HandleSelectionEscape(EditorState& editor, const PanelState& panels) {
  if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
      !panels.import_busy && !ImGui::GetIO().WantTextInput &&
      !ImGui::IsAnyItemActive() &&
      !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId |
                                       ImGuiPopupFlags_AnyPopupLevel) &&
      ImGui::IsKeyPressed(ImGuiKey_Escape, false))
    editor.ClearSelection();
}

const char* WrapName(int value) {
  if (value == 33071) return "Clamp";
  if (value == 33648) return "Mirror";
  return "Repeat";
}

const char* FilterName(int value) {
  switch (value) {
    case 9728:
      return "Nearest";
    case 9729:
      return "Linear";
    case 9984:
      return "Nearest / nearest mip";
    case 9985:
      return "Linear / nearest mip";
    case 9986:
      return "Nearest / linear mip";
    default:
      return "Linear / linear mip";
  }
}

void DrawTextures(const ImportedScene& scene) {
  bool found = false;
  for (std::size_t i = 0; i < scene.meshes.size(); ++i) {
    const auto& mesh = *scene.meshes[i];
    for (std::size_t j = 0; j < mesh.primitives.size(); ++j) {
      const auto& texture = mesh.primitives[j].base_color_texture;
      if (!texture) continue;
      found = true;
      ImGui::Text("Mesh %zu / primitive %zu", i + 1, j + 1);
      ImGui::TextWrapped("Base color: PNG %u x %u (sRGB)",
                         texture->image->width, texture->image->height);
      ImGui::TextWrapped("UV0 / U: %s / V: %s",
                         WrapName(texture->sampler.wrap_s),
                         WrapName(texture->sampler.wrap_t));
      ImGui::TextWrapped("Min: %s / Mag: %s",
                         FilterName(texture->sampler.min_filter),
                         FilterName(texture->sampler.mag_filter));
    }
  }
  if (!found) ImGui::TextUnformatted("Base color: factor only (no texture)");
}

void DrawScene(EditorState& editor, PanelState& panels) {
  if (panels.hierarchy) {
    const bool shown = ImGui::Begin("Scene", &panels.hierarchy);
    ++panels.panels_drawn;
    if (shown) {
      HandleSelectionEscape(editor, panels);
      DrawLabel("UNTITLED WORLD");
      ImGui::Separator();
      if (ImGui::Button("+ Cube")) editor.AddCube();
      ImGui::SameLine();
      if (ImGui::Button("New")) ImGui::OpenPopup("New scene?");
      if (ImGui::BeginPopupModal("New scene?", nullptr,
                                 ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(
            "Discard this in-memory scene?\nProject saving arrives in the Mesh "
            "milestone.");
        if (ImGui::Button("Discard and create")) {
          editor.NewScene();
          ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
      }
      ImGui::Spacing();
      for (const auto& entity : editor.entities()) {
        ImGui::PushID(static_cast<int>(entity.id));
        auto flags =
            ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (editor.selection() == entity.id)
          flags |= ImGuiTreeNodeFlags_Selected;
        const bool open =
            ImGui::TreeNodeEx("model", flags, "%s%s", entity.name.c_str(),
                              entity.visible ? "" : " (hidden)");
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
          editor.Select(entity.id);
        if (open) {
          if (entity.asset) DrawImportedNodes(*entity.asset);
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
      ImGui::Spacing();
      ImGui::Separator();
      DrawLabel("SCENE CONTENT");
      ImGui::TextWrapped(
          "%zu entities\nSelect an object to edit its transform.",
          editor.entities().size());
      ImGui::BeginDisabled(!editor.selected());
      if (ImGui::Button("Clear selection")) editor.ClearSelection();
      ImGui::EndDisabled();
      // 仅 Scene 空白可清选；视口点击尚未实现几何拾取。
      // Only Scene background clears selection; viewport picking is not built.
      if (!panels.import_busy && ImGui::IsWindowHovered() &&
          !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive() &&
          ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        editor.ClearSelection();
    }
    ImGui::End();
  }
}

void DrawInspector(EditorState& editor, PanelState& panels) {
  if (panels.inspector) {
    const bool shown = ImGui::Begin("Inspector", &panels.inspector);
    ++panels.panels_drawn;
    if (shown) {
      if (auto* entity = editor.selected()) {
        DrawLabel("SELECTED OBJECT");
        ImGui::TextColored(kAccent, "%s", entity->name.c_str());
        ImGui::Separator();
        ImGui::Checkbox("Visible", &entity->visible);
        if (ImGui::CollapsingHeader("Transform",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          // 先编辑副本，通过校验后再提交，非法输入不会破坏原变换。
          // Edits a copy; commits only valid values to preserve existing state.
          auto transform = entity->transform;
          DrawLabel("Position");
          ImGui::SetNextItemWidth(-1);
          bool changed = ImGui::DragFloat3(
              "##position", glm::value_ptr(transform.position), .02f);
          DrawLabel("Rotation / degrees");
          ImGui::SetNextItemWidth(-1);
          changed |= ImGui::DragFloat3(
              "##rotation", glm::value_ptr(transform.rotation_degrees), .5f);
          DrawLabel("Scale");
          ImGui::SetNextItemWidth(-1);
          changed |= ImGui::DragFloat3(
              "##scale", glm::value_ptr(transform.scale), .01f, .01f, 100.0f);
          if (changed && !editor.SetTransform(entity->id, transform))
            editor.Log("WARN", "Rejected invalid transform");
          if (ImGui::Button("Reset transform")) {
            Transform reset;
            editor.SetTransform(entity->id, reset);
          }
        }
        if (ImGui::CollapsingHeader("Appearance",
                                    ImGuiTreeNodeFlags_DefaultOpen))
          ImGui::ColorEdit3("Tint", glm::value_ptr(entity->color),
                            ImGuiColorEditFlags_NoInputs);
        if (ImGui::CollapsingHeader("Geometry",
                                    ImGuiTreeNodeFlags_DefaultOpen)) {
          if (entity->asset) {
            ImGui::Text("%zu meshes / %zu nodes", entity->asset->meshes.size(),
                        entity->asset->nodes.size());
            ImGui::Text("%zu triangles / %zu vertices",
                        entity->asset->triangles, entity->asset->vertices);
            ImGui::TextUnformatted("Base color / diffuse preview");
            DrawTextures(*entity->asset);
          }
        }
        ImGui::Spacing();
        if (ImGui::Button("Frame selected"))
          editor.camera().Frame(*entity,
                                static_cast<float>(panels.viewport_width) /
                                    panels.viewport_height);
        if (ImGui::Button("Delete selected")) editor.RemoveSelected();
      } else {
        DrawLabel("NO SELECTION");
        ImGui::TextWrapped("Choose an object in the Scene panel.");
      }
    }
    ImGui::End();
  }
}

bool DrawViewport(EditorState& editor, ViewportRenderer& renderer,
                  PanelState& panels, std::string* error) {
  {
    const bool shown = ImGui::Begin(
        "Viewport", nullptr,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ++panels.panels_drawn;
    if (shown) {
      HandleSelectionEscape(editor, panels);
      ImGui::TextColored(kAccent, "PERSPECTIVE");
      ImGui::Checkbox("Ground grid", &editor.display_options().grid);
      ImGui::SameLine();
      ImGui::Checkbox("Wireframe", &editor.display_options().wireframe);
      ImGui::SameLine();
      ImGui::Checkbox("Selection overlay",
                      &editor.display_options().selection_overlay);
      if (ImGui::Button("Frame") && editor.selected())
        editor.camera().Frame(
            *editor.selected(),
            static_cast<float>(panels.viewport_width) / panels.viewport_height);
      ImGui::SameLine();
      if (ImGui::Button("Reset camera")) editor.camera() = OrbitCamera{};
      ImGui::TextWrapped("%s", panels.import_status.c_str());
      const auto size = ImGui::GetContentRegionAvail();
      if (size.x > 1 && size.y > 1) {
        const auto origin = ImGui::GetCursorScreenPos();
        panels.image_x = origin.x;
        panels.image_y = origin.y;
        const auto& io = ImGui::GetIO();
        // UI 布局用逻辑尺寸，离屏附件按 framebuffer 比例换算成像素。
        // Converts logical UI size to pixel dimensions for offscreen
        // attachments.
        panels.viewport_width = std::max(
            1, static_cast<int>(size.x * io.DisplayFramebufferScale.x));
        panels.viewport_height = std::max(
            1, static_cast<int>(size.y * io.DisplayFramebufferScale.y));
        // Render 可能重建纹理，因此必须先渲染，再向 ImGui 提交句柄。
        // Render may recreate the texture; submit its handle only afterward.
        if (!renderer.Render(editor.BuildRenderScene(
                                 static_cast<float>(panels.viewport_width) /
                                 panels.viewport_height),
                             panels.viewport_width, panels.viewport_height,
                             error)) {
          ImGui::End();
          return false;
        }
        // 翻转 V 坐标，使 GL 纹理的上下方向与 UI 图像一致。
        // Flips V coordinates to align GL texture orientation with the UI.
        ImGui::Image(static_cast<ImTextureID>(renderer.texture_token()), size,
                     ImVec2(0, 1), ImVec2(1, 0));
        // 仅悬停视口时消费导航输入，防止操作其他面板时误动相机。
        // 相机变化在下一帧渲染时生效。
        // Consumes navigation only over the viewport, not over other panels.
        // Camera changes affect the next rendered frame.
        if (ImGui::IsItemHovered()) {
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            editor.camera().Orbit(io.MouseDelta.x, io.MouseDelta.y);
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
            editor.camera().Pan(io.MouseDelta.x, io.MouseDelta.y);
          if (io.MouseWheel != 0) editor.camera().Zoom(io.MouseWheel);
          if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F) &&
              editor.selected())
            editor.camera().Frame(*editor.selected(),
                                  static_cast<float>(panels.viewport_width) /
                                      panels.viewport_height);
        }
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddText(ImVec2(origin.x + 14, origin.y + 14),
                      IM_COL32(155, 175, 184, 255), "WORLD / 01");
        draw->AddText(ImVec2(origin.x + 14, origin.y + size.y - 26),
                      IM_COL32(155, 175, 184, 255),
                      "RMB orbit   MMB pan   Wheel zoom   F frame");
        draw->AddCircleFilled(ImVec2(origin.x + size.x - 20, origin.y + 20), 4,
                              IM_COL32(114, 193, 153, 255));
      }
    }
    ImGui::End();
  }
  return true;
}

void DrawConsole(const EditorState& editor, PanelState& panels) {
  if (panels.console) {
    const bool shown = ImGui::Begin("Console", &panels.console);
    ++panels.panels_drawn;
    if (shown) {
      DrawLabel("SESSION LOG");
      ImGui::Separator();
      ImGui::BeginChild("LogScroll", ImVec2(0, 0));
      for (const auto& log : editor.logs()) {
        ImGui::TextColored(log.level == "WARN" ? kAccent : kMuted, "[%s]",
                           log.level.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", log.message.c_str());
      }
      ImGui::EndChild();
    }
    ImGui::End();
  }
}

void DrawAssets(EditorState& editor, PanelState& panels) {
  if (panels.assets) {
    const bool shown = ImGui::Begin("Assets", &panels.assets);
    ++panels.panels_drawn;
    if (shown) {
      DrawLabel("BUILT-IN PRIMITIVES");
      if (ImGui::Button("Cube\nAdd to scene", ImVec2(130, 65)))
        editor.AddCube();
      ImGui::SameLine();
      ImGui::BeginDisabled(panels.import_busy);
      if (ImGui::Button("Import GLB...", ImVec2(130, 65)))
        panels.import_requested = true;
      ImGui::EndDisabled();
      ImGui::Separator();
      ImGui::TextWrapped("%s", panels.import_status.c_str());
      if (!panels.import_metrics.empty())
        ImGui::TextWrapped("%s", panels.import_metrics.c_str());
      DrawLabel("Static triangles / embedded resources / base color preview");
      for (const auto& entity : editor.entities()) {
        if (entity.asset)
          ImGui::Text("%s: %zu vertices, %zu triangles", entity.name.c_str(),
                      entity.asset->vertices, entity.asset->triangles);
      }
    }
    ImGui::End();
  }
}

void DrawPerformance(const ViewportRenderer& renderer, PanelState& panels,
                     const std::string& gpu) {
  // 记录 CPU 帧间隔的环形历史；该数值不是 GPU 执行耗时。
  // Keeps a ring of CPU frame intervals, not GPU execution timings.
  panels.frame_times[panels.frame_index++ % panels.frame_times.size()] =
      ImGui::GetIO().DeltaTime * 1000;
  if (panels.performance) {
    const bool shown = ImGui::Begin("Performance", &panels.performance);
    ++panels.panels_drawn;
    if (shown) {
      ImGui::TextColored(kAccent, "%.1f FPS", ImGui::GetIO().Framerate);
      DrawLabel("CPU FRAME INTERVAL");
      ImGui::PlotLines(
          "##frames", panels.frame_times.data(),
          static_cast<int>(panels.frame_times.size()),
          static_cast<int>(panels.frame_index % panels.frame_times.size()),
          nullptr, 0, 50, ImVec2(-1, 45));
      ImGui::Text("Viewport %d x %d", renderer.width(), renderer.height());
      ImGui::SameLine();
      ImGui::Text("/ %u draws", renderer.draws());
      ImGui::Text("Geometry: %zu cached meshes / %.2f MiB",
                  renderer.cached_meshes(),
                  renderer.gpu_bytes() / (1024.0 * 1024.0));
      ImGui::TextWrapped("Textures: %zu images / %zu samplers / %.2f MiB",
                         renderer.cached_images(), renderer.cached_samplers(),
                         renderer.texture_bytes() / (1024.0 * 1024.0));
      DrawLabel("Cache estimates incl. mipmaps; excludes driver/FBO memory.");
      ImGui::TextWrapped("%s", gpu.c_str());
      DrawLabel("GPU timing is not implemented in this preview.");
    }
    ImGui::End();
  }
}
}  // namespace

bool DrawStudio(EditorState& editor, ViewportRenderer& renderer,
                PanelState& panels, const std::string& gpu,
                std::string* error) {
  DrawHeader(editor, panels);
  DrawDock(panels);
  // 先处理场景与属性编辑，再绘制视口，使这些修改在本帧可见。
  // Applies scene/inspector edits before rendering them in this frame.
  DrawScene(editor, panels);
  DrawInspector(editor, panels);
  if (!DrawViewport(editor, renderer, panels, error)) return false;
  DrawConsole(editor, panels);
  DrawAssets(editor, panels);
  DrawPerformance(renderer, panels, gpu);
  return true;
}
}  // namespace wf
