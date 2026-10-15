// NVN → Metal 重放层第一片（实现）。见 nvn_mtl.h。
// 编译：macOS / iOS（CI 的 macOS runner 离屏验证）。
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>
#include "nvn_mtl.h"

struct nvn_mtl_ctx {
    id<MTLDevice> dev;
    id<MTLCommandQueue> queue;
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
"#include <metal_stdlib>
"
"using namespace metal;
"
"struct VSOut { float4 pos [[position]]; };
"
"vertex VSOut vs_full(uint vid [[vertex_id]]) {
"
"    float2 p[3] = { float2(-1.0, -1.0), float2(3.0, -1.0), float2(-1.0, 3.0) };
"
"    VSOut o; o.pos = float4(p[vid], 0.0, 1.0); return o;
"
"}
"
"fragment float4 fs_full(VSOut in [[stage_in]], constant float4 &color [[buffer(0)]]) {
"
"    return color;
"
"}
";

int nvn_mtl_offscreen_draw(nvn_mtl_ctx *c, uint32_t w, uint32_t h,
                           float r, float g, float b,
                           uint8_t *out, size_t cap) {
    if (!c || !out || cap < (size_t)w * h * 4) return -1;
    @autoreleasepool {
        NSError *err = nil;
        id<MTLLibrary> lib = [c->dev newLibraryWithSource:kVS options:nil error:&err];
        if (!lib) {
            fprintf(stderr, "[nvn_mtl] 着色器编译失败：%s
", err ? err.localizedDescription.UTF8String : "?");
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
            fprintf(stderr, "[nvn_mtl] 管线创建失败：%s
", err ? err.localizedDescription.UTF8String : "?");
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
