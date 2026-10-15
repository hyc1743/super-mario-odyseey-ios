// NVN → Metal 重放层第一片（实现）。见 nvn_mtl.h。
// 编译：macOS / iOS（CI 的 macOS runner 离屏验证）。
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "nvn_mtl.h"

struct nvn_mtl_ctx {
    id<MTLDevice> dev;
    id<MTLCommandQueue> queue;
    NSMutableDictionary *pso_cache;      // 程序 id → 管线（缓存复用：iPhone 上编管线很贵）
};

int nvn_mtl_init(nvn_mtl_ctx **out) {
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) return -1;                       // 无 Metal（CI runner 有：Apple Paravirtual）
        nvn_mtl_ctx *c = calloc(1, sizeof *c);
        if (!c) return -1;
        c->dev = dev;
        c->queue = [dev newCommandQueue];
        if (!c->queue) { free(c); return -1; }
        c->pso_cache = [NSMutableDictionary new];
        *out = c;
        return 0;
    }
}

void nvn_mtl_term(nvn_mtl_ctx *c) {
    if (!c) return;
    // ARC 下不能显式 release；对象随 ctx 释放。这里空着是故意的，不是漏了。
    (void)c;
}

int nvn_mtl_offscreen_clear(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                            float r, float g, float b, float a,
                            uint8_t *out, size_t cap) {
    if (!c || !out || cap < (size_t)w * h * 4) return -1;
    @autoreleasepool {
        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:w height:h mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> tex = [c->dev newTextureWithDescriptor:td];
        if (!tex) return -1;

        id<MTLCommandBuffer> cb = [c->queue commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = tex;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(r, g, b, a);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc endEncoding];

        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status == MTLCommandBufferStatusError) return -1;

        [tex getBytes:out bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
        return (int)(w * h * 4);
    }
}

// 全屏三角形的内置着色器：vertex_id 法（不需要顶点缓冲，三个顶点正好盖住全屏）。
static NSString *const kVS = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct VSOut { float4 pos [[position]]; };\n"
"vertex VSOut vs_full(uint vid [[vertex_id]]) {\n"
"    float2 p[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };\n"
"    VSOut o; o.pos = float4(p[vid], 0.0, 1.0); return o;\n"
"}\n"
"fragment float4 fs_full(VSOut in [[stage_in]], constant float4 &color [[buffer(0)]]) {\n"
"    return color;\n"
"}\n";

int nvn_mtl_offscreen_draw(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                           float r, float g, float b,
                           uint8_t *out, size_t cap) {
    if (!c || !out || cap < (size_t)w * h * 4) return -1;
    @autoreleasepool {
        NSError *err = nil;
        id<MTLLibrary> lib = [c->dev newLibraryWithSource:kVS options:nil error:&err];
        if (!lib) {
            fprintf(stderr, "[nvn_mtl] 着色器编译失败：%s\n", err ? err.localizedDescription.UTF8String : "?");
            return -1;
        }
        id<MTLFunction> vs = [lib newFunctionWithName:@"vs_full"];
        id<MTLFunction> fs = [lib newFunctionWithName:@"fs_full"];
        if (!vs || !fs) return -1;

        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = vs;
        pd.fragmentFunction = fs;
        pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        id<MTLRenderPipelineState> pso = [c->dev newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!pso) {
            fprintf(stderr, "[nvn_mtl] 管线创建失败：%s\n", err ? err.localizedDescription.UTF8String : "?");
            return -1;
        }

        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:w height:h mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> tex = [c->dev newTextureWithDescriptor:td];
        if (!tex) return -1;

        id<MTLCommandBuffer> cb = [c->queue commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = tex;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:pso];
        float color[4] = { r, g, b, 1.0f };
        [enc setFragmentBytes:color length:sizeof color atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];

        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status == MTLCommandBufferStatusError) return -1;

        [tex getBytes:out bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
        return (int)(w * h * 4);
    }
}

// 离屏执行一条**重放帧**：清成 clear_rgba32（0xAABBGGRR 直读），然后按顺序为每个绘制项画一遍
// 全屏三角形，颜色 = 该项管线键的低 24 位（状态变颜色就变）。
// 这是「重放记账层的计划项 → Metal 帧」的合龙点；真实参数的翻译在此之后逐步接进来。
int nvn_mtl_replay_frame(nvn_mtl_ctx *c, uint32_t w, uint32_t h, uint32_t clear_rgba32,
                         const uint64_t *draw_keys, unsigned n_draws,
                         uint8_t *out, size_t cap) {
    if (!c || !out || cap < (size_t)w * h * 4) return -1;
    @autoreleasepool {
        NSError *err = nil;
        id<MTLLibrary> lib = [c->dev newLibraryWithSource:kVS options:nil error:&err];
        if (!lib) return -1;
        id<MTLFunction> vs = [lib newFunctionWithName:@"vs_full"];
        id<MTLFunction> fs = [lib newFunctionWithName:@"fs_full"];
        if (!vs || !fs) return -1;
        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = vs;
        pd.fragmentFunction = fs;
        pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        id<MTLRenderPipelineState> pso = [c->dev newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!pso) return -1;

        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:w height:h mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> tex = [c->dev newTextureWithDescriptor:td];
        if (!tex) return -1;

        id<MTLCommandBuffer> cb = [c->queue commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = tex;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake((float)(clear_rgba32 & 0xFF) / 255.0f,
                                                              (float)((clear_rgba32 >> 8) & 0xFF) / 255.0f,
                                                              (float)((clear_rgba32 >> 16) & 0xFF) / 255.0f,
                                                              (float)((clear_rgba32 >> 24) & 0xFF) / 255.0f);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:pso];
        for (unsigned i = 0; i < n_draws; i++) {
            float color[4] = {
                (float)((draw_keys[i] >> 16) & 0xFF) / 255.0f,
                (float)((draw_keys[i] >> 8) & 0xFF) / 255.0f,
                (float)(draw_keys[i] & 0xFF) / 255.0f,
                1.0f,
            };
            [enc setFragmentBytes:color length:sizeof color atIndex:0];
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status == MTLCommandBufferStatusError) return -1;
        [tex getBytes:out bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
        return (int)(w * h * 4);
    }
}

// 纹理采样的着色器：VS 输出带 uv 的全屏三角形（vertex_id 法），FS 用最近邻采样 texture(0)。
static NSString *const kVS_tex = @
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"struct VSOut { float4 pos [[position]]; float2 uv; };\n"
"vertex VSOut vs_tex(uint vid [[vertex_id]]) {\n"
"    float2 p[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };\n"
"    VSOut o; o.pos = float4(p[vid], 0.0, 1.0); o.uv = (p[vid] + 1.0) * 0.5; return o;\n"
"}\n"
"fragment float4 fs_tex(VSOut in [[stage_in]], texture2d<float> tex [[texture(0)]]) {\n"
"    constexpr sampler s(mag_filter::nearest, min_filter::nearest);\n"
"    return tex.sample(s, in.uv);\n"
"}\n";

// 纹理上传往返：上传 → 最近邻采样画全屏 → 回读。最近邻 + 1:1 之下必须与输入逐字节相同。
int nvn_mtl_offscreen_texture(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                              const uint8_t *rgba, uint8_t *out, size_t cap) {
    if (!c || !rgba || !out || cap < (size_t)w * h * 4) return -1;
    @autoreleasepool {
        NSError *err = nil;
        id<MTLLibrary> lib = [c->dev newLibraryWithSource:kVS_tex options:nil error:&err];
        if (!lib) {
            fprintf(stderr, "[nvn_mtl] 纹理着色器编译失败：%s\n", err ? err.localizedDescription.UTF8String : "?");
            return -1;
        }
        id<MTLFunction> vs = [lib newFunctionWithName:@"vs_tex"];
        id<MTLFunction> fs = [lib newFunctionWithName:@"fs_tex"];
        if (!vs || !fs) return -1;
        MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
        pd.vertexFunction = vs;
        pd.fragmentFunction = fs;
        pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        id<MTLRenderPipelineState> pso = [c->dev newRenderPipelineStateWithDescriptor:pd error:&err];
        if (!pso) return -1;

        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:w height:h mipmapped:NO];
        td.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> src = [c->dev newTextureWithDescriptor:td];
        if (!src) return -1;
        [src replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0 withBytes:rgba bytesPerRow:w * 4];

        MTLTextureDescriptor *rtd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                      width:w height:h mipmapped:NO];
        rtd.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> dsttex = [c->dev newTextureWithDescriptor:rtd];
        if (!dsttex) return -1;

        id<MTLCommandBuffer> cb = [c->queue commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = dsttex;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc setRenderPipelineState:pso];
        [enc setFragmentTexture:src atIndex:0];
        [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status == MTLCommandBufferStatusError) return -1;
        [dsttex getBytes:out bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
        return (int)(w * h * 4);
    }
}

// 按程序 id 取管线（缓存命中直接复用）
static id<MTLRenderPipelineState> pso_for(nvn_mtl_ctx *c, unsigned prog, uint32_t w, uint32_t h, id<MTLTexture> *src_out) {
    NSNumber *key = @(prog);
    id<MTLRenderPipelineState> hit = c->pso_cache[key];
    if (hit) return hit;
    NSError *err = nil;
    id<MTLLibrary> lib = [c->dev newLibraryWithSource:(prog == 0 ? kVS : kVS_tex) options:nil error:&err];
    if (!lib) return nil;
    id<MTLFunction> vs = [lib newFunctionWithName:(prog == 0 ? @"vs_full" : @"vs_tex")];
    id<MTLFunction> fs = [lib newFunctionWithName:(prog == 0 ? @"fs_full" : @"fs_tex")];
    if (!vs || !fs) return nil;
    MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
    pd.vertexFunction = vs;
    pd.fragmentFunction = fs;
    pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    id<MTLRenderPipelineState> pso = [c->dev newRenderPipelineStateWithDescriptor:pd error:&err];
    if (!pso) return nil;
    c->pso_cache[key] = pso;
    (void)w; (void)h; (void)src_out;      // 尺寸与纹理句柄留给真实实现；本片先不用
    return pso;
}

unsigned nvn_mtl_pipeline_count(nvn_mtl_ctx *c) {
    return c ? (unsigned)c->pso_cache.count : 0;
}

int nvn_mtl_replay_frame_multi(nvn_mtl_ctx *c, uint32_t w, uint32_t h, uint32_t clear_rgba32,
                               const uint64_t *keys, const unsigned *progs, unsigned n_draws,
                               const uint8_t *tex_rgba, uint32_t tw, uint32_t th,
                               uint8_t *out, size_t cap) {
    if (!c || !keys || !progs || !out || cap < (size_t)w * h * 4) return -1;
    @autoreleasepool {
        id<MTLTexture> src = nil;
        if (tex_rgba) {
            MTLTextureDescriptor *sd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                         width:tw height:th mipmapped:NO];
            sd.usage = MTLTextureUsageShaderRead;
            src = [c->dev newTextureWithDescriptor:sd];
            if (!src) return -1;
            [src replaceRegion:MTLRegionMake2D(0, 0, tw, th) mipmapLevel:0 withBytes:tex_rgba bytesPerRow:tw * 4];
        }
        MTLTextureDescriptor *rtd = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                      width:w height:h mipmapped:NO];
        rtd.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> dsttex = [c->dev newTextureWithDescriptor:rtd];
        if (!dsttex) return -1;
        id<MTLCommandBuffer> cb = [c->queue commandBuffer];
        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = dsttex;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake((float)(clear_rgba32 & 0xFF) / 255.0f,
                                                              (float)((clear_rgba32 >> 8) & 0xFF) / 255.0f,
                                                              (float)((clear_rgba32 >> 16) & 0xFF) / 255.0f,
                                                              (float)((clear_rgba32 >> 24) & 0xFF) / 255.0f);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        for (unsigned i = 0; i < n_draws; i++) {
            id<MTLRenderPipelineState> pso = pso_for(c, progs[i], w, h, &src);
            if (!pso) return -1;
            [enc setRenderPipelineState:pso];
            if (progs[i] == 0) {
                float color[4] = {
                    (float)((keys[i] >> 16) & 0xFF) / 255.0f,
                    (float)((keys[i] >> 8) & 0xFF) / 255.0f,
                    (float)(keys[i] & 0xFF) / 255.0f,
                    1.0f,
                };
                [enc setFragmentBytes:color length:sizeof color atIndex:0];
            } else {
                [enc setFragmentTexture:src atIndex:0];
            }
            [enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
        }
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status == MTLCommandBufferStatusError) return -1;
        [dsttex getBytes:out bytesPerRow:w * 4 fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
        return (int)(w * h * 4);
    }
}
