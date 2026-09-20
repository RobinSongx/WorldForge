// Copyright 2026 WorldForge contributors
// SPDX-License-Identifier: MIT

#ifndef WORLDFORGE_SRC_STUDIO_STUDIO_APP_H_
#define WORLDFORGE_SRC_STUDIO_STUDIO_APP_H_

#include <string>

namespace wf {

// 运行到窗口关闭或冒烟检查完成；output 是测试使用的 UTF-8 目录。
// 返回 0 表示成功，1 表示初始化、渲染或 I/O 失败，2 表示冒烟检查失败。
// error 必须非空，失败时接收诊断。
// Runs until close or smoke completion; output is a UTF-8 test directory.
// Returns 0 on success, 1 on setup/render/I/O failure, 2 on smoke failure.
// Requires a non-null error output for failure diagnostics.
// mesh_path 非空时，冒烟测试通过真实后台链路导入指定 GLB。
// A nonempty mesh_path imports a GLB through the real background smoke path.
int RunStudio(bool smoke_test, const std::string& output, std::string* error,
              const std::string& mesh_path = {});

}  // namespace wf

#endif  // WORLDFORGE_SRC_STUDIO_STUDIO_APP_H_
