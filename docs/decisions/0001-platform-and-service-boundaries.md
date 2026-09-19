# ADR 0001：三平台桌面与未来 Web 的边界

> **历史记录：相关首期范围和排期已被 [ADR 0003](0003-windows-world-first.md) 覆盖。当前 Windows 先发布，多平台、CLI/Agent 后做；以下内容保留用于追溯，不作为实施门槛。**

- 日期：2026-09-19
- 状态：已部分被 ADR 0003 覆盖，历史方案
- 来源：引用对话中开源同类项目及 Windows/Linux/macOS + 后续 Web 的新增要求

## 背景

原计划使用 GLFW、OpenGL 4.5 和 Windows/Linux 验证。新要求把 macOS 客户端提升为首期目标，并要求未来轻量 Web 入口，必须调整公共能力基线和业务边界。

## 决策

1. SDL3 + ImGui docking，单窗口停靠起步，暂不启用多原生窗口。
2. 延续已选 OpenGL 路线，以 GL 4.1 为公共基线；Dawn/WebGPU 为第二后端方向。
3. CUDA 为可选 provider，CPU 排序 + GL raster 保证非 NVIDIA 平台也有 Gaussian 显示路径，性能独立披露。
4. Desktop、CLI、未来 Web server 共享命令、服务和 DTO。EditorSession 与 ImGui 分离，Web 用独立 React/TS UI。
5. OpenUSD 是可选交换适配器，不把 UsdStage 作为 Core 强制数据结构。
6. 模块化单体与窄接口，不提前建设微服务、动态插件 ABI 或完整 RenderGraph。

## 备选方案

| 方案 | 本轮未选择的原因 |
| --- | --- |
| 保持 GL 4.5 + GLFW | GL 4.5 不能作为 Mac 公共基线；虽 GLFW 也跨平台，本轮用 SDL3 统一平台服务 |
| v0.1 全部采用 Dawn/WebGPU | 改变已选学习路线并增加构建、shader/CUDA 集成工作；保留第二后端路线 |
| 分别自研 DX12/Vulkan/Metal | 不符合每周 15–20 小时的交付范围 |
| Qt 主框架 | 初期侧重视口与调试，暂不承担另一套 UI 集成；不代表 Qt 无法满足要求 |
| 整体桌面程序移植 WASM | 不能直接复用本机 CUDA/文件/重任务能力，Web 需后端服务 |
| 直接 fork 现有产品 | 会继承不同产品的模型与范围，优先借鉴组织方式 |

## 后果和验证门槛

GL 4.1 不能作为依赖 compute/SSBO 的基线，portable Gaussian 的规模和 CPU 排序成本必须公开。Apple 已弃用 OpenGL，后端隔离要通过接口与测试落实。

首个切片须在 Windows/Linux/macOS 构建 Studio/CLI，验证窗口、纹理、resize、输入与 DPI。目标 Mac 若无法建立 GL 4.1，必须修订此决策并提前实现 Dawn/Metal 路径；不能移除 Mac 或仅展示空窗口后宣称支持。

本轮设计不实现 Dawn、Web server 或 React；需要明确 DTO、任务、资源所有权及文件接口，避免桌面类型泄漏。详细契约见 [ARCHITECTURE](../ARCHITECTURE.md)。
