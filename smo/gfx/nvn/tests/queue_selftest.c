#include "../nvn_host.h"
#include "../nvn_gpu.h"
#include "../nvn_state.h"
#include "../nvn_cmd.h"
#include "../include/nvn.h"
#include <assert.h>

/* Explicitly installed test backend; production never uses simulated GPU completion. */
static uint64_t submitted, completed, failed;
static uint64_t submit(int count, const uint64_t *handles) {
    assert((count==0 && !handles)||(count==1 && handles));
    return ++submitted;
}
static int poll(uint64_t seq) { return failed && seq>=failed ? -1 : completed>=seq; }
static nvn_present_request last_present;
static uint64_t present(const nvn_present_request *request) {
    last_present=*request;
    return ++submitted;
}
static uint64_t call(const char *suffix, void *obj, uint64_t a, uint64_t b, uint64_t c) {
    char name[128]; snprintf(name,sizeof name,"nvn%s",suffix);
    nvn_fn fn=nvn_lookup(name); assert(fn && fn!=nvn_unimplemented);
    return fn((uintptr_t)obj,a,b,c,0,0,0,0);
}
int main(void) {
    NVNqueue q; NVNqueueBuilder b; NVNsync s;
    static const nvn_gpu_backend backend={submit,poll,present};
    call("QueueBuilderSetDefaults",&b,0,0,0);
    assert(!call("QueueInitialize",&q,(uintptr_t)&b,0,0));
    nvn_gpu_install_backend(&backend);
    assert(call("QueueInitialize",&q,(uintptr_t)&b,0,0));
    /* 未实现的命令现在**降级**：照常提交（backend 看得到），缺的那几条在录制时就已跳过。
     * 以前整单拒掉 → 上层 NVN_DIE → 真机在场景切换时闪退（第九轮改成降级）。 */
    NVNcommandBuffer cb;
    NVNmemoryPool pool;
    _Alignas(8) unsigned char command_memory[256];
    nvn_st_ent *p=nvn_st_new(&pool,4); assert(p);
    p->v[1]=(uintptr_t)command_memory; p->v[2]=sizeof command_memory;
    uint64_t base,length;
    assert(nvn_st_pool_range((uintptr_t)command_memory+8,248,&base,&length));
    assert(base==(uintptr_t)command_memory && length==256);
    assert(!nvn_st_pool_range((uintptr_t)command_memory+8,249,&base,&length));
    assert(!nvn_st_pool_range((uintptr_t)command_memory,UINT64_MAX,&base,&length));
    assert(call("CommandBufferInitialize",&cb,0,0,0));
    call("CommandBufferAddCommandMemory",&cb,(uintptr_t)&pool,0,sizeof command_memory);
    call("CommandBufferBeginRecording",&cb,0,0,0);
    call("CommandBufferDispatchComputeIndirect",&cb,0,0,3);
    call("CommandBufferResolveDepthBuffer",&cb,0,0,0);
    uint64_t unknown=call("CommandBufferEndRecording",&cb,0,0,0);
    assert(nvn_gpu_submit(1,&unknown) && submitted==1);   /* 降级：仍然提交 */
    submitted=0;                                          /* 后面的断言按自己的计数来 */
    call("CommandBufferFinalize",&cb,0,0,0);
    NVNcommandBuffer safe_cb;
    assert(call("CommandBufferInitialize",&safe_cb,0,0,0));
    call("CommandBufferAddCommandMemory",&safe_cb,(uintptr_t)&pool,0,sizeof command_memory);
    call("CommandBufferBeginRecording",&safe_cb,0,0,0);
    call("CommandBufferSetDepthClamp",&safe_cb,0,0,0);
    uint64_t safe=call("CommandBufferEndRecording",&safe_cb,0,0,0);
    assert(nvn_gpu_submit(1,&safe)==1 && submitted==1);
    call("CommandBufferFinalize",&safe_cb,0,0,0);
    submitted=0;
    assert(call("SyncInitialize",&s,0,0,0));
    call("QueueFenceSync",&q,(uintptr_t)&s,0,0);
    assert(submitted==1);
    assert(call("SyncWait",&s,0,0,0)==2);
    assert(call("SyncWait",&s,1000000,0,0)==2);
    completed=1;
    assert(call("SyncWait",&s,0,0,0)==0);
    assert(call("SyncWait",&s,0,0,0)==0);
    call("QueueFenceSync",&q,(uintptr_t)&s,0,0);
    assert(submitted==2 && call("SyncWait",&s,0,0,0)==2);
    completed=2;
    call("QueueWaitSync",&q,(uintptr_t)&s,0,0);
    call("QueueFinish",&q,0,0,0);
    call("QueueFenceSync",&q,(uintptr_t)&s,0,0);
    failed=3; completed=3;
    assert(call("SyncWait",&s,0,0,0)==3);
    failed=0;
    NVNwindow window;NVNwindowBuilder wb;NVNtexture textures[2];
    NVNtexture *texture_array[]={&textures[0],&textures[1]};
    for(unsigned i=0;i<2;i++){
        nvn_st_ent *t=nvn_st_new(&textures[i],12);assert(t);
        t->v[1]=16;t->v[2]=16;t->v[5]=37;t->v[15]=101+i;
    }
    call("WindowBuilderSetDefaults",&wb,0,0,0);
    call("WindowBuilderSetNativeWindow",&wb,1,0,0);
    call("WindowBuilderSetPresentInterval",&wb,2,0,0);
    call("WindowBuilderSetTextures",&wb,2,(uintptr_t)texture_array,0);
    texture_array[0]=NULL; /* The builder copied the array, not its address. */
    assert(call("WindowInitialize",&window,(uintptr_t)&wb,0,0));
    assert(call("WindowGetNativeWindow",&window,0,0,0)==1);
    assert(call("WindowGetPresentInterval",&window,0,0,0)==2);
    NVNrectangle crop={0};
    ((PFNNVNWINDOWSETCROPPROC)(uintptr_t)nvn_lookup("nvnWindowSetCrop"))(&window,1,2,8,8);
    ((PFNNVNWINDOWGETCROPPROC)(uintptr_t)nvn_lookup("nvnWindowGetCrop"))(&window,&crop);
    assert(crop.x==1 && crop.y==2 && crop.width==8 && crop.height==8);
    int index=-1;
    assert(!call("WindowAcquireTexture",&window,(uintptr_t)&s,(uintptr_t)&index,0) && index==0);
    assert(call("SyncWait",&s,0,0,0)==0); /* Unused buffer is initially owned by the window. */
    call("QueuePresentTexture",&q,(uintptr_t)&window,0,0);
    uint64_t first_present=submitted;
    assert(first_present>completed && last_present.texture.words[15]==101 && last_present.interval==2);
    assert(!memcmp(last_present.crop,&crop,sizeof crop));
    assert(!call("QueueAcquireTexture",&q,(uintptr_t)&window,(uintptr_t)&index,0) && index==1);
    call("QueuePresentTexture",&q,(uintptr_t)&window,1,0);
    assert(last_present.texture.words[15]==102 && submitted==first_present+1);
    completed=submitted;
    assert(!call("WindowAcquireTexture",&window,(uintptr_t)&s,(uintptr_t)&index,0) && index==0);
    assert(nvn_st_get(&s,9)->v[2]==first_present && call("SyncWait",&s,0,0,0)==0);
    call("WindowFinalize",&window,0,0,0);
    call("QueueFinalize",&q,0,0,0);
    call("SyncFinalize",&s,0,0,0);
    nvn_st_reset();
    puts("queue/fence/window: backend required, missing commands, pool bounds, ordered completion, timeout/error, copied texture arrays, crop, ownership and GPU-sequence reuse passed");
    return 0;
}
