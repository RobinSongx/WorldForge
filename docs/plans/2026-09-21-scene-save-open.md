# Scene 保存与重新打开：第一增量设计

2026-09-28 实施更新：用户已授权开发，第一增量已落地；具体实现取舍、验证证据及未验项见 [实现说明](../learning/0004-scene-save-open.md) 和 [验收记录](../benchmarks/2026-09-28-scene-save-open.md)。下文保留 9/21 设计基线，不作为最新运行状态声明。

日期：2026-09-21。状态：设计草案，等待开发；尚未实现、未执行本方案中的测试。
依据：当前代码 ffe88d6（运行能力对应 dce2f6b）、[ROADMAP](../ROADMAP.md) 的 0.0.3，以及用户要求先设计 Scene 保存与重开。
决策摘要见 [ADR 0005](../decisions/0005-scene-save-open-increment.md)。这是 0.0.3 的一个切片，不代表该版本的 PBR、撤销或最终项目包已完成。

## 1. 目标与边界

首个可验收结果：

    导入 GLB + 添加 Cube → 编辑 TRS/Tint/Visible → 保存 .wfscene
    → 退出程序 → 打开 .wfscene → 恢复对象、共享引用与已保存视图

首版采用一个 UTF-8 JSON 场景文件，引用外部自包含 GLB。保存描述，不将 Mesh 顶点、PNG 或 GPU 缓存塞进 JSON，也不自动复制/修改源文件。

| 本轮必须完成 | 本轮不展开 |
| --- | --- |
| 场景/资产/实体持久身份；明确序列化契约 | 世界实体父子编辑、逐 GLB 节点编辑 |
| Save、Save As、Open、New/Exit 未保存保护 | 完整撤销/重做、自动保存、最近项目 |
| 恢复包装根、Cube、GLB、相机与显示开关 | 新材质通道、PBR、Gaussian |
| 文件级事务保存、打开失败隔离、写入冲突检查 | 完整项目包、自动复制依赖、缺失资产占位/修复 UI |
| 路径重基准、内容指纹、进程间单写者约束 | 产品 CLI/Agent、远程协同或插件协议 |

搬移验收针对“场景文件及其相对目录中的资产一起移动”。单独复制 .wfscene 不等于打包世界。完整项目包仍是 0.0.3 后续收尾/v0.1 的独立退出条件，不能因这一增量完成而删掉原路线图要求。

## 2. 当前代码必须先补齐的缺口

| 现状 | 本次需要改变 |
| --- | --- |
| EntityId、MeshAsset.id 是进程内整数 | 引入持久 UUID；保留会话句柄/GPU ID，禁止写入文件 |
| Entity 只有解码后的 asset 指针，路径仅留在导入结果中 | 保存 AssetRecord：来源、路径、内容 hash、导入配置及 CPU payload |
| EditorState 同时承载文档与编辑会话 | 建立 SceneDocument 与 SceneSnapshot 的明确边界；相机/选择留在会话 |
| UI 直接修改 Tint、Visible、相机和显示开关 | 收敛文档修改入口，准确维护 revision/dirty；本轮不做通用命令注册器 |
| OrbitCamera 仅有操作方法，没有完整恢复入口 | 增加校验过的 CaptureState/RestoreState，包含影响裁剪的 framing_radius |
| Prepare 一次上传一个 ImportedScene | 打开整场景需一次事务准备全部唯一资产，不能逐个发布半成品 |
| New/退出立即执行；系统 close 直接停止循环 | 接入统一 PendingAction 和 Save/Discard/Cancel 流程 |
| ImportService 只处理单文件导入 | 扩展为单工作线程的类型化文档 I/O 服务，串行处理 Import/Open/Save |

不能仅对 entities 数组做 JSON dump：那样无法恢复来源、共享关系、相机内部状态，也无法保证覆盖保存与场景切换的正确性。

## 3. 文件格式 v1

扩展名建议为 .wfscene；内容是 JSON，format="worldforge.scene"，format_version=1。
文件编码 UTF-8，无 BOM；固定字段输出顺序，2 空格缩进，结尾换行，便于 diff。

[完整示例](../formats/scene-v1.example.wfscene) 包含 Cube 和两个引用同一 Duck 资产的对象。示例 UUID 是演示数据；Duck SHA256 取自已经验证的样例，assets/小鸭.glb 需要使用者自行准备，不随文档打包模型。

| 字段 | 契约 |
| --- | --- |
| format / format_version | 必填；v1 只读写已知版本，不猜测迁移，不覆盖未来版本 |
| scene_id | 持久 UUID；New 创建；Save/Save As/Open 保持 |
| scene_revision | 非负 uint64 的十进制字符串，标识文档修改序号；避免跨 JSON 客户端整数精度差异 |
| assets[] | 场景引用的唯一 AssetRecord；通过 ID 引用，不靠数组下标 |
| entities[] | 保留 UI 顺序；持久 id、name、asset_id、transform、tint_srgb、visible |
| editor_state | 可选视图快照；缺省使用默认相机/显示值，不属于世界语义 |
| editor_state.camera | target、yaw_radians、pitch_radians、distance、framing_radius |
| editor_state.display | grid、wireframe、selection_overlay |

实体 transform v1 使用当前包装根的 position、rotation_degrees、scale，各为三个数；矩阵语义固定 T × Rz × Ry × Rx × S。源 GLB 的 local matrix/TRS 与层级通过重新导入保留，不展开复制进文件。未来加入世界父子层级或任意仿射矩阵时升级 schema，不向 v1 偷塞两套互相冲突的权威变换。

tint_srgb 明确颜色选择器数值的空间。GLB 的 baseColorFactor、纹理等继续来自资产，不重复序列化可推导的材质。

builtin 来源写 kind=builtin、name=cube、version=1；定义当前单位立方体及硬边生成规则。GLB 来源写 kind=glb、path、sha256、import_profile="static-glb-png-v1"。导入语义变化时升级 profile；不承诺未来任意 importer 都与该 profile 兼容。

不写入：会话 EntityId、Mesh/Texture GPU 缓存 ID、GL handle、选中/悬停对象、日志、导入进度、framebuffer 尺寸、FPS、面板布局。布局继续使用现有 layout.ini。

### 身份与序列化校验

- UUID 采用规范小写 8-4-4-4-12 字符串。创建使用注入的 IdGenerator，Windows 实现提供随机 UUID；不使用时间戳或资产 hash 冒充实体身份。
- scene_id、Entity 与资产 ID 在同一文件内全局查重，不允许互相混用。重复实体 ID、重复资产 ID、无效引用均拒绝。
- 多实体引用同一个 asset_id 时只重建一份 CPU payload，并由 GPU 缓存共享。不同 asset_id 即使路径/hash 一样也不自动合并；跨导入内容去重不在本轮。
- 首版 assets 只保存被实体引用的记录；打开遇到悬挂引用或无引用记录拒绝，空场景允许两数组均为空。
- 类型精确检查；不把字符串转数字、不把 0/1 当布尔。拒绝重复 JSON key、未知字段/来源类型、尾随垃圾、非法 UTF-8/嵌入 NUL、非有限及转 float 后溢出值，避免默默丢数据再覆盖原文件。
- TRS 复用 SetTransform 的范围：scale 每轴 0.01–100、位置绝对值不大于 1e12、角度不大于 1e6 度；Tint 每轴 [0,1]。空名字允许，名字最长 1024 UTF-8 bytes。
- 相机数值有限；pitch 在 ±1.45，distance 在 [0.0001,1e15]，framing_radius 在 [0.0001,1e15]；target 每轴绝对值不大于 1e12，yaw 绝对值不大于 1e6 弧度。需验证转换后矩阵有效；不静默 clamp 损坏文件。
- 浮点输出必须往返恢复当前 float 值；禁止格式化成三位小数。用 parse/serialize/parse 语义对比及不同数字 locale 测试把关。
- 诊断返回错误码、JSON 字段路径/资产 ID、用户可读消息。错误示例：InvalidReference at entities[2].asset_id。

### 首版预算

场景 JSON ≤16 MiB；JSON 嵌套深度 ≤32；解析器请求内存 ≤64 MiB；实体 ≤10000、资产记录 ≤256，单字符串 ≤4096 bytes（name 另限 1024）。不把这些数字作为最终产品容量承诺。

每个 GLB 沿用现有 256 MiB 文件/几何及 accessor 限制。候选场景按唯一资产累计 CPU geometry ≤256 MiB、decoded image ≤128 MiB；必须逐资产累计检查，而非每个文件各过限制后无限累加。导入到已有文档也要使用同一剩余预算规则。

打开期间旧场景与候选场景同时存在，峰值更高；加载器逐文件处理，不同时持有所有源文件字节。GPU 暂存也可能与旧缓存共存。超限返回明确错误并保留旧场景，不先卸载旧资源来“凑空间”；不声称控制了总进程/驱动内存。

## 4. 路径与资产身份规则

运行时 AssetRecord 保留解析后的规范绝对路径；序列化时按保存目标重新计算 path。不得以进程 cwd 解析场景中的相对路径。

| path.base | path.value | 行为 |
| --- | --- | --- |
| scene | assets/小鸭.glb 或 ../models/小鸭.glb | 相对 .wfscene 所在目录；允许父目录引用，因为本轮是外部引用文档 |
| absolute | D:/models/小鸭.glb | 仅本机定位；不同盘无法表达相对路径时使用，并在保存摘要提示外部绝对引用 |

首版只支持 Windows 本地磁盘文件。内部 UTF-8，平台层转换到 Windows 宽字符路径；序列化统一正斜杠。拒绝 URL、UNC 网络路径、设备路径、盘符相对路径（D:foo）、ADS 和嵌入 NUL。路径解析、比较和文件身份处理归平台适配器，不把 Win32 或 filesystem 引入 domain。

符号链接/目录重解析点通过平台层解析到真实目标用于身份检查；保留/生成的引用以规范目标为准。若目标或父目录无法可靠解析，明确失败。首版不宣称可搬移保留符号链接别名，也不把允许 ../ 的外部引用场景当作安全沙箱。

### Save As

先将旧引用解析到绝对目标，再以新场景目录计算相对路径；绝不能原样复制相对字符串。例如 C:/A/world.wfscene 的 models/duck.glb 另存到 C:/B/world.wfscene，仍应指向 C:/A/models/duck.glb，而不是 C:/B/models/duck.glb。

Save As 保持 scene/entity/asset IDs，表示同一文档的另一存放位置；不是创建独立世界身份的 Fork。成功才切换 current_path 和写锁；失败或取消时旧路径、旧锁、dirty 状态不变。目标与任一源资产的规范路径/文件身份相同，必须拒绝覆盖。

### 内容 hash 的用途

首次 GLB 导入时，对实际解析的完整字节计算 SHA256，并将来源与 hash 一起发布。打开时先读字节并核对 hash，再解析同一份字节；不能 hash 一次、重新打开另一份变化的数据解析。

保存前检查所有被引用 GLB 可读取且 hash 仍与已加载版本一致。缺失或变化则失败，提示重新导入/恢复源文件；不静默更新 hash、也不写出承诺了错误版本的场景。源文件在保存后仍可被外部修改，重开时会明确报 SourceChanged；外部引用不提供资产内容保管。

SHA256 校验不等于复制资源，也不是资产去重策略。builtin 不使用外部路径/hash。source.path/name/hash 必须在元数据中保存，不能只依赖 Console 文本。

## 5. 数据与模块调整

~~~mermaid
flowchart LR
  UI["面板 / 菜单"] --> Session["EditorState + DocumentSession"]
  Session --> Doc["SceneDocument：实体 / 资产表 / revision"]
  Session --> Snapshot["SceneSnapshot：可序列化值数据"]
  Snapshot --> Codec["io SceneCodec：JSON"]
  Codec --> Storage["platform SceneStorage：路径 / 写锁 / 文件"]
  Storage --> Worker["单个 DocumentIoService worker"]
  Worker --> Candidate["PreparedScene：未发布文档与资产"]
  Candidate --> GPU["主线程暂存全部 GPU 资源"]
  GPU --> Commit["一次性替换活动场景"]
~~~

图示是职责连接，不表示工作线程中的文件处理发生在 codec 之后的固定调用顺序；Save 与 Open 的准确顺序见后文。

| 层与建议文件 | 主要职责 |
| --- | --- |
| domain/scene_document.h/.cpp | 持久 ID 值、AssetSource、SceneDocument、实体校验、文档 revision；CPU 数据和不可变 payload 所有权 |
| domain/scene_snapshot.h | 纯值 SceneSnapshot；显式排除指针和运行时 ID，editor_state 作为可选快照数据 |
| editor/editor_state.* | 拥有活动文档和会话；BuildRenderScene 根据 asset_id 查资产；导出快照/一次性替换；会话整数 ID 可保留 |
| application/document_session.* | current_path、已保存 revision、dirty、PendingAction、operation token 和写锁租约 |
| application/document_io_service.* | 从现有 ImportService 演进；类型化 Import/Open/Save 请求，共用一个 worker，不搭通用 Job 系统 |
| io/scene_codec.* | JSON 编解码/版本与字段校验；无文件、Editor、SDL、GL 依赖 |
| platform/scene_storage.* | UTF-8 路径、SHA256/UUID 实现、受限读取、临时写入、提交/恢复、进程写锁；由接口注入 |
| platform/file_picker.* | 从 GlbFilePicker 抽出复用邮箱，分别配置 GLB/Scene Open/Scene Save 对话框 |
| render/viewport_renderer.* | 整场景资源 Prepare，事务成功才允许文档切换；资源仍主线程 RAII |
| ui/studio_panels.* / studio_app.* | 菜单、快捷键、脏标记、未保存提示、任务展示及组合 |

SceneDocument 是唯一业务权威：实体保存 asset_id，AssetRecord 统一持有不可变 ImportedScene；不同时维护两个可独立修改的实体数组。SceneSnapshot 是保存/加载边界 DTO，不是另一个长期运行的世界。

相机、显示选项、选择、日志继续属于 Editor 会话。EditorState 不直接调用文件 I/O。新代码继续 .cpp/.h、Google C++、函数空行、核心中英文注释、RAII、显式错误，不使用异常或 std::filesystem。

JSON 适配器首选已经固定的 tinygltf_json_c C API。已核对它提供带 depth/memory/string 上限的解析入口、树访问、stringify 和显式释放；解析对象保留条目，可在适配层逐层检查重复键。接口藏在 io 内，不向 domain 暴露。当前符号已由 wf_tinygltf 编译，禁止再生成一份重复实现。

实施第一步必须做 JSON 数字往返、UTF-8、重复键及 locale 探针；探针不过即调整 codec 实现，不降低契约。长期架构草案里的 nlohmann/json/Catch2 不是已固定的现有依赖，本轮不因草案引入异常包装或迁移测试框架。

## 6. dirty、revision 与 UI 行为

文档 revision 只在成功改变场景语义时递增：新增/删除实体、导入成功、名称/TRS/Tint/Visible 变化。无效输入、等值赋值、选择/清选、日志或布局不递增。所有相关 UI 写操作收敛到 EditorState 的校验入口；这为后续 Undo 留边界，不预先实现 CommandRegistry。

document_dirty = current_document_revision != saved_document_revision；未命名且有初始默认 Cube 的 New 文档视为未保存。打开成功建立 clean 基线；保存成功记录此次快照 revision，保存失败不清 dirty。New 创建新 scene_id；保存不额外增加 revision。

相机和显示开关只随显式 Save/Save As 记录到 editor_state，不增加业务 revision、不因为一次鼠标旋转触发退出的“世界未保存”提示。Save 即使文档 clean 也可以执行，保存当前视图。恢复视图后不自动 Frame，否则会覆盖存下来的相机。缺少 editor_state 时使用默认值；选择默认清空。

File 增加 Open、Save、Save As，保留 Import GLB；快捷键建议 Ctrl+O、Ctrl+S、Ctrl+Shift+S。文本输入/模态弹窗按 ImGui 当前上下文处理，不能抢正在编辑的输入。窗口标题显示文档名和 * 脏标记；错误附带资产/路径信息。

New、Open、File Exit、系统窗口关闭共用一个离开流程：

    有未保存世界修改 → Save / Discard / Cancel
    Save → 保存成功后继续原动作；取消选框/保存失败则停止原动作
    Discard → 允许继续，但在 Open 真正成功前仍保留旧场景
    Cancel → 无状态变化

Open 文件对话框取消、文件错误或加载失败都保留旧文档与 dirty。New 在确认后创建默认 Cube；Open 空文件场景保持空，不偷偷添加 Cube。一个 PendingAction 防止多次 close/Open 叠加执行。

## 7. 保存事务

建议 Save/Open/Import 共用一个 I/O busy 门禁。任务执行时窗口继续绘制，但禁用文档编辑、相机/显示变更和新的文件动作，降低首版快照竞争复杂度；保留错误展示。无假百分比，显示读取/校验/写入/提交阶段。对话框取消正常返回 Idle。

Save 步骤：

1. 主线程捕获已校验 SceneSnapshot、当前 generation、revision、operation token。Save As 先选目标，重新计算相对引用。
2. 取得目标写锁；已有当前锁保持到 Save As 成功。目标是当前源资产或其别名时拒绝。
3. worker 验证资产存在/hash、编码 JSON；检查输出大小。现有目标须与打开/上次保存时的文件 hash 一致；首次 Save As 若目标已存在，先读取其 hash，再由应用展示覆盖确认并绑定该基线；原生选框的覆盖提示不能替代这一步。提交前再核对，冲突拒绝。
4. 在目标同目录创建唯一临时文件，以 CREATE_NEW 防止误覆盖；完整写入、检查短写，FlushFileBuffers 并关闭写句柄。
5. 已存在目标用平台替换原语和唯一备份路径提交；新目标用同卷、不允许覆盖的移动提交，提交期间出现目标则报告冲突。不得用 truncate 原文件后逐段写 JSON。
6. 确认已提交目标可读取且 hash 对应完整快照后，向主线程返回 CommitSucceeded。再更新路径/锁/保存基线，清 dirty。
7. 若提交前失败，删除本次拥有的临时文件，旧目标不变；若系统报告部分替换或状态不确定，保留临时/备份并返回 RecoveryRequired，禁止假报保存成功或自动反复覆盖。

Windows 初选 ReplaceFileW（已有目标）与 MoveFileExW（新目标、同卷且不覆盖）。同目录临时文件避免跨卷“移动变复制”。ReplaceFileW 的 WRITE_THROUGH 标志不受支持，必须显式 Flush 临时文件；某些错误允许旧文件被移到备份，不能把任何失败都解释为“旧路径未变化”。需按错误码和实际文件 hash 定位旧/新内容，恢复旧目标；若无法安全恢复，报告旧内容备份路径并停留 dirty。依据：[ReplaceFileW](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew)、[FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)。

平台返回值需区分 NotCommitted / Committed / RecoveryRequired；保存结束收到的陈旧 token 不应修改当前 Editor，但已经完成的磁盘提交也不能假装撤销。首版通过 busy 期间不允许切场景、提交临界段不接受取消来约束这个问题。

保存验收承诺为本机受支持文件系统上的完整文件提交与可恢复旧内容，不承诺任何硬件断电、网络盘或云同步冲突下绝对持久。保留最近一次正常保存备份；异常备份不自动删除，清理只操作本次/明确识别的所有者路径。

### 写锁与外部修改

用规范目标路径旁的 sidecar 锁文件，由 CreateFileW 获取拒绝共享的持有句柄；锁的有效性来自活句柄，不来自“文件存在”。进程退出释放句柄，遗留空文件可重新打开，不能见到 .lock 就永久拒绝。依据：[CreateFileW](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew)。

Open 候选与 Save As 获取新租约，当前文档租约只在新操作成功切换后释放。若目标身份就是当前文档，复用已有租约，不能再次排他打开自己的锁；重新打开当前文件仍须走未保存保护与完整候选加载。另一个 Studio 占用时明确拒绝；首版没有只读打开模式。锁失败不能退化为无保护写入。需规范大小写/目录重解析别名；场景目标若是硬链接多别名且无法保证单一锁身份，首版拒绝编辑。

写锁只约束协作的 Studio；外部编辑器不遵循 sidecar。因此仍需目标 hash 冲突检查。它不能消除所有非协作进程的恶意竞态，不将本机编辑保护宣传为协同编辑协议。

## 8. 打开事务与资源边界

~~~mermaid
sequenceDiagram
  participant UI
  participant Main as 主线程会话
  participant Worker as I/O worker
  participant GPU as Renderer
  UI->>Main: Open（先处理未保存修改）
  Main->>Worker: 路径 + operation token
  Worker->>Worker: 获取候选锁 / 解析 JSON / 校验引用
  Worker->>Worker: 逐唯一资产读取、hash、导入并累计预算
  Worker-->>Main: PreparedScene 或完整错误
  Main->>Main: 校验 token / generation
  Main->>GPU: 一次准备所有候选资源
  alt 全部成功
    Main->>Main: 替换文档、视图、路径、锁；清选择/dirty
    Main->>GPU: CollectUnused
  else 任一步失败
    Main->>Main: 释放候选；保留旧场景与状态
  end
~~~

PreparedScene 必须拥有所有 CPU 资产、实体及恢复视图的合法状态；主线程发布前不再执行可失败的磁盘 I/O。先准备候选运行时实体/ID 映射，再上传 GPU，最后用移动/交换方式提交，不能循环 AddImported 触发逐个实体发布、重选与取景。

同一 asset_id 只加载一次；隐藏实体的资产也验证/准备，避免打开成功后显示时才遇到缺资源。任何缺失文件、hash 不符、GLB/PNG 错误、预算超限或 GPU 失败都让整次 Open 失败。本轮不生成残缺占位对象。

GPU 必须暂存“整份候选场景”的 Mesh/Image/Sampler：可把各 ImportedScene 的唯一 Mesh 汇总给一次 Prepare，或增加等价的 PrepareBatch 接口。不能对多个资产逐个 Prepare 成功后就将它们逐个计入已发布新场景。旧场景强引用与写锁持有到最终切换之后；失败释放候选并回收临时资源，原图像/选择/相机仍可用。

当前 generation 继续隔离旧导入结果；另有单调 operation token 防止迟到文件对话框或旧文档动作被复用。Open 成功才增加场景 generation；失败不增加。New 增加 generation 并生成新身份。

关闭窗口遇到 I/O 进行中时记录退出请求，取消尚未开始/仍可取消的阶段并等待 worker 返回；进入保存提交临界段后必须等提交结果，再决定退出与保存提示。不 detach/强杀线程，不让 GPU 在 context 销毁后回调上传。

## 9. 失败验收矩阵

| 用例 | 期望 |
| --- | --- |
| Cube + 带贴图 GLB，编辑后退出重开 | 持久 ID、顺序、TRS、Tint、Visible、已存相机/开关一致 |
| 多实体引用同一 asset_id | 加载一次、共享 CPU/GPU；删一个不释放最后仍需资源 |
| 空场景、无选择 | 保存重开仍为空；Inspector/Frame 安全 |
| 中文/空格路径，场景改变 cwd 后打开 | 相对路径仅基于场景目录 |
| 同盘 Save As、跨盘 Save As | 重基准后仍指向原资产；绝对引用有提示 |
| 场景+内部相对资产目录一起搬移 | 原路径不可用时仍可打开 |
| 只搬场景、外部绝对/父目录引用失效 | 明确失败；不称为可携带项目包 |
| 缺失/被修改的 GLB，CRC 坏 PNG | 整次 Open 失败，旧世界和相机/dirty 不变 |
| 非法 JSON、未来版本、重复 key/ID、错误引用/类型/数值 | 不触发资产上传，不修改活动文档 |
| 超 JSON 深度、实体/资产数、累计图片/几何预算 | 有界失败，不仅逐文件检查 |
| 第 N 个资产 GPU 上传失败 | 前 N-1 个候选资源不发布，旧缓存计数/画面仍正确 |
| 临时文件创建失败、短写、flush/替换失败 | 原内容保留或可从备份恢复；dirty 不清 |
| 提交状态不确定、恢复也失败 | RecoveryRequired，列出有效候选/备份，不继续覆盖 |
| 外部修改目标、第二实例写锁占用 | Save/Open 明确 Conflict/Busy，不丢外部更改 |
| 重新打开当前文档、Save As 到当前路径 | 复用租约，不自锁；仍校验基线及未保存保护 |
| Save As 指向源 GLB/其别名 | 拒绝，源资产字节不变 |
| Save/Discard/Cancel 后续动作，选框取消 | 只在符合条件时继续原 New/Open/Quit；旧文档不提前清空 |
| 保存中退出、打开中退出、迟到回调 | 等待/丢弃规则生效，无越代发布、无悬空 GL/UI 调用 |
| 浮点往返、不同 locale、两次无修改保存 | 语义稳定；保存本身不增加文档 revision |

CPU 测试使用注入 IdGenerator/Storage/Reader 和故障点，不依赖真实对话框。
Windows 文件集成测试放临时目录，验证 UTF-8、写锁、替换、冲突与恢复，不对用户文件注入故障。
GPU 测试覆盖候选资源失败和重开图像/缓存；ImGui 测试覆盖菜单、dirty 和离开流程。
原生 Open/Save 选择框须额外手工或可用 Computer Use 验收，不能用程序化路径加载替代。

所有 C++ 变更必须通过现有 style check、Windows Release 和相关 CPU/GPU/UI 测试。验证前本文件中的项目均是计划，不是已通过结论。

## 10. 实施分段与交付顺序

| 阶段 | 内容 | 可单独检查的结果 |
| --- | --- | --- |
| A：格式与文档模型 | JSON codec 探针、SceneDocument/持久 ID、来源/hash、快照与脏状态入口 | 无窗口 round-trip、非法 schema/数值和共享引用测试 |
| B：文件事务 | 平台路径、锁、SHA、临时写/提交/恢复、Save As 重基准 | 临时目录中失败注入与多进程锁测试 |
| C：打开与 UI 集成 | 单 worker 演进、整场景 GPU 暂存、替换、菜单/快捷键、离开保护 | 真实 GLB 重开、失败保留旧场景、交互测试 |
| D：收尾验收 | 搬移、退出/取消、中文路径、开发包、实现与验收说明 | 可复现演示和明确未验项 |

按当前工程估计约 12–18 小时，建议拆成 3–4 个开发晚上；其中 B 的 Windows 文件语义与 C 的退出保护需要预留故障验证时间。该估计不是交付承诺，不以省略回滚/dirty/资源验收把它压成一个晚上的表面功能。

最先实现 A，再进入 B/C；本轮设计阶段不创建空接口、不修改 C++。完整项目包、基础 Undo 与不透明 PBR 仍按 0.0.3/v0.1 退出条件继续交付。
