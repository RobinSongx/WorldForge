# Windows 开发与运行

验证日期：2026-09-19。当前版本为 0.0.1 桌面框架预览，P0 核心已实现；Mesh 资产系统和完整首期尚未交付。

## 已验证环境

Windows x64、Visual Studio Community 2026（MSVC 19.51）、Windows SDK 10.0.26100.0、CMake 4.2.1、Python 3.12、RTX 4070 Ti。SDL3 3.2.28、ImGui 1.92.5 docking、GLM 1.0.1、glad 2.0.8 的精确提交固定在 CMakeLists.txt。

UI 使用系统 Segoe UI（若存在），不打包 Windows 字体。SDL 静态链接；发布包附带构建工具链允许再分发的 MSVC runtime。

## 配置与构建

需要 VS 的 Desktop development with C++、CMake 与 Python。首次配置下载依赖；后续使用构建缓存。

```powershell
py -3 -m venv build/python
build/python/Scripts/python.exe -m pip install -r tools/build-requirements.txt
cmake --preset windows -DPython_EXECUTABLE="$PWD/build/python/Scripts/python.exe"
cmake --build --preset windows-release --parallel 8
ctest --preset windows-release
build/windows/Release/worldforge-studio.exe
```

VS 2022 使用 windows-vs2022 / vs2022-release presets。VS 2026 preset 需要 CMake 4.2 或更新；VS 2022 可使用 CMake 3.25+。如果 Python 不在 PATH，通过 Python_EXECUTABLE 指定其真实路径，不把本机专用路径提交到仓库。

SDL 从源码归档构建时显式指定 SDL_REVISION，避免把上层 WorldForge 的 Git 仓库误认为 SDL 版本。glad 使用 REPRODUCIBLE，从固定版本内置规格生成。

## 验证与打包

CPU 测试覆盖实体 ID/选择/删除、变换矩阵、非法数值、相机边界、frame selected 和日志上限。可用 cpu-only preset 关闭 Studio，核心测试不需要窗口或图形上下文。

GPU smoke 是开发验证入口，不是面向 Agent 的产品 CLI：

```powershell
$p = Start-Process build/windows/Release/worldforge-studio.exe -ArgumentList '--smoke-test',"$PWD/artifacts/smoke" -PassThru -Wait
$p.ExitCode
Get-Content artifacts/smoke/report.json
cmake --install build/windows --config Release --prefix out/WorldForge
```

smoke 创建隐藏的实际 GL 上下文，运行 36 帧，检查非空视口、窗口/FBO resize、变换、ImGui 鼠标轨道输入、面板数量、GL 错误，并输出 studio.bmp/report.json。GPU 环境不可用时失败，不跳过后宣称通过。设置 WF_ENABLE_GPU_TESTS=ON 可注册到 CTest，默认 CI 仅 CPU 测试，因为托管 runner 不保证 GL 4.5。

本机 Release CPU 测试与 GPU smoke 已通过，安装目录中的程序也已通过同一 GPU smoke，截图已目视检查。自动检查不能代替对所有鼠标控件和所有显示器的全面测试；多显示器动态 DPI 与更多 GPU 仍需后续验证。

## 操作与数据边界

- Scene 中选择；+ Cube 新增；Inspector 编辑位置、旋转、缩放、颜色和显隐。
- 视口右键拖动旋转、中键拖动平移、滚轮缩放，F 或 Frame 定位所选。
- View 菜单开关面板，Reset layout 恢复布局。
- Scene 的 New 会提示丢弃当前内存场景。
- 当前不保存场景，关闭程序丢失物体修改；只有布局通过 SDL 用户配置目录中的 layout.ini 保存。
- shader 是简单 diffuse 预览，不是 PBR。GLB、项目保存、Gaussian、CUDA、文件资产库均未实现。Assets 仅含内建 cube。

## 当前代码边界

- src/editor：不依赖 SDL/GL/ImGui 的示例实体、变换、相机与日志。
- src/platform：SDL 生命周期、GL 上下文、交换与 GPU 信息。
- src/render：离屏 framebuffer、基础 shader、grid/cube、resize 与资源释放。
- src/ui：停靠布局、面板与输入映射。
- src/studio：组合与主循环、smoke 验证，apps/studio 是入口。

此处是可运行切片，不是完整架构全部模块的实现。下一步引入真实 Mesh 数据与导入任务，再以服务/快照替换示例状态与 renderer 的直接参数。
