#pragma once
#include "editor/EditorState.h"
#include "render/ViewportRenderer.h"
#include <array>
#include <string>
namespace wf {
struct PanelState {
    bool hierarchy{true},inspector{true},console{true},performance{true},assets{true};
    bool resetLayout{false},quitRequested{false};
    std::array<float,120> frameTimes{};
    unsigned int frameIndex{};
    unsigned int panelsDrawn{};
    int viewportWidth{1},viewportHeight{1};
    float imageX{},imageY{};
};
void applyTheme(float scale);
void drawStudio(EditorState& editor,ViewportRenderer& renderer,PanelState& panels,const std::string& gpu);
}
