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

// 离屏执行**一条重放帧**：清成 clear_rgba32（0xAABBGGRR 直读，无 Metal 头耦合），
// 然后按顺序为每个绘制项画一遍全屏三角形，颜色 = 该项管线键的低 24 位（状态变颜色就变）。
// 这是「重放记账层的计划项 → Metal 帧」的合龙点：之后把真实参数的翻译接进来即可。
int nvn_mtl_replay_frame(nvn_mtl_ctx *c, uint32_t w, uint32_t h, uint32_t clear_rgba32,
                         const uint64_t *draw_keys, unsigned n_draws,
                         uint8_t *out, size_t cap);

// 管线缓存：prog 0 = 纯色（buffer(0)），prog 1 = 最近邻采样 texture(0)。
// 每帧：清屏 + 每个绘制项按 prog 取管线、按 key 着色（prog1 时改采样纹理）。
// 返回当前缓存里的管线数（验证「同 prog 复用、异 prog 新建」）。tex_rgba 为 NULL 时 prog1 无效。
int nvn_mtl_replay_frame_multi(nvn_mtl_ctx *c, uint32_t w, uint32_t h, uint32_t clear_rgba32,
                               const uint64_t *keys, const unsigned *progs, unsigned n_draws,
                               const uint8_t *tex_rgba, uint32_t tw, uint32_t th,
                               uint8_t *out, size_t cap);
unsigned nvn_mtl_pipeline_count(nvn_mtl_ctx *c);

// 离屏做**纹理上传往返**：把 w×h 的 RGBA8 数据上传进 MTLTexture，用最近邻采样画全屏四边形，
// 回读。最近邻 + 1:1，回读必须与输入**逐字节相同** —— 这是「CPU 纹理链 → GPU」的交接验证。
int nvn_mtl_offscreen_texture(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                              const uint8_t *rgba, uint8_t *out, size_t cap);

// 离屏画一个**全屏三角形**（内置 MSL 着色器，片段颜色经 uniform 传入），回读像素。
// 这一步验证的是着色器链：newLibraryWithSource（Metal 编译器）→ 两个入口函数 → 管线状态 →
// drawPrimitives → 片段 uniform。之后把 NVN 的程序/管线按键接进来，就长成真正的重放层。
int nvn_mtl_offscreen_draw(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                           float r, float g, float b,
                           uint8_t *out, size_t cap);
