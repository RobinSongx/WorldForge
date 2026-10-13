// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "ui/editor_theme.h"

#include <algorithm>
// 上游公共头位于依赖根目录。
// Upstream public header lives at the dependency root.
#include "imgui.h"  // NOLINT(build/include_subdir)

namespace wf {
UiMetrics CurrentUiMetrics() {
  const float unit = std::max(ImGui::GetFontSize() / 14.0f, 0.75f);
  return {unit, 28 * unit, 42 * unit, 14 * unit};
}

void ApplyTheme(float scale) {
  // 始终从基准样式重建，避免跨显示器时重复乘上 DPI 比例。
  // Rebuild from defaults so moving displays never compounds DPI scaling.
  ImGui::GetStyle() = ImGuiStyle{};
  ImGui::StyleColorsDark();
  auto& style = ImGui::GetStyle();
  style.FontSizeBase = 14.0f;
  style.WindowRounding = 8;
  style.FrameRounding = 6;
  style.GrabRounding = 4;
  style.TabRounding = 6;
  style.ScrollbarRounding = 5;
  style.WindowBorderSize = 0;
  style.FrameBorderSize = 0;
  style.WindowPadding = {15, 13};
  style.FramePadding = {9, 6};
  style.ItemSpacing = {8, 9};
  style.ItemInnerSpacing = {6, 5};
  style.CellPadding = {7, 6};
  style.ScrollbarSize = 11;
  style.IndentSpacing = 15;
  auto* colors = style.Colors;
  colors[ImGuiCol_WindowBg] = {.080f, .091f, .106f, 1};
  colors[ImGuiCol_ChildBg] = {.080f, .091f, .106f, 1};
  colors[ImGuiCol_PopupBg] = {.12f, .14f, .16f, 1};
  colors[ImGuiCol_Border] = {.21f, .24f, .27f, 1};
  colors[ImGuiCol_Separator] = {.17f, .20f, .23f, 1};
  colors[ImGuiCol_TitleBg] = {.095f, .11f, .13f, 1};
  colors[ImGuiCol_TitleBgActive] = {.11f, .13f, .15f, 1};
  colors[ImGuiCol_MenuBarBg] = {.067f, .077f, .090f, 1};
  colors[ImGuiCol_Header] = {.19f, .23f, .26f, 1};
  colors[ImGuiCol_HeaderHovered] = {.24f, .29f, .32f, 1};
  colors[ImGuiCol_HeaderActive] = {.31f, .32f, .29f, 1};
  colors[ImGuiCol_FrameBg] = {.13f, .15f, .17f, 1};
  colors[ImGuiCol_FrameBgHovered] = {.18f, .21f, .24f, 1};
  colors[ImGuiCol_FrameBgActive] = {.21f, .25f, .28f, 1};
  colors[ImGuiCol_Button] = {.18f, .21f, .24f, 1};
  colors[ImGuiCol_ButtonHovered] = {.25f, .29f, .32f, 1};
  colors[ImGuiCol_ButtonActive] = {.34f, .33f, .28f, 1};
  colors[ImGuiCol_CheckMark] = kUiAccent;
  colors[ImGuiCol_SliderGrab] = kUiAccent;
  colors[ImGuiCol_SliderGrabActive] = kUiAccent;
  colors[ImGuiCol_Tab] = {.10f, .12f, .14f, 1};
  colors[ImGuiCol_TabSelected] = {.18f, .21f, .23f, 1};
  colors[ImGuiCol_TabHovered] = {.23f, .27f, .30f, 1};
  colors[ImGuiCol_TabSelectedOverline] = kUiAccent;
  colors[ImGuiCol_TabDimmed] = {.09f, .11f, .13f, 1};
  colors[ImGuiCol_TabDimmedSelected] = {.18f, .21f, .23f, 1};
  colors[ImGuiCol_TabDimmedSelectedOverline] = kUiAccent;
  colors[ImGuiCol_DockingPreview] = {.87f, .68f, .39f, .30f};
  colors[ImGuiCol_Text] = {.91f, .93f, .94f, 1};
  colors[ImGuiCol_TextDisabled] = kUiMuted;
  style.ScaleAllSizes(scale);
  style.FontScaleDpi = scale;
}
}  // namespace wf
