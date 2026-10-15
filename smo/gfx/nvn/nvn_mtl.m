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
    @autoreleasepool {
        [c->queue release];
        [c->dev release];
    }
    free(c);
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
        [tex release];
        return (int)(w * h * 4);
    }
}
