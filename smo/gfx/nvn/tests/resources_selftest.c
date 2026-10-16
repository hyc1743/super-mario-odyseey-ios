#include "../nvn_resources.h"
#include "../nvn_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define DECL(Name) extern uint64_t nvn_##Name(uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t)
DECL(TexturePoolInitialize); DECL(TexturePoolFinalize); DECL(TexturePoolRegisterTexture);
DECL(TexturePoolRegisterImage); DECL(SamplerPoolInitialize); DECL(SamplerPoolFinalize);
DECL(SamplerPoolRegisterSampler); DECL(SamplerPoolRegisterSamplerBuilder);
DECL(TexturePoolGetSize); DECL(TexturePoolGetMemoryPool); DECL(TexturePoolGetMemoryOffset);
DECL(TextureViewSetDefaults); DECL(TextureViewSetLevels); DECL(TextureViewSetLayers);
DECL(TextureViewSetFormat); DECL(TextureViewSetSwizzle); DECL(TextureViewGetLevels);
DECL(TextureViewGetLayers); DECL(TextureViewGetSwizzle); DECL(TextureViewCompare);
DECL(DeviceGetTextureHandle); DECL(DeviceGetSeparateSamplerHandle);
DECL(DeviceGetSeparateTextureHandle); DECL(DeviceGetImageHandle);
#define U(x) ((uint64_t)(uintptr_t)(x))
#define CALL(Name,a,b,c,d,e) nvn_##Name(U(a),U(b),U(c),U(d),U(e),0,0,0)
/* Standalone test target links only state/resources. */
void nvn_host_log(const char *fmt,...) { (void)fmt; }
int main(void) {
    unsigned char memory[512], memory_object[64], tp[64], tp2[64], sp[64], texture[192], texture2[192], sampler[96], builder[96];
    nvn_st_ent *m=nvn_st_new(memory_object,4);
    m->v[1]=U(memory); m->v[2]=sizeof memory;
    assert(!CALL(TexturePoolInitialize,tp,memory_object,-32,4,0));
    assert(!CALL(TexturePoolInitialize,tp,memory_object,1,4,0));
    assert(!CALL(TexturePoolInitialize,tp,memory_object,480,2,0));
    assert(!CALL(TexturePoolInitialize,tp,memory_object,0,-1,0));
    assert(CALL(TexturePoolInitialize,tp,memory_object,32,8,0));
    assert(CALL(TexturePoolInitialize,tp2,memory_object,0,8,0));
    assert(CALL(SamplerPoolInitialize,sp,memory_object,0,8,0));
    assert(CALL(TexturePoolGetSize,tp,0,0,0,0)==8);
    assert(CALL(TexturePoolGetMemoryPool,tp,0,0,0,0)==U(memory_object));
    assert(CALL(TexturePoolGetMemoryOffset,tp,0,0,0,0)==32);

    nvn_view_state v,copy;
    CALL(TextureViewSetDefaults,&v,0,0,0,0);
    int32_t base=99,count=99;
    assert(!CALL(TextureViewGetLevels,&v,&base,&count,0,0));
    CALL(TextureViewSetLevels,&v,2,3,0,0);
    CALL(TextureViewSetLayers,&v,4,5,0,0);
    CALL(TextureViewSetFormat,&v,37,0,0,0);
    CALL(TextureViewSetSwizzle,&v,5,4,3,2);
    memcpy(&copy,&v,sizeof copy);
    memset(&v,0xee,sizeof v);
    assert(CALL(TextureViewGetLevels,&copy,&base,&count,0,0)&&base==2&&count==3);
    assert(CALL(TextureViewGetLayers,&copy,&base,&count,0,0)&&base==4&&count==5);
    int32_t sw[4]={0};
    assert(CALL(TextureViewGetSwizzle,&copy,sw,sw+1,sw+2,sw+3));
    assert(sw[0]==5&&sw[1]==4&&sw[2]==3&&sw[3]==2);
    memcpy(&v,&copy,sizeof v);
    assert(CALL(TextureViewCompare,&v,&copy,0,0,0));
    CALL(TextureViewSetLevels,&v,0,1,0,0);
    assert(!CALL(TextureViewCompare,&v,&copy,0,0,0));

    nvn_st_ent *t=nvn_st_new(texture,12); t->v[1]=128; t->v[5]=37; t->v[15]=123;
    nvn_st_ent *t2=nvn_st_new(texture2,12); t2->v[1]=64; t2->v[15]=456;
    CALL(TexturePoolRegisterTexture,tp,3,texture,&copy,0);
    CALL(TexturePoolRegisterTexture,tp2,3,texture2,NULL,0);
    nvn_resource_snapshot a,b; nvn_view_state view;
    assert(nvn_resources_texture(tp,3,0,&a,&view));
    assert(a.object==U(texture)&&a.words[1]==128&&a.words[15]==123&&view.base_level==2);
    assert(nvn_resources_texture(tp2,3,0,&b,NULL)&&b.words[1]==64);
    assert(!nvn_resources_texture(tp,2,0,&b,NULL));
    assert(!nvn_resources_texture(tp,8,0,&b,NULL));
    assert(!nvn_resources_texture(tp,3,1,&b,NULL));
    t->v[1]=999; nvn_st_del(texture,12); memset(&copy,0,sizeof copy);
    assert(nvn_resources_texture(tp,3,0,&a,&view)&&a.words[1]==128&&view.base_level==2);
    /* Invalid registration cannot destroy a previously valid descriptor. */
    CALL(TexturePoolRegisterTexture,tp,3,texture,NULL,0);
    assert(nvn_resources_texture(tp,3,0,&a,NULL)&&a.words[1]==128);
    CALL(TexturePoolRegisterImage,tp,4,texture2,NULL,0);
    assert(nvn_resources_texture(tp,4,1,&a,NULL)&&a.words[1]==64);

    nvn_st_ent *s=nvn_st_new(sampler,8); s->v[6]=16;
    CALL(SamplerPoolRegisterSampler,sp,1,sampler,0,0);
    s->v[6]=0; nvn_st_del(sampler,8);
    assert(nvn_resources_sampler(sp,1,&a)&&a.words[6]==16);
    nvn_st_ent *sb=nvn_st_new(builder,7); sb->v[6]=8;
    CALL(SamplerPoolRegisterSamplerBuilder,sp,2,builder,0,0);
    assert(nvn_resources_sampler(sp,2,&a)&&a.words[6]==8);
    assert(CALL(DeviceGetTextureHandle,0,123,7,0,0)==(123u|(7ull<<20)));
    assert(CALL(DeviceGetSeparateTextureHandle,0,123,0,0,0)==123);
    assert(CALL(DeviceGetSeparateSamplerHandle,0,7,0,0,0)==(7ull<<20));
    assert(CALL(DeviceGetImageHandle,0,123,0,0,0)==123);
    /* Reinitialization releases descriptors, repeated finalization is safe. */
    assert(CALL(TexturePoolInitialize,tp,memory_object,0,4,0));
    assert(!nvn_resources_texture(tp,3,0,&a,NULL));
    CALL(TexturePoolFinalize,tp,0,0,0,0); CALL(TexturePoolFinalize,tp,0,0,0,0);
    CALL(TexturePoolFinalize,tp2,0,0,0,0); CALL(SamplerPoolFinalize,sp,0,0,0,0);
    assert(!nvn_resources_sampler(sp,1,&a));
    nvn_st_reset();
    puts("resources_selftest: passed");
    return 0;
}
