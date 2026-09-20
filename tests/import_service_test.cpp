// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT
#include "application/import_service.h"

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "editor/editor_state.h"

namespace {
void Require(bool value, const char* message) {
  if (!value) {
    std::cerr << message << '\n';
    std::exit(1);
  }
}

wf::ImportResult Wait(wf::ImportService& service) {
  const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < limit) {
    if (auto result = service.TakeResult()) return std::move(*result);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  Require(false, "Import worker timed out");
  return {};
}
}  // namespace

int main() {
  std::ifstream file(std::string(WF_FIXTURE_DIR) + "/indices_5123.glb",
                     std::ios::binary);
  Require(file.good(), "Missing fixture");
  const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),
                                        std::istreambuf_iterator<char>()};
  std::promise<void> entered;
  auto entered_future = entered.get_future();
  std::promise<void> release;
  auto gate = release.get_future().share();
  bool first = true;
  wf::EditorState editor;
  wf::ImportService service([&](const std::string& path,
                                std::vector<std::uint8_t>* output,
                                std::string* error) {
    if (first) {
      first = false;
      entered.set_value();
      gate.wait();
    }
    if (path == "missing") {
      *error = "File not found";
      return false;
    }
    *output = path == "invalid" ? std::vector<std::uint8_t>{1, 2} : bytes;
    return true;
  });
  const auto old_generation = editor.generation();
  Require(service.Start("valid", old_generation), "Start failed");
  Require(entered_future.wait_for(std::chrono::seconds(5)) ==
              std::future_status::ready,
          "Reader not entered");
  Require(service.busy() && service.stage() == wf::ImportStage::kReading,
          "Reading stage missing");
  Require(!service.Start("duplicate", old_generation),
          "Concurrent start accepted");
  editor.NewScene();
  release.set_value();
  auto stale = Wait(service);
  Require(stale.scene && stale.generation != editor.generation(),
          "New scene must invalidate old generation");
  Require(editor.entities().size() == 1, "Worker mutated editor");
  stale.scene.reset();
  Require(service.Start("valid", editor.generation()), "Restart failed");
  auto valid = Wait(service);
  Require(
      valid.scene && valid.error.empty() && valid.file_bytes == bytes.size(),
      "Successful import result missing");
  Require(valid.read_ms >= 0 && valid.parse_ms >= 0, "Invalid stage metrics");
  for (const char* path : {"missing", "invalid"}) {
    Require(service.Start(path, editor.generation()), "Failure test start");
    const auto failed = Wait(service);
    Require(!failed.scene && !failed.error.empty() && !service.busy(),
            "Failure must reset busy state and retain diagnostics");
  }
  // 析构必须等待已进入 reader 的工作完成，不能让工作线程悬空。
  // Destruction joins an active reader instead of leaving a dangling worker.
  std::promise<void> shutdown_entered;
  auto shutdown_ready = shutdown_entered.get_future();
  {
    wf::ImportService closing([&](const std::string&,
                                  std::vector<std::uint8_t>* output,
                                  std::string*) {
      shutdown_entered.set_value();
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      *output = bytes;
      return true;
    });
    Require(closing.Start("valid", 1), "Shutdown start");
    Require(shutdown_ready.wait_for(std::chrono::seconds(5)) ==
                std::future_status::ready,
            "Shutdown reader not entered");
  }
  std::cout << "Import service: busy, generation, failure, restart and "
               "shutdown passed\n";
  return 0;
}
