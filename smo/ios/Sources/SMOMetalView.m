#import "SMOMetalView.h"
#import <QuartzCore/CAMetalLayer.h>
#include "logbuf.h"

@implementation SMOMetalView {
    NSLock *_lock;
    dispatch_queue_t _worker;
    id<MTLTexture> _pending;
    uint64_t _latestOrdinal,_presented;
    int _interval;
    BOOL _scheduled,_active;
    id<MTLCommandQueue> _queue;
    id<MTLRenderPipelineState> _pipeline;
}
+ (Class)layerClass { return CAMetalLayer.class; }
- (instancetype)initWithFrame:(CGRect)frame {
    if((self=[super initWithFrame:frame])){
        _lock=[NSLock new];_active=YES;
        _worker=dispatch_queue_create("smo.display",DISPATCH_QUEUE_SERIAL);
        CAMetalLayer *layer=(CAMetalLayer *)self.layer;
        layer.device=MTLCreateSystemDefaultDevice();
        layer.pixelFormat=MTLPixelFormatBGRA8Unorm_sRGB;
        layer.framebufferOnly=YES;layer.opaque=YES;
        layer.allowsNextDrawableTimeout=YES;
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(suspendDisplay) name:UIApplicationWillResignActiveNotification object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(resumeDisplay) name:UIApplicationDidBecomeActiveNotification object:nil];
    }
    return self;
}
- (void)dealloc { [[NSNotificationCenter defaultCenter] removeObserver:self]; }
- (void)layoutSubviews {
    [super layoutSubviews];
    CAMetalLayer *layer=(CAMetalLayer *)self.layer;
    CGFloat scale=self.window.screen.scale ?: UIScreen.mainScreen.scale;
    /* The game renders at its own resolution. Avoid a second full native
     * 3x-resolution pass just to scale a 720p image on a phone. */
    scale=MIN(scale,1280.0/MAX(1,MAX(self.bounds.size.width,self.bounds.size.height)));
    layer.contentsScale=scale;
    layer.drawableSize=CGSizeMake(MAX(1,self.bounds.size.width*scale),MAX(1,self.bounds.size.height*scale));
}
- (void)suspendDisplay { [_lock lock];_active=NO;_pending=nil;[_lock unlock]; }
- (void)resumeDisplay { [_lock lock];_active=YES;[_lock unlock]; }
- (void)offerTexture:(id<MTLTexture>)texture interval:(int)interval ordinal:(uint64_t)ordinal {
    [_lock lock];
    if(!_active || ordinal<=_latestOrdinal){[_lock unlock];return;}
    _latestOrdinal=ordinal;_pending=texture;_interval=interval;
    if(!_scheduled){
        _scheduled=YES;
        dispatch_async(_worker, ^{ [self drainFrames]; });
    }
    [_lock unlock];
}
- (BOOL)preparePipeline {
    if(_pipeline && _queue)return YES;
    CAMetalLayer *layer=(CAMetalLayer *)self.layer;
    /* This independently authored shader copies/scales the completed game
     * image. It never replaces guest scene shaders or creates game content. */
    NSString *source=@"#include <metal_stdlib>\nusing namespace metal;\n"
        "struct V {float4 p[[position]];float2 uv;};\n"
        "vertex V copyVertex(uint i[[vertex_id]]){float2 p[3]={float2(-1,-1),float2(3,-1),float2(-1,3)};"
        "V v;v.p=float4(p[i],0,1);v.uv=float2((p[i].x+1)*0.5,(1-p[i].y)*0.5);return v;}\n"
        "fragment float4 copyFragment(V v[[stage_in]],texture2d<float> image[[texture(0)]]){"
        "constexpr sampler s(coord::normalized,address::clamp_to_edge,filter::linear);return image.sample(s,v.uv);}\n";
    NSError *error=nil;id<MTLLibrary> library=[layer.device newLibraryWithSource:source options:nil error:&error];
    MTLRenderPipelineDescriptor *desc=[MTLRenderPipelineDescriptor new];
    desc.vertexFunction=[library newFunctionWithName:@"copyVertex"];
    desc.fragmentFunction=[library newFunctionWithName:@"copyFragment"];
    desc.colorAttachments[0].pixelFormat=layer.pixelFormat;
    if(library)_pipeline=[layer.device newRenderPipelineStateWithDescriptor:desc error:&error];
    _queue=[layer.device newCommandQueue];
    if(!_pipeline || !_queue){dm_log_emit("NVN_DISPLAY_PIPELINE_FAILED domain=%s code=%ld",error.domain.UTF8String ?: "unknown",(long)error.code);return NO;}
    return YES;
}
- (void)drainFrames {
    for(;;){
      @autoreleasepool {
        [_lock lock];id<MTLTexture> texture=_pending;_pending=nil;
        int interval=_interval;BOOL active=_active;
        if(!texture || !active){_scheduled=NO;[_lock unlock];return;}
        [_lock unlock];
        if(![self preparePipeline])continue;
        CAMetalLayer *layer=(CAMetalLayer *)self.layer;
        if(texture.device!=layer.device){dm_log_emit("NVN_DISPLAY_DEVICE_MISMATCH");continue;}
        id<CAMetalDrawable> drawable=[layer nextDrawable];if(!drawable)continue;
        MTLRenderPassDescriptor *pass=[MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture=drawable.texture;
        pass.colorAttachments[0].loadAction=MTLLoadActionClear;
        pass.colorAttachments[0].storeAction=MTLStoreActionStore;
        pass.colorAttachments[0].clearColor=MTLClearColorMake(0,0,0,1);
        id<MTLCommandBuffer> command=[_queue commandBuffer];
        id<MTLRenderCommandEncoder> encoder=[command renderCommandEncoderWithDescriptor:pass];
        if(!encoder)continue;
        double width=drawable.texture.width,height=drawable.texture.height;
        double scale=MIN(width/texture.width,height/texture.height);
        double w=texture.width*scale,h=texture.height*scale;
        [encoder setViewport:(MTLViewport){(width-w)/2,(height-h)/2,w,h,0,1}];
        [encoder setRenderPipelineState:_pipeline];[encoder setFragmentTexture:texture atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];[encoder endEncoding];
        [drawable addPresentedHandler:^(id<MTLDrawable> shown){
            (void)shown;[self->_lock lock];uint64_t count=++self->_presented;[self->_lock unlock];
            if(count==1 || count%60==0)dm_log_emit("GUEST_DISPLAY_FRAME_PRESENTED count=%llu",(unsigned long long)count);
            if(count==1)dispatch_async(dispatch_get_main_queue(), ^{ if(self.onFirstFrame)self.onFirstFrame(); });
        }];
        if(interval>0)[command presentDrawable:drawable afterMinimumDuration:interval/60.0];
        else [command presentDrawable:drawable];
        [command commit];[command waitUntilCompleted];
        if(command.status!=MTLCommandBufferStatusCompleted)
            dm_log_emit("NVN_DISPLAY_GPU_FAILED domain=%s code=%ld",command.error.domain.UTF8String ?: "unknown",(long)command.error.code);
      }
    }
}
@end
