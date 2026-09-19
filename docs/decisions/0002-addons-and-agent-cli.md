# ADR 0002：后续 Addons 与首期 Agent CLI

> **历史记录：相关首期范围和排期已被 [ADR 0003](0003-windows-world-first.md) 覆盖。当前 Windows 先发布，多平台、CLI/Agent 后做；以下内容保留用于追溯，不作为实施门槛。**

日期：2026-09-19。状态：首期 CLI 和三平台门槛被 ADR 0003 覆盖；源码 addons 方向保留，尚未实现。

## 依据

usdtweak 官方 README 将 addons 描述为与应用一起构建的 C++ 扩展，避免管理动态库。本次核实到这一官方说明；专门 Addons 文档未成功获取，未审计其注册或生命周期源码，不猜测具体 API。[官方扩展说明](https://github.com/cpichard/usdtweak#extending-usdtweak)

## 决策

插件功能先采用源码集成、重新编译的 addons 路线。首期完成内建 CommandRegistry/AssetProcessorRegistry，后续 P5 再公开 SDK 和第三方扩展入口；动态库 ABI、热重载不列入既定交付。

CLI 是首期产品入口，不是后期附带调试工具。其命令和未来插件命令通过相同 registry 暴露，Agent 可发现参数/结果 schema 并调用业务能力。具体规范见 [CLI_SPEC](../CLI_SPEC.md)。

## P5 Addon 设计边界

- `AddonDescriptor` 包含唯一 ID、版本、host API version、依赖 ID 和能力列表，采用反域名或组织前缀避免冲突。
- CMake 显式选择源码目录，构建时链接；启动时注册，按依赖拓扑顺序激活，循环/缺失依赖和重复 ID 返回诊断，逆序 shutdown。不扫描并执行任意下载文件。
- `AddonContext` 提供只读查询、Command/AssetProcessor 注册、任务提交及日志端口；不暴露可变 Scene、GL handle 或 Application 内存布局。
- addon 分为 headless 部分与可选 desktop UI 部分。算法/命令在无窗口 CLI 中可用，面板仅在 Studio 注册；无 UI 进程不加载 ImGui 依赖。
- 资产处理器输出不可变产物和报告，附着场景通过 host 命令提交。UI 面板必须复用已注册命令，不暗藏无法被 CLI 调用的业务逻辑。
- shutdown 先停止该 addon 新任务，等待完成/取消，再注销 handler/panel。源码 addons 是可信同进程代码，不宣称提供沙箱隔离；不允许存在任务引用时卸载代码。
- SDK 先保证同一主程序源码版本下的源代码兼容，不承诺跨编译器二进制兼容。manifest 中所需 addon 缺失时保留未知数据只读、报告受影响能力，不静默删除。

## 插件功能验收

提供一个独立目录的示例 addon：注册可配置的资产检查器、CLI 命令/schema 和一个展示报告的面板。不开启时核心正常构建；开启后 GUI/CLI 得到同一报告。覆盖依赖/版本不符、重复 ID、任务退出与无 Studio 构建。三平台均完成构建与对应运行检查，才称插件功能交付。

P6 的 HTTP/MCP adapter 可直接复用命令 schema，但 addon 的 ImGui 面板不会自动变成 Web 控件。Web 先展示通用报告/参数表单，专用 UI 另行实现。
