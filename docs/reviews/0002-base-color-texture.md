# 基础颜色贴图代码复查

日期：2026-09-21。本轮实现后的自查；不是独立安全审计。

## 已处理

1. 旧 shader 直接输出线性乘积，不能只加纹理采样。现在 sRGB 纹理过滤前解码，Tint 转线性、factor 保持线性，最终编码一次；RGBA8 视口由 ImGui 直接合成。四象限、灰度乘色与高频 mip 像素测试通过。
2. 图片与 sampler 分离，CPU 按源索引共享，GPU 分别弱引用缓存；最后一个引用释放后回收。不同 sampler、多个实例、隐藏/删除/New Scene 有 GPU 生命周期测试。
3. Prepare 将几何、图片和 sampler 暂存，全部成功再合并。测试先上传成功对象，再让后续对象失败，确认缓存与旧场景不变。
4. GL 上传失败可能设置不止一个错误位。新上传器在关键步骤立即检查并排空错误，避免下一帧错误地拒绝旧场景；真实非法 sampler 参数回滚测试通过。
5. PNG 先校验 chunk 范围、全部 CRC 和 IEND，再检查尺寸/剩余预算，解码分配由线程内预算钩子约束。禁止解码库磁盘与 C++ 包装；第三方源保持原样，打包许可证。
6. Esc 处理限定聚焦 Scene/Viewport，无 active item/text/popup/import busy；用实际 ImGui InputText、modal 与输入事件验证。Scene 空白点击与视口点击明确区分。
7. project-owned .cpp/.h 保留 Google 格式、函数空行和核心中英注释；style check 与 Windows Release/8 项测试通过。

## 仍需后续验证

原生文件对话框完整操作受 computer-use 初始化故障阻塞。其他 GPU/Linux、完整 PNG/glTF 兼容性、模糊测试和驱动显存耗尽不在本轮已验证范围。LodePNG 版本来自固定依赖的历史快照，不把限定 fixture 检查声称为解码器安全审计。

本轮不加入保存、完整 PBR 或通用插件接口；这些继续按 ROADMAP 的版本退出条件交付。
