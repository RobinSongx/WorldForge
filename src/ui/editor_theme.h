// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_UI_EDITOR_THEME_H_
#define WORLDFORGE_SRC_UI_EDITOR_THEME_H_
// 上游公共头位于依赖根目录。
// Upstream public header lives at the dependency root.
#include "imgui.h"  // NOLINT(build/include_subdir)

namespace wf {
inline constexpr ImVec4 kUiAccent{0.87f, 0.68f, 0.39f, 1.0f};
inline constexpr ImVec4 kUiMuted{0.57f, 0.63f, 0.68f, 1.0f};
inline constexpr ImVec4 kUiSuccess{0.43f, 0.77f, 0.62f, 1.0f};

// 所有交互尺寸从当前字体比例导出，DPI 变化后无需手工倍乘。
// Interactive dimensions derive from font scale; DPI changes stay consistent.
struct UiMetrics {
  float unit = 1;
  float row_height = 28;
  float overlay_height = 42;
  float overlay_gap = 14;
};

UiMetrics CurrentUiMetrics();
void ApplyTheme(float scale);
}  // namespace wf
#endif  // WORLDFORGE_SRC_UI_EDITOR_THEME_H_
