# 01：Windows GUI 框架与真实视口

补充日期：2026-09-21。本文解释 GUI 框架的设计，并按代码基线 dce2f6b（0.0.2-texture-dev）核对当前实现。最初框架以 Cube 验证窗口、编辑与渲染；当前 Cube 已接入第二篇的通用 Mesh 路径，选择显示和颜色处理已接入第三篇。

阅读目标：能从程序入口追到一帧画面，说明数据由谁持有、什么时候修改、为什么 GPU 资源要按特定顺序释放。本文描述已经实现的代码；未来的 Hybrid Renderer、Command/Undo、项目保存不在这条链路中。

## 1. 先看整体：三个不同的职责

- SDL3 管窗口、系统事件和 OpenGL context。context 可以理解为 GL 命令操作的图形环境；有窗口不等于已经有可用的 GL context。
- Dear ImGui 管面板、按钮、输入和停靠布局，最后生成 UI 绘制命令。
- ViewportRenderer 管 3D 几何、shader、深度测试和离屏输出。ImGui 的 Image 控件展示这个输出。

因此，小鸭和 Cube 都是真实的 GPU 几何绘制。UI 上的标签、工具栏和角落提示则由 ImGui 绘制。两部分最后合成到同一个窗口。

~~~mermaid
flowchart LR
  Input["SDL 事件"] --> UI["ImGui 控件与输入"]
  UI --> Editor["EditorState：实体 / 相机 / 选择"]
  Editor --> Snapshot["RenderScene：当前帧绘制快照"]
  Snapshot --> Renderer["ViewportRenderer"]
  Renderer --> FBO["离屏颜色纹理 + 深度附件"]
  FBO --> Image["ImGui Image"]
  Image --> Window["默认 framebuffer：完整窗口"]
  Window --> Swap["SDL 交换缓冲"]
~~~

这里的分层已经落在源码和 CMake target 中，但还没有通用 RHI、多窗口渲染或插件系统。

## 2. 文件地图与建议阅读顺序

| 顺序 | 入口 | 阅读时回答的问题 |
| --- | --- | --- |
| 1 | [main.cpp](../../apps/studio/main.cpp) | 正常模式与 smoke 模式怎样进入 Studio？错误怎样交给用户？ |
| 2 | [RunStudio](../../src/studio/studio_app.cpp) | 谁创建依赖？主循环按什么顺序调用？ |
| 3 | [SdlHost](../../src/platform/sdl_host.cpp) | 窗口、context、GL 函数和交换缓冲怎样准备好？ |
| 4 | [DrawStudio / DrawViewport](../../src/ui/studio_panels.cpp) | UI 怎样修改状态，又怎样取得视口画面？ |
| 5 | [EditorState / OrbitCamera](../../src/editor/editor_state.cpp) | 实体、选择与相机数据放在哪里？ |
| 6 | [RenderScene](../../src/domain/render_scene.h) | Renderer 每帧真正需要哪些数据？ |
| 7 | [ViewportRenderer](../../src/render/viewport_renderer.cpp) | 几何如何上传，FBO 如何重建，绘制如何完成？ |

对应构建边界是 wf_platform、wf_imgui、wf_editor、wf_domain、wf_renderer；worldforge-studio 将它们组装起来。具体依赖见 [CMakeLists.txt](../../CMakeLists.txt)。Renderer 不调用 SDL，domain 不包含 GL/SDL/ImGui 类型；UI 当前直接调用 EditorState，尚未全部经由 Command。

## 3. 从入口到第一个窗口

main 识别开发用 --smoke-test 参数，将运行交给 RunStudio。正常模式失败时显示错误对话框；smoke 模式写错误文件和日志。这个入口服务自动验收，不是已经交付的产品 CLI。

SdlHost::Initialize 的顺序有实际依赖：

1. 初始化 SDL 视频子系统。
2. 请求 OpenGL 4.5 Core、双缓冲和 24 位深度缓冲。
3. 创建支持调整尺寸和高像素密度的窗口，初始 1440×900，最小 900×600。
4. 创建 GL context，并让它在当前线程上生效。
5. 通过 SDL_GL_GetProcAddress 让 glad 加载驱动的 GL 函数，核对 4.5 能力。
6. 正常模式请求交换间隔 1；隐藏的 smoke 模式请求 0。因此 smoke FPS 不能直接当作日常交互帧率。

glad 是函数加载器，不是渲染器。必须先有当前有效 context，再加载和调用对应驱动的函数。

随后 RunStudio 创建 ImGui context，初始化 SDL/OpenGL 两个后端，再创建 ViewportRenderer、EditorState 和面板状态。当前版本还创建导入服务和文件选择器，见[第二篇](0002-mesh-import.md)。

## 4. 一帧实际做了什么

下表只列正常交互主线；代码里的 smoke 分支是验收动作。

| 阶段 | 当前代码的操作 | 为什么放在这里 |
| --- | --- | --- |
| 系统事件 | SDL_PollEvent；先交给 ImGui SDL 后端，再处理退出和 DPI 变化 | UI 与应用共享事件来源，退出仍由应用负责 |
| 后台结果 | ProcessImports 接收结果、检查 generation、上传和发布 | 工作线程不修改 Editor，也不调用 GL |
| 准备 UI 帧 | 两个后端 NewFrame，再 ImGui::NewFrame | 更新输入、窗口尺寸和本帧 UI 状态 |
| Scene / Inspector | 按钮和属性修改 EditorState | 放在视口之前，使对象编辑可用于本帧绘制 |
| Viewport | 读取面板尺寸，生成 RenderScene，执行离屏渲染，再提交 ImGui::Image | 纹理尺寸或句柄可能在 Render 内改变 |
| 导航输入 | 视口 Image 悬停时处理拖动、滚轮与 F | 当前写在渲染之后，相机导航结果下一帧可见 |
| 完整窗口 | ImGui::Render，绑定默认 framebuffer，绘制 ImGui draw data | 将所有面板和视口纹理合成到窗口 |
| 呈现 | SdlHost::Swap | 将完成的后缓冲交给窗口显示 |

“即时模式 UI”表示每帧重新描述控件，不表示每帧销毁场景、重建全部 GPU 资源。EditorState 跨帧保存业务状态，Renderer 跨帧保存缓存，ImGui 自己保留停靠和控件交互状态。

窗口最小化时，程序仍先处理系统事件和导入结果，然后延迟并跳过 UI/视口绘制；不是另建后台渲染线程。

## 5. 为什么要先渲染到 FBO

如果把模型直接画到整个窗口，停靠面板改变尺寸、遮挡或布局时，很难把 3D 画面作为普通面板内容组合。当前采用“先生成视口图片，再交给 UI”的方式。

| 名称 | 本项目中的用途 |
| --- | --- |
| VBO | 保存顶点的位置、法线、UV |
| EBO | 保存三角形顶点索引 |
| VAO | 描述顶点属性布局及所绑定的索引缓冲 |
| shader / program | 变换顶点、计算预览颜色 |
| FBO | 将绘制输出定向到视口附件 |
| RGBA8 颜色纹理 | 保存视口结果，供 ImGui Image 采样 |
| 深度 renderbuffer | 支持三角形间的前后遮挡；当前不交给 UI 采样 |

ViewportRenderer::Render 先准备缺失的缓存资源，再调用 Resize。Resize 将每轴尺寸限制在 1–4096；尺寸未变直接复用附件，尺寸变化才重新创建颜色/深度附件并检查 FBO 完整性。调整视口尺寸不会修改 Mesh 顶点。

绘制顺序是绑定 FBO、设置 viewport 和 GL 状态、清颜色/深度、画 grid 和 Mesh、按开关叠加选中线。结束时恢复填充模式、解绑使用的纹理/采样器，并返回默认 framebuffer，供 ImGui 合成。

与 ImGui 共用 context 意味着必须明确 GL 状态的交接。当前 Renderer 为自己显式设置所需状态，并在退出时准备 UI 所需的边界；这不是能自动保存/恢复所有 GL 状态的通用渲染沙箱。

当前颜色契约是：shader 将光照结果编码成显示颜色后写进 RGBA8 视口，ImGui 直接合成，避免 framebuffer 再编码一次。该行为是在贴图增量中补齐的，详见[第三篇颜色空间说明](0003-base-color-texture.md)。

## 6. 逻辑尺寸、像素尺寸与上下翻转

ImGui 的可用面板大小是逻辑尺寸；UI 将其乘以 DisplayFramebufferScale，得到请求的离屏像素尺寸。例如逻辑区域 800×500、比例为 1.5 时，请求约 1200×750 的附件，实际值仍受 Renderer 的 4096 上限约束。

不要把三个量混在一起：窗口外观大小、Viewport 面板可用大小、离屏纹理像素大小。Performance 的 Viewport 尺寸来自 Renderer，反映最后实际分配的附件。

DPI 改变时 ApplyTheme 先恢复默认 style，再应用主题和缩放；直接在上一次缩放结果上累乘会使间距越来越大。字体优先使用系统微软雅黑，回退 Segoe UI；安装包不分发 Windows 字体。

ImGui::Image 使用 (0,1) 到 (1,0) 的 UV 展示视口，使 framebuffer 的上下方向与 UI 对齐。这与 glTF 图片自身的 UV 约定不同：不要为了修正视口朝向，再把模型基础色图片翻一次。

## 7. 编辑状态为什么不会变成悬空指针

EditorState 持有实体数组、相机、可选的 selection ID、显示选项和日志。PanelState 保存面板开关、导入提示、视口尺寸及帧间隔历史等 UI 状态。

选择保存 EntityId，而不是数组下标或长期 Entity*。删除会改变数组布局，增加对象也可能使 vector 扩容，因此 Inspector 当帧通过 selected() 重新查询；增删或 New Scene 后不得继续使用此前取得的实体指针。

Inspector 编辑 Transform 时先改副本，再调用 SetTransform 校验。拒绝 NaN/Infinity、过大输入和不可用的 scale 后，原值保持不变。当前根缩放每轴限制为 0.01–100，不能用编辑器输入零或负缩放。

RenderScene 每帧复制相机矩阵、显示选项、各 draw 的模型矩阵/Tint/选择标记，并共享持有不可变 Mesh。Renderer 不需要借用正在变化的 Entity。当前这是同步主线程的只读调用约定，不代表已经有跨线程渲染队列。

2026-09-21 后，选择与显示进一步分开：ClearSelection 只清除 ID；Selection overlay 只控制黄色线框，默认关闭。Ground grid、Wireframe 各自独立，操作不会改变模型变换。

## 8. 相机怎样工作

OrbitCamera 保存目标点 target、距离 distance、水平 yaw 和俯仰 pitch，Eye 根据这些参数计算。

- 右键拖动改变 yaw/pitch；pitch 限制在 ±1.45 弧度，避免相机方向接近向上轴导致轨道奇异。
- 中键沿相机 right/up 平移 target，平移速度随观察距离变化。
- 滚轮使用指数缩放，改变距离的比例，避免固定步长在远近场景下手感悬殊。
- Frame 根据变换后的世界包围盒定位 target，并按包围球和视口宽高比计算距离；窄视口不能只考虑垂直视角。

物体根 Transform 使用列向量约定，矩阵是 T × Rz × Ry × Rx × S，从右向左作用。shader 用模型矩阵逆转置变换法线，保证非均匀缩放时法线方向仍用于正确的预览光照。

导航只在视口 Image 悬停时消费；F 还要求没有文本输入且存在选择。Esc 的聚焦/输入保护及清选行为见第三篇。当前没有视口几何拾取、漫游碰撞或相机动画。

## 9. 生命周期：谁必须最后销毁

在 RunStudio 中，重要对象按依赖顺序创建，离开作用域时反向析构：

~~~text
构造：SdlHost → settings_path → ImGuiLifetime → Renderer → Editor
                                      → PanelState → ImportService → Picker

析构：Picker → ImportService（join）→ PanelState → Editor
          → Renderer → ImGuiLifetime → settings_path → SdlHost
~~~

这里省略了普通局部变量。重点是 Renderer 和 ImGui OpenGL 后端释放 GL 对象时，host 的 context 仍然存在且当前有效。之后才销毁 context、窗口，最后退出 SDL。

各所有者记录已经取得的资源，所以初始化中途返回失败也能清理已成功创建的部分。ImGuiLifetime 分别记录两个后端是否初始化成功，避免对未完成初始化的后端调用 Shutdown。

另一个容易忽略的点是 io.IniFilename 借用 settings_path.c_str()。字符串声明在 ImGui 生命周期管理对象之前，后续也不再修改它，保证 ImGui 销毁时指针仍有效。

layout.ini 保存的是窗口/面板布局。它不是场景文件，不保存对象、材质、相机编辑或显示选项。smoke 模式将 IniFilename 设为空，避免自动测试读写用户布局。

## 10. 验证能证明什么

| 证据 | 证明范围 | 不能据此推导 |
| --- | --- | --- |
| [editor_state_test](../../tests/editor_state_test.cpp) | 选择、删除、变换拒绝、取景和相机数值约束 | 实际驱动绘制正确 |
| Studio GPU smoke | 离屏有内容、resize、变换、经输入队列触发相机、截图和 GL 错误检查 | 所有图像颜色都正确或所有鼠标操作都已覆盖 |
| [selection_ui_test](../../tests/selection_ui_test.cpp) | 当前版本真实 ImGui 控件、Esc、文本/弹窗保护、Scene 空白点击 | Windows 原生文件对话框已验收 |
| [texture_gpu_test](../../tests/texture_gpu_test.cpp) | 当前贴图像素、过滤、颜色转换及资源生命周期 | 完整 PBR 或跨厂商 GPU 一致性 |
| Performance 帧间隔 | 本机 CPU 帧间隔和 draw/cache 观察 | GPU 执行时间或完整显存占用 |

初始框架到当前版本的验证记录见[开发与运行](../DEVELOPMENT.md)和[贴图验收](../benchmarks/2026-09-21-base-color-texture.md)。此轮补充文档只核对源码，不新增运行测试结论。

## 11. 带着预期结果做实验

| 实验 | 预期观察 | 对应代码 |
| --- | --- | --- |
| 一个 Cube，关闭全部显示附加项 | 基础 Mesh 一次 draw；关闭叠加不清选 | Render / DisplayOptions |
| 依次开启 Ground grid、Selection overlay | 当前单 primitive Cube 的 draw 数各增加 1；选中叠加需仍有选择 | Render 的 grid/overlay 分支 |
| 再开启 Wireframe | 只画基础线框，不重复画黄色选中叠加；grid 仍独立 | Render 的 wireframe 条件 |
| 调整 Cube 的非均匀 scale | 形状和预览光照变化；输入零缩放被拒绝 | SetTransform / 法线矩阵 |
| 拖动停靠分隔线缩小视口 | Performance 像素尺寸变化，物体数据不变 | DrawViewport / Resize |
| 点击 Scene 空白，再按 F | Inspector 无选择，F 不移动相机、不删除对象 | ClearSelection / F 条件 |
| 保存布局后关闭并重新运行 | 面板布局恢复；对象编辑不恢复 | IniFilename / EditorState 构造 |

读完后，应能解释：为什么 Render 必须在 ImGui::Image 之前调用、为什么导航可能下一帧生效、为什么析构顺序决定 GL 资源能否正确释放。接着阅读[静态 Mesh 导入](0002-mesh-import.md)，理解如何把真实资产接入同一条视口链路。
