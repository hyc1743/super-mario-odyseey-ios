/* Hardware-backed command/fence test: no synthetic completion or CPU visitor. */
#include "../nvn_host.h"
#include "../nvn_gpu.h"
#include "../nvn_state.h"
#include "../include/nvn.h"
#include <assert.h>
#include <unistd.h>

#define P(x) ((uint64_t)(uintptr_t)(x))
static uint64_t call(const char *suffix, void *object, uint64_t a, uint64_t b,
                     uint64_t c, uint64_t d, uint64_t e) {
    char name[128]; snprintf(name,sizeof name,"nvn%s",suffix);
    nvn_fn fn=nvn_lookup(name); assert(fn && fn!=nvn_unimplemented && !nvn_is_missing_command(fn));
    return fn(P(object),a,b,c,d,e,0,0);
}
#define CALL(name,obj,a,b,c,d,e) call(#name,obj,a,b,c,d,e)
int main(void) {
    size_t page=(size_t)sysconf(_SC_PAGESIZE);
    void *memory=NULL;
    assert(!posix_memalign(&memory,page,page*4));
    memset(memory,0,page*4);
    NVNmemoryPool pool; NVNcommandBuffer cb; NVNqueue q; NVNqueueBuilder qb; NVNsync sync;
    nvn_st_ent *s=nvn_st_new(&pool,4); assert(s);
    s->v[1]=P(memory); s->v[2]=page*4;
    uint32_t *dst=(uint32_t *)((char *)memory+page*2);
    uint32_t *copy=(uint32_t *)((char *)memory+page*3);
    uint32_t data[4]={0x11223344,0x55667788,0xabcdef01,0x76543210};
    assert(CALL(CommandBufferInitialize,&cb,0,0,0,0,0));
    CALL(CommandBufferAddCommandMemory,&cb,P(&pool),0,page,0,0);
    CALL(CommandBufferBeginRecording,&cb,0,0,0,0,0);
    CALL(CommandBufferClearBuffer,&cb,P(dst),64,0x99887766,0,0);
    CALL(CommandBufferUpdateUniformBuffer,&cb,P(dst),64,4,16,P(data));
    CALL(CommandBufferBarrier,&cb,0,0,0,0,0);
    CALL(CommandBufferCopyBufferToBuffer,&cb,P(dst),P(copy),64,0,0);
    uint64_t handle=CALL(CommandBufferEndRecording,&cb,0,0,0,0,0);
    data[0]=0; assert(dst[0]==0 && copy[0]==0);
    CALL(QueueBuilderSetDefaults,&qb,0,0,0,0,0);
    assert(CALL(QueueInitialize,&q,P(&qb),0,0,0,0));
    assert(CALL(SyncInitialize,&sync,0,0,0,0,0));
    CALL(QueueSubmitCommands,&q,1,P(&handle),0,0,0);
    CALL(QueueFenceSync,&q,P(&sync),0,0,0,0);
    uint64_t result=CALL(SyncWait,&sync,UINT64_C(5000000000),0,0,0,0);
    assert(result==0 || result==1);
    assert(CALL(SyncWait,&sync,0,0,0,0,0)==0);
    assert(dst[0]==0x99887766 && dst[1]==0x11223344 && dst[4]==0x76543210);
    assert(!memcmp(dst,copy,64));
    /* Reuse the handle and fence: copied update bytes remain immutable. */
    CALL(QueueSubmitCommands,&q,1,P(&handle),0,0,0);
    CALL(QueueFenceSync,&q,P(&sync),0,0,0,0);
    result=CALL(SyncWait,&sync,UINT64_C(5000000000),0,0,0,0);
    assert(result==0 || result==1);
    assert(copy[1]==0x11223344);
    CALL(QueueFinalize,&q,0,0,0,0,0);
    CALL(SyncFinalize,&sync,0,0,0,0,0);
    CALL(CommandBufferFinalize,&cb,0,0,0,0,0);
    nvn_st_reset(); free(memory);
    puts("METAL_GUEST_QUEUE_PASS: real GPU clear/update/copy, readback, repeated handles and fence completion");
    return 0;
}
