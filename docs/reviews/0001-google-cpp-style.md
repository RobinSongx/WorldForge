# 首轮 Google C++ 规范 Review

日期：2026-09-19。检查基线：bfee42e。范围：所有自有 C++ 代码、构建入口、文档与 CI。

## 已发现并修复

- 多条语句挤在一行、混合缩进、长行和 include 顺序：使用固定版本 Google preset 全量格式化。
- 文件与符号使用多套命名方式：源码改为 snake_case.cc / .h，普通函数改为 UpperCamelCase，变量使用 snake_case。
- pragma once 和传递依赖：改为带项目及路径前缀的 include guard，补齐直接依赖。
- class 公共数据及相机不变量暴露：状态字段私有化，相机状态通过访问器读取和方法修改；被动显示选项保留为 struct。
- 构造函数执行可失败的 SDL/GL 初始化并抛异常：迁移为显式 Initialize + 错误返回，RAII 负责失败时清理；移除自有异常处理和 filesystem。
- const selected() 通过 const_cast 委托：改为 const 查找。
- UI 总绘制函数过长：拆出 Dock、Scene、Inspector、Viewport、Console、Assets、Performance 子函数。
- GPU smoke 的报告写入结果未检查，提前关闭可能成功退出：检查 SDL 文件写入结果；提前中断明确返回失败。

## 自动约束

README、AGENTS.md、CODING_STYLE.md 声明统一规范；.clang-format、.editorconfig 与 check_style.py 固定格式和检查入口；Windows CI 在编译前执行全部自有 C++ 风格检查。

## 验证范围

固定版本 clang-format/cpplint、Windows Release 构建、CPU 测试以及正常 GPU smoke 均已通过。
已验证中文输出目录的 GPU smoke 成功（退出码 0，六个面板、缩放、变换、相机输入通过，GL 错误为 0）；输出目录被文件占用、报告文件不可写均正确失败（退出码 1）。
结果以实际执行记录为准；静态检查不替代人工语义 Review。
