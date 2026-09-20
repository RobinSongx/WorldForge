# WorldForge 文档

截至 2026-09-20：0.0.2 Mesh 开发预览已实现静态 GLB 导入与根对象编辑，通过本机 Windows Release 和 CPU/GPU 自动测试；原生文件选择框操作待人工验收。完整 P1 和首期尚未完成。优先级仍为 **Windows 桌面框架 → Mesh → 混合渲染 → 世界资产闭环与首发**。

## 当前文档

- [今晚 P1.1 范围与方案](plans/2026-09-20-mesh-import.md)：静态 GLB 导入与基础编辑的交付边界。
- [P1.1 实现说明](learning/0002-mesh-import.md)：数据、线程、矩阵与失败隔离。
- [P1.1 验证记录](benchmarks/2026-09-20-mesh-import.md)：测试、真实样例及未自动验收项。

- [C++ 编码规范](CODING_STYLE.md)：严格遵循 Google C++ Style Guide，含本地与 CI 检查。
- [首轮代码 Review](reviews/0001-google-cpp-style.md)：本次问题与修复范围。

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

P1.1 已建立真实 Mesh 导入与编辑链路；下一步完善 P1 材质、贴图及项目保存重开，并补充原生文件选择交互验收。每次交付一个可运行功能、设计说明、验证结果与学习实验。

多平台、CLI/Agent、插件和 Web 仍在后续路线中；目前不创建对应空壳或将其作为 Windows 验收前提。根目录 README 提供英文快速启动，关键设计与学习记录保留在 docs。

## 后续文档

ASSET_SPEC 随实际格式细化，DEVELOPMENT 记录已验证的构建/运行，LIMITATIONS 记录真实限制；decisions、benchmarks、learning 分别存决策、实测和学习记录。文档描述的计划能力不能当作实现证据。
