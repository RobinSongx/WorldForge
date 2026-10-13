# WorldForge 文档

截至 2026-09-28：0.0.2-scene-dev 已增加 Scene 保存/重开、持久身份、事务写入和未保存保护；GLB/PNG 与独立选择显示继续保留。当前为 0.0.3 增量，完整 PBR、Undo、项目包和 Hybrid 尚未完成。Windows 优先，CLI/Agent、插件和其他平台按后续路线图推进。

## 当前文档

- [10/01—10/07 每日 5 小时计划](plans/2026-10-01-to-10-07.md)：0.0.3 收口及 v0.1 Hybrid 首切片的 35 小时安排与验收门槛。
- [现代编辑器 UI 实现说明](learning/0005-modern-editor-ui.md)：视口优先布局、主题、已保留操作与体验边界。

- [Scene 实现说明](learning/0004-scene-save-open.md)：文档模型、文件格式、事务、线程/UI 协调与设计取舍。
- [Scene 验收记录](benchmarks/2026-09-28-scene-save-open.md)：10 项回归、真实 Duck 像素往返、文件故障与未验项。

- [Scene 保存与重开设计](plans/2026-09-21-scene-save-open.md)：第一增量设计基线；2026-09-28 已实现，具体差异见实现说明。
- [ADR 0005：Scene 外部引用增量](decisions/0005-scene-save-open-increment.md)：与长期项目包、编辑会话和缺失资产策略的边界。
- [Scene v1 格式示例](formats/scene-v1.example.wfscene)：Cube 与两个共享 GLB 资产的实体；仅示例数据。

- [今晚任务：选择显示与基础颜色贴图](plans/2026-09-21-base-color-texture.md)：0.0.3 的首个增量，已实现并自动验收。
- [贴图实现说明](learning/0003-base-color-texture.md)：数据、解码预算、颜色空间与 GPU 所有权。
- [贴图验收记录](benchmarks/2026-09-21-base-color-texture.md)：8 项测试、Duck 实测、包与未验证项。
- [贴图代码复查](reviews/0002-base-color-texture.md)：失败回滚、GL 错误清理与边界。

- [今晚 P1.1 范围与方案](plans/2026-09-20-mesh-import.md)：静态 GLB 导入与基础编辑的交付边界。
- [静态 Mesh 导入实现说明](learning/0002-mesh-import.md)：源码导航、资产/实例、accessor 校验、矩阵、线程状态机与失败隔离；已补充当前版本差异。
- [P1.1 验证记录](benchmarks/2026-09-20-mesh-import.md)：测试、真实样例及未自动验收项。

- [C++ 编码规范](CODING_STYLE.md)：严格遵循 Google C++ Style Guide，含本地与 CI 检查。
- [首轮代码 Review](reviews/0001-google-cpp-style.md)：本次问题与修复范围。

- [构建与运行](DEVELOPMENT.md)：当前已验证的 Windows 框架、操作与限制。
- [GUI 框架实现说明](learning/0001-gui-framework.md)：启动与帧流程、FBO/DPI、相机、资源生命周期，以及带预期结果的实验。

- [路线图与版本规划](ROADMAP.md)：0.0.3 → v0.1 Hybrid → v0.2 Asset/Capture → v0.3 优化与跨平台，以及面试前裁剪顺序。
- [ADR 0004：面向主投岗位的交付](decisions/0004-interview-delivery-priorities.md)：2026-09-21 最新规划依据。
- [Windows 首期范围](PHASE_1.md)：必做功能、排除项与完整验收脚本。
- [软件架构](ARCHITECTURE.md)：当前实现边界与明确标记的后续扩展。
- [CLI / Agent 协议草案](CLI_SPEC.md)：v0.2 最小资产入口与后续完整协议的边界。
- [ADR 0003：Windows 与世界闭环优先](decisions/0003-windows-world-first.md)：历史决策，部分被 ADR 0004 覆盖。
- [ADR 0001](decisions/0001-platform-and-service-boundaries.md)、[ADR 0002](decisions/0002-addons-and-agent-cli.md)：历史方案；保留参考，已标记被覆盖的部分。
- [开源参考](REFERENCES.md)：同类项目借鉴点与官方来源。

## 下一步

历史 P1.1 已建立真实 Mesh 导入与编辑链路，PNG 基础色增量已完成；Scene 第一增量已完成，用户已反馈体验验证通过；完整 PBR、基础撤销和项目包继续按 0.0.3/v0.1 规划推进。选中开关与应用内部交互已验证，原生选框边界场景尚无逐项记录；另安排采集可行性/Linux 核心构建验证。每次交付一个可运行功能、设计说明、验证结果与学习实验。

Linux 核心验证与最小资产 CLI 按新版本计划接入；完整 Agent、插件、macOS 和 Web 继续后置，不先创建空壳。根目录 README 提供英文快速启动，关键设计与学习记录保留在 docs。

## 后续文档

ASSET_SPEC 随实际格式细化，DEVELOPMENT 记录已验证的构建/运行，LIMITATIONS 记录真实限制；decisions、benchmarks、learning 分别存决策、实测和学习记录。文档描述的计划能力不能当作实现证据。
