# 开源参考与设计借鉴

核查日期：2026-09-19。来源为项目官方仓库和文档；本轮是文档级调研，没有 clone、运行或完成源码审计。README 功能描述不视为性能、稳定性或质量验证。

| 项目与官方来源 | 核实的定位与借鉴点 | 不直接继承的范围 |
| --- | --- | --- |
| [usdtweak](https://github.com/cpichard/usdtweak) | C++、ImGui、GLFW 的三平台 OpenUSD 编辑器；参考场景树、属性面板、视口工具与编辑目标分离 | 不 fork 为主程序；不引入完整 USD composition / Hydra |
| [SuperSplat Viewer](https://github.com/playcanvas/supersplat-viewer) | 独立 Web viewer；支持单独 collision 输入和状态/事件接口；参考 Viewer 生命周期与独立碰撞表示 | 原生渲染核心不依赖 PlayCanvas，不复制全部编辑范围 |
| [gsplat-rt](https://github.com/matthewhamilton3141/gsplat-rt) | 描述 RGB → Gaussian + collision → OpenUSD 工作流；参考重建与导出边界 | 实时性和 SLAM 指标未复现，不依赖其描述保证碰撞质量 |
| [SuGaR](https://github.com/Anttwo/SuGaR) | 从 3DGS 提取 Mesh，通过表面绑定构造混合表示 | 表面绑定/联合优化与普通 Mesh/GS 深度合成不同，不是 v0.2 显示的必要步骤 |
| [OpenSplat](https://github.com/WebODM/OpenSplat) | C++ Gaussian 工程，提供跨平台计算配置；参考可选后端与导入流程 | 不将 LibTorch、训练器或代码整体引入客户端 |
| [OpenUSD](https://openusd.org/release/intro.html) | 场景描述、组合与交换；借鉴引用、实例、单位与坐标约定 | 不声称实现完整 USD 语义或无损往返 |
| [Genie Sim](https://github.com/AgibotTech/genie_sim) | 机器人仿真平台；参考资产与下游消费边界 | 不将训练、ROS 或仿真服务作为 Core 依赖 |

上述项目是研究对象，不等于依赖清单。复制代码、引入 SDK 和再分发数据前分别记录具体版本的许可证及第三方条件；自有 MIT 代码不改变外部代码的许可。

## 技术依据

- [SDL3 平台说明](https://wiki.libsdl.org/SDL3/README-platforms)：窗口与事件跨平台入口。
- [ImGui 后端说明](https://github.com/ocornut/imgui/blob/docking/docs/BACKENDS.md)：平台和图形后端分离。
- [Apple OpenGL 4.1 Core](https://developer.apple.com/documentation/appkit/nsopenglprofileversion4_1core)：后续 Mac 移植的能力限制与弃用状态，不再决定 Windows 首期基线。
- [Dawn README](https://github.com/google/dawn/blob/main/README.md)：后续 WebGPU 后端依据，不保证 GLSL/CUDA 自动复用。
- [CUDA OpenGL interop](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__OPENGL.html)：注册与设备关联依据。

## 深入研究顺序

2026-09-19 补充：usdtweak 的 [Extending usdtweak](https://github.com/cpichard/usdtweak#extending-usdtweak) 明确说明 C++ addons 随应用构建，避免管理动态库。专门 Addons 文档本次未成功获取，未核实源码注册 API；WorldForge 的注册与生命周期设计是自身决策，见 [ADR 0002](decisions/0002-addons-and-agent-cli.md)。

实施前阅读 usdtweak 的编辑操作组织与 SuperSplat Viewer API；Hybrid 阶段研究 SuGaR/OpenSplat 的表示与计算分离；交换阶段再深入 gsplat-rt/Genie Sim。具体研究结论写入 `decisions/`，性能结论由本项目实验验证。
