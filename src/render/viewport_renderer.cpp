// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "render/viewport_renderer.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#include "glad/gl.h"
#include "glm/glm.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "editor/editor_state.h"

namespace wf {
namespace {
constexpr char kVertexSource[] = R"(#version 450 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
uniform mat4 uModel;
uniform mat4 uViewProjection;
out vec3 normal;
out vec3 world;
void main() {
  vec4 p = uModel * vec4(aPosition, 1);
  world = p.xyz;
  normal = mat3(transpose(inverse(uModel))) * aNormal;
  gl_Position = uViewProjection * p;
}
)";
constexpr char kFragmentSource[] = R"(#version 450 core
in vec3 normal;
in vec3 world;
uniform vec3 uColor;
uniform bool uUnlit;
out vec4 outColor;
void main() {
  float light = uUnlit ? 1.0 : 0.28 + 0.72 *
      max(dot(normalize(normal), normalize(vec3(0.4, 0.9, 0.6))), 0.0);
  vec3 color = uColor * light;
  outColor = vec4(color, 1);
}
)";
unsigned int CompileShader(unsigned int kind, const char* source,
                           std::string* error) {
  auto id = glCreateShader(kind);
  glShaderSource(id, 1, &source, nullptr);
  glCompileShader(id);
  int ok{};
  glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char message[4096]{};
    glGetShaderInfoLog(id, sizeof(message), nullptr, message);
    glDeleteShader(id);
    *error = message;
    return 0;
  }
  return id;
}
void CreateMesh(unsigned int& vao, unsigned int& vbo,
                const std::vector<float>& data) {
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(data.size() * sizeof(float)),
               data.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        reinterpret_cast<void*>(3 * sizeof(float)));
}
}  // namespace
bool ViewportRenderer::Initialize(std::string* error) {
  const auto vs = CompileShader(GL_VERTEX_SHADER, kVertexSource, error);
  if (!vs) return false;
  const auto fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentSource, error);
  if (!fs) {
    glDeleteShader(vs);
    return false;
  }
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  int ok{};
  glGetProgramiv(program_, GL_LINK_STATUS, &ok);
  if (!ok) {
    char message[4096]{};
    glGetProgramInfoLog(program_, sizeof(message), nullptr, message);
    *error = message;
    return false;
  }
  std::vector<float> cube;
  const glm::vec3 normals[] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                               {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  const glm::vec3 corners[][4] = {
      {{.5f, -.5f, -.5f}, {.5f, .5f, -.5f}, {.5f, .5f, .5f}, {.5f, -.5f, .5f}},
      {{-.5f, -.5f, .5f},
       {-.5f, .5f, .5f},
       {-.5f, .5f, -.5f},
       {-.5f, -.5f, -.5f}},
      {{-.5f, .5f, -.5f}, {-.5f, .5f, .5f}, {.5f, .5f, .5f}, {.5f, .5f, -.5f}},
      {{-.5f, -.5f, .5f},
       {-.5f, -.5f, -.5f},
       {.5f, -.5f, -.5f},
       {.5f, -.5f, .5f}},
      {{-.5f, -.5f, .5f}, {.5f, -.5f, .5f}, {.5f, .5f, .5f}, {-.5f, .5f, .5f}},
      {{.5f, -.5f, -.5f},
       {-.5f, -.5f, -.5f},
       {-.5f, .5f, -.5f},
       {.5f, .5f, -.5f}}};
  for (int face = 0; face < 6; ++face)
    for (int index : {0, 1, 2, 0, 2, 3}) {
      const auto p = corners[face][index];
      const auto n = normals[face];
      cube.insert(cube.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
    }
  CreateMesh(vao_, vbo_, cube);
  std::vector<float> grid;
  auto point = [&grid](float x, float z) {
    grid.insert(grid.end(), {x, 0, z, 0, 1, 0});
  };
  for (int i = -12; i <= 12; ++i) {
    point(static_cast<float>(i), -12);
    point(static_cast<float>(i), 12);
    point(-12, static_cast<float>(i));
    point(12, static_cast<float>(i));
  }
  grid_count_ = static_cast<int>(grid.size() / 6);
  CreateMesh(grid_vao_, grid_vbo_, grid);
  glBindVertexArray(0);
  return true;
}
ViewportRenderer::~ViewportRenderer() {
  glDeleteFramebuffers(1, &fbo_);
  glDeleteTextures(1, &color_);
  glDeleteRenderbuffers(1, &depth_);
  glDeleteBuffers(1, &vbo_);
  glDeleteBuffers(1, &grid_vbo_);
  glDeleteVertexArrays(1, &vao_);
  glDeleteVertexArrays(1, &grid_vao_);
  glDeleteProgram(program_);
}
bool ViewportRenderer::Resize(int w, int h, std::string* error) {
  w = std::clamp(w, 1, 4096);
  h = std::clamp(h, 1, 4096);
  if (w == width_ && h == height_) return true;
  glDeleteFramebuffers(1, &fbo_);
  glDeleteTextures(1, &color_);
  glDeleteRenderbuffers(1, &depth_);
  width_ = w;
  height_ = h;
  glGenFramebuffers(1, &fbo_);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glGenTextures(1, &color_);
  glBindTexture(GL_TEXTURE_2D, color_);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                         color_, 0);
  glGenRenderbuffers(1, &depth_);
  glBindRenderbuffer(GL_RENDERBUFFER, depth_);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                            GL_RENDERBUFFER, depth_);
  const bool complete =
      glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (!complete) {
    width_ = 0;
    height_ = 0;
    *error = "Viewport framebuffer is incomplete";
    return false;
  }
  return true;
}
bool ViewportRenderer::Render(const EditorState& state, int w, int h,
                              std::string* error) {
  if (!Resize(w, h, error)) return false;
  draws_ = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, width_, height_);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glClearColor(0.055f, 0.071f, 0.084f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(program_);
  const auto vp =
      state.camera().Projection(static_cast<float>(width_) / height_) *
      state.camera().View();
  const auto identity = glm::mat4(1);
  glUniformMatrix4fv(glGetUniformLocation(program_, "uViewProjection"), 1,
                     GL_FALSE, glm::value_ptr(vp));
  if (state.display_options().grid) {
    glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE,
                       glm::value_ptr(identity));
    glUniform3f(glGetUniformLocation(program_, "uColor"), .16f, .20f, .23f);
    glUniform1i(glGetUniformLocation(program_, "uUnlit"), 1);
    glBindVertexArray(grid_vao_);
    glDrawArrays(GL_LINES, 0, grid_count_);
    ++draws_;
  }
  glBindVertexArray(vao_);
  glUniform1i(glGetUniformLocation(program_, "uUnlit"), 0);
  for (const auto& e : state.entities())
    if (e.visible) {
      const auto model = e.transform.Matrix();
      glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE,
                         glm::value_ptr(model));
      glUniform3fv(glGetUniformLocation(program_, "uColor"), 1,
                   glm::value_ptr(e.color));
      glPolygonMode(GL_FRONT_AND_BACK,
                    state.display_options().wireframe ? GL_LINE : GL_FILL);
      glDrawArrays(GL_TRIANGLES, 0, 36);
      ++draws_;
      if (state.selection() == e.id && !state.display_options().wireframe) {
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1, -1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUniform1i(glGetUniformLocation(program_, "uUnlit"), 1);
        glUniform3f(glGetUniformLocation(program_, "uColor"), .95f, .73f, .38f);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        ++draws_;
        glDisable(GL_POLYGON_OFFSET_LINE);
        glUniform1i(glGetUniformLocation(program_, "uUnlit"), 0);
      }
    }
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBindVertexArray(0);
  glUseProgram(0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_DEPTH_TEST);
  return true;
}
bool ViewportRenderer::HasRenderedContent() const {
  if (!fbo_) return false;
  std::vector<unsigned char> pixels(static_cast<size_t>(width_) * height_ * 4);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
  glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
  glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
  size_t colored{};
  for (size_t i = 0; i < pixels.size(); i += 4)
    if (pixels[i] > 70 && pixels[i + 1] > 40) ++colored;
  return colored > 100;
}
}  // namespace wf
