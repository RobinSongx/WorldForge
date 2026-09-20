// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "application/import_service.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "io/glb_importer.h"

namespace wf {
ImportService::ImportService(Reader reader)
    : reader_(std::move(reader)), worker_(&ImportService::Work, this) {}

ImportService::~ImportService() {
  {
    const std::lock_guard lock(mutex_);
    stopping_ = true;
  }
  wake_.notify_one();
  worker_.join();
}

bool ImportService::Start(std::string path, std::uint64_t generation) {
  const std::lock_guard lock(mutex_);
  if (busy_ || stopping_) return false;
  path_ = std::move(path);
  generation_ = generation;
  busy_ = true;
  pending_ = true;
  stage_ = ImportStage::kReading;
  wake_.notify_one();
  return true;
}

std::optional<ImportResult> ImportService::TakeResult() {
  const std::lock_guard lock(mutex_);
  if (!result_) return std::nullopt;
  auto result = std::move(result_);
  result_.reset();
  busy_ = false;
  stage_ = ImportStage::kIdle;
  return result;
}

ImportStage ImportService::stage() const {
  const std::lock_guard lock(mutex_);
  return stage_;
}

bool ImportService::busy() const {
  const std::lock_guard lock(mutex_);
  return busy_;
}

void ImportService::Work() {
  for (;;) {
    ImportResult result;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || pending_; });
      if (stopping_) return;
      result.path = path_;
      result.generation = generation_;
      pending_ = false;
    }
    std::vector<std::uint8_t> bytes;
    const auto read_start = std::chrono::steady_clock::now();
    const bool read = reader_(result.path, &bytes, &result.error);
    const auto parse_start = std::chrono::steady_clock::now();
    result.file_bytes = bytes.size();
    result.read_ms =
        std::chrono::duration<double, std::milli>(parse_start - read_start)
            .count();
    if (read) {
      {
        const std::lock_guard lock(mutex_);
        if (stopping_) return;
        stage_ = ImportStage::kParsing;
      }
      ImportGlb(bytes, &result.scene, &result.error);
    }
    result.parse_ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - parse_start)
                          .count();
    {
      const std::lock_guard lock(mutex_);
      if (stopping_) return;
      result_ = std::move(result);
      stage_ = ImportStage::kAwaitingUpload;
    }
  }
}
}  // namespace wf
