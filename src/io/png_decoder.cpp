// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "io/png_decoder.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <utility>

// 上游 C 接口头位于包含根目录；仅豁免路径检查。
// The upstream C API header is at the include root; waive its path only.
#include "domain/mesh_asset.h"
#include "lodepng.h"  // NOLINT(build/include_subdir)

namespace {
// 解码器所有临时分配共享线程内硬上限，限制畸形压缩流的膨胀。
// All decoder allocations share a per-thread cap, bounding malformed streams.
constexpr std::size_t kDecoderBudget = 192 * 1024 * 1024;
thread_local std::size_t allocated_bytes = 0;

struct alignas(std::max_align_t) Allocation {
  std::size_t size;
};
}  // namespace

// 上游替换分配器要求这些精确符号名；内存必须在分配线程释放。
// Upstream allocator hooks require these exact names; free on the same thread.
void* lodepng_malloc(std::size_t size) {
  if (allocated_bytes > kDecoderBudget - sizeof(Allocation) ||
      size > kDecoderBudget - allocated_bytes - sizeof(Allocation))
    return nullptr;
  auto* block =
      static_cast<Allocation*>(std::malloc(sizeof(Allocation) + size));
  if (!block) return nullptr;
  block->size = size;
  allocated_bytes += size + sizeof(Allocation);
  return block + 1;
}

void lodepng_free(void* pointer) {
  if (!pointer) return;
  auto* block = static_cast<Allocation*>(pointer) - 1;
  allocated_bytes -= block->size + sizeof(Allocation);
  std::free(block);
}

void* lodepng_realloc(void* pointer, std::size_t size) {
  if (!pointer) return lodepng_malloc(size);
  if (!size) {
    lodepng_free(pointer);
    return nullptr;
  }
  auto* block = static_cast<Allocation*>(pointer) - 1;
  const auto previous = block->size;
  if (size > kDecoderBudget - (allocated_bytes - previous)) return nullptr;
  auto* resized =
      static_cast<Allocation*>(std::realloc(block, sizeof(Allocation) + size));
  if (!resized) return nullptr;
  resized->size = size;
  allocated_bytes = allocated_bytes - previous + size;
  return resized + 1;
}

namespace wf {
namespace {
std::uint32_t ReadBigEndian(const std::uint8_t* value) {
  return (static_cast<std::uint32_t>(value[0]) << 24) |
         (static_cast<std::uint32_t>(value[1]) << 16) |
         (static_cast<std::uint32_t>(value[2]) << 8) | value[3];
}

// 严格校验所有 chunk 的区间与 CRC，包括解码器忽略的元数据。
// Checks all chunk ranges and CRCs, including decoder-ignored metadata.
bool ValidateChunks(std::span<const std::uint8_t> bytes) {
  constexpr std::uint8_t kSignature[] = {137, 80, 78, 71, 13, 10, 26, 10};
  if (bytes.size() < 8 || std::memcmp(bytes.data(), kSignature, 8))
    return false;
  std::size_t offset = 8;
  bool data_seen = false;
  while (offset < bytes.size()) {
    if (bytes.size() - offset < 12) return false;
    const auto length = ReadBigEndian(bytes.data() + offset);
    if (length > bytes.size() - offset - 12 || length > 0x7fffffff)
      return false;
    const auto* type = bytes.data() + offset + 4;
    if (lodepng_crc32(type, static_cast<std::size_t>(length) + 4) !=
        ReadBigEndian(type + 4 + length))
      return false;
    if (!std::memcmp(type, "IHDR", 4) && offset != 8) return false;
    if (offset == 8 && std::memcmp(type, "IHDR", 4)) return false;
    if (!std::memcmp(type, "IDAT", 4)) data_seen = true;
    offset += length + 12;
    if (!std::memcmp(type, "IEND", 4))
      return length == 0 && data_seen && offset == bytes.size();
  }
  return false;
}

class PngState {
 public:
  PngState() { lodepng_state_init(&state_); }

  PngState(const PngState&) = delete;

  PngState& operator=(const PngState&) = delete;

  ~PngState() { lodepng_state_cleanup(&state_); }

  LodePNGState* get() { return &state_; }

 private:
  LodePNGState state_{};
};
}  // namespace

bool DecodePng(std::span<const std::uint8_t> bytes, std::size_t remaining,
               TextureImage* output, std::string* error) {
  if (!ValidateChunks(bytes)) {
    *error = "Invalid PNG signature, chunk range, CRC or IEND";
    return false;
  }
  PngState state;
  unsigned int width = 0;
  unsigned int height = 0;
  auto code =
      lodepng_inspect(&width, &height, state.get(), bytes.data(), bytes.size());
  if (code) {
    *error = std::string("Invalid PNG header: ") + lodepng_error_text(code);
    return false;
  }
  // 4096² RGBA8 = 64 MiB；先做宽整数运算再分配，支持常见 PNG 色型。
  // 4096 squared RGBA8 is 64 MiB; use wide arithmetic before allocation.
  const auto size = static_cast<std::uint64_t>(width) * height * 4;
  if (!width || !height || width > 4096 || height > 4096 ||
      size > kMaxImageBytes || size > remaining) {
    *error = "PNG exceeds 4096 dimension, 64 MiB image or 128 MiB scene limit";
    return false;
  }
  unsigned char* raw = nullptr;
  code = lodepng_decode(&raw, &width, &height, state.get(), bytes.data(),
                        bytes.size());
  std::unique_ptr<unsigned char, decltype(&lodepng_free)> decoded(raw,
                                                                  lodepng_free);
  if (code) {
    *error = std::string("PNG decode failed: ") + lodepng_error_text(code);
    return false;
  }
  TextureImage image;
  image.id = NextAssetId();
  image.width = width;
  image.height = height;
  image.rgba.assign(raw, raw + static_cast<std::size_t>(size));
  *output = std::move(image);
  return true;
}
}  // namespace wf
