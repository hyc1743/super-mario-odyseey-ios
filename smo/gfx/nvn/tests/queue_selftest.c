#include "../nvn_host.h"
#include "../nvn_gpu.h"
#include "../nvn_state.h"
#include "../nvn_cmd.h"
#include "../include/nvn.h"
#include <assert.h>

/* Explicitly installed test backend; production never uses simulated GPU completion. */
static uint64_t submitted, completed, failed;
static uint64_t submit(int count, const uint64_t *handles) {
    assert(count==0 && !handles);
    return ++submitted;
}
static int poll(uint64_t seq) { return failed && seq>=failed ? -1 : completed>=seq; }
static uint64_t call(const char *suffix, void *obj, uint64_t a, uint64_t b, uint64_t c) {
    char name[128]; snprintf(name,sizeof name,"nvn%s",suffix);
    nvn_fn fn=nvn_lookup(name); assert(fn && fn!=nvn_unimplemented);
    return fn((uintptr_t)obj,a,b,c,0,0,0,0);
}
int main(void) {
    NVNqueue q; NVNqueueBuilder b; NVNsync s;
    static const nvn_gpu_backend backend={submit,poll};
    call("QueueBuilderSetDefaults",&b,0,0,0);
    assert(!call("QueueInitialize",&q,(uintptr_t)&b,0,0));
    nvn_gpu_install_backend(&backend);
    assert(call("QueueInitialize",&q,(uintptr_t)&b,0,0));
    /* An unknown recorded GPU command rejects the entire batch before the
     * backend sees it, rather than dropping a draw and signaling its fence. */
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
    assert(!nvn_gpu_submit(1,&unknown) && submitted==0);
    call("CommandBufferFinalize",&cb,0,0,0);
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
    call("QueueFinalize",&q,0,0,0);
    call("SyncFinalize",&s,0,0,0);
    nvn_st_reset();
    puts("queue/fence: backend required, missing commands rejected, pool bounds, ordered completion, rearm, timeout and GPU failure passed");
    return 0;
}
