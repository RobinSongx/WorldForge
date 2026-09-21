# 基础颜色贴图与选择显示实现

日期：2026-09-21。开发标识 0.0.2-texture-dev；这是 0.0.3 的首个增量，尚未完成 Scene 保存重开或完整材质。

## 1. 可以直接体验的变化

原先选中模型必然叠加黄色三角线，Wireframe 关闭也无法隐藏。本轮把选择状态和显示策略分开：Selection overlay 默认关闭，只控制黄色叠加；Scene 高亮和 Inspector 保持选中。Ground grid 只控制地面，Wireframe 只控制几何线框。

Scene 提供 Clear selection 按钮和菜单、空白点击清选；Scene/Viewport 聚焦时 Esc 清选。输入文本、弹窗和导入忙状态受到保护。视口尚未实现鼠标拾取，所以单击视口不会清除或切换选择。无选择时 Frame/F 安全无操作。

导入带嵌入 PNG 的 GLB 后显示基础颜色贴图。Tint 是 sRGB 颜色选择器的整体乘色，导入默认白色；纯色 Cube 继续工作。Inspector 显示图片大小、UV0、寻址和过滤参数，Performance 区分几何与图片/采样器缓存。

## 2. 数据与调用链

| 文件 | 职责 |
| --- | --- |
| src/domain/mesh_asset.h | TextureImage 的 RGBA8 像素、TextureSampler、BaseColorTexture，以及 primitive 引用 |
| src/io/glb_textures.cpp | 校验 texture/image/sampler/UV 引用，按一次导入的源索引共享 CPU 图片 |
| src/io/png_decoder.cpp | PNG chunk/CRC/尺寸验证、受限分配、RGBA8 解码；不接触 GL |
| src/render/gpu_texture.cpp | GL 图片和采样器 RAII、sRGB 存储与 mipmap |
| src/render/viewport_renderer.cpp | 几何+图片+sampler 事务上传、共享缓存、着色和回收 |
| src/editor/editor_state.cpp | ClearSelection 和只读显示快照 |
| src/ui/studio_panels.cpp | 三个独立开关、受限 Esc、清选按钮、图片和缓存信息 |
| src/studio/studio_app.cpp | 后台结果代次检查、最终发布、阶段统计和 smoke 报告 |

工作线程读取/解析/解码，没有 GL 调用。主线程确认结果属于当前场景后上传；任一资源失败，暂存 RAII 对象销毁，已发布场景不变。全部成功才把缓存合并并增加模型根。沿用现有单任务 busy、场景 generation 和关闭 join。

同一导入的不同 primitive、节点实例共享图片；图片和 sampler 分开，同一图片使用 repeat 与 clamp 不会复制显存。不同文件或重新导入暂未按内容 hash 去重。图片引用由纹理绑定持有、纹理由 Mesh 持有；GPU 缓存只弱引用它们。隐藏保留资源，最后一个实体/快照引用消失后或 New Scene 后回收。

## 3. PNG 边界与依赖

使用已经固定在 tinygltf 提交 0d9ce1a9561688c6c6e7471430d15a50a0b15f1a 中的 LodePNG 20190210；源文件不修改，独立 wf_png 静态库。禁用 C++ 包装、磁盘、编码和辅助元数据解析，许可证随包安装。不是新增通用图片或第三方文件加载系统。

- 仅读取 baseColorTexture 实际引用的、嵌入 bufferView 的 PNG；外部/data URI 不允许。
- 校验引用、范围、MIME、PNG 签名、全部 chunk CRC、IEND、尺寸；解码器校验压缩流并统一 RGBA8。
- 每轴上限 4096，单图 RGBA8 上限 64 MiB，一次导入独立图片合计上限 128 MiB。
- 解码器分配钩子限制同时持有的请求字节为 192 MiB/线程（含自身记录头，不含系统分配器内部开销）；不是整个进程的内存上限。源 GLB、解析 arena、几何、CPU 图片和解码临时数据可能同时存在。
- 分配钩子采用上游要求的精确符号名 lodepng_malloc/realloc/free；释放必须发生在原解码线程。
- 只支持 UV0、无纹理变换、OPAQUE。JPEG、UV1、KHR_texture_transform、压缩图片明确失败。normal/AO/emissive/metallic-roughness 等通道未渲染，给出预览警告。
- 所有 glTF 基础 wrap/min/mag 枚举透传到独立 GL sampler；省略时选择 repeat、linear mag、linear-mipmap-linear min。GPU 总是生成完整 mip 链。

RGBA、RGB、灰度和调色板样例有自动覆盖；这不是完整 PNG/glTF 一致性认证。未做整套图片模糊测试、跨厂商 GPU 验证或驱动内存耗尽测试。

## 4. 为什么同时修改颜色空间

glTF 基础色图片的 RGB 是 sRGB，baseColorFactor 是线性值。直接把 PNG 字节乘旧 shader 会让颜色/光照出错；仅把纹理改成 sRGB 而不处理最终输出也会偏暗。

本轮链路：

    PNG sRGB → GL_SRGB8_ALPHA8 自动解码/线性过滤
                       × glTF linear factor
                       × sRGB Tint 转线性
                       × 预览漫反射光照
             → shader 编码 sRGB → RGBA8 视口 → ImGui 直接合成

视口渲染显式关闭 GL_FRAMEBUFFER_SRGB，避免重复编码。Grid/选中线等编辑器颜色直接作为显示颜色。PNG 顶行直接存储，glTF UV (0,0) 对应第一行；不对资产图片再做翻转。ImGui 展示离屏 framebuffer 的 UV 翻转是另一件事，仍保留。

标准依据：[Khronos glTF 2.0 材质规范](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc#metallic-roughness-material)。

## 5. 统计的含义

GPU geometry bytes 是顶点+索引存储；texture bytes 为 RGBA8 各级 mip 尺寸之和，图片按唯一 ID 统计。采样器单列数量，其驱动开销不纳入字节。均不包含驱动内部开销、framebuffer、窗口/UI 资源，不代表总显存或 GPU 时间。

Assets/Console 的 parse+decode 包括 GLB 解析校验和 PNG 解码；upload+publish 是 CPU 侧提交/发布耗时，不是 GPU timer。没有借此宣称完成 CUDA profiling 或优化 benchmark。

## 6. 后续建议

下一增量仍沿 0.0.3 完成 Scene 持久 ID、保存/重开和必要命令边界；不把本轮纹理预览当作完整 PBR。需要材质扩展时沿 TextureImage/绑定边界添加通道，避免让 UI 或格式库进入 Runtime。Hybrid 深度与透明排序另外设计。
