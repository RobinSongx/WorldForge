// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#include "render/viewport_renderer.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "glad/gl.h"
#include "glm/glm.hpp"
#include "glm/gtc/color_space.hpp"
#include "glm/gtc/type_ptr.hpp"

#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "domain/render_scene.h"
#include "render/gpu_texture.h"

namespace wf {
namespace {
// 法线采用模型矩阵的逆转置，保证非均匀缩放下的光照方向正确。
// Transforms normals by the inverse transpose for nonuniform scaling.
constexpr char kVertexSource[] = R"(#version 450 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUv;
out vec2 uv;
uniform mat4 uModel;
uniform mat4 uViewProjection;
out vec3 normal;
out vec3 world;
void main() {
  vec4 p = uModel * vec4(aPosition, 1);
  world = p.xyz;
  uv = aUv;
  normal = mat3(transpose(inverse(uModel))) * aNormal;
  gl_Position = uViewProjection * p;
}
)";
// 仅用于预览的漫反射；网格与选中线框走无光照分支。
// Provides diffuse preview shading; grid and selection lines are unlit.
constexpr char kFragmentSource[] = R"(#version 450 core
in vec3 normal;
in vec3 world;
uniform vec3 uColor;
uniform bool uUnlit;
uniform bool uTextured;
uniform sampler2D uBaseColor;
in vec2 uv;
out vec4 outColor;
void main() {
  float light = uUnlit ? 1.0 : 0.28 + 0.72 *
      max(dot(normalize(normal), normalize(vec3(0.4, 0.9, 0.6))), 0.0);
  // sRGB 纹理在过滤前解码；因子和光照保持线性。
  // sRGB textures decode before filtering; factors and light remain linear.
  vec3 color = uColor * light;
  if (uTextured && !uUnlit) color *= texture(uBaseColor, uv).rgb;
  // RGBA8 视口存放显示编码值，供 ImGui 直接复制。
  // The RGBA8 viewport holds display-encoded values for ImGui's direct copy.
  vec3 encoded = mix(1.055 * pow(max(color, vec3(0)), vec3(1.0 / 2.4)) - 0.055,
                     color * 12.92, lessThanEqual(color, vec3(0.0031308)));
  outColor = vec4(uUnlit ? uColor : encoded, 1);
}
)";

// 成功返回由调用者释放的 shader；失败先清理，再返回 0 和诊断。
// Returns a caller-owned shader, or cleans up and returns 0 with an error.
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

// 顶点交错存储位置与法线，各三个 float；创建的 VAO/VBO 由调用者持有。
// Uploads interleaved position/normal triples; the caller owns VAO/VBO.
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

// GPU 缓存只弱引用 CPU Mesh；最后一个场景引用消失后可回收。
// The GPU cache weakly references CPU meshes, allowing last-user collection.
class ViewportRenderer::GpuMesh {
 public:
  struct Primitive {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    int count = 0;
  };

  explicit GpuMesh(const std::shared_ptr<const MeshAsset>& asset)
      : owner_(asset) {}

  GpuMesh(const GpuMesh&) = delete;
  GpuMesh& operator=(const GpuMesh&) = delete;

  ~GpuMesh() {
    for (const auto& primitive : primitives_) {
      glDeleteBuffers(1, &primitive.ebo);
      glDeleteBuffers(1, &primitive.vbo);
      glDeleteVertexArrays(1, &primitive.vao);
    }
  }

  bool Upload(const MeshAsset& mesh, std::string* error) {
    if (glGetError() != GL_NO_ERROR) {
      *error = "Existing GL error before mesh upload";
      return false;
    }
    for (const auto& source : mesh.primitives) {
      primitives_.emplace_back();
      auto& primitive = primitives_.back();
      glGenVertexArrays(1, &primitive.vao);
      glGenBuffers(1, &primitive.vbo);
      glGenBuffers(1, &primitive.ebo);
      glBindVertexArray(primitive.vao);
      glBindBuffer(GL_ARRAY_BUFFER, primitive.vbo);
      const auto vertex_bytes = source.vertices.size() * sizeof(MeshVertex);
      const auto index_bytes = source.indices.size() * sizeof(std::uint32_t);
      glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertex_bytes),
                   source.vertices.data(), GL_STATIC_DRAW);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, primitive.ebo);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(index_bytes), source.indices.data(),
                   GL_STATIC_DRAW);
      glEnableVertexAttribArray(0);
      glVertexAttribPointer(
          0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
          reinterpret_cast<void*>(offsetof(MeshVertex, position)));
      glEnableVertexAttribArray(1);
      glVertexAttribPointer(
          1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
          reinterpret_cast<void*>(offsetof(MeshVertex, normal)));
      glEnableVertexAttribArray(2);
      glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(MeshVertex),
                            reinterpret_cast<void*>(offsetof(MeshVertex, uv)));
      primitive.count = static_cast<int>(source.indices.size());
      bytes_ += vertex_bytes + index_bytes;
      const auto gl_error = glGetError();
      glBindVertexArray(0);
      if (gl_error != GL_NO_ERROR || !primitive.vao || !primitive.vbo ||
          !primitive.ebo) {
        *error = "Mesh upload failed, GL error " + std::to_string(gl_error);
        return false;
      }
    }
    return true;
  }

  bool expired() const { return owner_.expired(); }

  std::size_t bytes() const { return bytes_; }

  const std::vector<Primitive>& primitives() const { return primitives_; }

 private:
  std::weak_ptr<const MeshAsset> owner_;
  std::vector<Primitive> primitives_;
  std::size_t bytes_ = 0;
};

ViewportRenderer::ViewportRenderer() = default;

bool ViewportRenderer::Prepare(const ImportedScene& scene, std::string* error) {
  if (glGetError() != GL_NO_ERROR) {
    *error = "Existing GL error before resource preparation";
    return false;
  }
  std::map<std::uint64_t, std::unique_ptr<GpuMesh>> staged;
  std::map<std::uint64_t, std::unique_ptr<GpuImage>> images;
  std::map<std::uint64_t, std::unique_ptr<GpuSampler>> samplers;
  for (const auto& asset : scene.meshes) {
    for (const auto& primitive : asset->primitives) {
      const auto& texture = primitive.base_color_texture;
      if (!texture) continue;
      if (!texture->image) {
        *error = "Texture binding has no image";
        return false;
      }
      const auto& source = texture->image;
      if (!images_.contains(source->id) && !images.contains(source->id)) {
        auto image = std::make_unique<GpuImage>(source);
        if (!image->Upload(*source, error)) return false;
        images.emplace(source->id, std::move(image));
      }
      if (!samplers_.contains(texture->id) && !samplers.contains(texture->id)) {
        auto sampler = std::make_unique<GpuSampler>(texture);
        if (!sampler->Upload(texture->sampler, error)) return false;
        samplers.emplace(texture->id, std::move(sampler));
      }
    }
    if (meshes_.contains(asset->id) || staged.contains(asset->id)) continue;
    auto mesh = std::make_unique<GpuMesh>(asset);
    if (!mesh->Upload(*asset, error)) return false;
    staged.emplace(asset->id, std::move(mesh));
  }
  meshes_.merge(staged);
  images_.merge(images);
  samplers_.merge(samplers);
  return true;
}

void ViewportRenderer::CollectUnused() {
  std::erase_if(images_,
                [](const auto& entry) { return entry.second->expired(); });
  std::erase_if(samplers_,
                [](const auto& entry) { return entry.second->expired(); });
  for (auto it = meshes_.begin(); it != meshes_.end();) {
    if (it->second->expired())
      it = meshes_.erase(it);
    else
      ++it;
  }
}

std::size_t ViewportRenderer::gpu_bytes() const {
  std::size_t total = 0;
  for (const auto& entry : meshes_) total += entry.second->bytes();
  return total;
}

std::size_t ViewportRenderer::texture_bytes() const {
  std::size_t total = 0;
  for (const auto& entry : images_) total += entry.second->bytes();
  return total;
}

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
  // 链接后绘制只需 program，释放独立 shader 的应用侧所有权。
  // After linking, rendering uses the program; release shader ownership.
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
  glDeleteBuffers(1, &grid_vbo_);
  glDeleteVertexArrays(1, &grid_vao_);
  glDeleteProgram(program_);
}

bool ViewportRenderer::Resize(int w, int h, std::string* error) {
  w = std::clamp(w, 1, 4096);
  h = std::clamp(h, 1, 4096);
  // 只在像素尺寸变化时重建附件，避免每帧重复分配显存。
  // Recreates attachments only when pixel dimensions change.
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

bool ViewportRenderer::Render(const RenderScene& scene, int w, int h,
                              std::string* error) {
  CollectUnused();
  ImportedScene pending;
  for (const auto& draw : scene.meshes) pending.meshes.push_back(draw.mesh);
  if (!Prepare(pending, error)) return false;
  if (!Resize(w, h, error)) return false;
  draws_ = 0;
  glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
  glViewport(0, 0, width_, height_);
  // 与 ImGui 共用上下文，显式设置本次绘制所需状态，避免继承 UI 状态。
  // Shares the context with ImGui; sets render state rather than inheriting UI
  // state.
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_BLEND);
  // Shader 已编码视口输出；UI 直接合成，禁止 framebuffer 再做一次编码。
  // The shader encodes viewport output; disable a second encoding during UI.
  glDisable(GL_FRAMEBUFFER_SRGB);
  glBindSampler(0, 0);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glClearColor(0.055f, 0.071f, 0.084f, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glUseProgram(program_);
  const auto& vp = scene.view_projection;
  const auto identity = glm::mat4(1);
  glUniformMatrix4fv(glGetUniformLocation(program_, "uViewProjection"), 1,
                     GL_FALSE, glm::value_ptr(vp));
  if (scene.grid) {
    glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE,
                       glm::value_ptr(identity));
    glUniform3f(glGetUniformLocation(program_, "uColor"), .16f, .20f, .23f);
    glUniform1i(glGetUniformLocation(program_, "uUnlit"), 1);
    glBindVertexArray(grid_vao_);
    glDrawArrays(GL_LINES, 0, grid_count_);
    ++draws_;
  }
  glUniform1i(glGetUniformLocation(program_, "uUnlit"), 0);
  glDisable(GL_CULL_FACE);
  for (const auto& draw : scene.meshes) {
    const auto& gpu = *meshes_.at(draw.mesh->id);
    glUniformMatrix4fv(glGetUniformLocation(program_, "uModel"), 1, GL_FALSE,
                       glm::value_ptr(draw.model));
    for (std::size_t i = 0; i < gpu.primitives().size(); ++i) {
      const auto& primitive = gpu.primitives()[i];
      const auto& material = draw.mesh->primitives[i];
      // Tint 来自 sRGB 颜色选择器；glTF factor 本身已经在线性空间。
      // Tint is from an sRGB picker; the glTF factor is already linear.
      const auto color =
          glm::convertSRGBToLinear(draw.tint) * material.base_color;
      const auto& texture = material.base_color_texture;
      glUniform1i(glGetUniformLocation(program_, "uTextured"),
                  texture != nullptr);
      glUniform1i(glGetUniformLocation(program_, "uBaseColor"), 0);
      glBindTextureUnit(0, texture ? images_.at(texture->image->id)->id() : 0);
      glBindSampler(0, texture ? samplers_.at(texture->id)->id() : 0);
      glUniform3fv(glGetUniformLocation(program_, "uColor"), 1,
                   glm::value_ptr(color));
      glBindVertexArray(primitive.vao);
      glPolygonMode(GL_FRONT_AND_BACK, scene.wireframe ? GL_LINE : GL_FILL);
      glDrawElements(GL_TRIANGLES, primitive.count, GL_UNSIGNED_INT, nullptr);
      ++draws_;
      if (scene.selection_overlay && draw.selected && !scene.wireframe) {
        // 轻微前移线框深度，避免与填充三角形重叠闪烁。
        // Offsets selection lines to avoid z-fighting with filled triangles.
        glEnable(GL_POLYGON_OFFSET_LINE);
        glPolygonOffset(-1, -1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glUniform1i(glGetUniformLocation(program_, "uUnlit"), 1);
        glUniform3f(glGetUniformLocation(program_, "uColor"), .95f, .73f, .38f);
        glDrawElements(GL_TRIANGLES, primitive.count, GL_UNSIGNED_INT, nullptr);
        ++draws_;
        glDisable(GL_POLYGON_OFFSET_LINE);
        glUniform1i(glGetUniformLocation(program_, "uUnlit"), 0);
      }
    }
  }
  // 恢复填充模式与默认 framebuffer，供后续 ImGui 合成使用。
  // Restores fill mode and the default framebuffer for ImGui composition.
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glBindVertexArray(0);
  glUseProgram(0);
  glBindSampler(0, 0);
  glBindTextureUnit(0, 0);
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
  // 检查比背景更亮的像素；测试另行核对绘制资产和资源计数。
  // Checks brighter pixels; tests separately assert asset identity and counts.
  size_t colored{};
  for (size_t i = 0; i < pixels.size(); i += 4)
    if (pixels[i] > 70 && pixels[i + 1] > 40) ++colored;
  return colored > 100;
}
}  // namespace wf
