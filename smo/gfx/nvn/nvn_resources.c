/* NVN descriptor pools and copied texture views. Handle bit layout and view
 * semantics adapted from d2r-mobile, Copyright (c) 2026 jhaizhou, MIT.
 * See third_party/d2r-mobile.LICENSE. Pool storage is scoped to each object. */
#include "nvn_resources.h"
#include "nvn_state.h"
#include "nvn_host.h"
#include <pthread.h>
#include <limits.h>
#include <stdatomic.h>

enum { T_MEMORY=4, T_SAMPLER_BUILDER=7, T_SAMPLER=8, T_TEXTURE=12,
       T_TEXTURE_POOL=16, T_SAMPLER_POOL=17 };
enum { P_MEMORY, P_OFFSET, P_COUNT, P_SLOTS };
typedef struct {
    nvn_resource_snapshot resource;
    nvn_view_state view;
    unsigned image;
} descriptor;
static pthread_mutex_t resource_lock = PTHREAD_MUTEX_INITIALIZER;

/* These APIs have integer/pointer arguments only. The dispatch table stores
 * the same eight-register function signature used by the existing core. */
#define ARGS uint64_t a0,uint64_t a1,uint64_t a2,uint64_t a3,uint64_t a4,uint64_t a5,uint64_t a6,uint64_t a7
#define UNUSED do { (void)a0;(void)a1;(void)a2;(void)a3;(void)a4;(void)a5;(void)a6;(void)a7; } while(0)
#define PTR(x) ((void *)(uintptr_t)(x))
#define VIEW(x) ((void *)(uintptr_t)(x))

typedef struct { uint64_t magic,owner,serial; } resource_identity;
#define RESOURCE_MAGIC UINT64_C(0x534d4f5245530000)
/* NVNtexture is 0xC0 bytes: identity (24) + descriptor words (128) fit. Real NVN
 * objects are self-describing data, so a guest byte copy keeps describing the
 * texture it was copied from even after the original object is re-initialized.
 * 真机 v0.6.15：Start 后 ×86743 NVN_RESOURCE_MISS，诊断全是 reason=stale（原对象已重建）。 */
enum { EMBED_OFFSET=sizeof(resource_identity), EMBED_BYTES=16*sizeof(uint64_t) };
_Static_assert(EMBED_OFFSET+EMBED_BYTES<=0xC0,"texture descriptor embed fits NVNtexture");
void nvn_resources_stamp(void *object,uint32_t type) {
    nvn_st_ent *e=nvn_st_get(object,type);
    if(!e)return;
    resource_identity id={RESOURCE_MAGIC|type,(uintptr_t)object,e->v[15]};memcpy(object,&id,sizeof id);
    if(type==T_TEXTURE)memcpy((uint8_t *)object+EMBED_OFFSET,e->v,EMBED_BYTES);
}
int nvn_resources_snapshot(const void *object, uint32_t type, nvn_resource_snapshot *out) {
    if(!object || !out)return 0;
    nvn_st_ent *e = nvn_st_get(object, type);
    if(!e && (type==8 || type==10 || type==12)){
        resource_identity id;memcpy(&id,object,sizeof id);
        static _Atomic unsigned alias_misses;
        if(id.magic!=(RESOURCE_MAGIC|type) || !id.serial){
            /* 真机 v0.6.14：Start 后 ×8408 NVN_RESOURCE_MISS 走的都是这条（SNAPSHOT_MISS 一次没出现），
             * 多数是栈上的纹理副本。记下头 24 字节，判断是「没盖戳的副本」还是「戳被游戏覆写」。 */
            if(atomic_fetch_add(&alias_misses,1)<16)
                NVN_LOG("NVN_RESOURCE_ALIAS_MISS reason=stamp type=%u object=%p words=%#llx,%#llx,%#llx",type,object,
                    (unsigned long long)id.magic,(unsigned long long)id.owner,(unsigned long long)id.serial);
            return 0;
        }
        e=nvn_st_get((void *)(uintptr_t)id.owner,type);
        if((!e || e->v[15]!=id.serial) && type==T_TEXTURE){
            uint64_t words[16];memcpy(words,(const uint8_t *)object+EMBED_OFFSET,sizeof words);
            if(words[15]==id.serial){
                static _Atomic unsigned embedded;
                if(atomic_fetch_add(&embedded,1)<8)
                    NVN_LOG("NVN_RESOURCE_EMBEDDED object=%p owner=%#llx serial=%llu live=%d",object,
                        (unsigned long long)id.owner,(unsigned long long)id.serial,e!=NULL);
                out->object=id.owner;memcpy(out->words,words,sizeof out->words);
                return 1;
            }
        }
        if(!e || e->v[15]!=id.serial){
            if(atomic_fetch_add(&alias_misses,1)<16)
                NVN_LOG("NVN_RESOURCE_ALIAS_MISS reason=%s type=%u object=%p owner=%#llx serial=%llu live_serial=%llu",
                    e?"stale":"finalized",type,object,(unsigned long long)id.owner,(unsigned long long)id.serial,
                    (unsigned long long)(e?e->v[15]:0));
            return 0;
        }
        static _Atomic unsigned aliases;
        if(atomic_fetch_add(&aliases,1)<8)NVN_LOG("NVN_RESOURCE_ALIAS object=%p owner=%p type=%u serial=%llu",object,e->key,type,(unsigned long long)id.serial);
    }
    if (!e) {
        static _Atomic unsigned misses;
        unsigned n=atomic_fetch_add(&misses,1);
        if(n<32)NVN_LOG("NVN_RESOURCE_SNAPSHOT_MISS type=%u object=%p",type,object);
        return 0;
    }
    out->object=(uint64_t)(uintptr_t)e->key;
    memcpy(out->words,e->v,sizeof out->words);
    return 1;
}
void nvn_resources_view(const void *object, nvn_view_state *out) {
    if (object) memcpy(out,object,sizeof *out);
    else memset(out,0,sizeof *out);
}
static void release_pool(void *object, unsigned type) {
    nvn_st_ent *e=nvn_st_get(object,type);
    if (!e) return;
    descriptor **slots=PTR(e->v[P_SLOTS]);
    for (uint64_t i=0;i<e->v[P_COUNT];i++) free(slots[i]);
    free(slots);
    nvn_st_del(object,type);
}
static uint64_t initialize_pool(void *object,void *memory,uint64_t offset,uint64_t count,unsigned type) {
    nvn_st_ent *m=nvn_st_get(memory,T_MEMORY);
    /* Both advertised descriptor sizes are 32 (DeviceGetInteger 0xf/0x10).
     * Check overflow before multiplying or adding addresses. */
    if (!object || !m || !m->v[1] || offset>INT64_MAX || offset%32 ||
        !count || count>(type==T_TEXTURE_POOL?1048576u:4096u) || offset>m->v[2] ||
        count>(m->v[2]-offset)/32 || m->v[1]>UINT64_MAX-m->v[2] ||
        count>SIZE_MAX/sizeof(descriptor *)) {
        NVN_LOG("DescriptorPoolInitialize invalid object=%p memory=%p offset=%llu count=%llu",object,memory,
                (unsigned long long)offset,(unsigned long long)count);
        return 0;
    }
    descriptor **slots=calloc((size_t)count,sizeof *slots);
    if (!slots) return 0;
    pthread_mutex_lock(&resource_lock);
    release_pool(object,type);
    nvn_st_ent *e=nvn_st_new(object,type);
    if (e) { e->v[P_MEMORY]=(uintptr_t)memory; e->v[P_OFFSET]=offset;
        e->v[P_COUNT]=count; e->v[P_SLOTS]=(uintptr_t)slots; }
    pthread_mutex_unlock(&resource_lock);
    if (!e) free(slots);
    return e!=NULL;
}
static void register_resource(void *pool,uint64_t id,void *object,const void *view,
                              unsigned pool_type,unsigned object_type,unsigned image) {
    descriptor next;
    memset(&next,0,sizeof next);
    if (!nvn_resources_snapshot(object,object_type,&next.resource)) {
        NVN_LOG("Descriptor registration invalid resource=%p type=%u",object,object_type); return;
    }
    nvn_resources_view(view,&next.view); next.image=image;
    pthread_mutex_lock(&resource_lock);
    nvn_st_ent *p=nvn_st_get(pool,pool_type);
    if (!p || id>=p->v[P_COUNT]) {
        pthread_mutex_unlock(&resource_lock);
        NVN_LOG("Descriptor registration invalid pool=%p id=%llu",pool,(unsigned long long)id); return;
    }
    descriptor **slots=PTR(p->v[P_SLOTS]);
    if (!slots[id]) slots[id]=malloc(sizeof next);
    if (slots[id]) *slots[id]=next;
    else NVN_LOG("Descriptor registration allocation failed");
    pthread_mutex_unlock(&resource_lock);
}
static int lookup(const void *pool,uint32_t id,unsigned type,int image,
                  nvn_resource_snapshot *out,nvn_view_state *view) {
    int found=0;
    pthread_mutex_lock(&resource_lock);
    nvn_st_ent *p=nvn_st_get(pool,type);
    if (p && id<p->v[P_COUNT]) {
        descriptor **slots=PTR(p->v[P_SLOTS]); descriptor *d=slots[id];
        if (d && (image<0 || (int)d->image==image)) {
            if (out) *out=d->resource;
            if (view) *view=d->view;
            found=1;
        }
    }
    pthread_mutex_unlock(&resource_lock);
    return found;
}
int nvn_resources_texture(const void *pool,uint32_t id,int image,nvn_resource_snapshot *out,nvn_view_state *view) {
    return lookup(pool,id,T_TEXTURE_POOL,!!image,out,view);
}
int nvn_resources_sampler(const void *pool,uint32_t id,nvn_resource_snapshot *out) {
    return lookup(pool,id,T_SAMPLER_POOL,-1,out,NULL);
}

#define POOL_API(Name,Type) \
uint64_t nvn_##Name##Initialize(ARGS) { UNUSED; return initialize_pool(PTR(a0),PTR(a1),a2,(uint32_t)a3,Type); } \
uint64_t nvn_##Name##Finalize(ARGS) { UNUSED; pthread_mutex_lock(&resource_lock); release_pool(PTR(a0),Type); pthread_mutex_unlock(&resource_lock); return 0; } \
uint64_t nvn_##Name##GetMemoryPool(ARGS) { UNUSED; nvn_st_ent *p=nvn_st_get(PTR(a0),Type); return p?p->v[P_MEMORY]:0; } \
uint64_t nvn_##Name##GetMemoryOffset(ARGS) { UNUSED; nvn_st_ent *p=nvn_st_get(PTR(a0),Type); return p?p->v[P_OFFSET]:0; } \
uint64_t nvn_##Name##GetSize(ARGS) { UNUSED; nvn_st_ent *p=nvn_st_get(PTR(a0),Type); return p?p->v[P_COUNT]:0; } \
uint64_t nvn_##Name##SetDebugLabel(ARGS) { UNUSED; return 0; }
POOL_API(TexturePool,T_TEXTURE_POOL)
POOL_API(SamplerPool,T_SAMPLER_POOL)
uint64_t nvn_TexturePoolRegisterTexture(ARGS) { UNUSED; register_resource(PTR(a0),(uint32_t)a1,PTR(a2),PTR(a3),T_TEXTURE_POOL,T_TEXTURE,0); return 0; }
uint64_t nvn_TexturePoolRegisterImage(ARGS) { UNUSED; register_resource(PTR(a0),(uint32_t)a1,PTR(a2),PTR(a3),T_TEXTURE_POOL,T_TEXTURE,1); return 0; }
uint64_t nvn_SamplerPoolRegisterSampler(ARGS) { UNUSED; register_resource(PTR(a0),(uint32_t)a1,PTR(a2),NULL,T_SAMPLER_POOL,T_SAMPLER,0); return 0; }
uint64_t nvn_SamplerPoolRegisterSamplerBuilder(ARGS) { UNUSED; register_resource(PTR(a0),(uint32_t)a1,PTR(a2),NULL,T_SAMPLER_POOL,T_SAMPLER_BUILDER,0); return 0; }

/* Maxwell texture IDs occupy low20 bits; sampler IDs start at bit20. */
uint64_t nvn_DeviceGetTextureHandle(ARGS) { UNUSED; return ((uint64_t)(uint32_t)a2<<20)|((uint32_t)a1&0xfffffu); }
uint64_t nvn_DeviceGetTexelFetchHandle(ARGS) { UNUSED; return (uint32_t)a1&0xfffffu; }
uint64_t nvn_DeviceGetImageHandle(ARGS) { UNUSED; return (uint32_t)a1&0xfffffu; }
uint64_t nvn_DeviceGetSeparateTextureHandle(ARGS) { UNUSED; return (uint32_t)a1&0xfffffu; }
uint64_t nvn_DeviceGetSeparateSamplerHandle(ARGS) { UNUSED; return (uint64_t)(uint32_t)a1<<20; }

uint64_t nvn_TextureViewSetDefaults(ARGS) { UNUSED; if (a0) memset(VIEW(a0),0,sizeof(nvn_view_state)); return 0; }
#define SET_PAIR(Name,Flag,F1,F2) \
uint64_t nvn_TextureViewSet##Name(ARGS) { UNUSED; nvn_view_state v; if(!a0)return 0; nvn_resources_view(PTR(a0),&v); v.flags|=Flag; v.F1=(int32_t)a1; v.F2=(int32_t)a2; memcpy(PTR(a0),&v,sizeof v); return 0; } \
uint64_t nvn_TextureViewGet##Name(ARGS) { UNUSED; nvn_view_state v; if(!a0)return 0; nvn_resources_view(PTR(a0),&v); if(a1)*(int32_t*)PTR(a1)=v.F1; if(a2)*(int32_t*)PTR(a2)=v.F2; return !!(v.flags&Flag); }
SET_PAIR(Levels,NVN_VIEW_LEVELS,base_level,level_count)
SET_PAIR(Layers,NVN_VIEW_LAYERS,base_layer,layer_count)
#define SET_ONE(Name,Flag,Field) \
uint64_t nvn_TextureViewSet##Name(ARGS) { UNUSED; nvn_view_state v; if(!a0)return 0; nvn_resources_view(PTR(a0),&v); v.flags|=Flag; v.Field=(int32_t)a1; memcpy(PTR(a0),&v,sizeof v); return 0; } \
uint64_t nvn_TextureViewGet##Name(ARGS) { UNUSED; nvn_view_state v; if(!a0)return 0; nvn_resources_view(PTR(a0),&v); if(a1)*(int32_t*)PTR(a1)=v.Field; return !!(v.flags&Flag); }
SET_ONE(Format,NVN_VIEW_FORMAT,format)
SET_ONE(Target,NVN_VIEW_TARGET,target)
SET_ONE(DepthStencilMode,NVN_VIEW_DEPTH_STENCIL,depth_stencil)
uint64_t nvn_TextureViewSetSwizzle(ARGS) {
    UNUSED; if(!a0)return 0; nvn_view_state v; nvn_resources_view(PTR(a0),&v);
    v.flags|=NVN_VIEW_SWIZZLE; v.swizzle[0]=(uint8_t)a1; v.swizzle[1]=(uint8_t)a2;
    v.swizzle[2]=(uint8_t)a3; v.swizzle[3]=(uint8_t)a4; memcpy(PTR(a0),&v,sizeof v); return 0;
}
uint64_t nvn_TextureViewGetSwizzle(ARGS) {
    UNUSED; if(!a0)return 0; nvn_view_state v; nvn_resources_view(PTR(a0),&v);
    if(a1)*(int32_t*)PTR(a1)=v.swizzle[0];
    if(a2)*(int32_t*)PTR(a2)=v.swizzle[1];
    if(a3)*(int32_t*)PTR(a3)=v.swizzle[2];
    if(a4)*(int32_t*)PTR(a4)=v.swizzle[3];
    return !!(v.flags&NVN_VIEW_SWIZZLE);
}
uint64_t nvn_TextureViewCompare(ARGS) {
    UNUSED; if(!a0||!a1)return 0; nvn_view_state x,y; nvn_resources_view(PTR(a0),&x); nvn_resources_view(PTR(a1),&y);
    if(x.flags!=y.flags)return 0;
    if((x.flags&NVN_VIEW_LEVELS)&&(x.base_level!=y.base_level||x.level_count!=y.level_count))return 0;
    if((x.flags&NVN_VIEW_LAYERS)&&(x.base_layer!=y.base_layer||x.layer_count!=y.layer_count))return 0;
    if((x.flags&NVN_VIEW_FORMAT)&&x.format!=y.format)return 0;
    if((x.flags&NVN_VIEW_TARGET)&&x.target!=y.target)return 0;
    if((x.flags&NVN_VIEW_DEPTH_STENCIL)&&x.depth_stencil!=y.depth_stencil)return 0;
    if((x.flags&NVN_VIEW_SWIZZLE)&&memcmp(x.swizzle,y.swizzle,4))return 0;
    return 1;
}
