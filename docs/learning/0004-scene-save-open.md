# Scene 保存与重新打开：实现说明

日期：2026-09-28。运行版本：0.0.2-scene-dev，属于 0.0.3 的 Scene 增量。
设计基线见 [方案](../plans/2026-09-21-scene-save-open.md)，验证与未验项见 [验收记录](../benchmarks/2026-09-28-scene-save-open.md)。

## 1. 用户能完成什么

导入静态 GLB、添加 Cube，修改位置/旋转/缩放、Tint、Visible 后，用 File → Save 或 Ctrl+S 保存 .wfscene。关闭程序后 File → Open scene 或 Ctrl+O 重开，恢复对象和已保存视图。

File → Save As / Ctrl+Shift+S 保存到新位置并重算资产引用。窗口标题和 Scene 面板用 * 表示未保存世界修改。New、Open、Exit 和系统关闭共用 Save / Discard / Cancel；Save 成功后继续原动作，失败或取消选框则停止原动作。

相机和显示开关随显式保存记录，但不让世界变脏。只旋转相机后退出不会提示保存视图；需要保留该视图时主动 Ctrl+S。布局仍由 layout.ini 管理，选中对象不保存。

## 2. 数据模型为什么要拆开

[SceneDocument / SceneSnapshot](../../src/domain/scene_document.h) 将运行文档和文件值数据分开：

| 数据 | 内容与寿命 |
| --- | --- |
| SceneDocument | 场景 UUID、业务 revision、资产表、活动实体；EditorState 唯一拥有 |
| AssetRecord | 持久 UUID、来源、完整 GLB SHA256、不可变 ImportedScene payload |
| Entity | 会话整数 ID、持久 UUID、asset_id、包装根属性；asset 是表中 payload 的只读别名 |
| SceneSnapshot | 无指针、无 GL handle 的文件值；UUID、来源、TRS、颜色、Visible 和编辑视图 |
| CameraState | target、yaw/pitch、distance、framing_radius，包含重建裁剪所需内部数据 |

保留会话整数 ID，避免 UI 选择和 GPU 缓存直接依赖文件身份。文件只使用 UUID；导入器生成的 Mesh/Image ID 仍是进程内缓存身份。两个实体引用相同 asset_id，重开后绑定同一个 payload。

Entity 的公开查询已改成 const，UI 通过 SetTransform/SetAppearance 修改；不会再直接写 Visible/Tint 绕过 dirty。增加/删除/导入和有效属性变化递增 revision，等值赋值和无效修改不递增。保存成功才更新已保存 revision。首次未命名场景视为 dirty。

[EditorState](../../src/editor/editor_state.cpp) 的三个关键方法：

- CaptureSnapshot：提取值数据，后台不读取活动 Editor。
- BuildDocument：将已验证快照和 CPU 资产绑定成候选文档，发生在 GPU 上传前。
- PublishDocument：GPU 准备成功后移动候选文档，恢复相机/显示，清选择，增加 generation，建立 clean 基线。

## 3. 文件格式与解析

[scene_codec.cpp](../../src/io/scene_codec.cpp) 只编解码值，不访问文件或图形 API。示例见 [Scene v1](../formats/scene-v1.example.wfscene)。

文件采用 UTF-8 JSON，format=worldforge.scene、format_version=1。业务 revision 是十进制 uint64 字符串；TRS 使用世界单位和度，保持 T × Rz × Ry × Rx × S。GLB 内部节点矩阵、材质和 PNG 通过重新导入恢复，不重复写入 JSON。

解析使用已有固定 tinygltf_json_c C API，新增适配层执行：

- 精确字段/类型校验；拒绝重复 key、未知字段、未来版本、非法 UTF-8、NUL、错误引用和重复 UUID。
- 场景 JSON 16 MiB、深度 32、解析分配预算 64 MiB、单字符串 4096 bytes、名称 1024 bytes。
- 最多 10000 实体、256 资产；允许空场景，拒绝没有实体引用的资产记录。
- 校验 TRS、颜色、相机范围及相机矩阵有效性；失败保留调用者原输出。

MSVC 下上游解析器内部使用 strtod，因此用线程局部 C 数字 locale 包住解析。写出使用 classic locale 和 float 的 max_digits10，避免三位小数造成往返损失。测试实际切换德语数字 locale，核对字节稳定与浮点一致。没有修改上游代码，也没有引入 C++ 异常。

## 4. 路径、身份和保存事务

[scene_identity.cpp](../../src/platform/scene_identity.cpp) 使用 Windows CNG 生成随机 UUID v4 和 SHA256；平台失败返回空值，由上层停止操作。

[scene_storage.cpp](../../src/platform/scene_storage.cpp) 封装 Win32 宽字符文件 API：

1. 规范化本地磁盘路径，解析已有文件/父目录及重解析点；拒绝 UNC、设备路径、盘符相对路径与 ADS。
2. 相对资产路径始终以 .wfscene 所在目录解析，不使用进程 cwd。
3. Save As 从内存中的绝对来源重新计算路径；不同盘保留绝对引用并写 Console 提示。
4. 拒绝覆盖源资产及其文件身份别名；多硬链接场景目标拒绝编辑。
5. 获取 .wfscene.lock 的排他句柄；遗留锁文件本身不代表占用。重新打开当前文档复用已有租约。
6. 核对目标 hash 基线，防止外部修改被覆盖。另存覆盖先捕获基线，再显示应用确认，提交前重新核对。
7. 同目录唯一临时文件完整写入并 Flush，已有目标用 ReplaceFileW，新文件用同卷不覆盖移动。
8. 读回核对新内容 hash 后才报告成功；返回未提交、已提交或 RecoveryRequired。

正常覆盖会保留唯一名称的 .bak；本轮不自动清理备份，避免误删用户已有旁路文件。确认新场景可用后，可自行整理这些备份。异常路径明确列出目标/临时/备份位置；不会自动反复覆盖。某些 ReplaceFileW 失败可能移动旧文件，因此恢复路径检查实际内容；目标缺失时才尝试将验证过的旧备份移回。

工作目录应可写，因为打开就需要取得单写者租约。本轮没有只读打开模式，网络盘和协同编辑也不在范围内。

## 5. 为什么打开不会先清空旧世界

[scene_io.cpp](../../src/application/scene_io.cpp) 的 ReadScene 在后台建立独立结果：

~~~text
读取 JSON → 严格校验 → 按唯一资产读取同一份字节
→ 核对 SHA256 → GLB/PNG 导入 → 累计候选预算 → 返回候选
~~~

主线程再执行：

~~~text
BuildDocument → 汇总全部 Mesh 到一次 Renderer.Prepare
→ 全部 Mesh/Image/Sampler 准备成功 → PublishDocument → 回收旧缓存
~~~

隐藏实体的资产也准备。不能循环 AddImported，否则会逐个发布半成品并覆盖相机/选择。任何来源缺失、变化、解析或 GPU 失败，都保留旧场景。已有渲染器的 Prepare 整批暂存机制得到复用，GPU 测试在最后一个候选资源制造错误，验证此前暂存不进入缓存。

候选 CPU 几何累计限制 256 MiB，解码图片累计限制 128 MiB，导入到活动场景也检查剩余预算。按文件顺序读取并释放源字节；候选阈值在每个导入结果完成后检查。单文件解码临时内存、旧场景和候选会同时存在，这些限制不是总进程或驱动内存上限。

## 6. 后台线程与界面协调

[ImportService](../../src/application/import_service.cpp) 复用现有唯一 worker：GLB 导入保持原接口，Scene 任务经 StartTask / TakeTaskDone 串行执行，完成信号的 mutex 同步也负责发布结果。不并发启动导入与文档操作。

[DocumentController](../../src/studio/document_controller.cpp) 持有当前路径、hash 基线、写锁、待继续动作和阶段。后台闭包只持有独立结果、快照和租约，不捕获 Editor、窗口或 this。

本轮的阶段为 Idle、Unsaved、ChooseOpen/ChooseSave、PrepareSave、Overwrite、Saving、Opening、Error。期间禁用编辑和新文件动作，窗口继续绘制。关闭请求排队，等待当前 worker 结束后再处理未保存状态；不强杀线程或在 GPU context 销毁后发布资源。

文件选择器保留共享邮箱生命周期规则，增加可注入 DialogProvider。自动 UI 测试注入选择/取消结果，再执行实际菜单、后台任务和弹窗；这不等于原生 Windows 文件选择框已经自动验收。

## 7. 与设计草案的具体取舍

- 未引入另一个 DocumentIoService 线程或通用 Job 系统；复用 ImportService 的单工作线程，以 SceneIoResult 表达文档结果。
- 使用单操作 busy 门禁、选择器邮箱和 generation 隔离；没有额外单调 operation token。门禁直到结果被消费才解除，不能同时出现两个候选操作。
- 运行中的 I/O 不提供中途取消；关闭排队等待任务结束。选框仍可取消。这避免提交临界段取消含义不明确。
- JSON 使用既有 C 解析器，自有固定 schema 输出器；采用稳定缩进，短对象和向量保留在一行。
- 备份保留唯一名称，不自动轮换或清理。
- Windows 适配器独立于 domain；当前目标仍只验证 Windows，不能据此宣称 Linux 构建通过。

## 8. 建议体验顺序

1. 导入小鸭，改位置、Tint 和 Visible，调整相机，Ctrl+S 保存到可写文件夹。
2. 改位置后点击 New，分别体验 Cancel、Save、Discard。
3. 退出再运行，Ctrl+O 打开，检查对象、贴图和已保存相机。
4. Ctrl+Shift+S 保存到另一个同盘目录；检查 JSON 路径重算。
5. 关闭 Studio，将场景和相对资产目录一起移动，再打开。
6. 备份源 GLB 后暂时改名，打开应报错且保留当前场景；恢复名称后再次打开。

外部引用文件不是打包项目。完整项目包、缺失资产重定位、Undo、PBR、Gaussian、CLI/Agent 不在本次实现中。
