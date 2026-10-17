#ifndef SMO_NVN_METAL_PRIVATE_H
#define SMO_NVN_METAL_PRIVATE_H
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "nvn_resources.h"

@interface SMOGuestEncoder : NSObject
@property(nonatomic,strong) id<MTLDevice> device;
@property(nonatomic,strong) id<MTLCommandBuffer> command;
@property(nonatomic,strong) id<MTLBlitCommandEncoder> blit;
@property(nonatomic,strong) id<MTLRenderCommandEncoder> render;
@property(nonatomic,strong) NSObject *renderState;
@property(nonatomic,strong) NSMutableDictionary<NSString *,id<MTLBuffer>> *buffers;
@property(nonatomic,strong) id<MTLBuffer> visibilityBuffer;
@property(nonatomic) NSUInteger visibilityCursor;
@end
id<MTLBuffer> nvn_metal_buffer(SMOGuestEncoder *,uint64_t,uint64_t,NSUInteger *);
id<MTLDevice> nvn_metal_device(void);
NSObject *nvn_metal_render_state(void);
int nvn_metal_render_encode(SMOGuestEncoder *,uint32_t,const void *,size_t);
void nvn_metal_render_end(SMOGuestEncoder *);
uint64_t nvn_metal_render_generation(NSObject *,const nvn_resource_snapshot *);
uint64_t nvn_metal_offscreen_frames(void);
/* The sink receives an immutable GPU-completed snapshot on a native thread.
 * It must enqueue bounded display work and return without acquiring a drawable. */
void nvn_metal_set_frame_sink(void (^)(id<MTLTexture>,int,uint64_t));
/* Optional frontend backpressure, invoked on the native encoding worker
 * before taking a present snapshot. Never called while sink_lock is held. */
void nvn_metal_set_frame_pacer(void (^)(int));
#endif
