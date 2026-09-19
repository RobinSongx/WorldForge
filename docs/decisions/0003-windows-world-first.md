# ADR 0003：Windows 与世界资产闭环优先

日期：2026-09-19。状态：当前有效，按用户最新指示修订。

## 决策

1. Windows 先发布，多平台后做；不以 Linux/macOS CI、portable Gaussian 或第二渲染后端阻塞首期。
2. 首期执行顺序为桌面框架、Mesh、混合渲染、世界资产项目包闭环；完整 LOD/优化体系不再是混合渲染前置条件。
3. CLI/Agent 在主功能较后期实现，首期不实现 schema 发现、跨进程幂等收据或常驻任务服务。
4. 性能测量尽早加入，优化随功能瓶颈进行，不设置必须完成后才能继续功能的独立性能阶段。
5. Windows 后端采用原计划的 OpenGL 4.5/GLSL 450 能力；保留 SDL3/ImGui。Mac 的 GL 4.1 限制不再决定当前后端，后续移植时独立选择后端。
6. Gaussian 首先实现 Windows/NVIDIA CUDA 路径，明确发布硬件要求；非 CUDA provider 延后。Mesh 客户端启动不依赖 CUDA。
7. 插件仍参考 usdtweak 源码 addons，后续实现，不删除长期方向。核心与 UI 分离、业务服务入口继续保留。

## 覆盖关系

ADR 0001 的首期三平台门槛、GL 4.1 公共基线、先 portable 后 CUDA 均被覆盖。
ADR 0002 的首期 CLI/registry 和插件三平台验收前提被覆盖；源码 addons 和 GUI/CLI 共享语义方向保留。
CLI_SPEC 只是 P4 协议草案，其“首版”含义是首个 CLI 版本，不是 Windows 软件首发。

P0–P3 为 Windows 首期；P4 为主功能深化及 CLI，P5 插件，P6 多平台，P7 Web/服务。详细时间与范围以 ROADMAP/PHASE_1 为准。

## 取舍与验证

减少前期移植和通用框架成本，尽早形成可用世界。代价是后续 Mac/非 NVIDIA 移植需要专门 renderer/provider 工作；源码模块边界只能降低成本，不能保证无需改动即可跨平台。

首发以实际 Windows 构建、可分发包、Mesh/GS 编辑与保存搬移重开证明完成；不将项目保存等同于 USD/仿真兼容。按实测帧时和画质处理性能问题，不预设提速数字。
