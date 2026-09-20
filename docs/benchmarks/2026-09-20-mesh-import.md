# P1.1 验证记录

日期：2026-09-20。Windows x64 / MSVC 19.51 / RTX 4070 Ti。开发版本 0.0.2；不是正式 Release。此次验证不构成大型模型性能承诺。

## 自动检查

- 固定 clang-format 21.1.8 Google preset + cpplint 2.0.2，全量自有 C++ 检查。
- Windows Release 构建。
- glb_importer：三种索引宽度、归一化 UV、offset/stride、无索引和平面法线、多 primitive、共享 Mesh、matrix/TRS/default scene；31 类非法输入必须失败且输出不变。
- import_service：受控 reader 阻塞期间拒绝重复提交、保留场景代次、失败恢复、重新提交、退出 join。
- editor_state：选择/删除/ID、非法变换隔离、旋转与非均匀缩放后窄视口八角取景、相机边界和日志上限。
- studio_gpu_smoke：原有 Cube、resize、根变换、ImGui 相机输入、六个面板、像素和 GL 错误。
- studio_mesh_smoke：通过真实后台导入和主线程发布路径导入生成 GLB，移除 Cube，再检查模型像素、resize、根变换与输入。
- mesh_gpu_smoke：独立真实 GL 上下文验证共享几何、多个实例、变换、显隐、失败隔离、最后引用释放、New Scene、Cube 共存和重新导入。

这六个 CTest 项在本机通过。GPU 冒烟创建隐藏窗口和真实上下文；没有 GPU 时失败，不能用 CPU 通过代替。

## 真实模型与中文路径

使用 [Khronos Duck](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/c6a6bd13ab2b3c685c7903d03561b8a9392f38b8/Models/Duck)，原作者 Sony，copyright 2006；模型采用 [SCEA Shared Source License 1.0](https://github.com/KhronosGroup/glTF-Sample-Assets/blob/c6a6bd13ab2b3c685c7903d03561b8a9392f38b8/LICENSES/SCEA.txt)。模型未修改、未纳入仓库或安装包，仅下载到忽略的 artifacts/models/小鸭.glb 进行本地验证。

- 样例库固定提交：c6a6bd13ab2b3c685c7903d03561b8a9392f38b8。
- GLB：120484 字节；2399 顶点、4212 三角形、1 Mesh、3 节点。
- SHA-256：65BF938F54D6073E619E76E007820BBF980CDC3DC0DAEC0D94830FFC4AE54AB5。
- 中文路径经 SDL UTF-8 reader 成功读取。
- Mesh GPU 生命周期测试通过。
- Studio 后台导入 smoke 返回 0，importedMesh=true、viewportContent=true、resize=true、transform=true、cameraInput=true、cachedMeshes=1、glError=0。
- artifacts/duck_smoke/studio.bmp 已目视检查：显示真实 Duck 几何、中文名称及编辑后的根变换。材质预览为基础色，不显示 Duck 的纹理。
- 本次尝试较大的 Avocado 下载因网络缓慢中止，未将不完整文件计为验证通过；采用完整 Duck 样例验收。

可重复运行：

```powershell
cmake --preset windows -DWF_ENABLE_GPU_TESTS=ON
cmake --build --preset windows-release --parallel 8
ctest --preset windows-release
build/windows/Release/wf_mesh_gpu_tests.exe "$PWD/artifacts/models/小鸭.glb"
$p = Start-Process build/windows/Release/worldforge-studio.exe -WindowStyle Hidden -PassThru -Wait -ArgumentList '--smoke-test',"$PWD/artifacts/duck_smoke","$PWD/artifacts/models/小鸭.glb"
$p.ExitCode
Get-Content artifacts/duck_smoke/report.json
```

自造 GLB 由 tools/generate_mesh_fixtures.py 生成到构建目录，无网络和第三方样例依赖。GPU 测试默认不在托管 CI 注册；CPU 测试在 Windows CI 运行。

## 安装包复验与本次阶段耗时

out/WorldForge-dev/worldforge-studio.exe 对同一中文路径样例执行后台导入 smoke 返回 0；安装目录包含 tinygltf 在内的五份第三方许可证及 MSVC runtime。

本机一次安装包运行记录：文件 120484 字节，读取 0.1664 ms、解析 0.2658 ms、上传及发布 0.2723 ms，几何上传 127312 字节。这是热缓存下的小样例单次观测，不是吞吐基准或性能保证；原始结果位于 artifacts/duck_packaged/report.json。

## 尚未自动验收

Windows Computer Use 的 node_repl 在初始化和重置重试后均异常退出，首次错误为 sandbox helper_unknown_error。原生文件选择框的点击选文件、取消、选框打开时关闭主窗口，以及真实用户连续菜单点击尚未自动操作验收。

实现已包含回调生命周期与取消处理；工作线程并发/代次/关闭由自动测试覆盖，但这些不能代替上述原生交互测试。体验时请依次检查 File → Import GLB、取消、导入失败后继续操作、导入中 New Scene/关闭窗口。其他显卡、动态多显示器 DPI、超大模型和上传失败注入也没有本次实测结论。
