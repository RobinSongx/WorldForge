// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_STUDIO_STUDIO_APP_H_
#define WORLDFORGE_SRC_STUDIO_STUDIO_APP_H_

#include <string>

namespace wf {

// Output is a UTF-8 directory path used only by smoke checks.
// Runs until the window closes, or until the smoke checks finish.
// Returns 0 on success, 1 on initialization/I/O failure, or 2 on smoke failure.
// On failure, writes a diagnostic to the non-null error output.
int RunStudio(bool smoke_test, const std::string& output, std::string* error);

}  // namespace wf

#endif  // WORLDFORGE_SRC_STUDIO_STUDIO_APP_H_
