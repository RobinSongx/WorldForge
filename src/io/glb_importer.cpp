// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "io/glb_importer.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/quaternion.hpp"
// 上游 C API 头文件位于包含根目录，仅豁免此路径检查。
// The upstream C API header lives at the include root; waive its path only.
#include "domain/imported_scene.h"
#include "domain/mesh_asset.h"
#include "tiny_gltf_v3.h"  // NOLINT(build/include_subdir)

namespace wf {
namespace {
constexpr std::uint64_t kMaxElements = 8'000'000;
constexpr std::size_t kMaxUploadBytes = 256 * 1024 * 1024;

std::string Text(tg3_str value) {
  return value.data ? std::string(value.data, value.len) : std::string{};
}

bool Fail(const std::string& message, std::string* error) {
  *error = message;
  return false;
}

// v3 alpha 的容器读取较宽松；适配层额外校验 chunk 顺序、重复和对齐。
// The v3 alpha reader is permissive; also validate chunk order/alignment.
bool ValidateContainer(std::span<const std::uint8_t> bytes,
                       std::string* error) {
  std::size_t offset = 12;
  unsigned int chunk_index = 0;
  bool binary_seen = false;
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 8)
      return Fail("Incomplete GLB chunk header", error);
    std::uint32_t length = 0;
    std::uint32_t type = 0;
    std::memcpy(&length, bytes.data() + offset, 4);
    std::memcpy(&type, bytes.data() + offset + 4, 4);
    offset += 8;
    if (length % 4 != 0 || length > bytes.size() - offset)
      return Fail("Invalid GLB chunk alignment or length", error);
    if ((chunk_index == 0 && type != 0x4e4f534a) ||
        (chunk_index != 0 && type == 0x4e4f534a))
      return Fail("GLB must begin with exactly one JSON chunk", error);
    if (type == 0x004e4942) {
      if (binary_seen || chunk_index != 1)
        return Fail("GLB BIN must be the second chunk and occur once", error);
      binary_seen = true;
    }
    offset += length;
    ++chunk_index;
  }
  return true;
}

// 错误消息先复制到自有字符串，随后统一释放解析 arena 和错误栈。
// Copies diagnostics before releasing the parser arena and error stack.
class ParsedModel {
 public:
  ParsedModel() { tg3_error_stack_init(&errors_); }

  ParsedModel(const ParsedModel&) = delete;
  ParsedModel& operator=(const ParsedModel&) = delete;

  ~ParsedModel() {
    tg3_error_stack_free(&errors_);
    tg3_model_free(&model_);
  }

  bool Parse(std::span<const std::uint8_t> bytes, std::string* error) {
    tg3_parse_options options;
    tg3_parse_options_init(&options);
    options.strictness = TG3_STRICT;
    options.images_as_is = 1;
    options.borrow_input_buffers = 1;
    options.memory.memory_budget = kMaxGlbBytes;
    options.memory.max_single_alloc = kMaxGlbBytes;
    const auto result = tg3_parse_glb(&model_, &errors_, bytes.data(),
                                      bytes.size(), nullptr, 0, &options);
    if (result == TG3_OK && !errors_.has_error) return true;
    *error = "GLB parse failed";
    for (std::uint32_t i = 0; i < errors_.count; ++i) {
      if (errors_.entries[i].message) {
        *error += ": ";
        *error += errors_.entries[i].message;
        break;
      }
    }
    return false;
  }

  const tg3_model& model() const { return model_; }

 private:
  tg3_model model_{};
  tg3_error_stack errors_{};
};

bool ValidIndex(int index, std::uint32_t count) {
  return index >= 0 && static_cast<std::uint32_t>(index) < count;
}

bool Finite(const glm::vec3& value) {
  return std::isfinite(value.x) && std::isfinite(value.y) &&
         std::isfinite(value.z);
}

bool ValidMatrix(const glm::mat4& matrix) {
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      if (!std::isfinite(matrix[col][row])) return false;
  if (matrix[0][3] != 0 || matrix[1][3] != 0 || matrix[2][3] != 0 ||
      matrix[3][3] != 1)
    return false;
  const float determinant = glm::determinant(matrix);
  if (!std::isfinite(determinant) || determinant == 0) return false;
  const auto inverse = glm::inverse(matrix);
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      if (!std::isfinite(inverse[col][row])) return false;
  return true;
}

struct AccessorView {
  const std::uint8_t* data = nullptr;
  std::uint64_t count = 0;
  std::uint64_t stride = 0;
  int component = 0;
  int component_size = 0;
  bool normalized = false;

  float ReadFloat(std::uint64_t index, int channel) const {
    const auto* value = data + index * stride + channel * component_size;
    if (component == TG3_COMPONENT_TYPE_FLOAT) {
      float result = 0;
      std::memcpy(&result, value, sizeof(result));
      return result;
    }
    if (component == TG3_COMPONENT_TYPE_UNSIGNED_BYTE)
      return static_cast<float>(*value) / 255.0f;
    std::uint16_t result = 0;
    std::memcpy(&result, value, sizeof(result));
    return static_cast<float>(result) / 65535.0f;
  }

  std::uint32_t ReadIndex(std::uint64_t index) const {
    const auto* value = data + index * stride;
    if (component_size == 1) return *value;
    if (component_size == 2) {
      std::uint16_t result = 0;
      std::memcpy(&result, value, sizeof(result));
      return result;
    }
    std::uint32_t result = 0;
    std::memcpy(&result, value, sizeof(result));
    return result;
  }
};

bool ReadAccessor(const tg3_model& model, int index, int type,
                  AccessorView* output, std::string* error) {
  if (!ValidIndex(index, model.accessors_count))
    return Fail("Missing or invalid accessor index", error);
  const auto& accessor = model.accessors[index];
  if (accessor.sparse.is_sparse || accessor.type != type ||
      accessor.count == 0 || accessor.count > kMaxElements ||
      !ValidIndex(accessor.buffer_view, model.buffer_views_count))
    return Fail("Unsupported accessor type, sparse data, or element count",
                error);
  const auto& view = model.buffer_views[accessor.buffer_view];
  if (!ValidIndex(view.buffer, model.buffers_count))
    return Fail("Invalid buffer index", error);
  const auto& buffer = model.buffers[view.buffer];
  const int component_size = tg3_component_size(accessor.component_type);
  const int components = type == TG3_TYPE_SCALAR ? 1 : type;
  if (component_size < 1 || components < 1 || components > 4)
    return Fail("Invalid accessor component type", error);
  const auto element = static_cast<std::uint64_t>(component_size * components);
  const std::uint64_t stride = view.byte_stride ? view.byte_stride : element;
  // 先用减法校验区间，避免不可信 offset/count 的乘加溢出。
  // Checks ranges by subtraction before multiplying untrusted offsets/counts.
  if (!buffer.data.data || view.byte_offset > buffer.byte_length ||
      view.byte_length > buffer.byte_length - view.byte_offset ||
      buffer.byte_length > buffer.data.count ||
      accessor.byte_offset > view.byte_length ||
      element > view.byte_length - accessor.byte_offset || stride < element ||
      (view.byte_stride && (stride > 252 || stride % 4 != 0)) ||
      stride % component_size != 0 ||
      accessor.byte_offset % component_size != 0 ||
      view.byte_offset % component_size != 0 ||
      accessor.count - 1 >
          (view.byte_length - accessor.byte_offset - element) / stride)
    return Fail("Accessor offset, stride or range is invalid", error);
  output->data = buffer.data.data + view.byte_offset + accessor.byte_offset;
  output->count = accessor.count;
  output->stride = stride;
  output->component = accessor.component_type;
  output->component_size = component_size;
  output->normalized = accessor.normalized != 0;
  return true;
}

int Attribute(const tg3_primitive& primitive, const char* name) {
  for (std::uint32_t i = 0; i < primitive.attributes_count; ++i)
    if (Text(primitive.attributes[i].key) == name)
      return primitive.attributes[i].value;
  return -1;
}

bool ValidateFeatures(const tg3_model& model, ImportedScene* scene,
                      std::string* error) {
  if (Text(model.asset.version) != "2.0" ||
      (!Text(model.asset.min_version).empty() &&
       Text(model.asset.min_version) != "2.0"))
    return Fail("Only glTF 2.0 is supported", error);
  if (model.extensions_required_count)
    return Fail(
        "Unsupported required extension: " + Text(model.extensions_required[0]),
        error);
  if (model.animations_count || model.skins_count)
    return Fail("Animations and skins are not supported", error);
  if (!model.scenes_count || model.nodes_count > 100000 ||
      model.meshes_count > 100000)
    return Fail("Missing scene or scene complexity limit exceeded", error);
  for (std::uint32_t i = 0; i < model.accessors_count; ++i)
    if (model.accessors[i].sparse.is_sparse)
      return Fail("Sparse accessors are not supported", error);
  for (std::uint32_t i = 0; i < model.buffer_views_count; ++i)
    if (model.buffer_views[i].ext.extensions_count)
      return Fail("Buffer-view extensions/compression are not supported",
                  error);
  for (std::uint32_t i = 0; i < model.buffers_count; ++i)
    if (model.buffers[i].uri.len)
      return Fail("GLB buffer URIs are not supported", error);
  for (std::uint32_t i = 0; i < model.images_count; ++i)
    if (model.images[i].uri.len)
      return Fail("Image URIs are not supported; embed images in GLB", error);
  for (std::uint32_t i = 0; i < model.materials_count; ++i) {
    const auto& material = model.materials[i];
    const auto mode = Text(material.alpha_mode);
    if (!mode.empty() && mode != "OPAQUE")
      return Fail("Transparent/masked materials are not supported", error);
    for (double value : material.pbr_metallic_roughness.base_color_factor)
      if (!std::isfinite(value) || value < 0 || value > 1)
        return Fail("Invalid material baseColorFactor", error);
  }
  if (model.materials_count || model.textures_count)
    scene->warnings.push_back(
        "Preview uses baseColorFactor only; textures, PBR and material "
        "extensions are not displayed. Surfaces are shown two-sided.");
  if (model.cameras_count || model.lights_count)
    scene->warnings.push_back("Source cameras/lights are not used in preview.");
  if (model.extensions_used_count)
    scene->warnings.push_back("Optional appearance extensions are ignored.");
  return true;
}

bool ReadPrimitive(const tg3_model& model, const tg3_primitive& source,
                   MeshPrimitive* primitive, ImportedScene* scene,
                   std::string* error) {
  if ((source.mode != -1 && source.mode != TG3_MODE_TRIANGLES) ||
      source.targets_count || source.ext.extensions_count)
    return Fail("Only uncompressed static TRIANGLES are supported", error);
  AccessorView positions;
  if (!ReadAccessor(model, Attribute(source, "POSITION"), TG3_TYPE_VEC3,
                    &positions, error))
    return false;
  if (positions.component != TG3_COMPONENT_TYPE_FLOAT || positions.normalized)
    return Fail("POSITION must be non-normalized FLOAT VEC3", error);
  AccessorView normals;
  const int normal_index = Attribute(source, "NORMAL");
  if (normal_index != -1) {
    if (!ReadAccessor(model, normal_index, TG3_TYPE_VEC3, &normals, error))
      return false;
    if (normals.component != TG3_COMPONENT_TYPE_FLOAT || normals.normalized ||
        normals.count != positions.count)
      return Fail("NORMAL must be FLOAT VEC3 matching POSITION count", error);
  }
  AccessorView uv;
  const int uv_index = Attribute(source, "TEXCOORD_0");
  if (uv_index != -1) {
    if (!ReadAccessor(model, uv_index, TG3_TYPE_VEC2, &uv, error)) return false;
    const bool supported =
        (uv.component == TG3_COMPONENT_TYPE_FLOAT && !uv.normalized) ||
        ((uv.component == TG3_COMPONENT_TYPE_UNSIGNED_BYTE ||
          uv.component == TG3_COMPONENT_TYPE_UNSIGNED_SHORT) &&
         uv.normalized);
    if (!supported || uv.count != positions.count)
      return Fail("Unsupported TEXCOORD_0 type or mismatched count", error);
  }
  for (std::uint32_t i = 0; i < source.attributes_count; ++i) {
    const auto name = Text(source.attributes[i].key);
    if (name != "POSITION" && name != "NORMAL" && name != "TEXCOORD_0") {
      scene->warnings.push_back("Attribute ignored in preview: " + name);
    }
  }
  AccessorView indices;
  if (source.indices != -1) {
    if (!ReadAccessor(model, source.indices, TG3_TYPE_SCALAR, &indices, error))
      return false;
    if (indices.normalized ||
        (indices.component != TG3_COMPONENT_TYPE_UNSIGNED_BYTE &&
         indices.component != TG3_COMPONENT_TYPE_UNSIGNED_SHORT &&
         indices.component != TG3_COMPONENT_TYPE_UNSIGNED_INT) ||
        model.buffer_views[model.accessors[source.indices].buffer_view]
            .byte_stride)
      return Fail("Indices must be packed unsigned integers", error);
  }
  const auto count = source.indices == -1 ? positions.count : indices.count;
  if (count % 3 != 0)
    return Fail("Triangle index count must be divisible by 3", error);
  const auto vertex_count = normal_index == -1 ? count : positions.count;
  const auto bytes =
      vertex_count * sizeof(MeshVertex) + count * sizeof(std::uint32_t);
  if (bytes > kMaxUploadBytes - scene->upload_bytes)
    return Fail("Decoded geometry exceeds 256 MiB preview limit", error);
  primitive->vertices.resize(static_cast<std::size_t>(positions.count));
  for (std::uint64_t i = 0; i < positions.count; ++i) {
    auto& vertex = primitive->vertices[static_cast<std::size_t>(i)];
    for (int channel = 0; channel < 3; ++channel) {
      vertex.position[channel] = positions.ReadFloat(i, channel);
      if (normal_index != -1)
        vertex.normal[channel] = normals.ReadFloat(i, channel);
    }
    if (uv_index != -1) {
      for (int channel = 0; channel < 2; ++channel)
        vertex.uv[channel] = uv.ReadFloat(i, channel);
    }
    if (!Finite(vertex.position) || !Finite(vertex.normal) ||
        !std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y) ||
        !std::isfinite(glm::length(vertex.normal)) ||
        glm::length(vertex.normal) == 0)
      return Fail("Non-finite vertex or invalid normal", error);
    vertex.normal = glm::normalize(vertex.normal);
    primitive->bounds.Include(vertex.position);
  }
  primitive->indices.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t i = 0; i < count; ++i) {
    const auto index = source.indices == -1 ? static_cast<std::uint32_t>(i)
                                            : indices.ReadIndex(i);
    if (index >= positions.count)
      return Fail("Triangle index exceeds vertex count", error);
    primitive->indices.push_back(index);
  }
  std::vector<MeshVertex> flat;
  if (normal_index == -1) flat.reserve(static_cast<std::size_t>(count));
  for (std::size_t i = 0; i < primitive->indices.size(); i += 3) {
    const auto& a = primitive->vertices[primitive->indices[i]];
    const auto& b = primitive->vertices[primitive->indices[i + 1]];
    const auto& c = primitive->vertices[primitive->indices[i + 2]];
    const auto cross =
        glm::cross(b.position - a.position, c.position - a.position);
    const float length = glm::length(cross);
    if (!std::isfinite(length) || length == 0)
      return Fail("Degenerate triangle or geometry outside numeric range",
                  error);
    if (normal_index == -1) {
      for (const auto* original : {&a, &b, &c}) {
        auto vertex = *original;
        vertex.normal = cross / length;
        flat.push_back(vertex);
      }
    }
  }
  if (normal_index == -1) {
    primitive->vertices = std::move(flat);
    for (std::size_t i = 0; i < primitive->indices.size(); ++i)
      primitive->indices[i] = static_cast<std::uint32_t>(i);
    scene->warnings.push_back(
        "Missing NORMAL: generated temporary flat normals.");
  }
  if (source.material != -1) {
    if (!ValidIndex(source.material, model.materials_count))
      return Fail("Invalid material reference", error);
    const auto& color = model.materials[source.material]
                            .pbr_metallic_roughness.base_color_factor;
    primitive->base_color = {color[0], color[1], color[2]};
  }
  scene->vertices += primitive->vertices.size();
  scene->triangles += primitive->indices.size() / 3;
  scene->upload_bytes += static_cast<std::size_t>(bytes);
  return true;
}

bool ReadMeshes(const tg3_model& model, ImportedScene* scene,
                std::string* error) {
  for (std::uint32_t i = 0; i < model.meshes_count; ++i) {
    const auto& source = model.meshes[i];
    if (source.weights_count || !source.primitives_count)
      return Fail("Morph weights or empty mesh are not supported", error);
    auto mesh = std::make_shared<MeshAsset>();
    mesh->id = NextAssetId();
    for (std::uint32_t j = 0; j < source.primitives_count; ++j) {
      MeshPrimitive primitive;
      if (!ReadPrimitive(model, source.primitives[j], &primitive, scene,
                         error)) {
        *error = "Mesh " + std::to_string(i) + ", primitive " +
                 std::to_string(j) + ": " + *error;
        return false;
      }
      mesh->bounds.Include(primitive.bounds);
      mesh->primitives.push_back(std::move(primitive));
    }
    scene->meshes.push_back(std::move(mesh));
  }
  return true;
}

bool ReadNodes(const tg3_model& model, ImportedScene* scene,
               std::string* error) {
  scene->nodes.resize(model.nodes_count);
  std::vector<int> parents(model.nodes_count, -1);
  for (std::uint32_t i = 0; i < model.nodes_count; ++i) {
    const auto& source = model.nodes[i];
    auto& node = scene->nodes[i];
    if (source.skin != -1 || source.weights_count ||
        source.ext.extensions_count)
      return Fail("Skinned/morph nodes or node extensions are not supported",
                  error);
    if (source.mesh != -1 && !ValidIndex(source.mesh, model.meshes_count))
      return Fail("Invalid node mesh reference", error);
    node.name = Text(source.name);
    if (node.name.empty()) node.name = "Node " + std::to_string(i);
    node.mesh = source.mesh;
    if (source.has_matrix) {
      for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
          node.local[col][row] =
              static_cast<float>(source.matrix[col * 4 + row]);
    } else {
      const glm::vec3 position{source.translation[0], source.translation[1],
                               source.translation[2]};
      const glm::vec3 scale{source.scale[0], source.scale[1], source.scale[2]};
      const glm::quat rotation{static_cast<float>(source.rotation[3]),
                               static_cast<float>(source.rotation[0]),
                               static_cast<float>(source.rotation[1]),
                               static_cast<float>(source.rotation[2])};
      const float length = glm::length(rotation);
      if (!std::isfinite(length) || std::abs(length - 1) > 0.001f)
        return Fail("Node quaternion must be normalized", error);
      node.local = glm::translate(glm::mat4(1), position) *
                   glm::mat4_cast(glm::normalize(rotation)) *
                   glm::scale(glm::mat4(1), scale);
    }
    if (!ValidMatrix(node.local))
      return Fail("Non-finite, non-affine or singular node transform", error);
    for (std::uint32_t j = 0; j < source.children_count; ++j) {
      const int child = source.children[j];
      if (!ValidIndex(child, model.nodes_count) || parents[child] != -1)
        return Fail("Invalid child index or multiple node parents", error);
      parents[child] = static_cast<int>(i);
      node.children.push_back(child);
    }
  }
  // 拓扑遍历校验整份节点图；不递归，避免深层文件耗尽线程栈。
  // Validates the entire graph topologically without recursive stack growth.
  std::vector<int> order;
  for (std::uint32_t i = 0; i < model.nodes_count; ++i)
    if (parents[i] == -1) order.push_back(static_cast<int>(i));
  for (std::size_t i = 0; i < order.size(); ++i) {
    const auto& children = scene->nodes[order[i]].children;
    order.insert(order.end(), children.begin(), children.end());
  }
  if (order.size() != model.nodes_count)
    return Fail("Node graph contains a cycle", error);
  const int scene_index = model.default_scene == -1 ? 0 : model.default_scene;
  if (!ValidIndex(scene_index, model.scenes_count))
    return Fail("Invalid default scene", error);
  const auto& source_scene = model.scenes[scene_index];
  std::vector<int> active;
  for (std::uint32_t i = 0; i < source_scene.nodes_count; ++i) {
    const int root = source_scene.nodes[i];
    if (!ValidIndex(root, model.nodes_count) || parents[root] != -1 ||
        scene->nodes[root].active)
      return Fail("Invalid or duplicate scene root", error);
    scene->roots.push_back(root);
    scene->nodes[root].active = true;
    active.push_back(root);
  }
  for (std::size_t i = 0; i < active.size(); ++i) {
    auto& node = scene->nodes[active[i]];
    node.world = parents[active[i]] == -1
                     ? node.local
                     : scene->nodes[parents[active[i]]].world * node.local;
    if (!ValidMatrix(node.world))
      return Fail("Invalid accumulated node transform", error);
    if (node.mesh != -1) {
      const auto bounds =
          scene->meshes[node.mesh]->bounds.Transformed(node.world);
      if (!Finite(bounds.min) || !Finite(bounds.max) ||
          !std::isfinite(glm::length(bounds.max - bounds.min)))
        return Fail("World bounds exceed numeric range", error);
      scene->bounds.Include(bounds);
    }
    for (int child : node.children) {
      scene->nodes[child].active = true;
      active.push_back(child);
    }
  }
  if (!scene->bounds.valid)
    return Fail("Selected scene contains no triangle geometry", error);
  return true;
}
}  // namespace

bool ImportGlb(std::span<const std::uint8_t> bytes,
               std::shared_ptr<const ImportedScene>* output,
               std::string* error) {
  if (bytes.size() < 20 || bytes.size() > kMaxGlbBytes)
    return Fail("GLB must be between 20 bytes and 256 MiB", error);
  if (!ValidateContainer(bytes, error)) return false;
  ParsedModel parsed;
  if (!parsed.Parse(bytes, error)) return false;
  auto scene = std::make_shared<ImportedScene>();
  if (!ValidateFeatures(parsed.model(), scene.get(), error) ||
      !ReadMeshes(parsed.model(), scene.get(), error) ||
      !ReadNodes(parsed.model(), scene.get(), error))
    return false;
  *output = std::move(scene);
  error->clear();
  return true;
}
}  // namespace wf
