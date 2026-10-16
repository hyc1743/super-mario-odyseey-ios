#include "../nvn_host.h"
#include "../nvn_gpu.h"
#include "../nvn_state.h"
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
    puts("queue/fence: backend required, ordered completion, rearm, timeout and GPU failure passed");
    return 0;
}
