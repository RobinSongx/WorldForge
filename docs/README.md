# WorldForge 文档

截至 2026-09-19：Windows GUI 框架预览 0.0.1 已实现并通过本机 Release/CPU/GPU smoke；完整首期尚未完成。当前优先级为 **Windows 桌面框架 → Mesh → 混合渲染 → 世界资产闭环与首发**，性能按需穿插。

## 当前文档

- [构建与运行](DEVELOPMENT.md)：当前已验证的 Windows 框架、操作与限制。
- [GUI 框架学习记录](learning/0001-gui-framework.md)：代码链路与实验。

- [分期计划](ROADMAP.md)：当前实施顺序、目标时间与退出条件。
- [Windows 首期范围](PHASE_1.md)：必做功能、排除项与完整验收脚本。
- [软件架构](ARCHITECTURE.md)：当前实现边界与明确标记的后续扩展。
- [CLI / Agent 后期协议草案](CLI_SPEC.md)：主功能稳定后在 P4 落实，不阻塞首发。
- [ADR 0003：Windows 与世界闭环优先](decisions/0003-windows-world-first.md)：最新决策，覆盖早期三平台/CLI 首期安排。
- [ADR 0001](decisions/0001-platform-and-service-boundaries.md)、[ADR 0002](decisions/0002-addons-and-agent-cli.md)：历史方案；保留参考，已标记被覆盖的部分。
- [开源参考](REFERENCES.md)：同类项目借鉴点与官方来源。

## 下一步

P0 核心已可运行；下一步推进 P1：真实 Mesh 数据、GLB 导入与材质。每次交付一个可运行功能、设计说明、验证结果与学习实验。

多平台、CLI/Agent、插件和 Web 仍在后续路线中；目前不创建对应空壳或将其作为 Windows 验收前提。根目录 README 提供英文快速启动，关键设计与学习记录保留在 docs。

## 后续文档

ASSET_SPEC 随实际格式细化，DEVELOPMENT 记录已验证的构建/运行，LIMITATIONS 记录真实限制；decisions、benchmarks、learning 分别存决策、实测和学习记录。文档描述的计划能力不能当作实现证据。
