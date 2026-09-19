# 01：Windows GUI 框架与真实视口

## 本次形成的链路

SDL 窗口和 GL context → glad 函数加载 → ImGui 帧 → 编辑器状态 → 离屏 GL 渲染 → 纹理交给 ImGui Image → 交换窗口缓冲。

ImGui 是面板与输入层，立方体不是 UI 画出来的示意图：Renderer 使用顶点缓冲、shader、深度测试和 FBO。窗口缩放时只调整视口 framebuffer，不修改物体数据。

## 关键取舍

- 先做程序化 cube/grid 验证 GPU 与编辑流程，不将格式导入耦合到窗口启动。
- 只有主线程调用 GL。作用域析构确保 Renderer 和 ImGui 先释放 GPU 对象，再销毁 SDL context。
- 物体 ID 与容器下标分离，删除后清空选择；变换拒绝非有限值与奇异缩放。
- 相机 pitch/distance 限制避免极点和穿过 target；GUI 只在鼠标位于视口时消费导航。
- 像素分辨率和面板逻辑尺寸分开；DPI 更新先重置 style 再缩放，避免累乘。
- GL 预览与 ImGui 共用 context，Renderer 显式设置自己的状态，结束后由 ImGui backend 恢复其渲染状态。

## 已做验证

CPU 场景/相机测试；Release 编译；GPU smoke 的画面读回、resize、transform、轨道输入与 GL error；实际截图检查。布局保存路径字符串必须比 ImGui context 活得更久，这是本次实现中特别处理的生命周期细节。

## 你可以亲自做的实验

1. 将一个 cube 的三个 scale 改为不同值，观察光照与矩阵变换。
2. 增加多个 cube，再删除所选，确认 Inspector 没有指向旧对象。
3. 缩小/放大 Viewport，观察 Performance 的像素尺寸。
4. 打开 Wireframe，观察 cube 由 12 个三角形组成；关闭 Grid 对比 draw 数。
5. 阅读 ViewportRenderer::Render，解释为何先绑定 FBO、最后返回默认 framebuffer。

下一节围绕真正的 MeshAsset 与 GLB 导入展开。当前没有项目保存、PBR 或 Gaussian。
