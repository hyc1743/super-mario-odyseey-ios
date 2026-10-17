/* Window ownership and reuse track real GPU completion. API signatures and the
 * four-texture NVN limit follow the supplied header and d2r-mobile reference.
 * Copyright (c) 2026 jhaizhou, MIT; see third_party/d2r-mobile.LICENSE. */
#include "nvn_host.h"
#include "nvn_state.h"
#include "nvn_gpu.h"
#include "include/nvn.h"
#include <pthread.h>

enum { WINDOW_BUILDER=18, WINDOW=19, SYNC=9 };
enum { B_DEVICE,B_NATIVE,B_INTERVAL,B_COUNT,B_TEXTURES };
typedef struct {
    pthread_mutex_t lock;
    uint64_t device,native;
    unsigned count,next,acquired;
    int interval;
    int32_t crop[4];
    nvn_resource_snapshot textures[4];
    uint64_t pending[4];
} window_state;
static nvn_st_ent *builder(const NVNwindowBuilder *b) {
    nvn_st_ent *s=nvn_st_get(b,WINDOW_BUILDER);
    if(!s)NVN_DIE("window builder is not initialized");
    return s;
}
static window_state *window(const NVNwindow *w) {
    nvn_st_ent *s=nvn_st_get(w,WINDOW);
    if(!s || !s->v[0])NVN_DIE("window is not initialized");
    return (window_state *)(uintptr_t)s->v[0];
}
void nvn_WindowBuilderSetDefaults(NVNwindowBuilder *b) {
    nvn_st_ent *old=nvn_st_get(b,WINDOW_BUILDER);
    uint64_t device=old?old->v[B_DEVICE]:0;
    nvn_st_ent *s=nvn_st_new(b,WINDOW_BUILDER);
    if(!s)NVN_DIE("window builder allocation failed");
    s->v[B_DEVICE]=device;s->v[B_INTERVAL]=1;
}
static nvn_st_ent *ensure_builder(NVNwindowBuilder *b) {
    if(!nvn_st_get(b,WINDOW_BUILDER))nvn_WindowBuilderSetDefaults(b);
    return builder(b);
}
void nvn_WindowBuilderSetDevice(NVNwindowBuilder *b,NVNdevice *device) { ensure_builder(b)->v[B_DEVICE]=(uintptr_t)device; }
void nvn_WindowBuilderSetNativeWindow(NVNwindowBuilder *b,NVNnativeWindow native) { ensure_builder(b)->v[B_NATIVE]=(uintptr_t)native; }
void nvn_WindowBuilderSetPresentInterval(NVNwindowBuilder *b,int interval) {
    if(interval<0 || interval>5)NVN_DIE("invalid window present interval");
    ensure_builder(b)->v[B_INTERVAL]=(unsigned)interval;
}
void nvn_WindowBuilderSetTextures(NVNwindowBuilder *b,int count,NVNtexture *const *textures) {
    if(count<0 || count>4 || (count&&!textures))NVN_DIE("invalid window textures");
    nvn_st_ent *s=ensure_builder(b);s->v[B_COUNT]=(unsigned)count;
    memset(s->v+B_TEXTURES,0,4*sizeof(uint64_t));
    for(int i=0;i<count;i++)s->v[B_TEXTURES+i]=(uintptr_t)textures[i];
}
NVNnativeWindow nvn_WindowBuilderGetNativeWindow(const NVNwindowBuilder *b) { return (void *)(uintptr_t)builder(b)->v[B_NATIVE]; }
int nvn_WindowBuilderGetPresentInterval(const NVNwindowBuilder *b) { return (int)builder(b)->v[B_INTERVAL]; }
NVNboolean nvn_WindowInitialize(NVNwindow *w,const NVNwindowBuilder *b) {
    nvn_st_ent *s=builder(b);
    if(nvn_st_get(w,WINDOW) || s->v[B_COUNT]<2 || s->v[B_COUNT]>4 || !s->v[B_NATIVE])return 0;
    window_state *state=calloc(1,sizeof *state);if(!state)return 0;
    state->device=s->v[B_DEVICE];state->native=s->v[B_NATIVE];
    state->count=(unsigned)s->v[B_COUNT];state->interval=(int)s->v[B_INTERVAL];
    for(unsigned i=0;i<state->count;i++) {
        if(!nvn_resources_snapshot((void *)(uintptr_t)s->v[B_TEXTURES+i],12,&state->textures[i])){free(state);return 0;}
        const uint64_t *t=state->textures[i].words;
        if(!t[1] || !t[2] || (i && (t[1]!=state->textures[0].words[1] || t[2]!=state->textures[0].words[2] || t[5]!=state->textures[0].words[5]))){free(state);return 0;}
    }
    if(pthread_mutex_init(&state->lock,NULL)){free(state);return 0;}
    s=nvn_st_new(w,WINDOW);
    if(!s){pthread_mutex_destroy(&state->lock);free(state);return 0;}
    s->v[0]=(uintptr_t)state;
    NVN_LOG("NVN_WINDOW_READY textures=%u width=%llu height=%llu",state->count,
        (unsigned long long)state->textures[0].words[1],(unsigned long long)state->textures[0].words[2]);
    return 1;
}
static int acquire(NVNwindow *w,NVNsync *sync,int *index) {
    if(!index)return 1;
    nvn_st_ent *fence=sync?nvn_st_get(sync,SYNC):NULL;
    if(sync&&!fence)return 1;
    window_state *s=window(w);
    for(;;) {
        pthread_mutex_lock(&s->lock);
        for(unsigned n=0;n<s->count;n++) {
            unsigned i=(s->next+n)%s->count;
            if(s->acquired&(1u<<i))continue;
            uint64_t sequence=s->pending[i];int status=sequence?nvn_gpu_poll(sequence):1;
            if(status<0){pthread_mutex_unlock(&s->lock);return 1;}
            if(!status)continue;
            s->acquired|=1u<<i;s->next=(i+1)%s->count;*index=(int)i;
            if(fence){
                __atomic_store_n(&fence->v[2],sequence,__ATOMIC_RELEASE);
                /* Initially free buffers have no preceding GPU use. This is
                 * native ownership readiness, not a fabricated GPU completion. */
                __atomic_store_n(&fence->v[1],sequence?0:1,__ATOMIC_RELEASE);
            }
            pthread_mutex_unlock(&s->lock);return 0;
        }
        pthread_mutex_unlock(&s->lock);nvn_host_sleep_us(200);
    }
}
NVNwindowAcquireTextureResult nvn_WindowAcquireTexture(NVNwindow *w,NVNsync *sync,int *index) { return (NVNwindowAcquireTextureResult)acquire(w,sync,index); }
NVNqueueAcquireTextureResult nvn_QueueAcquireTexture(NVNqueue *q,NVNwindow *w,int *index) {
    if(!nvn_st_get(q,14))return (NVNqueueAcquireTextureResult)1;
    return (NVNqueueAcquireTextureResult)acquire(w,NULL,index);
}
void nvn_QueuePresentTexture(NVNqueue *q,NVNwindow *w,int index) {
    window_state *s=window(w);pthread_mutex_lock(&s->lock);
    if(index<0 || (unsigned)index>=s->count || !(s->acquired&(1u<<index)))NVN_DIE("present requires an acquired window texture");
    nvn_present_request request={s->textures[index],{0,0,0,0},s->interval};
    memcpy(request.crop,s->crop,sizeof request.crop);
    uint64_t sequence=nvn_gpu_present_queue(q,&request);
    if(!sequence)NVN_DIE("NVN_GPU_PRESENT_FAILED index=%d",index);
    s->pending[index]=sequence;s->acquired&=~(1u<<index);
    pthread_mutex_unlock(&s->lock);
}
void nvn_WindowSetCrop(NVNwindow *w,int x,int y,int width,int height) {
    window_state *s=window(w);
    if(x<0 || y<0 || width<0 || height<0 || ((width==0)!=(height==0)) ||
       (width && ((uint64_t)x+(unsigned)width>s->textures[0].words[1] || (uint64_t)y+(unsigned)height>s->textures[0].words[2])))NVN_DIE("invalid window crop");
    pthread_mutex_lock(&s->lock);s->crop[0]=x;s->crop[1]=y;s->crop[2]=width;s->crop[3]=height;pthread_mutex_unlock(&s->lock);
}
void nvn_WindowGetCrop(const NVNwindow *w,NVNrectangle *crop) {
    if(!crop)NVN_DIE("null window crop output");
    window_state *s=window(w);pthread_mutex_lock(&s->lock);
    _Static_assert(sizeof *crop==sizeof s->crop,"NVNrectangle ABI");
    memcpy(crop,s->crop,sizeof *crop);
    pthread_mutex_unlock(&s->lock);
}
void nvn_WindowSetPresentInterval(NVNwindow *w,int interval) {
    if(interval<0 || interval>5)NVN_DIE("invalid present interval");
    window_state *s=window(w);pthread_mutex_lock(&s->lock);s->interval=interval;pthread_mutex_unlock(&s->lock);
}
int nvn_WindowGetPresentInterval(const NVNwindow *w) {
    window_state *s=window(w);pthread_mutex_lock(&s->lock);int interval=s->interval;pthread_mutex_unlock(&s->lock);return interval;
}
NVNnativeWindow nvn_WindowGetNativeWindow(const NVNwindow *w) { return (void *)(uintptr_t)window(w)->native; }
void nvn_WindowSetDebugLabel(NVNwindow *w,const char *label) { (void)window(w);(void)label; }
void nvn_WindowFinalize(NVNwindow *w) {
    window_state *s=window(w);
    for(unsigned i=0;i<s->count;i++)if(s->pending[i]){
        int status;while(!(status=nvn_gpu_poll(s->pending[i])))nvn_host_sleep_us(200);
        if(status<0)NVN_DIE("window finalization GPU failure");
    }
    pthread_mutex_destroy(&s->lock);free(s);nvn_st_del(w,WINDOW);
}

#define ABI(name,upper) _Static_assert(__builtin_types_compatible_p(__typeof__(&nvn_##name),PFNNVN##upper##PROC),"NVN " #name " signature")
ABI(WindowBuilderSetDefaults,WINDOWBUILDERSETDEFAULTS);
ABI(WindowBuilderSetDevice,WINDOWBUILDERSETDEVICE);
ABI(WindowBuilderSetNativeWindow,WINDOWBUILDERSETNATIVEWINDOW);
ABI(WindowBuilderSetTextures,WINDOWBUILDERSETTEXTURES);
ABI(WindowBuilderSetPresentInterval,WINDOWBUILDERSETPRESENTINTERVAL);
ABI(WindowBuilderGetNativeWindow,WINDOWBUILDERGETNATIVEWINDOW);
ABI(WindowBuilderGetPresentInterval,WINDOWBUILDERGETPRESENTINTERVAL);
ABI(WindowInitialize,WINDOWINITIALIZE);
ABI(WindowFinalize,WINDOWFINALIZE);
ABI(WindowAcquireTexture,WINDOWACQUIRETEXTURE);
ABI(QueueAcquireTexture,QUEUEACQUIRETEXTURE);
ABI(QueuePresentTexture,QUEUEPRESENTTEXTURE);
ABI(WindowGetCrop,WINDOWGETCROP);
ABI(WindowSetCrop,WINDOWSETCROP);
ABI(WindowSetPresentInterval,WINDOWSETPRESENTINTERVAL);
ABI(WindowGetPresentInterval,WINDOWGETPRESENTINTERVAL);
ABI(WindowGetNativeWindow,WINDOWGETNATIVEWINDOW);
ABI(WindowSetDebugLabel,WINDOWSETDEBUGLABEL);
#undef ABI
