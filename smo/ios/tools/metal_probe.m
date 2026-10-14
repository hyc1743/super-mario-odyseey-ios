// Metal 探测：CI 的 macOS runner 有没有可用的 Metal 设备？
//
// 为什么要先探这个：整个 Metal 重放层能不能在 CI 上验证，取决于 runner 有没有 GPU。
// GitHub 的 macOS runner 跑在 Apple 硬件上，部分报告说 Metal 可用（虚拟 GPU）——
// 若可用，Metal 层就能走"离屏渲染 + 回读像素 + 断言"这条与本项目其他部分同级的证据路线；
// 若不可用，Metal 层就只能靠真机日志/截帧驱动（见 STATUS.md 里对这条分岔的说明）。
//
// 探测内容（从易到难，任何一步失败都报错误注解，公开可见）：
//   1. MTLCreateSystemDefaultDevice() 非空
//   2. 离屏 8×8 RGBA8 纹理，清屏成纯红，提交并等待完成
//   3. 回读 (0,0) 像素，必须是 (255,0,0,255)
//
// 注意：输出里的 `::notice::` / `::error::` 是 GitHub Actions 的注解语法 ——
// 它直接进检查页（公开可读，不需要管理员看 job 日志）。
//
// 编译：xcrun -sdk macosx clang -fobjc-arc -framework Metal -framework Foundation -o metal_probe metal_probe.m
#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

int main(void) {
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev) {
            printf("::error title=Metal 探测::MTLCreateSystemDefaultDevice 返回 nil —— runner 上没有 Metal 设备\n");
            printf("runner 无 Metal：Metal 层只能真机验证（离屏 CI 验证这条路关闭）\n");
            return 1;
        }
        printf("::notice title=Metal 探测::设备可用：%s\n", dev.name.UTF8String);

        MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                     width:8 height:8 mipmapped:NO];
        td.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> tex = [dev newTextureWithDescriptor:td];
        if (!tex) { printf("::error title=Metal 探测::离屏纹理创建失败\n"); return 1; }

        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLCommandBuffer> cb = [q commandBuffer];

        MTLRenderPassDescriptor *rp = [MTLRenderPassDescriptor renderPassDescriptor];
        rp.colorAttachments[0].texture = tex;
        rp.colorAttachments[0].loadAction = MTLLoadActionClear;
        rp.colorAttachments[0].clearColor = MTLClearColorMake(1, 0, 0, 1);
        rp.colorAttachments[0].storeAction = MTLStoreActionStore;

        id<MTLRenderCommandEncoder> enc = [cb renderCommandEncoderWithDescriptor:rp];
        [enc endEncoding];
        [cb commit];
        [cb waitUntilCompleted];
        if (cb.status == MTLCommandBufferStatusError) {
            printf("::error title=Metal 探测::命令缓冲出错（status=Error，无详细错误对象）\n");
            return 1;
        }

        uint8_t px[4] = {0};
        [tex getBytes:px bytesPerRow:4 fromRegion:MTLRegionMake2D(0, 0, 1, 1) mipmapLevel:0];
        printf("::notice title=Metal 探测::回读像素 = (%d,%d,%d,%d)，期望 (255,0,0,255)\n", px[0], px[1], px[2], px[3]);
        if (px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255) {
            printf("::notice title=Metal 探测::离屏渲染 + 回读在 CI runner 上可用 —— Metal 层可以在 CI 上验\n");
            return 0;
        }
        printf("::error title=Metal 探测::回读像素不对（GPU 呈现与回读路径不可靠，不能用作验证手段）\n");
        return 1;
    }
}
