/* Actual guest submission backend. The separate nvn_mtl.m remains an offline
 * shader/rendering probe and is not used as evidence of game rendering. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include "nvn_cmd.h"
#include "nvn_gpu.h"
#include "nvn_host.h"
#include "nvn_state.h"
#include "nvn_cmd_render.h"
#import "nvn_metal_private.h"
#include <pthread.h>
#include <stdatomic.h>
#include <unistd.h>

static id<MTLDevice> device;
static id<MTLCommandQueue> metal_queue;
static NSObject *render_state;
id<MTLDevice> nvn_metal_device(void) { return device; }
static uint64_t submitted;
static _Atomic uint64_t completed;
static _Atomic uint64_t first_failure;
typedef struct completion_node {
    uint64_t sequence;
    int done;
    struct completion_node *next;
} completion_node;
static completion_node *completion_head, *completion_tail;
static pthread_mutex_t completion_lock=PTHREAD_MUTEX_INITIALIZER;

@implementation SMOGuestEncoder
@end

id<MTLBuffer> nvn_metal_buffer(SMOGuestEncoder *e, uint64_t address,
                                uint64_t size, NSUInteger *offset) {
    uint64_t base, length;
    if (!nvn_st_pool_range(address, size, &base, &length)) {
        NVN_LOG("NVN_GPU_INVALID_RANGE address=0x%llx size=%llu",
                (unsigned long long)address, (unsigned long long)size);
        return nil;
    }
    uint64_t page = (uint64_t)sysconf(_SC_PAGESIZE);
    uint64_t aligned = base & ~(page - 1);
    if (length > UINT64_MAX - base || base + length > UINT64_MAX - (page - 1)) return nil;
    uint64_t bytes = ((base + length + page - 1) & ~(page - 1)) - aligned;
    if (!bytes || bytes > device.maxBufferLength) return nil;
    NSString *key = [NSString stringWithFormat:@"%llx:%llx",
                    (unsigned long long)aligned, (unsigned long long)bytes];
    id<MTLBuffer> buffer = e.buffers[key];
    if (!buffer) {
        buffer = [device newBufferWithBytesNoCopy:(void *)(uintptr_t)aligned
                                          length:(NSUInteger)bytes
                                         options:MTLResourceStorageModeShared
                                     deallocator:nil];
        if (!buffer) return nil;
        e.buffers[key] = buffer;
    }
    *offset = (NSUInteger)(address - aligned);
    return buffer;
}
static id<MTLBlitCommandEncoder> blit_for(SMOGuestEncoder *e) {
    nvn_metal_render_end(e);
    if (!e.blit) e.blit = [e.command blitCommandEncoder];
    return e.blit;
}
static int encode(void *context, uint32_t op, const void *payload, size_t size) {
    SMOGuestEncoder *e = (__bridge SMOGuestEncoder *)context;
    if (op == NVN_CMD_BARRIER) {
        if (size != 8) return 0;
        [e.blit endEncoding]; e.blit = nil;
        nvn_metal_render_end(e);
        return 1;
    }
    if(op==NVN_CMD_COUNTER){
        nvn_cmd_counter c;if(size!=sizeof c)return 0;memcpy(&c,payload,sizeof c);
        if(c.type!=0 && c.type!=16){NVN_LOG("NVN_GPU_COUNTER_UNSUPPORTED type=%llu",(unsigned long long)c.type);return 0;}
        uint64_t base,length;if(!nvn_st_pool_range(c.address,16,&base,&length))return 0;
        /* Conservative boundary: drain actual encoded GPU work and use its
         * GPU clock result. This serializes timestamp reports until counter
         * sample buffers are implemented; it never substitutes a CPU clock. */
        [e.blit endEncoding];e.blit=nil;nvn_metal_render_end(e);
        /* Empty command buffers need not have a GPU timing interval. An
         * internal ordered blit gives this boundary an actual GPU clock sample. */
        id<MTLBuffer> marker=[device newBufferWithLength:4 options:MTLResourceStorageModeShared];
        id<MTLBlitCommandEncoder> clock=[e.command blitCommandEncoder];
        if(!marker||!clock){[clock endEncoding];return 0;}
        [clock fillBuffer:marker range:NSMakeRange(0,4) value:0];[clock endEncoding];
        [e.command commit];[e.command waitUntilCompleted];
        if(e.command.status!=MTLCommandBufferStatusCompleted)return 0;
        double gpu=e.command.GPUEndTime;
        if(!(gpu>0) || gpu>(double)UINT64_MAX/1e9)return 0;
        NVNcounterData value={0,(uint64_t)(gpu*1e9)};
        memcpy((void *)(uintptr_t)c.address,&value,sizeof value);
        e.command=[metal_queue commandBuffer];return e.command!=nil;
    }
    if(op>=NVN_CMD_SET_RT){
        int ok=nvn_metal_render_encode(e,op,payload,size);
        if(!ok)NVN_LOG("NVN_GPU_RENDER_ENCODING_FAILED op=%u size=%zu",op,size);
        return ok;
    }
    if (op == NVN_CMD_COPY_BUFFER) {
        nvn_cmd_copy c;
        if (size != sizeof c) return 0;
        memcpy(&c, payload, sizeof c);
        if (!c.size) return 1;
        if ((c.src | c.dst | c.size) & 3) return 0;
        if ((c.src <= c.dst && c.dst - c.src < c.size) ||
            (c.dst < c.src && c.src - c.dst < c.size)) return 0;
        NSUInteger src_offset, dst_offset;
        id<MTLBuffer> src = nvn_metal_buffer(e,c.src,c.size,&src_offset);
        id<MTLBuffer> dst = nvn_metal_buffer(e,c.dst,c.size,&dst_offset);
        id<MTLBlitCommandEncoder> blit = blit_for(e);
        if (!src || !dst || !blit) return 0;
        [blit copyFromBuffer:src sourceOffset:src_offset toBuffer:dst
          destinationOffset:dst_offset size:(NSUInteger)c.size];
        return 1;
    }
    uint64_t address, bytes;
    id<MTLBuffer> staging;
    if (op == NVN_CMD_UPDATE_BUFFER) {
        nvn_cmd_update u;
        if (size < sizeof u) return 0;
        memcpy(&u,payload,sizeof u);
        if (u.size > size - sizeof u) return 0;
        address=u.dst; bytes=u.size;
        if (!bytes) return 1;
        if (bytes > device.maxBufferLength || ((address | bytes) & 3)) return 0;
        staging = [device newBufferWithBytes:(const uint8_t *)payload + sizeof u
                                      length:(NSUInteger)bytes options:MTLResourceStorageModeShared];
    } else if (op == NVN_CMD_CLEAR_BUFFER) {
        nvn_cmd_clear c;
        if (size != sizeof c) return 0;
        memcpy(&c,payload,sizeof c);
        address=c.dst; bytes=c.size;
        if (!bytes) return 1;
        if (bytes > device.maxBufferLength || ((address | bytes) & 3)) return 0;
        staging = [device newBufferWithLength:(NSUInteger)bytes options:MTLResourceStorageModeShared];
        if (staging) {
            uint32_t *p=staging.contents;
            for (uint64_t i=0;i<bytes/4;++i) p[i]=c.value;
        }
    } else {
        NVN_LOG("NVN_GPU_UNSUPPORTED_OPCODE op=%u",op);
        return 0;
    }
    NSUInteger offset;
    id<MTLBuffer> dst=nvn_metal_buffer(e,address,bytes,&offset);
    id<MTLBlitCommandEncoder> blit=blit_for(e);
    if (!staging || !dst || !blit) return 0;
    [blit copyFromBuffer:staging sourceOffset:0 toBuffer:dst destinationOffset:offset size:(NSUInteger)bytes];
    return 1;
}
static uint64_t submit(int count, const uint64_t *handles) {
    @autoreleasepool {
        SMOGuestEncoder *e=[SMOGuestEncoder new];
        e.device=device;e.renderState=render_state;
        e.command=[metal_queue commandBuffer];
        e.buffers=[NSMutableDictionary new];
        if (!e.command) return 0;
        for (int i=0;i<count;++i) {
            if (!nvn_cmd_walk(handles[i],encode,(__bridge void *)e)) {
                [e.blit endEncoding];
                nvn_metal_render_end(e);
                NVN_LOG("NVN_GPU_ENCODING_FAILED handle_index=%d",i);
                return 0;
            }
        }
        [e.blit endEncoding]; e.blit=nil;
        nvn_metal_render_end(e);
        uint64_t sequence=++submitted;
        completion_node *node=calloc(1,sizeof *node);
        if (!node) return 0;
        node->sequence=sequence;
        pthread_mutex_lock(&completion_lock);
        if (completion_tail) completion_tail->next=node;
        else completion_head=node;
        completion_tail=node;
        pthread_mutex_unlock(&completion_lock);
        e.command.label=[NSString stringWithFormat:@"SMO guest submit %llu",(unsigned long long)sequence];
        [e.command addCompletedHandler:^(id<MTLCommandBuffer> command) {
            pthread_mutex_lock(&completion_lock);
            if (command.status != MTLCommandBufferStatusCompleted) {
                uint64_t old=atomic_load(&first_failure);
                while ((!old || sequence<old) && !atomic_compare_exchange_weak(&first_failure,&old,sequence)) {}
                NVN_LOG("NVN_GPU_ERROR sequence=%llu error=%s",(unsigned long long)sequence,
                        command.error.description.UTF8String ?: "unknown");
            }
            node->done=1;
            /* Handlers may run on different host threads. Publish only a
             * contiguous prefix, so a later success cannot hide an error in
             * an earlier submission whose handler has not run yet. */
            while (completion_head && completion_head->done) {
                completion_node *head=completion_head;
                atomic_store(&completed,head->sequence);
                completion_head=head->next;
                free(head);
            }
            if (!completion_head) completion_tail=NULL;
            pthread_mutex_unlock(&completion_lock);
            if (sequence==1 || sequence%60==0)
                NVN_LOG("NVN_GPU_COMPLETED sequence=%llu status=%ld",(unsigned long long)sequence,(long)command.status);
        }];
        [e.command commit];
        if (sequence==1 || sequence%60==0)
            NVN_LOG("NVN_GPU_SUBMITTED sequence=%llu handles=%d",(unsigned long long)sequence,count);
        return sequence;
    }
}
static int poll(uint64_t sequence) {
    uint64_t done=atomic_load(&completed);
    uint64_t failed=atomic_load(&first_failure);
    if (failed && sequence>=failed) return -1;
    return done>=sequence;
}
static void initialize(void) {
    @autoreleasepool {
        device=MTLCreateSystemDefaultDevice();
        metal_queue=[device newCommandQueue];
        if (!metal_queue) { NVN_LOG("NVN_GPU_UNAVAILABLE: Metal device/queue unavailable"); return; }
        render_state=nvn_metal_render_state();
        static const nvn_gpu_backend backend={submit,poll};
        nvn_gpu_install_backend(&backend);
        NVN_LOG("NVN_GPU_BACKEND device=%s",device.name.UTF8String);
    }
}
void nvn_metal_runtime_init(void) {
    static pthread_once_t once=PTHREAD_ONCE_INIT;
    pthread_once(&once,initialize);
}
