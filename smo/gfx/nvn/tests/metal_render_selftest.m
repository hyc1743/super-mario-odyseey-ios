/* Independently authored test shaders exercise the same guest command backend.
 * They are test inputs, never substitutes for a missing game shader. */
#import "../nvn_metal_private.h"
#import "../nvn_metal_resources.h"
#include "../nvn_host.h"
#include "../nvn_state.h"
#include "../nvn_shader.h"
#include "../nvn_gpu.h"
#include "../nvn_render_state.h"
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#define P(x) ((uint64_t)(uintptr_t)(x))
#define API(n,t) ((t)(uintptr_t)nvn_lookup("nvn" #n))
static uint64_t call(const char *name,void *obj,uint64_t a,uint64_t b,uint64_t c,uint64_t d,uint64_t f) {
    nvn_fn fn=nvn_lookup(name);assert(fn&&fn!=nvn_unimplemented&&!nvn_is_missing_command(fn));
    return fn(P(obj),a,b,c,d,f,0,0);
}
#define C(n,o,a,b,c,d,f) call("nvn" #n,o,a,b,c,d,f)
typedef struct { NVNqueue *queue;NVNsync *sync;uint64_t handle; } small_stack_submission;
static void *submit_small_stack(void *context) {
    small_stack_submission *s=context;
    C(QueueSubmitCommands,s->queue,1,P(&s->handle),0,0,0);
    C(QueueFenceSync,s->queue,P(s->sync),0,0,0,0);
    return NULL;
}
static void write_shader(NSString *root,FILE *index,uint8_t *code,unsigned seed,int stage,NSString *source,NSArray *args,NSArray *inputs) {
    for(unsigned i=0;i<256;i++)code[i]=(uint8_t)(i*13+seed);
    uint64_t key=nvn_shader_key(code,256);
    NSString *name=[NSString stringWithFormat:@"%016llx",(unsigned long long)key];
    fprintf(index,"%016llx\t%d\t%s.metal\t256\t%016llx\t%016llx\t%016llx\n",
        (unsigned long long)key,stage,name.UTF8String,(unsigned long long)nvn_shader_key(code,64),
        (unsigned long long)nvn_shader_key(code+0x50,256-0x50),(unsigned long long)nvn_shader_key(code+0x50,64));
    assert([source writeToFile:[root stringByAppendingPathComponent:[name stringByAppendingString:@".metal"]]
        atomically:YES encoding:NSUTF8StringEncoding error:NULL]);
    NSDictionary *bindings=@{@"version":@1,@"key":name,@"stage":@(stage),@"entry":@"main0",@"argument_buffer":@0,
        @"arguments":args,@"unsupported_arguments":@[],@"vertex_inputs":inputs};
    NSData *data=[NSJSONSerialization dataWithJSONObject:bindings options:0 error:NULL];assert(data);
    assert([data writeToFile:[root stringByAppendingPathComponent:[name stringByAppendingString:@".bindings.json"]] atomically:YES]);
}
int main(void) {
    @autoreleasepool {
        size_t page=(size_t)sysconf(_SC_PAGESIZE);uint8_t *memory=NULL;assert(!posix_memalign((void **)&memory,page,page*8));memset(memory,0,page*8);
        NVNmemoryPool pool;NVNcommandBuffer cb;NVNqueue q;NVNqueueBuilder qb;NVNsync sync;
        NVNtexture target,sample;NVNsampler sampler;NVNsamplerBuilder sb;NVNprogram program;NVNtexturePool tp;NVNsamplerPool sp;
        nvn_st_ent *p=nvn_st_new(&pool,4);p->v[1]=P(memory);p->v[2]=page*8;
        nvn_st_ent *t=nvn_st_new(&target,12);uint64_t words[16]={0,16,16,1,1,37,16,1,1,P(&pool),page*3,64,0,0,0,1};memcpy(t->v,words,sizeof words);
        nvn_st_ent *st=nvn_st_new(&sample,12);words[1]=words[2]=1;words[10]=page*4;words[11]=32;words[15]=2;memcpy(st->v,words,sizeof words);
        memset(memory+page*4,255,4);
        float vertices[6]={-1,-1,3,-1,-1,3};uint16_t indices[3]={0,1,2};float color[4]={1,0,0,1};
        memcpy(memory+page*2,vertices,sizeof vertices);memcpy(memory+page*2+128,indices,sizeof indices);memcpy(memory+page*5,color,sizeof color);
        NSString *root=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        assert([[NSFileManager defaultManager] createDirectoryAtPath:root withIntermediateDirectories:YES attributes:nil error:NULL]);
        setenv("SMO_SHADER_DIR",root.UTF8String,1);NSString *idx=[root stringByAppendingPathComponent:@"shaders.idx"];
        FILE *index=fopen(idx.fileSystemRepresentation,"wb");assert(index);
        NSString *vs=@"#include <metal_stdlib>\nusing namespace metal;\nstruct Input{float2 p[[attribute(0)]];};\nvertex float4 main0(Input v[[stage_in]]){return float4(v.p,0,1);}\n";
        NSString *fs=@"#include <metal_stdlib>\nusing namespace metal;\nstruct A{constant float4 *color[[id(0)]];texture2d<float> image[[id(1)]];sampler smp[[id(2)]];};\nfragment float4 main0(constant A &a[[buffer(0)]]){return *a.color*a.image.sample(a.smp,float2(0.5));}\n";
        write_shader(root,index,memory+page*7,7,1,vs,@[],@[@{@"location":@0,@"type":@"vec2"}]);
        write_shader(root,index,memory+page*7+512,19,5,fs,@[
            @{@"id":@0,@"kind":@"cbuf",@"nvn_slot":@0},
            @{@"id":@1,@"kind":@"texture",@"nvn_slot":@0,@"array":@1,@"separate":@NO},
            @{@"id":@2,@"kind":@"sampler",@"nvn_slot":@0,@"array":@1,@"separate":@NO}],@[]);
        fclose(index);nvn_shader_reset();assert(nvn_shader_load(idx.fileSystemRepresentation)==2);
        C(ProgramInitialize,&program,0,0,0,0,0);nvn_shader_data shaders[2]={{P(memory+page*7),NULL},{P(memory+page*7+512),NULL}};
        assert(C(ProgramSetShaders,&program,2,P(shaders),0,0,0));
        C(SamplerBuilderSetDefaults,&sb,0,0,0,0,0);assert(C(SamplerInitialize,&sampler,P(&sb),0,0,0,0));
        assert(C(TexturePoolInitialize,&tp,P(&pool),page*6,16,0,0));assert(C(SamplerPoolInitialize,&sp,P(&pool),page*6+512,16,0,0));
        C(TexturePoolRegisterTexture,&tp,1,P(&sample),0,0,0);C(SamplerPoolRegisterSampler,&sp,1,P(&sampler),0,0,0);
        NVNvertexAttribState attribute;nvn_VertexAttribStateSetDefaults(&attribute);nvn_VertexAttribStateSetFormat(&attribute,NVN_FORMAT_RG32F,0);
        NVNvertexStreamState stream;nvn_VertexStreamStateSetDefaults(&stream);nvn_VertexStreamStateSetStride(&stream,8);
        assert(C(CommandBufferInitialize,&cb,0,0,0,0,0));C(CommandBufferAddCommandMemory,&cb,P(&pool),0,page*2,0,0);
        C(CommandBufferBeginRecording,&cb,0,0,0,0,0);const NVNtexture *targets[1]={&target};
        C(CommandBufferSetRenderTargets,&cb,1,P(targets),0,0,0);float clear[4]={0,0,1,1};C(CommandBufferClearColor,&cb,0,P(clear),15,0,0);
        C(CommandBufferSetViewport,&cb,0,0,16,16,0);C(CommandBufferSetScissor,&cb,4,4,8,8,0);
        C(CommandBufferBindVertexAttribState,&cb,1,P(&attribute),0,0,0);C(CommandBufferBindVertexStreamState,&cb,1,P(&stream),0,0,0);
        C(CommandBufferBindVertexBuffer,&cb,0,P(memory+page*2),sizeof vertices,0,0);
        C(CommandBufferBindUniformBuffer,&cb,1,0,P(memory+page*5),16,0);C(CommandBufferBindProgram,&cb,P(&program),3,0,0,0);
        C(CommandBufferSetTexturePool,&cb,P(&tp),0,0,0,0);C(CommandBufferSetSamplerPool,&cb,P(&sp),0,0,0,0);
        C(CommandBufferBindTexture,&cb,1,0,1|UINT64_C(1)<<20,0,0);
        C(CommandBufferDrawElements,&cb,NVN_DRAW_PRIMITIVE_TRIANGLES,NVN_INDEX_TYPE_UNSIGNED_SHORT,3,P(memory+page*2+128),0);
        C(CommandBufferReportCounter,&cb,0,P(memory+page*5+128),0,0,0);
        uint64_t handle=C(CommandBufferEndRecording,&cb,0,0,0,0,0);
        C(QueueBuilderSetDefaults,&qb,0,0,0,0,0);assert(C(QueueInitialize,&q,P(&qb),0,0,0,0));assert(C(SyncInitialize,&sync,0,0,0,0,0));
        small_stack_submission submission={&q,&sync,handle};
        pthread_attr_t attr;pthread_t thread;assert(!pthread_attr_init(&attr));
        size_t stack=16384;if(stack<PTHREAD_STACK_MIN)stack=PTHREAD_STACK_MIN;
        assert(!pthread_attr_setstacksize(&attr,stack));
        assert(!pthread_create(&thread,&attr,submit_small_stack,&submission));
        assert(!pthread_attr_destroy(&attr));assert(!pthread_join(thread,NULL));
        uint64_t wait=C(SyncWait,&sync,UINT64_C(5000000000),0,0,0,0);assert(wait<2);
        nvn_resource_snapshot snapshot;assert(nvn_resources_snapshot(&target,12,&snapshot));
        id<MTLTexture> tex=nvn_metal_texture(nvn_metal_device(),&snapshot,NULL,0);assert(tex);
        uint8_t pixels[16*16*4];[tex getBytes:pixels bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,16) mipmapLevel:0];
        for(int y=0;y<16;y++)for(int x=0;x<16;x++){
            uint8_t *pixel=pixels+(y*16+x)*4;int inside=x>=4&&x<12&&y>=4&&y<12;
            assert(pixel[0]==(inside?255:0)&&pixel[1]==0&&pixel[2]==(inside?0:255)&&pixel[3]==255);
        }
        NVNcounterData timestamp;memcpy(&timestamp,memory+page*5+128,sizeof timestamp);assert(timestamp.timestamp>0);
        assert(API(DeviceGetTimestampInNanoseconds,PFNNVNDEVICEGETTIMESTAMPINNANOSECONDSPROC)(NULL,&timestamp)==timestamp.timestamp);
        C(QueueFinalize,&q,0,0,0,0,0);C(SyncFinalize,&sync,0,0,0,0,0);C(CommandBufferFinalize,&cb,0,0,0,0,0);
        C(TexturePoolFinalize,&tp,0,0,0,0,0);C(SamplerPoolFinalize,&sp,0,0,0,0,0);
        nvn_metal_resources_reset();nvn_shader_reset();nvn_st_reset();free(memory);
        assert([[NSFileManager defaultManager] removeItemAtPath:root error:NULL]);
        puts("METAL_RENDER_PASS: guest indexed draw, vertex layout, shader compile, uniform/texture/sampler arguments, scissor, readback, GPU timestamp and fence");
    }
    return 0;
}
