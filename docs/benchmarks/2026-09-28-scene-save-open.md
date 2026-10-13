# Scene 保存与重开验收记录

日期：2026-09-28。版本：0.0.2-scene-dev。设计及取舍见 [实现说明](../learning/0004-scene-save-open.md)。

## 环境与命令

Windows x64，现有 VS 2026 Release preset；GPU 测试运行于当前桌面 OpenGL 环境。

~~~powershell
build/python/Scripts/python.exe tools/check_style.py
cmake --build --preset windows-release --parallel 8
ctest --preset windows-release --output-on-failure
build/windows/Release/wf_scene_gpu_tests.exe D:/code/WorldForge/artifacts/models/小鸭.glb
~~~

最终结果：Google clang-format/cpplint 检查通过，Windows Release 构建通过；CTest 10/10 通过（2.59 秒），真实 Duck 独立 GPU/UI 往返测试通过。日志保存在本地 artifacts/scene-style-final.log、scene-build-final.log、scene-ctest-final.log。

测试目标总数从 8 增至 10。新增 scene_document 与 scene_gpu_ui；原有解析、线程、编辑、选择、贴图和 Studio smoke 全部保留。

## 已覆盖

| 验证 | 证据 |
| --- | --- |
| JSON 往返 | float 精度、uint64 最大 revision、中文/转义字符、固定输出与德语数字 locale |
| 非法输入 | 未来版本、重复 key、未知字段、错误类型、非法 UTF-8/代理字符/NUL、越界数值、重复 UUID、悬挂引用 |
| 空场景与预算 | 空文档恢复；JSON 字节数、实体数上限；可选 editor_state 默认值 |
| 脏状态 | 属性修改/删除递增；相机/显示/等值赋值不递增；成功保存基线 |
| 资产身份 | 两实体共享同一 payload，隐藏属性保留；重开后添加 Cube 仍共享 builtin |
| 路径 | 中文和空格，同盘 Save As 的 ../ 重基准，跨盘字符串回退，UNC/盘符相对拒绝 |
| 搬移 | 释放所有租约后移动整目录，原路径消失，仍能打开相对资产 |
| 写锁 | 同进程及独立子进程竞争拒绝，句柄释放后遗留 .lock 可重新获取 |
| 保存失败 | 注入创建、写、flush、替换前失败，原内容 hash 与 dirty 保留 |
| 冲突与恢复 | 外部目标变化拒绝覆盖；提交后验证故障返回 RecoveryRequired |
| 来源变化 | 修改/移走 GLB 后打开失败，活动文档不受影响 |
| 后台串行 | Scene 任务运行时拒绝新导入，完成后才公开结果 |
| GPU 回滚 | 最后一个候选资源故障，旧 Mesh/Image/Sampler 数量与旧画面不变 |
| 画面往返 | 256×256 RGBA 像素逐字节比较；包含变换、Tint、PNG 与相机 |
| 实际资产 | 小鸭 GLB 同样通过保存重开像素一致性测试 |
| UI 协调 | New Cancel/Discard、Close Cancel、Ctrl+S、Save 后继续 New、Open 恢复、选框取消、保存中关闭 |
| 回归 | 原有 CPU/GPU/ImGui 与 Studio smoke |

UI 测试运行实际 ImGui 控件和 DocumentController；选择路径/取消通过注入平台结果完成，不冒充原生选框操作。

## 人工验证与剩余限制

2026-09-28：用户反馈此版本体验验证通过。此反馈作为整体验收记录；没有逐项操作记录，因此不将原生 Windows Open/Save 对话框的取消、窗口关闭等边界场景列为单独通过项。自动 UI 测试仍使用注入的选框结果。
- 未模拟所有真实磁盘/权限/硬件断电错误；故障点不等于 Windows 所有部分替换错误码都已实测。
- 跨盘绝对路径规则已单测，未进行两个物理磁盘之间的人工 Save As。
- 未进行候选容量上限附近的整场景压力或性能 benchmark；没有 GPU 时间或内存优化结论。
- 不含只读打开、自动依赖打包、缺失资产修复、撤销或其他平台验证。

正常覆盖保留唯一 .bak，异常情况保留恢复所需产物；.lock 的有效性取决于活句柄。当前备份没有自动清理策略。

## 交付位置

开发构建：build/windows/Release/worldforge-studio.exe。
安装包目录：out/WorldForge-scene-dev。
开发压缩包：out/WorldForge-Windows-0.0.2-scene-dev.zip。

此次交付为本地开发包，不创建 GitHub Release，也不将完整 0.0.3 标记完成。
