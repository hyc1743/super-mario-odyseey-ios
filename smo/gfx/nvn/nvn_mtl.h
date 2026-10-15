// NVN → Metal 重放层的第一片：设备 / 队列 / 离屏纹理 / 清屏。
//
// 为什么先做这么小的一片：整层（对应参考项目 nvn_mtl.m 的 627 KB）要靠 CI 的离屏渲染验证，
// 第一片就要把「编译（macOS SDK）→ 离屏渲染 → 回读像素 → 断言」这条循环打通。之后往里堆
// 计划项翻译（绑定/绘制/呈现）时，每一块都能用同一循环验证。
//
// 本文件只在 macOS / iOS 编译（CI 的 macOS runner 验）。
#pragma once
#include <stdint.h>
#include <stddef.h>

// 不透明上下文（真正的 id<MTLDevice> 等放在 .m 里，头文件不引入 Metal，方便以后加纯 C 调用方）
typedef struct nvn_mtl_ctx nvn_mtl_ctx;

// 拿系统默认设备与命令队列。失败返回 -1。
int nvn_mtl_init(nvn_mtl_ctx **out);
void nvn_mtl_term(nvn_mtl_ctx *c);

// 离屏画一帧：新建 w×h RGBA8 纹理，清成 (r,g,b,a)，提交并等完成，回读像素到 out（w*h*4 字节）。
// 返回写出字节数；失败 -1（设备不可用、纹理/队列失败）。
// 这是重放层「执行一条计划」的最小闭环 —— 之后每个翻译步骤都按这个形状验证。
int nvn_mtl_offscreen_clear(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                            float r, float g, float b, float a,
                            uint8_t *out, size_t cap);

// 离屏画一个**全屏三角形**（内置 MSL 着色器，片段颜色经 uniform 传入），回读像素。
// 这一步验证的是着色器链：newLibraryWithSource（Metal 编译器）→ 两个入口函数 → 管线状态 →
// drawPrimitives → 片段 uniform。之后把 NVN 的程序/管线按键接进来，就长成真正的重放层。
int nvn_mtl_offscreen_draw(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                           float r, float g, float b,
                           uint8_t *out, size_t cap);
