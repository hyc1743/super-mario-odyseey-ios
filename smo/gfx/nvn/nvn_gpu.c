#include "nvn_gpu.h"
#include "nvn_host.h"
#include "nvn_state.h"
#include "nvn_cmd.h"
#include "include/nvn.h"
#include <pthread.h>

static nvn_gpu_backend gpu;
static pthread_mutex_t submit_lock = PTHREAD_MUTEX_INITIALIZER;
typedef struct { unsigned count; const char *names[256]; int missing; } preflight;
static int check_command(void *context, uint32_t op, const void *payload, size_t size) {
    preflight *p=context;
    if (op != NVN_CMD_UNSUPPORTED) return 1;
    if (size != sizeof(nvn_cmd_missing)) return 0;
    nvn_cmd_missing m; memcpy(&m,payload,sizeof m);
    if(nvn_cmd_missing_is_skippable(m.name))return 1;
    p->missing=1;
    for (unsigned i=0;i<p->count;++i) if (p->names[i]==m.name) return 1;
    if (p->count<256) p->names[p->count++]=m.name;
    NVN_LOG("NVN_COMMAND_UNSUPPORTED %s",m.name);
    return 1; /* finish collecting this submission's gaps before rejecting it */
}
void nvn_gpu_install_backend(const nvn_gpu_backend *backend) { gpu = *backend; }
int nvn_gpu_init(void) {
#ifdef SMO_METAL_RUNTIME
    extern void nvn_metal_runtime_init(void);
    nvn_metal_runtime_init();
#endif
    return gpu.submit && gpu.poll;
}
uint64_t nvn_gpu_submit(int count, const uint64_t *handles) {
    if (!nvn_gpu_init() || count < 0 || (count && !handles)) return 0;
    preflight checked={0};
    for (int i=0;i<count;++i)
        if (!nvn_cmd_walk(handles[i],check_command,&checked)) return 0;
    if (checked.missing) return 0;
    pthread_mutex_lock(&submit_lock);
    uint64_t sequence = gpu.submit(count, handles);
    pthread_mutex_unlock(&submit_lock);
    return sequence;
}
int nvn_gpu_poll(uint64_t sequence) { return gpu.poll && sequence ? gpu.poll(sequence) : -1; }

enum { QUEUE_TYPE = 14, QUEUE_BUILDER_TYPE = 15, SYNC_TYPE = 9 };
enum { Q_DEVICE, Q_SEQUENCE };
enum { B_DEVICE, B_FLAGS, B_COMMAND, B_CONTROL, B_COMPUTE, B_THRESHOLD, B_MEMORY, B_SIZE };
static nvn_st_ent *queue(const NVNqueue *q) {
    nvn_st_ent *s = nvn_st_get(q, QUEUE_TYPE);
    if (!s) NVN_DIE("queue %p is not initialized", (const void *)q);
    return s;
}
uint64_t nvn_gpu_present_queue(void *q,const nvn_present_request *request) {
    nvn_st_ent *s=queue(q);
    if(!request || !nvn_gpu_init() || !gpu.present){
        NVN_LOG("NVN_GPU_PRESENT_UNAVAILABLE");return 0;
    }
    pthread_mutex_lock(&submit_lock);
    uint64_t sequence=gpu.present(request);
    if(sequence)__atomic_store_n(&s->v[Q_SEQUENCE],sequence,__ATOMIC_RELEASE);
    pthread_mutex_unlock(&submit_lock);
    return sequence;
}
static nvn_st_ent *builder(const NVNqueueBuilder *b) {
    nvn_st_ent *s = nvn_st_get(b, QUEUE_BUILDER_TYPE);
    if (!s) NVN_DIE("queue builder %p is not initialized", (const void *)b);
    return s;
}
void nvn_QueueBuilderSetDefaults(NVNqueueBuilder *b) {
    nvn_st_ent *s = nvn_st_get(b, QUEUE_BUILDER_TYPE);
    uint64_t device = s ? s->v[B_DEVICE] : 0;
    s = nvn_st_new(b, QUEUE_BUILDER_TYPE);
    if (!s) NVN_DIE("queue builder allocation failed");
    s->v[B_DEVICE] = device;
    s->v[B_COMMAND] = 65536; s->v[B_CONTROL] = 16384;
    s->v[B_COMPUTE] = 262144; s->v[B_THRESHOLD] = 8192;
}
void nvn_QueueBuilderSetDevice(NVNqueueBuilder *b, NVNdevice *d) {
    if (!nvn_st_get(b, QUEUE_BUILDER_TYPE)) nvn_QueueBuilderSetDefaults(b);
    builder(b)->v[B_DEVICE] = (uintptr_t)d;
}
#define SETTER(name,field) void nvn_QueueBuilderSet##name(NVNqueueBuilder *b, uint64_t value) { builder(b)->v[field] = value; }
SETTER(Flags,B_FLAGS)
SETTER(CommandMemorySize,B_COMMAND)
SETTER(ControlMemorySize,B_CONTROL)
SETTER(ComputeMemorySize,B_COMPUTE)
SETTER(CommandFlushThreshold,B_THRESHOLD)
#undef SETTER
void nvn_QueueBuilderSetQueueMemory(NVNqueueBuilder *b, void *memory, size_t size) {
    builder(b)->v[B_MEMORY] = (uintptr_t)memory; builder(b)->v[B_SIZE] = size;
}
size_t nvn_QueueBuilderGetQueueMemorySize(const NVNqueueBuilder *b) {
    (void)builder(b);
    /* All driver queue bookkeeping is host-owned. */
    return 0;
}
uint64_t nvn_QueueInitialize(NVNqueue *q, const NVNqueueBuilder *b) {
    nvn_st_ent *source = builder(b);
    if (!nvn_gpu_init()) { NVN_LOG("NVN_GPU_UNAVAILABLE: no real GPU backend"); return 0; }
    nvn_st_ent *s = nvn_st_new(q, QUEUE_TYPE);
    if (!s) return 0;
    s->v[Q_DEVICE] = source->v[B_DEVICE];
    return 1;
}
void nvn_QueueSubmitCommands(NVNqueue *q, int count, const NVNcommandHandle *handles) {
    nvn_st_ent *s = queue(q);
    uint64_t seq = nvn_gpu_submit(count, handles);
    if (!seq) NVN_DIE("NVN_GPU_SUBMIT_FAILED count=%d", count);
    __atomic_store_n(&s->v[Q_SEQUENCE], seq, __ATOMIC_RELEASE);
}
void nvn_QueueFlush(NVNqueue *q) { (void)queue(q); /* each submission commits immediately */ }
void nvn_QueueFinish(NVNqueue *q) {
    uint64_t seq = __atomic_load_n(&queue(q)->v[Q_SEQUENCE], __ATOMIC_ACQUIRE);
    if (!seq) return;
    int status;
    while (!(status = nvn_gpu_poll(seq))) nvn_host_sleep_us(200);
    if (status < 0) NVN_DIE("NVN_GPU_FAILED sequence=%llu", (unsigned long long)seq);
}
void nvn_QueueFinalize(NVNqueue *q) {
    nvn_QueueFinish(q);
    nvn_st_del(q, QUEUE_TYPE);
}
void nvn_QueueFenceSync(NVNqueue *q, NVNsync *sync, NVNsyncCondition condition, int flags) {
    (void)flags;
    if (condition != NVN_SYNC_CONDITION_ALL_GPU_COMMANDS_COMPLETE &&
        condition != NVN_SYNC_CONDITION_GRAPHICS_WORLD_SPACE_COMPLETE)
        NVN_DIE("unsupported fence condition %d", (int)condition);
    nvn_st_ent *s = nvn_st_get(sync, SYNC_TYPE);
    if (!s) NVN_DIE("FenceSync requires initialized sync");
    /* Commit a real ordered marker even before the queue's first workload.
     * This avoids inventing completion for an unsubmitted/empty timeline. */
    nvn_QueueSubmitCommands(q, 0, NULL);
    uint64_t seq = __atomic_load_n(&queue(q)->v[Q_SEQUENCE], __ATOMIC_ACQUIRE);
    __atomic_store_n(&s->v[1], 0, __ATOMIC_RELEASE);
    __atomic_store_n(&s->v[2], seq, __ATOMIC_RELEASE);
}
void nvn_QueueWaitSync(NVNqueue *q, const NVNsync *sync) {
    (void)queue(q);
    nvn_st_ent *s = nvn_st_get(sync, SYNC_TYPE);
    if (!s) NVN_DIE("QueueWaitSync requires initialized sync");
    /* A CPU wait is conservative across queues and preserves dependencies.
     * The GPU timeline is shared, so prior submitted work is already ordered. */
    while (!__atomic_load_n(&s->v[1], __ATOMIC_ACQUIRE)) {
        uint64_t seq = __atomic_load_n(&s->v[2], __ATOMIC_ACQUIRE);
        int status = seq ? nvn_gpu_poll(seq) : 0;
        if (status < 0) NVN_DIE("QueueWaitSync GPU failure");
        if (status > 0) return;
        nvn_host_sleep_us(200);
    }
}
