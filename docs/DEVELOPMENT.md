# Windows 开发与运行

验证日期：2026-09-20。当前开发版本为 0.0.2 Mesh 预览：P0 框架和 P1.1 静态 GLB 导入已实现；完整 P1 和世界资产首期尚未交付。

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
4. 视口右键拖动旋转，中键拖动平移，滚轮缩放；悬停视口按 F 取景。可切换网格、线框。
5. Console 查看失败原因与预览限制；Assets 查看文件/阶段耗时；Performance 查看 GPU 几何缓存字节和 draw 数。
6. Scene 的 New 会提示丢弃当前内存场景。导入中 New Scene 会丢弃旧结果；忙时禁用新导入。

当前对象修改**不会保存**，关闭后丢失；只保留窗口面板布局。基础色漫反射、双面显示，不含贴图/PBR。仅支持 glTF 2.0 GLB 的静态 TRIANGLES；动画、蒙皮、morph、稀疏、压缩、量化、透明材质、外部 URI 与结构扩展均会明确报错。文件和累计上传几何各限 256 MiB。完整边界见 [实现说明](learning/0002-mesh-import.md)。

## 验证与打包

CTest 覆盖解析、任务、编辑器、Cube Studio smoke、Mesh Studio smoke 和 GPU 资源生命周期。GPU smoke 是开发测试入口，**不是产品 CLI/Agent 功能**。

```powershell
$p = Start-Process build/windows/Release/worldforge-studio.exe -ArgumentList '--smoke-test',"$PWD/artifacts/smoke" -WindowStyle Hidden -PassThru -Wait
$p.ExitCode
# 第三个参数可指定测试 GLB，走实际后台导入链路：
$p = Start-Process build/windows/Release/worldforge-studio.exe -ArgumentList '--smoke-test',"$PWD/artifacts/model_smoke","$PWD/artifacts/models/小鸭.glb" -WindowStyle Hidden -PassThru -Wait
$p.ExitCode
cmake --install build/windows --config Release --prefix out/WorldForge-dev
```

安装目录带运行库、文档和第三方许可证。开发包使用独立目录，不覆盖原有 0.0.1 Release 包。测试输出 studio.bmp 和 report.json；真实模型来源、结果和未自动验收项见 [P1.1 验证记录](benchmarks/2026-09-20-mesh-import.md)。

文件选择框原生操作暂未自动验收：Computer Use 环境初始化失败；请在体验时检查选择/取消及开框后退出。后台工作线程和 GPU 生命周期已单独验证，不能混称原生交互全部通过。

## 代码边界

- src/domain：CPU Mesh、节点、Bounds、只读 RenderScene。
- src/io：内存 GLB 解析、转换和校验。
- src/application：单任务后台服务。
- src/editor：模型根、相机、选择、快照与日志。
- src/platform：SDL/GL 宿主、UTF-8 文件读取与选择框邮箱。
- src/render：共享几何上传、索引绘制、离屏附件与资源回收。
- src/ui：面板展示、操作和导航。
- src/studio：依赖组合与主线程发布；apps/studio 为入口。

本仓库严格遵循 [Google C++ 规范](CODING_STYLE.md)，使用 .cpp/.h、函数空行和核心中英文注释；格式化用 check_style.py --fix。后续优先 P1 材质和保存重开，再推进混合渲染；CLI/Agent、插件、多平台继续后置。
