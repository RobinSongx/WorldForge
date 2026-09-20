// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#ifndef WORLDFORGE_SRC_APPLICATION_IMPORT_SERVICE_H_
#define WORLDFORGE_SRC_APPLICATION_IMPORT_SERVICE_H_
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "domain/imported_scene.h"

namespace wf {
enum class ImportStage { kIdle, kReading, kParsing, kAwaitingUpload };

struct ImportResult {
  std::uint64_t generation = 0;
  std::string path;
  std::shared_ptr<const ImportedScene> scene;
  std::string error;
  std::size_t file_bytes = 0;
  double read_ms = 0;
  double parse_ms = 0;
};

// 单工作线程只读取和转换 CPU 数据；析构等待任务结束，不接触 GL。
// A single worker reads/converts CPU data; destruction joins it without GL.
class ImportService {
 public:
  using Reader = std::function<bool(const std::string&,
                                    std::vector<std::uint8_t>*, std::string*)>;
  explicit ImportService(Reader reader);

  ImportService(const ImportService&) = delete;

  ImportService& operator=(const ImportService&) = delete;

  ~ImportService();

  bool Start(std::string path, std::uint64_t generation);

  std::optional<ImportResult> TakeResult();

  ImportStage stage() const;
  bool busy() const;

 private:
  void Work();

  Reader reader_;
  mutable std::mutex mutex_;
  std::condition_variable wake_;
  bool stopping_ = false;
  bool pending_ = false;
  bool busy_ = false;
  ImportStage stage_ = ImportStage::kIdle;
  std::string path_;
  std::uint64_t generation_ = 0;
  std::optional<ImportResult> result_;
  std::thread worker_;
};
}  // namespace wf
#endif  // WORLDFORGE_SRC_APPLICATION_IMPORT_SERVICE_H_
