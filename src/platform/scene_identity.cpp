// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "platform/scene_identity.h"

#include <windows.h>

// Windows SDK 要求先定义 Win32 类型。
// Windows SDK requires Win32 types before the CNG declarations.
#include <bcrypt.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>

namespace wf {
namespace {
std::string Hex(std::span<const std::uint8_t> bytes) {
  constexpr char kDigits[] = "0123456789abcdef";
  std::string text;
  for (auto b : bytes) {
    text += kDigits[b >> 4];
    text += kDigits[b & 15];
  }
  return text;
}
}  // namespace

std::string GenerateUuid() {
  std::array<std::uint8_t, 16> bytes{};
  if (BCryptGenRandom(nullptr, bytes.data(), static_cast<ULONG>(bytes.size()),
                      BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
    return {};
  bytes[6] = (bytes[6] & 15) | 64;
  bytes[8] = (bytes[8] & 63) | 128;
  const auto text = Hex(bytes);
  return text.substr(0, 8) + "-" + text.substr(8, 4) + "-" +
         text.substr(12, 4) + "-" + text.substr(16, 4) + "-" + text.substr(20);
}

std::string Sha256(std::span<const std::uint8_t> bytes) {
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr,
                                  0) < 0)
    return {};
  BCRYPT_HASH_HANDLE hash = nullptr;
  auto status = BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
  std::array<std::uint8_t, 32> result{};
  if (status >= 0) {
    status = BCryptHashData(hash, const_cast<PUCHAR>(bytes.data()),
                            static_cast<ULONG>(bytes.size()), 0);
    if (status >= 0)
      status = BCryptFinishHash(hash, result.data(),
                                static_cast<ULONG>(result.size()), 0);
    BCryptDestroyHash(hash);
  }
  BCryptCloseAlgorithmProvider(algorithm, 0);
  return status >= 0 ? Hex(result) : std::string{};
}
}  // namespace wf
