# ADR 0004：面向主投岗位的版本与验收优先级

日期：2026-09-21。状态：依据用户最新优先级建立的当前规划基线；日期是目标，具体技术方案随实施验证。

## 输入

用户要求优先服务前 4–5 个主投岗位，重点保留 Hybrid Renderer、Asset Pipeline、CUDA Benchmark。
P0 包括 Scene/Runtime、Mesh/3DGS/Hybrid、Asset Factory、真实采集链路、GPU 优化、GUI Inspector 和 Windows/Linux 架构。
P1 包括场景 USD/Isaac、procedural/synthetic、CLI/batch/report；Web、专用 AI 集成、推理与 ROS2 为 P2。
公司方向是用户提供的规划依据，不是已独立核验的招聘要求。

## 决策

1. Windows 仍先交付；新增 Linux 核心构建、无窗口资产流程及后续 NVIDIA 图形运行的分层验证。macOS/多后端继续后置。
2. 用版本号组织实施，P0/P1/P2 只表示优先级。旧“P1.1 导入”等编号是历史任务名。
3. v0.1 是 Windows Hybrid World 首发；v0.2 补 Asset Factory 和真实采集；v0.3 必须有一次真实 CUDA 优化及跨平台证据。2027-01 留出三周收敛。
4. Asset Factory 提前成为面试前必交，限定一种有界修复/简化流程、一级 LOD、单凸包代理、单资产 USD 与报告；不在混合渲染之前建设完整加工平台。
5. 单资产 USD 属于 P0 资产流水线；完整 scene export、物理语义与 Isaac 消费属于 P1，分别验收。
6. 外部 COLMAP/GS 工具负责重建/训练，WorldForge 负责消费、检查、对齐、混合及项目复现；早期验证可行性，不自研训练器。
7. 性能采样持续进行，但“至少一个已测热点的实际优化与正确性对照”从按需工作升级为面试前门槛。
8. 内部 Command 随 Scene 形成；最小 CLI/batch 在 v0.2 服务资产复现。完整 Agent schema、收据、MCP/HTTP 仍后置。
9. 时间冲突先删 P2，再缩减 P1；不牺牲 Hybrid、Asset Pipeline、CUDA 证据的完整性。

## 对历史决策的影响

部分覆盖 ADR 0003：
- 保留 Windows 首发、CUDA/NVIDIA 优先、Mesh 不依赖 CUDA、插件后置。
- 覆盖“Linux 全部等未来多平台阶段”“LOD/USD 只在首发后按需做”“优化没有必交门槛”的安排。
- 历史 P0–P7 阶段编号退出当前排期；以 ROADMAP 的版本表为准。

PHASE_1 继续定义 v0.1，不把全部面试目标塞进第一次发布。CLI_SPEC 的完整协议表保留为后续候选，不能据此扩大 v0.2 的最小入口范围。

## 风险与验收

按每周 15–20 小时，只能交付每项的有边界切片。Linux GPU 环境、GS 工具许可/复现、USD 构建和采集质量应提前验证。缺少真实运行证据时标记未完成，不以可移植接口或一份截图替代验收。

具体范围、退出条件、裁剪顺序及版本窗口由 [ROADMAP](../ROADMAP.md) 维护。
