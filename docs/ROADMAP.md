# WorldForge 路线图与版本规划

修订日期：2026-09-21。依据：用户最新的主投岗位优先级，见 [ADR 0004](decisions/0004-interview-delivery-priorities.md)。
当前实现基线：0.0.2-texture-dev，已补 PNG 基础色和独立选择显示，是 0.0.3 的增量；不把以下完整版本计划当作已实现能力。
每周投入仍按 15–20 小时估算，目标是 2027 年 2 月面试前可复现展示，2027-01-31 前完成收敛。

## 1. 定位与优先级

WorldForge 的交付重点调整为：**C++ Hybrid World Runtime + Asset Pipeline + 可复现的 GPU 优化证据**。
GUI 是检查、编辑与比较这些能力的入口。以用户给出的 Seed、Meshy、智元、PICO 主投方向为规划输入；本文没有独立核验各公司实时 JD，也不据此承诺岗位匹配结果。

Windows 仍先发布。Linux 架构和核心构建验证提前；Linux 图形/CUDA 验证在 Windows 链路成立后实施。macOS、Web、插件和完整 Agent 协议继续后置。

从本次开始，**P0/P1/P2 表示功能优先级，版本号表示实施阶段**。旧文档中的“P0 框架”“P1.1 导入”保留为历史任务名称，不能与新优先级混用。

| 优先级 | 面试前目标 | 安排 |
| --- | --- | --- |
| P0 | C++ Scene/Runtime、Mesh Renderer、3DGS Renderer、Hybrid World、GUI World Inspector | 0.0.3 → v0.1；持续完善至 v0.3 |
| P0 | Asset Factory：validate / repair / LOD / collision / 单资产 USD | v0.2 的必交切片 |
| P0 | 真实采集 → COLMAP/外部 GS 工具 → WorldForge | 0.0.3 做可行性探路，v0.2 完整复现 |
| P0 | CUDA/GPU profiling 与一次真实优化 | v0.1 留基线，v0.3 提交对照证据 |
| P0 | Windows/Linux 跨平台架构 | 0.0.3 起检查边界与核心 CI，v0.3 汇总实际验证 |
| P1 | USD scene export、Isaac adapter、procedural scene、RGB/depth/segmentation | P0 达标且有余量后，优先挑一个完整下游消费案例 |
| P1 | CLI/Command、batch processing、validation report / benchmark | 内部 Command 随 Runtime 落地；v0.2 做必要的无窗口入口与报告，不先做完整协议 |
| P2 | Web Viewer、AI mesh 专用接入、inference runtime、ROS2 bridge | 不排入面试前承诺；需要时先验证通用 GLB 导入是否足够 |

资产校验报告、固定条件 benchmark 虽列在 P1 中，但证明 Asset Pipeline 和 CUDA 优化所需的最小报告是对应 P0 的验收材料；不因此建设通用报告平台。

## 2. 版本与目标窗口

以下是目标窗口，不是承诺日期；每个版本按退出条件验收。正式 tag/Release 在验证通过后单独发布。

| 版本 | 目标窗口 | 主交付 | 退出条件 |
| --- | --- | --- | --- |
| 0.0.2 | 已实现开发预览 | 静态 GLB、共享 Mesh、根编辑、后台导入、基础预览 | 本机已有自动测试记录；原生选择框完整交互仍需补验 |
| 0.0.3：Mesh/Scene 闭环 | 9/21–10/04 | 材质/贴图最小集、不透明 PBR、持久 ID、场景保存重开、基础命令/撤销、Inspector 改善 | 一个 Mesh 场景搬移后重开一致；源资产不被覆盖；Linux 核心构建开始验证 |
| v0.1：Hybrid World 首发 | 10/05–11/15 | 3DGS PLY、CUDA 基线、Mesh/GS 混合、对齐检查、混合项目包、Windows 可分发程序 | 前/后/交叉用例、混合项目搬移重开、固定场景 GPU 基线；见 PHASE_1 |
| v0.2：Asset Pipeline + Capture | 11/16–12/20 | 有界修复、一级 LOD、凸包碰撞代理、单资产 USD、派生资产溯源；一份真实采集世界；最小 CLI/批处理 | 源 → 检查 → 派生 → 世界消费 → 导出全过程可复现，有质量/失败报告 |
| v0.3：优化与跨平台证据 | 12/21–1/10 | 一个 CUDA 热点优化；Windows/Linux 实测矩阵；展示场景稳定 | 同资产/视角/画质的优化前后数据与正确性检查；Linux 实际构建/运行证据明确 |
| v0.3.x：面试展示收敛 | 1/11–1/31 | 修复、复现脚本、架构说明、3–5 分钟视频、技术复盘 | 从干净环境按说明复现；已知限制与贡献边界清晰；不再追加功能 |

0.0.3 至 v0.3 约 16 周，即 240–320 小时；最后三周约 45–60 小时用于收敛。不能把八个 P0 都扩成独立产品。v0.1 是 Windows 产品闭环；v0.3 才是本轮 P0 的面试验收目标。原来“v0.1 后再按需做资产处理/优化”的安排由本计划取代。

## 3. P0 的可交付边界

### Scene / Runtime 与 Inspector

- 持久资产/实体 ID、节点/实例、矩阵与 Bounds、项目 revision、资源所有权和失败隔离。
- GUI 与资产服务分离；先实现实际编辑需要的命令/撤销，不搭 ECS、脚本 VM 或通用引擎。
- Inspector 显示 Mesh/GS、源/派生关系、变换、碰撞代理、诊断和资源统计。
- 补取消选择与选中线框独立开关，避免高亮遮挡正常外观；这些属于近期体验修正。

### Renderer / Hybrid World

- 不透明 Mesh 材质最小集，明确支持的 3DGS PLY；共享相机/实体变换。
- CUDA provider 与 Mesh raster 的深度/透明贡献合成；测试前后与相交，披露近似和伪影。
- 保存、搬移、重开混合项目；不把只能同时画两种对象当成混合闭环。
- 不自研完整训练器、全套渲染后端或统一动态重光照。外部训练/渲染依赖与自有实现分别说明。

### Asset Factory

最小操作集合也必须具有可复现输入、输出和质量检查：

| 操作 | 必交范围 | 暂不承诺 |
| --- | --- | --- |
| validate | 索引、有限值、退化面、法线、拓扑问题诊断和统计 | 自动理解模型语义 |
| repair | 显式选择删除退化面、按容差合并重复顶点、重建法线；记录变更 | 通用补洞、自动 UV、保证任何模型闭合 |
| LOD | 一种简化流程、至少一级派生 LOD、实际三角形数与误差/视觉对照 | 完整自动 LOD 策略与流式调度 |
| collision | 单资产凸包代理、独立显示、与视觉表示关联和导出 | 凹体自动分解、机器人动力学与物理求解 |
| USD | 单 Mesh 及局部变换、单位/up-axis、必要引用的导出子集；用独立 OpenUSD 消费端检查 | 完整世界材质/实例/物理语义交换 |

所有操作保留源资产，派生结果关联输入、参数和工具版本。完整 USD scene export 与 Isaac 消费适配属于 P1；不能把输出一个 USD 文件直接称为仿真就绪。

### Real capture

- 选一个尺度可控、允许拍摄与展示的真实静态场景；记录输入照片、采集条件、工具版本和命令。
- 使用外部 COLMAP/GS 流程得到资产，再导入 WorldForge 对齐、混合、保存重开。
- 0.0.3 阶段先做一次有时限的采集/工具/许可证验证，避免最后才发现重建失败；完整流程在 v0.2 收口。
- 重建质量、坐标/尺度、资产来源和外部工具贡献均说明。不是在 WorldForge 内开发训练产品。

### CUDA 与 GPU 证据

v0.1 锁定一份有实际负载的资产、相机轨迹和参数；记录硬件、驱动、构建配置、分辨率、数量、预热及重复次数。保留 GPU 分段时间、P50/P95、显存和正确性/画质对照。

v0.3 针对测得的一个热点完成修改，保留基线与优化版本和复现命令，证明收益超过测量波动。不能仅降低 splat 数量/分辨率，不能用 CPU 帧间隔代替 GPU 时间，也不预设提速百分比。小 Duck 的导入计时不是 CUDA benchmark。

### Windows / Linux

- 近期：Domain/IO/服务不依赖 Win32 或窗口；Linux 核心编译和 CPU 测试进入验证范围。
- v0.2：资产处理/最小 CLI 在无窗口 Linux 环境复验。
- v0.3：在具备 NVIDIA 与图形会话的 Linux 环境验证 Mesh/GS/Hybrid；与 Windows 记录不同的能力/限制。
- 实际 Linux GPU 资源尚未确认，须在 v0.1 结束前落实。CI 编译通过不代表 CUDA/GL 互操作已验证，WSL CPU 通过也不代表 Linux 桌面通过。
- Windows 保持首发；Linux 安装器与全 UI 等价不阻塞 Windows 发布。若缺 Linux GPU 实测，必须保留未完成标记，不宣称完整跨平台渲染。macOS/非 CUDA provider 不进入本轮承诺。

## 4. 超期处理

每两周按已完成的验收证据重估。顺序依次是：

1. 移出 Web、ROS2、推理运行时、AI 生成专用集成。
2. 移出完整 USD scene/Isaac、procedural 和 synthetic 数据扩展；合成深度若包含 GS 必须标明近似，不能宣称物理真值。
3. 缩减 CLI 命令数量、复杂材质、资产格式数量、GUI 美化和修复算法种类。
4. 保留一个质量可靠的 Hybrid 场景、一条完整资产派生流程、一份真实采集资产、一次实测 CUDA 优化，以及明确的 Windows/Linux 验证证据。

P0 若仍未达到，标记版本未达标并说明缺口；不能改标签冒充完成。不要为八个方向各做一个不能复现的演示。

## 5. 当前下一步

继续当前 Mesh 切片，不重写已经验证的代码：
先补选中线框/取消选择体验，再落实材质与纹理、SceneDocument/持久 ID/保存重开，逐步将编辑操作放入命令层。同期安排小型真实采集可行性验证和 Linux 核心构建检查。

CLI、资产加工和 Linux GPU 工作在上述版本窗口接入；不把它们同时塞进下一晚。技术实现与验证见 DEVELOPMENT，v0.1 的具体首发门槛见 PHASE_1。
