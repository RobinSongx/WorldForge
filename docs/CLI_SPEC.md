# CLI 与 Agent 调用契约草案

日期：2026-09-21。状态：候选协议，当前不存在产品 CLI。按 ADR 0004，v0.2 提前实现资产复现所需的最小 CLI/batch；完整 Agent 协议继续后置，不阻塞 Windows v0.1。

## v0.2 的最小边界

仅围绕已实现的 validate、repair、LOD、collision、单资产 USD 服务提供输入/输出参数、JSON 结果、明确退出码和批处理示例；具体命令名在实现时冻结。GUI 与 CLI 复用服务/命令，不复制算法。

下面的 capabilities/schema、完整项目操作、跨进程 requestId/revision、收据和常驻能力仍是后续草案，不全部纳入 v0.2。资产处理应可无窗口执行；CUDA benchmark 若需要 GPU，则走明确启用的可选执行路径，不能宣称 GPU 测试完全无 GPU 依赖。


## 1. 定位

`worldforge` 是独立、无窗口的命令行程序；桌面程序命名为 `worldforge-studio`。CLI 链接与 GUI 相同的 Application、Domain、IO、Jobs，不链接 SDL、ImGui、OpenGL 或 CUDA。Agent 使用进程调用即可完成 CLI 初版的资产和项目操作，不需要自动点击 GUI，也不需要先启动 MCP/HTTP 服务。

CLI 初版为一次调用执行一个操作、完成后退出。Application 内部异步 Job 在 CLI 中等待到终态，退出前提交结果；JobId 只作本次调用关联信息。后台守护进程、跨进程 jobs 查询、操作运行中的 Studio、Agent MCP adapter 与 Web 服务属于后续阶段。

## 2. 命令表

| 命令 | 输入 | 成功结果 |
| --- | --- | --- |
| `worldforge --help` / `--version` | 无 | 人类帮助 / 软件版本 |
| `worldforge capabilities --json` | 无 | schemaVersion、支持命令/格式/能力及不可用原因 |
| `worldforge schema --command <name> --json` | 如 `scene.exec` | 命令参数、请求与结果的 JSON Schema，标明副作用 |
| `worldforge inspect <input.glb> --json` | 独立资产路径 | 统计与 ValidationReport，不修改资产 |
| `worldforge optimize <input.glb> --output <directory> --lod-ratios 0.5,0.25 --json` | 独立资产与派生目标目录 | 各级派生 GLB、LOD 清单、质量报告与实际比例 |
| `worldforge project create --project <directory> --json` | 新项目目录 | 空项目和 sceneId |
| `worldforge project inspect --project <directory> --json` | 已有项目目录 | 项目/场景/资产摘要及缺失资产诊断 |
| `worldforge project import --project <directory> --input <input.glb> --json` | 项目与 GLB | 导入、附着、保存；返回 AssetId/EntityId/revision |
| `worldforge project optimize --project <directory> --asset <uuid> --lod-ratios 0.5,0.25 --json` | 项目内资产 ID | 发布派生 revision、关联 LOD 并保存，保留原始资产 |
| `worldforge scene list --project <directory> --json` | 项目 | SceneDTO 与实体 ID、变换、可见性、表示引用 |
| `worldforge scene exec --project <directory> --request <file.json> --json` | 单个 CommandEnvelope；`--request -` 从 stdin 读 | 执行允许的场景命令，保存后返回新 revision |

`scene exec` CLI 初版仅开放 CreateEntity、DeleteEntity、SetTransform、SetVisibility、AttachRepresentation，使用与 GUI 一样的参数校验。Import/Optimize 通过专用命令暴露；不提供任意 C++、Python 或 shell 执行接口。会话内 undo/redo 属于 GUI，CLI 跨调用不维护撤销栈。

新建 Entity 可为空节点；AttachRepresentation 必须引用项目内现有资产。删除父实体按架构的子树语义处理，JSON Schema 明示这一效果。SetTransform 采用架构的列优先矩阵约定，不在 CLI 另创坐标格式。

## 3. 输出和错误

`--json` 模式 stdout 只包含一个完整 JSON 对象和末尾换行；日志和可选进度写 stderr，禁止 ANSI 颜色和交互提示。成功、失败均使用同一 envelope：

```json
{
  "schemaVersion": 1,
  "requestId": "caller-provided-or-generated-uuid",
  "ok": true,
  "result": {},
  "diagnostics": [],
  "artifacts": [],
  "error": null
}
```

失败时 `ok=false`，`error` 包含稳定 code、message、details。通常 `result=null`；若 inspect 已完成检查但发现质量错误，`result` 保留完整报告。人类提示可改变，Agent 只依赖 code/schema。产物记录 path、role、contentHash；路径为最终实际输出的绝对路径。业务 ID 保持 UUID 字符串，revision 使用十进制字符串避免未来 JS 整数精度损失。非有限数值不得进入 JSON。

| 退出码 | 意义 |
| --- | --- |
| 0 | 操作成功；ValidationReport 可含 warning，但不含 error |
| 2 | CLI 参数或 JSON 请求不合法 |
| 3 | 资产检查发现 error 级问题；inspect 仍返回完整报告，`ok=false`、`error.code=ValidationFailed` |
| 4 | Unsupported：格式、命令或当前不可用能力 |
| 5 | IOFailure / NotFound：输入、输出、权限或资源缺失 |
| 6 | Conflict：项目忙、revision 不符或输出冲突 |
| 7 | 执行失败或资源不足 |
| 8 | 工具内部超时，已停止提交 |
| 9 | 工具处理到取消请求，已停止提交 |

普通 warning 不令命令失败。损坏到无法解析的文件返回结构化导入错误，不伪造完整资产报告。`inspect` 的质量失败报告供 Agent 定位问题。

CLI 支持全局 `--request-id <uuid>`、`--timeout-ms <positive integer>`；超时未设置则无限等待。`scene exec` 的 envelope.requestId 是权威值，若同时提供全局值则必须相同。进程被外部强杀时无法保证输出 JSON，调用方应按异常进程终止处理，不能假定退出码 9。

## 4. 写入、并发与重试

- 默认不覆盖独立 optimize 的已有目标目录；首版无需 `--force`，冲突返回 6。输出先进入临时 staging，成功后发布完整产物目录。
- 项目操作持有独占项目锁，包括只读 inspect/list，以简化CLI 初版一致性。Studio 打开项目期间持锁；CLI 返回 ProjectBusy，不旁路修改。用户关闭项目后 Agent 才能编辑该项目。
- 项目写操作先校验，再 staging 资产，最后替换 manifest；失败、取消或超时不切换活动清单。已产生但未引用的数据可后续清理。
- 重试同一已提交项目修改需要相同 requestId 和相同规范化请求摘要。项目清单保留命令收据（ID、请求摘要和结果摘要），与状态在同一次 manifest 提交中发布；重放返回原结果，不重复导入。相同 ID 不同请求返回 Conflict。
- `scene exec` 必须提供 expectedRevision；project import/optimize 可接受 `--expected-revision`，未提供则在持锁后读取当前值。去重检查先于 revision 检查，以支持已提交但响应丢失的重试。
- 独立 inspect/optimize 不承诺跨进程 requestId 去重；optimize 的已存在目标返回 Conflict，Agent 可检查先前清单确认结果。`project create` 对已有项目返回 Conflict。
- 外部强制终止留下 staging 时，下一次打开只读取有效 manifest；锁使用 OS 进程锁而非仅凭文件存在判定占用，不能擅自破坏其他活进程的锁。

## 5. Agent 最小工作流

```text
capabilities / schema
  → inspect 原始 GLB
  → optimize 到新目录
  → inspect 派生 GLB 与读取报告
  → project create
  → project import
  → project optimize
  → scene list 取得 ID/revision
  → scene exec 修改变换
  → project inspect 验证保存结果
```

Agent 通过参数数组启动进程，JSON 请求通过文件或 stdin 传递，避免把自然语言拼成 shell 命令。文件路径、报告与命令 metadata 作为数据处理。无需CLI 初版自建对话助手或绑定特定模型。

## 6. 完整协议的后续验收（不作为 v0.2 最小入口门槛）

先在 Windows 执行成功/错误请求，校验 stdout JSON、stderr 分离与退出码；Linux 资产入口在 v0.2 复验；其他平台后续分别验证。覆盖含空格/中文路径、损坏输入、未知命令、已有输出、项目忙、revision 冲突、重复 requestId、取消/超时和保存失败。无显示服务、无 CUDA、关闭 Studio 的构建也须完成资产与项目流程。

同一资产与参数在 GUI/CLI 中得到一致的结构与质量报告；耗时等非确定字段不作相等比较。使用脚本模拟 Agent 完成上述全链路，并在 Studio 重开产物确认场景和 LOD。命令发现与执行从同一 registry 生成，避免帮助/schema 与实现各维护一份而漂移。
