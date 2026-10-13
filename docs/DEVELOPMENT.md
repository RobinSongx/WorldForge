# Windows 开发与运行

验证日期：2026-09-28。当前版本为 0.0.2-scene-dev：静态 GLB、PNG 基础色、独立选择显示、Scene 保存/重开与未保存保护已实现。仍为 0.0.3 增量，完整 PBR、Undo、项目包和 Hybrid 尚未交付。

## 环境与构建

已验证 Windows x64、Visual Studio Community 2026（MSVC 19.51）、SDK 10.0.26100.0、CMake 4.2.1、RTX 4070 Ti。SDL3、ImGui、GLM、glad、tinygltf 的精确提交见 CMakeLists.txt。系统字体优先微软雅黑，回退 Segoe UI，不打包 Windows 字体。

```powershell
py -3 -m venv build/python
build/python/Scripts/python.exe -m pip install -r tools/build-requirements.txt -r tools/style-requirements.txt
cmake --preset windows -DPython_EXECUTABLE="$PWD/build/python/Scripts/python.exe" -DWF_ENABLE_GPU_TESTS=ON
cmake --build --preset windows-release --parallel 8
build/python/Scripts/python.exe tools/check_style.py
ctest --preset windows-release
build/windows/Release/worldforge-studio.exe
```

VS 2022 使用 windows-vs2022 / vs2022-release presets。VS 2026 preset 需要 CMake 4.2+；VS 2022 需要 CMake 3.25+。WF_ENABLE_GPU_TESTS 默认为 OFF；无图形环境保持关闭。cpu-only preset 不构建 Studio，不依赖 SDL/GL。

SDL_REVISION 显式固定；glad 使用 REPRODUCIBLE。tinygltf 采用固定的 v3 alpha C11 实现，关闭第三方文件和图像解码；额外输入验证位于自有适配器中。

## 如何体验

1. 打开 Studio，通过 **File → Import GLB...** 或 **Assets → Import GLB...** 选择自包含的静态 GLB。
2. 导入成功后自动取景；Scene 中选择模型根，展开查看只读源节点。
3. Inspector 修改整体位置、旋转、缩放、Tint、Visible；Frame selected 定位，Delete selected 删除。
4. 视口右键旋转、中键平移、滚轮缩放，悬停按 F 取景。Ground grid、Wireframe、Selection overlay 独立；选中叠加默认关闭，不影响 Inspector 编辑。
   Scene 的 Clear selection 按钮/菜单、Scene 空白点击、Scene/Viewport 聚焦时 Esc 可清选；文本输入、弹窗或导入忙时 Esc 不抢输入。视口点击尚未提供拾取。
5. Console 查看失败原因与预览限制；Assets 查看文件/阶段耗时；Performance 分别查看几何缓存、图片/采样器数量和纹理含 mip 字节估算；它们不是总显存。
6. Ctrl+S / File → Save 保存 .wfscene；Ctrl+Shift+S 另存，Ctrl+O 重开。相对来源以场景目录解析，另存重算路径。
7. New/Open/Exit 和系统关闭提供 Save/Discard/Cancel。导入或文档任务期间禁用修改与新文件动作；关闭请求等待 worker 完成。
8. 场景修改在标题中显示 *；相机/显示开关只随显式保存记录，不单独触发未保存提示。

Scene 文件引用外部 GLB，不自动复制资产；移动/修改源文件会导致重开失败。覆盖保存保留唯一 .bak，当前不自动清理备份；.lock 由活句柄决定占用。用户已反馈此版本体验验证通过；原生选框边界场景尚无逐项记录。PNG 基础色贴图 × baseColorFactor × Tint，使用线性空间漫反射和双面显示，不含完整 PBR。仅支持 glTF 2.0 GLB 的静态 TRIANGLES；动画、蒙皮、morph、稀疏、压缩、量化、透明材质、外部 URI 与结构扩展均会明确报错。文件和累计上传几何各限 256 MiB。PNG 每轴限 4096、每图 RGBA8 限 64 MiB、每次导入独立图片合计限 128 MiB；解码器分配限 192 MiB/线程，不等于总进程内存。JPEG、UV1、纹理变换和其他 PBR 通道未实现。完整边界见 [贴图实现说明](learning/0003-base-color-texture.md)。

## 验证与打包

CTest 共 10 项，覆盖解析、任务、编辑器、Cube/Mesh Studio smoke、Mesh/纹理 GPU 和真实 ImGui 输入。GPU smoke 是开发测试入口，**不是产品 CLI/Agent 功能**。

```powershell
$p = Start-Process build/windows/Release/worldforge-studio.exe -ArgumentList '--smoke-test',"$PWD/artifacts/smoke" -WindowStyle Hidden -PassThru -Wait
$p.ExitCode
# 第三个参数可指定测试 GLB，走实际后台导入链路：
$p = Start-Process build/windows/Release/worldforge-studio.exe -ArgumentList '--smoke-test',"$PWD/artifacts/model_smoke","$PWD/artifacts/models/小鸭.glb" -WindowStyle Hidden -PassThru -Wait
$p.ExitCode
cmake --install build/windows --config Release --prefix out/WorldForge-dev
```

本次 Scene 独立开发包位于 out/WorldForge-scene-dev，压缩包为 out/WorldForge-Windows-0.0.2-scene-dev.zip；旧 out/WorldForge-dev 保留上次版本。安装目录带运行库、文档和第三方许可证。开发包使用独立目录，不覆盖原有 0.0.1 Release 包。测试输出 studio.bmp 和 report.json；真实模型来源见历史 P1.1 记录；Scene 本轮结果见 [保存重开验收](benchmarks/2026-09-28-scene-save-open.md)，历史贴图记录仍保留。

2026-09-28 用户反馈此版本体验验证通过。自动 UI 测试通过注入选框结果覆盖后台保存/重开与 ImGui 未保存流程；原生 Windows 选框的取消、开框后退出等边界尚无逐项操作记录。

## 代码边界

- src/domain：CPU Mesh、图片、采样描述、节点、Bounds、只读 RenderScene。
- src/io：内存 GLB 解析、材质引用校验、受限 PNG 解码。
- src/application：单任务后台服务。
- src/editor：模型根、相机、选择、快照与日志。
- src/platform：SDL/GL 宿主、UTF-8 文件读取与选择框邮箱。
- src/render：几何/图片/采样器暂存上传、颜色转换、索引绘制、离屏附件与资源回收。
- src/ui：面板展示、操作和导航。
- src/studio：依赖组合与主线程发布；apps/studio 为入口。

本仓库严格遵循 [Google C++ 规范](CODING_STYLE.md)，使用 .cpp/.h、函数空行和核心中英文注释；格式化用 check_style.py --fix。后续按 ROADMAP 完成 0.0.3 剩余 PBR、基础撤销与项目携带性，再推进 Hybrid；CLI、Linux 验证遵循 ADR 0004，完整 Agent 和插件继续后置。
