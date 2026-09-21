# 02：静态 Mesh 导入、实例与异步发布

原始增量：2026-09-20，0.0.2，历史任务名 P1.1。补充日期：2026-09-21；以下按 dce2f6b（0.0.2-texture-dev）核对调用链，并明确标出后来加入的贴图行为。P1.1 是旧任务编号，不是新版路线图的功能优先级 P1。

本篇回答：一个自包含 GLB 怎样从磁盘变成可编辑、可绘制、可删除的模型，以及为什么失败和迟到结果不会直接改坏旧场景。建议先理解[第一篇的主线程帧循环](0001-gui-framework.md)。

## 1. 这次增量建立了什么

输入是 glTF 2.0 的二进制 GLB；输出是内存中的不可变 ImportedScene，再由 EditorState 增加一个可编辑包装根 Entity。用户移动、旋转、缩放整个模型，源文件节点层级与局部矩阵保留，只读展开。

原始 0.0.2 仅显示 baseColorFactor × Tint 的漫反射预览，保存 UV 但没有采样。2026-09-21 已增加嵌入 PNG 基础色、采样器、正确颜色转换和独立选中叠加。具体贴图数据/预算见[第三篇](0003-base-color-texture.md)，不要把旧截图的纯色预览当作当前限制。

当前仍是静态资产预览；没有动画播放、逐节点编辑、场景保存/重开、完整 PBR 或 Gaussian。

## 2. 沿着一次点击走完整条链路

~~~mermaid
flowchart LR
  Button["UI 发起导入请求"] --> Picker["SDL 原生文件选择"]
  Picker --> Mailbox["复制路径到线程安全邮箱"]
  Mailbox --> Worker["ImportService 单工作线程"]
  Worker --> Reader["SDL UTF-8 文件读取"]
  Reader --> Parser["内存 GLB 解析与校验"]
  Parser --> Asset["不可变 ImportedScene"]
  Asset --> Generation["主线程检查 generation"]
  Generation --> Upload["Prepare 暂存 GPU 资源"]
  Upload --> Root["AddImported 发布包装根"]
  Root --> Snapshot["BuildRenderScene"]
  Snapshot --> Draw["缓存资源上的逐 primitive 绘制"]
~~~

UI 的导入按钮只设置 PanelState.import_requested。RunStudio 下一轮调用 ProcessImports 时消费请求；它负责协调文件选择器、后台服务、GPU 和 Editor，而不把这些职责放进按钮回调。

| 步骤 | 实际入口 | 线程与输出 |
| --- | --- | --- |
| 选择路径 | [GlbFilePicker::Open / Complete](../../src/platform/glb_files.cpp) | 主线程发起；SDL 回调只写独立邮箱 |
| 读取字节 | ReadGlbFile | 工作线程；检查扩展名/文件尺寸并完整读取，保留中文 UTF-8 路径 |
| 解析资产 | [ImportGlb](../../src/io/glb_importer.cpp) | 工作线程；成功才赋值 ImportedScene 输出 |
| 管理任务 | [ImportService](../../src/application/import_service.cpp) | 一个工作线程；Reader 由组合根注入 |
| 验证并发布 | [ProcessImports](../../src/studio/studio_app.cpp) | 主线程；校验代次、Prepare、AddImported、按视口宽高比取景 |
| 构建绘制列表 | [EditorState::BuildRenderScene](../../src/editor/editor_state.cpp) | 主线程；过滤隐藏实体/非 active 节点，输出只读快照 |
| 上传与绘制 | [ViewportRenderer](../../src/render/viewport_renderer.cpp) | 当前 GL context 所在主线程 |

CMake 暂把 io 适配器与 application 服务合在 wf_import target；这是现有小规模实现，不代表已经完成可插拔导入框架。io 不打开文件，application 不调用 SDL/GL/ImGui；SDL 文件读取通过 Reader 注入，所以任务测试可以用内存数据替换磁盘读取。

## 3. 先分清五种数据

| 类型 | 保存什么 | 为什么不合并 |
| --- | --- | --- |
| MeshVertex / MeshPrimitive | 顶点、索引、基础色和一个 primitive 的材质绑定 | 一份 Mesh 可以包含多组独立绘制的 primitive |
| MeshAsset | 几何集合、边界、进程内资产 ID | 几何可由多个节点或实体共享 |
| ImportedNode | 源名称、mesh 索引、children、local/world、active | 变换和层级属于实例节点，不属于顶点 |
| ImportedScene | Mesh 列表、源节点、当前源 scene 根、边界、警告和统计 | 一次导入的不可变资产结果 |
| Entity / MeshDraw | Entity 是可编辑包装根；MeshDraw 是本帧的 Mesh 引用和最终矩阵 | 编辑器状态与渲染输入分开 |

类型入口：[mesh_asset.h](../../src/domain/mesh_asset.h)、[imported_scene.h](../../src/domain/imported_scene.h)、[editor_state.h](../../src/editor/editor_state.h)、[render_scene.h](../../src/domain/render_scene.h)。

同一个源文件可能包含多个 glTF scene。当前选择 default_scene，缺省时使用第一个；active 表示节点是否从这个源 scene 的根可达。它不同于用户编辑器里的整个世界。

目前 ReadMeshes 会转换文件中的全部 Mesh，节点图也会整体验证；只在快照中筛选 active 节点。它还不是按可见性或按需加载的资产流送系统，统计和上传预算也不能简单等同于当前可见三角形数。

## 4. 实例共享究竟共享到哪一层

~~~mermaid
flowchart TD
  A["Entity A：可编辑根"] --> Asset["shared_ptr<const ImportedScene>"]
  B["Entity B：同一资产的另一包装根"] --> Asset
  Asset --> Nodes["Node 0 / Node 1：各自的矩阵"]
  Asset --> Mesh["shared_ptr<const MeshAsset>"]
  Nodes -. "mesh 索引" .-> Mesh
  Frame["当前 RenderScene 的 MeshDraw"] --> Mesh
  GPU["按 Mesh ID 索引的 GPU 缓存"] -. "weak_ptr：不延长 CPU 资产寿命" .-> Mesh
~~~

例如一份资产的两个节点都引用 Mesh 0，快照会有两条 draw 描述，矩阵不同，但 GPU 几何只上传一次。若 Mesh 0 有两个 primitive，基础绘制会有四次 glDrawElements；共享缓冲不等于已经实现 instanced draw 或自动合批。

EditorState::AddImported 接受 shared_ptr，因此两个包装根也可以共享同一份 ImportedScene，测试覆盖了这种情况。当前 GUI 没有“复制为共享实例”的专门命令。

**重复从磁盘导入同一个文件会重新解析并生成新资产 ID，通常再次上传。** 当前没有文件路径缓存或内容 hash 去重；不能通过重复导入实验，声称已经验证跨导入去重。

GPU 缓存只弱引用 CPU Mesh。删除一个共享实体时其余引用仍保留资源；删除最后一个实体、且没有当前帧快照等强引用后，CollectUnused 才回收。隐藏只影响绘制列表，不释放 Entity 对资产的所有权。New Scene 清掉导入实体，但编辑器会话内的内建 Cube 资产常驻。

所有 ID 均为进程内用途：EntityId 由 EditorState 递增，Mesh/图片/纹理 ID 使用进程内原子计数器。它们不是可以写入项目文件并跨重启恢复的持久 UUID。

## 5. GLB 解析为什么不直接交给第三方库就结束

tinygltf C11 API 负责格式解析，自有适配层负责“这份输入是否能被当前 Renderer 正确处理”。

采用固定提交 0d9ce1a9561688c6c6e7471430d15a50a0b15f1a 的 v3 alpha C API。ParsedModel 用 RAII 释放解析 arena 和错误栈；解析选项允许借用输入 buffer，但最终顶点/索引/节点信息复制到自有数据中，不把指向输入字节或第三方结构的指针交给 Editor。

当前调用顺序：

~~~text
ImportGlb
  → 检查文件字节数
  → ValidateContainer：chunk 顺序、重复、对齐、范围
  → ParsedModel::Parse：第三方解析
  → ValidateFeatures：支持范围与公共限制
  → ReadMeshes / ReadPrimitive / ReadAccessor：转换几何
      → 当前版本在这里读取材质引用、解码基础色 PNG
  → ReadNodes：验证整图、选择源 scene、累计矩阵与边界
  → 全部成功后才替换 output
~~~

适配器的重要约束是“失败不修改调用者已有 output”。局部构造的新资产随失败返回销毁；主线程拿不到一个只完成一半的成功结果。

### buffer、bufferView 和 accessor

- buffer 是原始二进制数据。
- bufferView 是 buffer 中的一段字节区间。
- accessor 描述如何解释该区间：起点、元素数量、类型、分量类型，以及 normalized 等规则。
- byteStride 决定相邻元素之间隔多少字节；位置、法线等可以交错排列，不能总是假设紧密存储。

ReadAccessor 返回内部 AccessorView，先证明范围合法，再逐项读取：

~~~text
first = bufferView.byteOffset + accessor.byteOffset
element_bytes = components × component_size
stride = bufferView.byteStride，未指定时取 element_bytes
last_end = first + (count - 1) × stride + element_bytes
~~~

这是便于理解的地址公式，代码不会先盲目计算全部乘加。实际用减法和除法证明区间容得下 count 个元素，避免不可信 offset/count 先溢出再通过检查；同时校验 stride、对齐和分量类型。读取多字节值使用 memcpy，不把未对齐字节强转为 float 指针。

### 顶点和索引怎样归一化

POSITION/NORMAL 接受非 normalized FLOAT VEC3。UV0 接受 FLOAT VEC2，或 normalized U8/U16 VEC2；整数 UV 转换到浮点值。索引接受 U8/U16/U32，统一存成 uint32_t；无索引输入生成顺序索引。

每个三角形验证索引范围、有限数值和非零面积。缺 NORMAL 时，计算 normalize(cross(b-a, c-a))，展开每个三角形的顶点以保存平面法线；这可能增加顶点数，是预览补全，不是平滑法线重建或写回修复。

因此不能用“原始文件字节数”预测最终 GPU 几何大小。当前内建 Cube 为了各面硬边也保留独立顶点：36 个顶点、36 个索引、12 个三角形；它走通用 Mesh 路径，但并没有专门做最小顶点数优化。

## 6. 节点图、矩阵与包装根

ReadNodes 先建立 parent 关系，拒绝非法 child、多父节点和重复关系。再从所有无父节点的节点开始迭代遍历：若最终访问数小于节点总数，说明存在循环。随后按选中的源 scene 根遍历 active 子树，累计 world。

使用迭代容器而非深层递归，避免恶意或极深层级耗尽线程栈。UI 的源节点展开也使用显式待处理列表。

矩阵有三层：

~~~text
source_local = 源 matrix，或源 T × quaternion_R × S
source_world = parent_world × source_local
draw_model   = entity_wrapper × source_world
~~~

例如根节点平移 (2,0,0)，子节点平移 (0,1,0)，子节点几何点为 (1,0,0)，源 world 中该点为 (3,1,0)；包装根再平移 (10,0,0)，最终为 (13,1,0)。包装根旋转或缩放时同样左乘整条源层级。

源 matrix 原样保留，不强行分解成编辑器欧拉角；源 TRS 的四元数转换也在 importer 内完成。Inspector 的欧拉角只是包装根的编辑表示，其矩阵为 T × Rz × Ry × Rx × S。

保留源坐标、单位和原点，不自动居中、归一化尺寸或落地。校验矩阵有限、仿射且可逆；源节点可以带非零负缩放，但当前 Inspector 对包装根限制为正缩放。shader 的法线使用最终模型矩阵逆转置，预览按双面绘制。

### Frame 为什么不改变模型

Bounds 由实际顶点计算，再变换局部 AABB 的八个角得到保守的世界 AABB；只变换 min/max 两个点无法正确处理旋转。对选中源 scene 合并边界，Frame 时再考虑包装根变换。

相机定位到包围盒中心，半对角线作为球半径 r。有效半视角取：

~~~text
half_angle = atan(tan(22.5°) × clamp(aspect, 0.01, 1))
distance   = 1.1 × r / sin(half_angle)
~~~

宽视口由垂直角限制，窄视口由水平角限制；1.1 留出边缘余量。嵌套 AABB 可能比几何真实边界更松，这是当前保守取景的取舍。没有承诺地理尺度或极端浮点数值下的精度。

## 7. 后台状态机与 generation

ImportService 的一个 worker 在条件变量上等待，读文件/解析时不持有邮箱 mutex。锁只保护任务元数据、状态与结果移交，不把整个解析过程锁住。

~~~mermaid
stateDiagram-v2
  [*] --> Idle
  Idle --> Reading: Start 接受任务
  Reading --> Parsing: Reader 成功
  Reading --> AwaitingUpload: Reader 失败
  Parsing --> AwaitingUpload: 保存成功或失败结果
  AwaitingUpload --> Idle: 主线程 TakeResult
~~~

kAwaitingUpload 的实际含义是“结果等待主线程处理”，失败结果也会进入这一状态，并不保证接下来一定有 GPU 上传。busy 从 Start 持续到 TakeResult；解析结束但结果尚未取走时，第二次 Start 仍会被拒绝。

TakeResult 把状态复位后，主线程同步处理 Prepare/AddImported；当前没有在这段处理之间并发启动第二个主线程导入请求。它不是有界多任务队列，也没有抢占或重试调度。

generation 解决的是结果归属，而不是强制停止工作：

1. 在场景 generation=7 时发起选框或导入。
2. 用户 New Scene，EditorState 将 generation 变成 8，并创建默认 Cube。
3. 旧选框或 worker 仍可能完成，结果携带 7。
4. ProcessImports 比较 7 与 8，丢弃结果，不上传、不加到新场景。

文件选择结果与 CPU 导入结果各检查一次。后台任务可能仍消耗一段时间；当前没有用户取消解析的 token。

## 8. 发布事务、错误与退出

| 发生位置 | 处理方式 | 旧场景 |
| --- | --- | --- |
| 用户取消选框 | 邮箱产生空路径，显示 Import cancelled | 保留 |
| 文件打开/读取失败 | ImportResult 保留错误；主线程显示诊断 | 保留 |
| 格式或支持范围失败 | 不发布 ImportedScene | 保留 |
| 旧 generation 到达 | 主线程丢弃 | 新场景保留 |
| 任一 GPU 资源准备失败 | 暂存 RAII 对象释放，不合并新缓存、不 AddImported | 保留并可继续渲染 |
| 全部准备完成 | 合并缓存、增加包装根、选择并取景 | 增加一个完整对象 |

原始增量的 Prepare 暂存 Mesh；当前同时暂存 geometry/image/sampler，细节见第三篇。这里的事务保证针对代码能显式返回的读取、校验和 GL 上传错误，不表示能从任意进程级内存耗尽或驱动崩溃恢复。

退出时 ImportService 设置 stopping、唤醒 worker 并 join。若 worker 已进入读取/解析，会等待该阶段返回，再按停止标记退出；不 detach、不强杀线程，因此慢读取也可能拖延退出。

原生文件对话框的回调另有生命周期：userdata 持有独立共享 Mailbox，回调立即复制路径/错误，不触碰 Editor。Picker 析构把 mailbox 标为 inactive；迟到回调只释放自己的上下文。对话框使用空 parent，不借用可能先销毁的窗口；当前没有显式撤销系统选框的实现。正常取消、开框后退出等 Windows 行为仍需单独验收，不能由 worker join 测试代替。

## 9. 支持范围与统计读法

| 项目 | 当前边界 |
| --- | --- |
| 容器 | 自包含 GLB / glTF 2.0；默认 scene，缺省用首个 |
| 几何 | 静态 TRIANGLES；不支持动画、蒙皮、morph、稀疏、量化或压缩几何 |
| 材质 | OPAQUE；当前已有嵌入 PNG 基础色，其他 PBR 通道仍为预览限制 |
| 扩展 | required 全部拒绝；primitive/bufferView/node 结构扩展拒绝；可忽略外观项给警告，贴图变换/压缩等当前不支持组合明确失败 |
| 路径 | 平台 Reader 只接受 .glb；不加载外部或 data URI 资源 |
| 文件/几何预算 | 文件 20 bytes–256 MiB；累计转换后上传几何最多 256 MiB |
| 元素/层级预算 | 单 accessor 最多 800 万元素；节点与 Mesh 各最多 10 万 |
| 图片预算 | 每轴 4096、单图 RGBA8 64 MiB、每次导入独立图片总计 128 MiB；详见第三篇 |

这些是分项限制，不是整个进程的统一硬内存上限。解析 arena、源字节、转换中的顶点、既有资产和 GPU 缓存可能同时存在。

Assets/Console 的 read 是读取墙钟耗时，parse+decode 包含解析、校验和当前 PNG 解码，upload+publish 是主线程提交/发布墙钟耗时。它们不是 GPU timer。Performance 几何字节统计缓存 VBO/EBO 数据；与当前可见模型数量、draw 数、文件大小是不同维度。

当前整份 GPU 准备仍在主线程同步进行，可能造成大资产导入时短暂停顿。之后是否分批上传、做资产去重或流送，应由真实大场景测量决定，不能从 Duck 小样例推导吞吐能力。

## 10. 阅读测试，比只看成功截图更有用

| 文件 | 重点观察 |
| --- | --- |
| [generate_mesh_fixtures.py](../../tools/generate_mesh_fixtures.py) | 小型确定性 GLB 如何精确控制索引宽度、stride、节点和坏输入 |
| [glb_importer_test.cpp](../../tests/glb_importer_test.cpp) | 转换结果、层级矩阵、非法输入，以及失败时 output 保持原值 |
| [import_service_test.cpp](../../tests/import_service_test.cpp) | 注入 Reader、用 promise 卡住读取、拒绝并发 Start、旧代次、错误后重启和退出 join |
| [editor_state_test.cpp](../../tests/editor_state_test.cpp) | 包装根校验、旋转/非均匀缩放后的窄视口取景、选择语义 |
| [mesh_gpu_test.cpp](../../tests/mesh_gpu_test.cpp) | 共享资产的缓存计数、像素、隐藏、删实例、最后引用、New Scene 和重新导入 |
| [texture_gpu_test.cpp](../../tests/texture_gpu_test.cpp) | 后续贴图增量对同一发布/回收模型的扩展 |

原始增量见[2026-09-20 验证记录](../benchmarks/2026-09-20-mesh-import.md)；当前 8 项测试与真实 Duck 贴图结果见[2026-09-21 验收](../benchmarks/2026-09-21-base-color-texture.md)。本次补文档只对照代码，不把历史测试包装成刚重新执行。

## 11. 建议亲自跟踪的四个实验

1. 导入 GLB，展开源节点，修改 Inspector 的整体 TRS。预期所有子几何一起移动；源节点矩阵和源文件不改。依次跟踪 SetTransform → BuildRenderScene → draw.model。
2. 用窄视口给旋转、非均匀缩放后的对象按 F。预期模型完整入镜，位置和 scale 不变。跟踪 Bounds::Transformed → OrbitCamera::Frame。
3. 重复从文件导入，再删除一个对象。预期第二次导入通常增加新 Mesh 缓存，删除后对应缓存可回收；真正共享同一资产的实验请读 mesh_gpu_test 中两次 AddImported 传入同一个 scene 的代码。
4. 阅读 worker 测试中 promise 控制的旧代次案例，再尝试导入过程中 New Scene。小文件太快时手工难以复现，测试可稳定控制先后关系；预期旧结果不进入新场景，而不是保证旧工作立即停止。

读完后，应能解释一个对象为何需要同时有 Entity、Node 和 Mesh 三层，为什么后台解析不能直接发布实体，以及为什么“资源共享”“GPU 合批”“重复导入去重”是三项不同能力。下一篇继续沿这条链路解释[基础颜色贴图与选择显示](0003-base-color-texture.md)。
