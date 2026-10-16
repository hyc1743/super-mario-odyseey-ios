#ifndef SMO_NVN_METAL_PRIVATE_H
#define SMO_NVN_METAL_PRIVATE_H
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

@interface SMOGuestEncoder : NSObject
@property(nonatomic,strong) id<MTLDevice> device;
@property(nonatomic,strong) id<MTLCommandBuffer> command;
@property(nonatomic,strong) id<MTLBlitCommandEncoder> blit;
@property(nonatomic,strong) id<MTLRenderCommandEncoder> render;
@property(nonatomic,strong) NSObject *renderState;
@property(nonatomic,strong) NSMutableDictionary<NSString *,id<MTLBuffer>> *buffers;
@end
id<MTLBuffer> nvn_metal_buffer(SMOGuestEncoder *,uint64_t,uint64_t,NSUInteger *);
id<MTLDevice> nvn_metal_device(void);
NSObject *nvn_metal_render_state(void);
int nvn_metal_render_encode(SMOGuestEncoder *,uint32_t,const void *,size_t);
void nvn_metal_render_end(SMOGuestEncoder *);
#endif
