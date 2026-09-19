#pragma once
#include "editor/EditorState.h"
#include <cstdint>
namespace wf {
class ViewportRenderer {
public:
    ViewportRenderer();
    ~ViewportRenderer();
    ViewportRenderer(const ViewportRenderer&)=delete;
    ViewportRenderer& operator=(const ViewportRenderer&)=delete;
    void render(const EditorState& state,int width,int height);
    std::uintptr_t textureToken() const { return color_; }
    int width() const { return width_; }
    int height() const { return height_; }
    unsigned int draws() const { return draws_; }
    bool hasRenderedContent() const;
private:
    void resize(int width,int height);
    unsigned int program_{},vao_{},vbo_{},gridVao_{},gridVbo_{};
    unsigned int fbo_{},color_{},depth_{};
    int width_{},height_{},gridCount_{};
    unsigned int draws_{};
};
}
