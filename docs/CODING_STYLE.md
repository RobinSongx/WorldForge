# C++ 编码规范

本仓库自有 C++ 代码**严格遵循 [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)**。
这同时约束格式、命名、头文件、接口、资源管理和语言特性，不能只以格式化通过作为合规证明。
本次对照日期：2026-09-19；语言标准为 C++20。

## 适用范围

适用于 apps/、src/、tests/ 及以后加入的自有 C++ 代码。第三方依赖、自动生成文件和构建产物保留上游规范，不对其批量改写；嵌入的 GLSL 依照其语言规则保持可读。

## 必须遵守

- 文件使用 lowercase_snake_case.cc / .h；类型使用 UpperCamelCase，普通函数使用 UpperCamelCase，访问器可使用 snake_case。
- 变量和参数使用 snake_case；class 数据成员为 private 且带尾部下划线；struct 仅承载无不变量的被动数据；常量使用 kUpperCamelCase。
- 两空格缩进，默认 80 列；具体格式以固定版本 clang-format 的 Google preset 为准。一行一条语句，一条声明一个变量。
- 头文件自包含，使用 WORLDFORGE_<仓库相对路径>_H_ include guard，不使用 pragma once。
- 直接包含所用声明的头文件，顺序为对应头、C 系统头、C++ 标准库、其他库、项目头；各组按字母排序。
- 不写 throw / try / catch，不使用 RTTI 或规范禁止的标准库功能（例如 filesystem）。当前使用 SDL 的 UTF-8 文件 API。
- 可失败操作显式返回状态和诊断；构造函数保持简单。GL、SDL、ImGui 资源由明确的所有者管理，失败路径也必须释放。
- 函数保持单一职责，长流程拆为可读的子函数；注释解释接口约束、单位、所有权和原因。
- Windows 工具链可以保留 STL 所需的异常编译选项；这不授权自有代码使用异常处理，符合 Google 指南的 Windows 说明。

## 本地执行

在仓库根目录：

```powershell
build/python/Scripts/python.exe -m pip install -r tools/style-requirements.txt
build/python/Scripts/python.exe tools/check_style.py --fix
build/python/Scripts/python.exe tools/check_style.py
cmake --build --preset windows-release --parallel 8
ctest --preset windows-release
```

clang-format 固定为 21.1.8，cpplint 固定为 2.0.2；CI 执行相同脚本。脚本扫描整个自有源码目录（包含新文件），验证文件名、格式并运行 cpplint。新增其他源码目录时必须同时更新 SOURCE_ROOTS。

cpplint 默认的 guard 计算受克隆目录名影响；脚本仅将其替换为固定的 WORLDFORGE + 仓库相对路径，保留 guard 检查及其他全部规则。没有全局关闭任何 lint 类别。
ImGui 的两个根目录公共头文件采用精确到行的 include_subdir 豁免并标注上游原因；后端头文件使用 backends/ 路径。

## Review 与变更门槛

自动工具不能证明完全符合指南。Review 仍必须检查命名语义、包含依赖、接口契约、RAII 生命周期、禁止的语言特性和错误处理。禁止通过全局关闭 lint、随意添加 NOLINT 或改动第三方源码来掩盖问题。
功能相关变更须有对应验证；资源、输入或渲染链路变化须运行 GPU smoke。规则升级与必要的互操作例外必须说明依据并记录在 docs/，不得静默偏离本规范。
