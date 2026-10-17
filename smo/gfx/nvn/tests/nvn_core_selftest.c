// NVN 核心家族（Device/MemoryPool/Buffer/Sync/Sampler/Program）状态层单测。
// 不需要游戏文件、不需要 Metal：客体对象在测试里就是几个本地数组（宿主侧状态按指针索引，
// 所以对象内容是什么、多大都无所谓 —— 这正是不依赖 NVN 结构布局的好处）。
#include "../nvn_host.h"
#include "../nvn_state.h"
#include "../include/nvn.h"
#include "../nvn_tex.h"
#include "../nvn_texture_transfer.h"
#include <pthread.h>

enum { STATE_WORKERS = 4, STATE_OBJECTS = 4096 };
static uint8_t state_keys[STATE_WORKERS][STATE_OBJECTS];
static nvn_st_ent *state_entries[STATE_WORKERS][STATE_OBJECTS];
void nvn_sync_signal(void *sync);
static void *signal_later(void *sync) {
    nvn_host_sleep_us(2000);
    nvn_sync_signal(sync);
    return NULL;
}
static void *create_states(void *arg) {
    unsigned worker = (unsigned)(uintptr_t)arg;
    for (unsigned i = 0; i < STATE_OBJECTS; i++) {
        nvn_st_ent *entry = nvn_st_new(&state_keys[worker][i], 42);
        if (!entry) return (void *)1;
        entry->v[0] = worker * STATE_OBJECTS + i;
        entry->v[15] = ~entry->v[0];
        state_entries[worker][i] = entry;
        if (nvn_st_get(&state_keys[worker][i], 42) != entry) return (void *)1;
    }
    return NULL;
}

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

// 声明要用的实现（都是统一签名，除 SetLodBias 的第二参数是 float）
#define DECL(n) uint64_t n(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
DECL(nvn_DeviceBuilderSetDefaults);
DECL(nvn_DeviceBuilderSetFlags);
DECL(nvn_DeviceInitialize);
DECL(nvn_DeviceFinalize);
DECL(nvn_DeviceGetInteger);
DECL(nvn_DeviceGetProcAddress);
DECL(nvn_MemoryPoolBuilderSetDefaults);
DECL(nvn_MemoryPoolBuilderSetDevice);
DECL(nvn_MemoryPoolBuilderSetStorage);
DECL(nvn_MemoryPoolBuilderSetFlags);
DECL(nvn_MemoryPoolInitialize);
DECL(nvn_MemoryPoolFinalize);
DECL(nvn_MemoryPoolMap);
DECL(nvn_MemoryPoolGetFlags);
DECL(nvn_BufferBuilderSetDefaults);
DECL(nvn_BufferBuilderSetStorage);
DECL(nvn_BufferInitialize);
DECL(nvn_BufferGetAddress);
DECL(nvn_BufferMap);
DECL(nvn_SyncInitialize);
DECL(nvn_SyncFinalize);
DECL(nvn_SyncWait);
DECL(nvn_SamplerBuilderSetDefaults);
DECL(nvn_SamplerBuilderSetDevice);
DECL(nvn_SamplerBuilderSetMinMagFilter);
DECL(nvn_SamplerBuilderSetWrapMode);
uint64_t nvn_SamplerBuilderSetMaxAnisotropy(void *, float);
DECL(nvn_SamplerInitialize);
DECL(nvn_SamplerFinalize);
DECL(nvn_ProgramInitialize);
DECL(nvn_ProgramSetShaders);
// float 形参：按 ABI 在 s0，不是 x1
uint64_t nvn_SamplerBuilderSetLodBias(uint64_t, float, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
void nvn_sync_signal(void *sync);

#define OBJ(x) ((uint64_t)(uintptr_t)(x))

#define API(name, type) ((type)(uintptr_t)nvn_lookup("nvn" #name))
static void sampler_abi_test(void) {
    NVNsamplerBuilder builder={0}; NVNsampler sampler={0};
    uint64_t dev[8]={0};
    nvn_SamplerBuilderSetDevice(OBJ(&builder),OBJ(dev),0,0,0,0,0,0);
    nvn_SamplerBuilderSetDefaults(OBJ(&builder),0,0,0,0,0,0,0);
    nvn_st_ent *s=nvn_st_get(&builder,7 /* sampler builder */);
    CHK(s && s->v[0]==OBJ(dev),"sampler defaults preserve device");
    NVNminFilter min; NVNmagFilter mag; NVNwrapMode u,v,w;
    API(SamplerBuilderGetMinMagFilter,PFNNVNSAMPLERBUILDERGETMINMAGFILTERPROC)(&builder,&min,&mag);
    API(SamplerBuilderGetWrapMode,PFNNVNSAMPLERBUILDERGETWRAPMODEPROC)(&builder,&u,&v,&w);
    CHK(min==4 && mag==1 && u==1 && v==1 && w==1,"sampler filter and wrap defaults");
    float lo=0,hi=0;
    API(SamplerBuilderGetLodClamp,PFNNVNSAMPLERBUILDERGETLODCLAMPPROC)(&builder,&lo,&hi);
    CHK(lo==-1000.f && hi==1000.f,"sampler default LOD range");
    API(SamplerBuilderSetLodClamp,PFNNVNSAMPLERBUILDERSETLODCLAMPPROC)(&builder,-2.5f,6.75f);
    API(SamplerBuilderSetMaxAnisotropy,PFNNVNSAMPLERBUILDERSETMAXANISOTROPYPROC)(&builder,8.f);
    API(SamplerBuilderSetMinMagFilter,PFNNVNSAMPLERBUILDERSETMINMAGFILTERPROC)(&builder,3,1);
    API(SamplerBuilderSetWrapMode,PFNNVNSAMPLERBUILDERSETWRAPMODEPROC)(&builder,1,3,7);
    API(SamplerBuilderSetCompare,PFNNVNSAMPLERBUILDERSETCOMPAREPROC)(&builder,1,6);
    float rgba[4]={0.25f,0.5f,0.75f,1.f},out[4]={0};
    API(SamplerBuilderSetBorderColor,PFNNVNSAMPLERBUILDERSETBORDERCOLORPROC)(&builder,rgba);
    memset(rgba,0,sizeof rgba);
    CHK(API(SamplerInitialize,PFNNVNSAMPLERINITIALIZEPROC)(&sampler,&builder),"typed sampler initialization");
    nvn_SamplerBuilderSetDefaults(OBJ(&builder),0,0,0,0,0,0,0);
    API(SamplerGetMinMagFilter,PFNNVNSAMPLERGETMINMAGFILTERPROC)(&sampler,&min,&mag);
    API(SamplerGetWrapMode,PFNNVNSAMPLERGETWRAPMODEPROC)(&sampler,&u,&v,&w);
    API(SamplerGetLodClamp,PFNNVNSAMPLERGETLODCLAMPPROC)(&sampler,&lo,&hi);
    CHK(min==3 && mag==1 && u==1 && v==3 && w==7,"sampler retains every filter and wrap argument");
    CHK(lo==-2.5f && hi==6.75f && API(SamplerGetMaxAnisotropy,PFNNVNSAMPLERGETMAXANISOTROPYPROC)(&sampler)==8.f,"float setter and getter ABI");
    NVNcompareMode mode; NVNcompareFunc func;
    API(SamplerGetCompare,PFNNVNSAMPLERGETCOMPAREPROC)(&sampler,&mode,&func);
    CHK(mode==1 && func==6,"sampler retains compare mode and function");
    API(SamplerGetBorderColor,PFNNVNSAMPLERGETBORDERCOLORPROC)(&sampler,out);
    CHK(out[0]==0.25f && out[1]==0.5f && out[2]==0.75f && out[3]==1.f,"border color captured before caller mutation");
    nvn_SamplerFinalize(OBJ(&sampler),0,0,0,0,0,0,0);
    nvn_st_del(&builder,7);
}

static void texture_layout_test(void) {
    NVNtextureBuilder builder={0};NVNtexture texture={0},alias={0};
    NVNmemoryPool pool={0};NVNmemoryPoolBuilder pb={0};
    uint8_t memory[16384]={0};
    API(MemoryPoolBuilderSetDefaults,PFNNVNMEMORYPOOLBUILDERSETDEFAULTSPROC)(&pb);
    API(MemoryPoolBuilderSetStorage,PFNNVNMEMORYPOOLBUILDERSETSTORAGEPROC)(&pb,memory,sizeof memory);
    CHK(API(MemoryPoolInitialize,PFNNVNMEMORYPOOLINITIALIZEPROC)(&pool,&pb),"texture test pool");
    API(TextureBuilderSetDefaults,PFNNVNTEXTUREBUILDERSETDEFAULTSPROC)(&builder);
    NVNtextureSwizzle r,g,b,a;
    API(TextureBuilderGetSwizzle,PFNNVNTEXTUREBUILDERGETSWIZZLEPROC)(&builder,&r,&g,&b,&a);
    CHK(r==2&&g==3&&b==4&&a==5,"base texture identity swizzle default");
    API(TextureBuilderSetSize2D,PFNNVNTEXTUREBUILDERSETSIZE2DPROC)(&builder,16,65);
    API(TextureBuilderSetFormat,PFNNVNTEXTUREBUILDERSETFORMATPROC)(&builder,0x25);
    API(TextureBuilderSetStorage,PFNNVNTEXTUREBUILDERSETSTORAGEPROC)(&builder,&pool,0);
    size_t automatic=API(TextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC)(&builder);
    NVNpackagedTextureLayout packed={{0}};
    API(TextureBuilderSetPackagedTextureLayout,PFNNVNTEXTUREBUILDERSETPACKAGEDTEXTURELAYOUTPROC)(&builder,&packed);
    packed.layout[0]=5;
    size_t packaged=API(TextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC)(&builder);
    CHK(packaged==4608&&automatic==8192,"explicit zero block height overrides automatic layout and copies source bytes");
    API(TextureBuilderSetSwizzle,PFNNVNTEXTUREBUILDERSETSWIZZLEPROC)(&builder,4,3,2,1);
    API(TextureBuilderSetDepthStencilMode,PFNNVNTEXTUREBUILDERSETDEPTHSTENCILMODEPROC)(&builder,1);
    CHK(API(TextureInitialize,PFNNVNTEXTUREINITIALIZEPROC)(&texture,&builder),"packaged texture initialization");
    memcpy(&alias,&texture,sizeof alias);
    API(TextureBuilderSetDefaults,PFNNVNTEXTUREBUILDERSETDEFAULTSPROC)(&builder);
    API(TextureGetSwizzle,PFNNVNTEXTUREGETSWIZZLEPROC)(&alias,&r,&g,&b,&a);
    CHK(r==4&&g==3&&b==2&&a==1&&API(TextureGetDepthStencilMode,PFNNVNTEXTUREGETDEPTHSTENCILMODEPROC)(&alias)==1,
        "initialized texture and copied alias retain base sampling state");
    API(TextureBuilderSetSize2D,PFNNVNTEXTUREBUILDERSETSIZE2DPROC)(&builder,16,65);
    API(TextureBuilderSetFormat,PFNNVNTEXTUREBUILDERSETFORMATPROC)(&builder,0x25);
    packed.layout[0]=255;
    API(TextureBuilderSetPackagedTextureLayout,PFNNVNTEXTUREBUILDERSETPACKAGEDTEXTURELAYOUTPROC)(&builder,&packed);
    CHK(!API(TextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC)(&builder),"invalid packaged block exponent rejected");
    API(TextureBuilderSetPackagedTextureLayout,PFNNVNTEXTUREBUILDERSETPACKAGEDTEXTURELAYOUTPROC)(&builder,NULL);
    CHK(API(TextureBuilderGetStorageSize,PFNNVNTEXTUREBUILDERGETSTORAGESIZEPROC)(&builder)==automatic,"null packaged layout restores automatic layout");
    API(TextureFinalize,PFNNVNTEXTUREFINALIZEPROC)(&texture);
    API(MemoryPoolFinalize,PFNNVNMEMORYPOOLFINALIZEPROC)(&pool);
    nvn_st_del(&builder,11);nvn_st_del(&pb,3);
}
static unsigned transfer_flushes;
static int observe_flush(const nvn_texture_transfer *t) {
    transfer_flushes++;return t->level==1&&t->region.zoffset==1&&t->region.depth==2;
}
static void texture_transfer_test(void) {
    uint8_t memory[65536]={0},pixels[16*65*4],input[72];
    NVNmemoryPool pool={0};NVNtexture texture={0},alias={0};
    nvn_st_ent *p=nvn_st_new(&pool,4),*s=nvn_st_new(&texture,12);
    p->v[1]=(uintptr_t)memory;p->v[2]=sizeof memory;
    uint64_t words[16]={0,16,65,1,1,0x25,0,1,1,(uintptr_t)&pool,128,0,NVN_TEX_HAS_PACKAGE,0,0,0};
    memcpy(s->v,words,sizeof words);s->v[15]=nvn_st_resource_serial();nvn_resources_stamp(&texture,12);
    memcpy(&alias,&texture,sizeof alias);
    memset(input,0xaa,sizeof input);
    for(unsigned y=0;y<3;y++)for(unsigned x=0;x<16;x++)input[y*24+x]=(uint8_t)(1+y*16+x);
    NVNcopyRegion r={3,7,0,4,3,1};
    API(TextureWriteTexelsStrided,PFNNVNTEXTUREWRITETEXELSSTRIDEDPROC)(&alias,NULL,&r,input,24,0);
    CHK(nvn_tex_detile(pixels,sizeof pixels,0,memory+128,sizeof memory-128,16,65,4,0)==sizeof pixels,
        "written packaged texture can be detiled");
    int ok=1;
    for(unsigned y=0;y<65;y++)for(unsigned x=0;x<64;x++){
        uint8_t expected=y>=7&&y<10&&x>=12&&x<28?(uint8_t)(1+(y-7)*16+x-12):0;
        if(pixels[y*64+x]!=expected)ok=0;
    }
    CHK(ok&&memory[127]==0,"strided subrectangle crosses GOB boundaries without changing surrounding storage");
    memset(memory,0,sizeof memory);
    s->v[1]=s->v[2]=8;s->v[3]=3;s->v[4]=3;s->v[7]=4;
    nvn_view_state v={.flags=NVN_VIEW_LEVELS|NVN_VIEW_LAYERS,.base_level=1,.level_count=1,.base_layer=1,.layer_count=2};
    r=(NVNcopyRegion){1,1,0,2,2,2};
    uint8_t layered[64];for(unsigned i=0;i<sizeof layered;i++)layered[i]=(uint8_t)(i+1);
    API(TextureWriteTexelsStrided,PFNNVNTEXTUREWRITETEXELSSTRIDEDPROC)(&texture,(const NVNtextureView *)&v,&r,layered,12,32);
    nvn_resource_snapshot snapshot;nvn_texture_transfer transfer;
    CHK(nvn_resources_snapshot(&texture,12,&snapshot)&&nvn_texture_transfer_prepare(&snapshot,&v,&r,&transfer),"array mip transfer validated");
    ok=1;
    for(unsigned layer=0;layer<3;layer++){
        uint8_t mip_pixels[64];uint64_t offset=128+layer*transfer.layer_stride+transfer.mip_offset;
        if(nvn_tex_detile(mip_pixels,sizeof mip_pixels,0,memory+offset,sizeof memory-offset,4,4,4,0)!=sizeof mip_pixels)ok=0;
        for(unsigned y=0;y<4;y++)for(unsigned x=0;x<16;x++){
            uint8_t expected=layer>=1&&y>=1&&y<3&&x>=4&&x<12?layered[(layer-1)*32+(y-1)*12+x-4]:0;
            if(mip_pixels[y*16+x]!=expected)ok=0;
        }
    }
    CHK(ok,"view-relative mip/layers and independent row/image strides preserve untouched layers");
    transfer_flushes=0;nvn_texture_set_flush_handler(observe_flush);
    API(TextureFlushTexels,PFNNVNTEXTUREFLUSHTEXELSPROC)(&alias,(const NVNtextureView *)&v,&r);
    nvn_texture_set_flush_handler(NULL);
    CHK(transfer_flushes==1,"flush resolves copied texture identity and exact absolute subresource");
    r.width=99;
    CHK(!nvn_texture_transfer_prepare(&snapshot,&v,&r,&transfer),"out of bounds transfer rejected before copying");
    memset(memory,0,sizeof memory);
    s->v[1]=6;s->v[2]=5;s->v[3]=1;s->v[4]=1;s->v[5]=0x41;s->v[7]=1;
    r=(NVNcopyRegion){4,4,0,2,1,1};uint8_t block[8]={1,2,3,4,5,6,7,8},blocks[32];
    API(TextureWriteTexels,PFNNVNTEXTUREWRITETEXELSPROC)(&texture,NULL,&r,block);
    CHK(nvn_tex_detile(blocks,sizeof blocks,0,memory+128,sizeof memory-128,2,2,8,0)==sizeof blocks&&
        !memcmp(blocks+24,block,8)&&blocks[0]==0&&blocks[23]==0,"compressed edge texels write one complete BC block");
    memset(memory,0,sizeof memory);
    s->v[1]=s->v[2]=9;s->v[3]=5;s->v[4]=1;s->v[5]=0x25;s->v[7]=2;
    r=(NVNcopyRegion){0,0,1,1,9,2};uint8_t volume[72];
    for(unsigned i=0;i<sizeof volume;i++)volume[i]=(uint8_t)(i+1);
    API(TextureWriteTexels,PFNNVNTEXTUREWRITETEXELSPROC)(&texture,NULL,&r,volume);
    CHK(!memcmp(memory+128+512,volume,4)&&!memcmp(memory+128+1024,volume+36,4)&&
        !memcmp(memory+128+2560,volume+32,4)&&!memcmp(memory+128+3072,volume+68,4)&&memory[128]==0,
        "volume texture writes address interleaved depth tiles and GOB rows");
    nvn_st_del(&texture,12);nvn_st_del(&pool,4);
}
static void virtual_pool_test(void) {
    NVNmemoryPool physical={0},mapped={0};uint8_t bytes[4096]={0};
    nvn_st_ent *p=nvn_st_new(&physical,4),*v=nvn_st_new(&mapped,4);
    p->v[1]=(uintptr_t)bytes;p->v[2]=sizeof bytes;p->v[3]=NVN_MEMORY_POOL_FLAGS_PHYSICAL;
    v->v[2]=2048;v->v[3]=NVN_MEMORY_POOL_FLAGS_VIRTUAL;
    NVNmappingRequest requests[2]={{&physical,1024,0,1024,0},{&physical,2048,1024,1024,0}};
    _Static_assert(sizeof(NVNmappingRequest)==40,"mapping request size");
    _Static_assert(offsetof(NVNmappingRequest,storageClass)==32,"mapping request class offset");
    CHK(API(MemoryPoolMapVirtual,PFNNVNMEMORYPOOLMAPVIRTUALPROC)(&mapped,2,requests)&&v->v[1]==(uintptr_t)(bytes+1024),
        "contiguous virtual mapping aliases physical storage with offsets");
    bytes[1031]=79;((uint8_t *)(uintptr_t)v->v[1])[8]=91;
    uint64_t base,length;
    CHK(((uint8_t *)(uintptr_t)v->v[1])[7]==79&&bytes[1032]==91&&
        nvn_st_pool_range(v->v[1],2048,&base,&length),"virtual mapping preserves live physical updates and GPU range lookup");
    requests[1].physicalOffset=3072;
    CHK(!API(MemoryPoolMapVirtual,PFNNVNMEMORYPOOLMAPVIRTUALPROC)(&mapped,2,requests)&&v->v[1]==(uintptr_t)(bytes+1024),
        "unsupported fragmented mapping leaves previous mapping intact");
    NVNmappingRequest unmap={NULL,0,0,2048,0};
    CHK(API(MemoryPoolMapVirtual,PFNNVNMEMORYPOOLMAPVIRTUALPROC)(&mapped,1,&unmap)&&!v->v[1],"full virtual mapping can be unmapped");
    requests[0].size=4097;
    CHK(!API(MemoryPoolMapVirtual,PFNNVNMEMORYPOOLMAPVIRTUALPROC)(&mapped,1,requests)&&!v->v[1],
        "physical and virtual bounds checked before mapping");
    nvn_st_del(&physical,4);nvn_st_del(&mapped,4);
}
int main(void) {
    virtual_pool_test();
    texture_transfer_test();
    texture_layout_test();
    sampler_abi_test();
    uint64_t devb[8] = {0}, dev[8] = {0}, poolb[8] = {0}, pool[8] = {0};
    uint64_t bufb[8] = {0}, buf[8] = {0}, sync[8] = {0}, sampb[8] = {0}, samp[8] = {0}, prog[8] = {0};
    static uint8_t storage[4096];

    printf("== 1. 设备：builder → Initialize ==\n");
    CHK(nvn_DeviceBuilderSetDefaults(OBJ(devb), 0, 0, 0, 0, 0, 0, 0) == 0, "BuilderSetDefaults 成功");
    CHK(nvn_DeviceBuilderSetFlags(OBJ(devb), 0x2, 0, 0, 0, 0, 0, 0) == 0, "BuilderSetFlags 成功");
    CHK(nvn_DeviceInitialize(OBJ(dev), OBJ(devb), 0, 0, 0, 0, 0, 0) == 1, "DeviceInitialize 成功");
    CHK(nvn_st_count() == 2, "状态表里此刻 2 个对象（builder + device）");
    const nvn_st_ent *de = nvn_st_get(dev, 1 /* NVN_T_DEVICE */);
    CHK(de && de->v[0] == 0x2, "设备记住了 builder 里的标志 0x2");
    // Actual game guards: main+0x731314 tests the low byte for true;
    // main+0x731358 requires API major 53 and minor >=13 before Graphics exists.
    struct { int value; uint32_t guard; } prop = {-1, 0x1234abcd};
    nvn_DeviceGetInteger(OBJ(dev), 0, OBJ(&prop.value), 0, 0, 0, 0, 0);
    CHK(prop.value == 53 && prop.guard == 0x1234abcd, "game API major guard passes; property writes exactly 4 bytes");
    nvn_DeviceGetInteger(OBJ(dev), 1, OBJ(&prop.value), 0, 0, 0, 0, 0);
    CHK(prop.value >= 13, "game API minor guard passes");
    nvn_DeviceGetInteger(0, 3, OBJ(&prop.value), 0, 0, 0, 0, 0);
    CHK(prop.value > 0, "global device limits can be queried without a device");
    CHK(nvn_DeviceInitialize(0, OBJ(devb), 0, 0, 0, 0, 0, 0) == 0, "invalid initialization returns NVN_FALSE");

    printf("== 2. 关键路径：GetProcAddress 拿到的必须是【我们】的实现 ==\n");
    uint64_t got = nvn_DeviceGetProcAddress(OBJ(dev), OBJ("nvnMemoryPoolMap"), 0, 0, 0, 0, 0, 0);
    CHK(got == OBJ(nvn_MemoryPoolMap), "查 nvnMemoryPoolMap 拿到的是宿主实现（游戏 461 个函数都走这条路）");
    got = nvn_DeviceGetProcAddress(OBJ(dev), OBJ("nvnBufferGetAddress"), 0, 0, 0, 0, 0, 0);
    CHK(got == OBJ(nvn_BufferGetAddress), "查 nvnBufferGetAddress 正确");
    CHK(nvn_DeviceGetProcAddress(OBJ(dev), OBJ("nvnNoSuchFunction"), 0, 0, 0, 0, 0, 0) == 0,
        "查不到的名字返回 0（NVN 的真实行为）");
    CHK(nvn_DeviceGetProcAddress(OBJ(dev), 0, 0, 0, 0, 0, 0, 0) == 0, "空名字返回 0");

    printf("== 3. 内存池：Map 返回可直接读写的地址 ==\n");
    CHK(nvn_MemoryPoolBuilderSetDefaults(OBJ(poolb), 0, 0, 0, 0, 0, 0, 0) == 0, "池 builder 默认值");
    CHK(nvn_MemoryPoolBuilderSetDevice(OBJ(poolb), OBJ(dev), 0, 0, 0, 0, 0, 0) == 0, "池 builder 设设备");
    CHK(nvn_MemoryPoolBuilderSetStorage(OBJ(poolb), OBJ(storage), sizeof storage, 0, 0, 0, 0, 0) == 0,
        "池 builder 设存储");
    CHK(nvn_MemoryPoolBuilderSetFlags(OBJ(poolb), 1, 0, 0, 0, 0, 0, 0) == 0, "池 builder 设标志 1");
    CHK(nvn_MemoryPoolInitialize(OBJ(pool), OBJ(poolb), 0, 0, 0, 0, 0, 0) == 1, "池 Initialize 成功");
    CHK(nvn_MemoryPoolMap(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == OBJ(storage),
        "Map 返回的地址就是客体给的存储地址（客体原生执行，地址通用，不需要拷贝）");
    CHK(nvn_MemoryPoolGetFlags(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == 1, "GetFlags 拿回 1");
    CHK(nvn_lookup("nvnMemoryPoolGetBufferAddress")(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == OBJ(storage),
        "GPU pool address equals backing storage through actual proc lookup");
    CHK(nvn_MemoryPoolMap(OBJ(storage), 0, 0, 0, 0, 0, 0, 0) == 0, "把非池对象当池用 → 返回 0");
    CHK(nvn_MemoryPoolFinalize(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == 0, "池 Finalize 成功");
    CHK(nvn_MemoryPoolMap(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == 0, "Finalize 之后不能再 Map");

    printf("== 4. 缓冲：地址来自池 ==\n");
    nvn_MemoryPoolInitialize(OBJ(pool), OBJ(poolb), 0, 0, 0, 0, 0, 0);
    CHK(nvn_BufferBuilderSetDefaults(OBJ(bufb), 0, 0, 0, 0, 0, 0, 0) == 0, "缓冲 builder 默认值");
    CHK(nvn_BufferBuilderSetStorage(OBJ(bufb), OBJ(pool), 64, 256, 0, 0, 0, 0) == 0,
        "缓冲 builder 设存储（+64 偏移，256 字节）");
    CHK(nvn_BufferInitialize(OBJ(buf), OBJ(bufb), 0, 0, 0, 0, 0, 0) == 1, "缓冲 Initialize 成功");
    CHK(nvn_BufferGetAddress(OBJ(buf), 0, 0, 0, 0, 0, 0, 0) == OBJ(storage) + 64, "GetAddress 返回 +64");
    uint64_t mapped = nvn_lookup("nvnBufferMap")(OBJ(buf), 0, 0, 0, 0, 0, 0, 0);
    CHK(mapped == OBJ(storage) + 64, "BufferMap dispatch returns pool storage plus byte offset");
    if (mapped) {
        memset((void *)(uintptr_t)mapped, 0xA7, 256);
        CHK(storage[63] == 0 && storage[64] == 0xA7 && storage[319] == 0xA7 && storage[320] == 0,
            "CPU writes through mapped buffer reach only its pool range");
    }
    nvn_BufferBuilderSetStorage(OBJ(bufb), OBJ(pool), 4090, 16, 0, 0, 0, 0);
    CHK(nvn_BufferInitialize(OBJ(bufb), OBJ(bufb), 0, 0, 0, 0, 0, 0) == 0, "reject buffer extending beyond pool");

    printf("== 5. Sync timeout / signal / persistent fence ==\n");
    CHK(nvn_SyncInitialize(OBJ(sync), OBJ(dev), 0, 0, 0, 0, 0, 0) == 1, "SyncInitialize 成功");
    CHK(nvn_SyncWait(OBJ(sync), 0, 0, 0, 0, 0, 0, 0) == 2, "zero timeout polls an unsignaled fence");
    CHK(nvn_SyncWait(OBJ(sync), 1000000ull, 0, 0, 0, 0, 0, 0) == 2, "1 ms timeout returns TIMEOUT_EXPIRED");
    nvn_sync_signal(sync);
    CHK(nvn_SyncWait(OBJ(sync), 0, 0, 0, 0, 0, 0, 0) == 0, "信号之后等待立即成功");
    CHK(nvn_SyncWait(OBJ(sync), 1000000ull, 0, 0, 0, 0, 0, 0) == 0, "completed fence remains signaled");
    nvn_SyncInitialize(OBJ(sync), OBJ(dev), 0, 0, 0, 0, 0, 0);
    pthread_t signaler;
    int created = pthread_create(&signaler, NULL, signal_later, sync) == 0;
    CHK(created, "launch fence completion thread");
    if (created) {
        uint64_t result = nvn_SyncWait(OBJ(sync), 200000000ull, 0, 0, 0, 0, 0, 0);
        // Scheduling may complete the signal before the wait call starts.
        CHK(result == 0 || result == 1, "concurrent completion returns a success enumerator");
        pthread_join(signaler, NULL);
    }
    CHK(nvn_SyncFinalize(OBJ(sync), 0, 0, 0, 0, 0, 0, 0) == 0, "SyncFinalize 成功");

    printf("== 6. 采样器：float 参数走 s0（统一 u64 签名对它不成立）==\n");
    CHK(nvn_SamplerBuilderSetDefaults(OBJ(sampb), 0, 0, 0, 0, 0, 0, 0) == 0, "采样器 builder 默认值");
    CHK(nvn_SamplerBuilderSetDevice(OBJ(sampb), OBJ(dev), 0, 0, 0, 0, 0, 0) == 0, "采样器 builder 设设备");
    CHK(nvn_SamplerBuilderSetMinMagFilter(OBJ(sampb), 3, 0, 0, 0, 0, 0, 0) == 0, "设过滤 3");
    CHK(nvn_SamplerBuilderSetWrapMode(OBJ(sampb), 4, 0, 0, 0, 0, 0, 0) == 0, "设环绕 4");
    CHK(nvn_SamplerBuilderSetMaxAnisotropy(sampb, 8.f) == 0, "设各向异性 8");
    CHK(nvn_SamplerBuilderSetLodBias(OBJ(sampb), 1.5f, 0, 0, 0, 0, 0, 0) == 0, "设 LOD 偏移 1.5f");
    CHK(nvn_SamplerInitialize(OBJ(samp), OBJ(sampb), 0, 0, 0, 0, 0, 0) == 1, "采样器 Initialize 成功");
    const nvn_st_ent *se = nvn_st_get(samp, 8 /* NVN_T_SAMPLER */);
    CHK(se != NULL, "采样器对象在状态表里");
    if (se) {
        CHK(se->v[1] == 3 && se->v[2] == 4 && se->v[6] == 0x41000000u, "过滤/环绕/各向异性都存对了");
        CHK(se->v[5] == 0x3FC00000u, "1.5f 的位模式 0x3FC00000 存对了（说明 float 确实从 s0 读到了）");
    }
    CHK(nvn_SamplerFinalize(OBJ(samp), 0, 0, 0, 0, 0, 0, 0) == 0, "采样器 Finalize 成功");

    printf("== 7. 类型检查：同址异类不会被认错 ==\n");
    CHK(nvn_st_get(dev, 4 /* NVN_T_POOL */) == NULL, "设备对象按池类型取 → NULL");
    CHK(nvn_st_get(devb, 1 /* NVN_T_DEVICE */) == NULL, "builder 按设备类型取 → NULL");

    printf("== 8. 程序：控制段被记下来（Metal 重放层要按它查已译着色器）==\n");
    static uint8_t ctrl[64];
    for (unsigned i = 0; i < sizeof ctrl; i++) ctrl[i] = (uint8_t)(i * 7);
    CHK(nvn_ProgramInitialize(OBJ(prog), OBJ(dev), 0, 0, 0, 0, 0, 0) == 1, "ProgramInitialize 成功");
    nvn_shader_data stages[2] = {{0x12345000, ctrl}, {0x12346000, ctrl + 16}};
    CHK(nvn_lookup("nvnProgramSetShaders")(OBJ(prog), 2, OBJ(stages), 0, 0, 0, 0, 0) == 1, "SetShaders accepts two NVNshaderData descriptors");
    const nvn_st_ent *pe = nvn_st_get(prog, 10 /* NVN_T_PROG */);
    stages[0].data = 0;
    CHK(pe && pe->v[NVN_PROG_NSTAGES] == 2 && pe->v[2] == 0x12345000 && pe->v[3] == OBJ(ctrl)
        && pe->v[4] == 0x12346000 && pe->v[5] == OBJ(ctrl + 16), "stage descriptors copied before caller array changes");
    CHK(!nvn_ProgramSetShaders(OBJ(prog), 7, OBJ(stages), 0, 0, 0, 0, 0)
        && !nvn_ProgramSetShaders(OBJ(prog), UINT32_MAX, OBJ(stages), 0, 0, 0, 0, 0)
        && !nvn_ProgramSetShaders(OBJ(prog), 1, 0, 0, 0, 0, 0, 0), "reject invalid stage count or null descriptor array");
    CHK(pe && pe->v[NVN_PROG_NSTAGES] == 2, "failed SetShaders preserves previous program");

    printf("== 9. 名字表整体 ==\n");
    unsigned miss = 0;
    for (unsigned i = 0; i < g_nvn_table_n; i++)
        if (!nvn_DeviceGetProcAddress(OBJ(dev), OBJ(g_nvn_table[i].name), 0, 0, 0, 0, 0, 0)) miss++;
    CHK(miss == 0, "表里每个名字都能取到地址（461 个名字由 nvnLoadCProcs 依次索取）");
    printf("     表内 %u：手写实现 38，接记录桩 %u，接未实现入口 %u\n",
           g_nvn_table_n, g_nvn_impl_n - 38, g_nvn_table_n - g_nvn_impl_n);

    printf("== 10. Texture storage through the game's proc dispatch ==\n");
    uint64_t tb[16] = {0}, texture[16] = {0};
    nvn_lookup("nvnTextureBuilderSetDevice")(OBJ(tb), OBJ(dev), 0, 0, 0, 0, 0, 0);
    nvn_lookup("nvnTextureBuilderSetDefaults")(OBJ(tb), 0, 0, 0, 0, 0, 0, 0);
    nvn_lookup("nvnTextureBuilderSetSize2D")(OBJ(tb), 4, 4, 0, 0, 0, 0, 0);
    nvn_lookup("nvnTextureBuilderSetFormat")(OBJ(tb), 0x25, 0, 0, 0, 0, 0, 0);
    CHK(nvn_lookup("nvnTextureBuilderGetStorageSize")(OBJ(tb), 0, 0, 0, 0, 0, 0, 0) == 512,
        "4x4 RGBA8 occupies one 512-byte GOB, never zero (PrimitiveTexture crash)");
    CHK(nvn_lookup("nvnTextureBuilderGetStorageAlignment")(OBJ(tb), 0, 0, 0, 0, 0, 0, 0) == 512,
        "block texture storage alignment is 512");
    nvn_lookup("nvnTextureBuilderSetLevels")(OBJ(tb), 3, 0, 0, 0, 0, 0, 0);
    CHK(nvn_lookup("nvnTextureBuilderGetStorageSize")(OBJ(tb), 0, 0, 0, 0, 0, 0, 0) == 1536,
        "three tiny mip levels occupy three GOBs");
    nvn_lookup("nvnTextureBuilderSetLevels")(OBJ(tb), 1, 0, 0, 0, 0, 0, 0);
    nvn_lookup("nvnTextureBuilderSetFlags")(OBJ(tb), 0x10, 0, 0, 0, 0, 0, 0);
    CHK(nvn_lookup("nvnTextureBuilderGetStorageSize")(OBJ(tb), 0, 0, 0, 0, 0, 0, 0) == 128,
        "linear texture uses aligned 32-byte rows");
    nvn_lookup("nvnTextureBuilderSetFlags")(OBJ(tb), 0, 0, 0, 0, 0, 0, 0);
    nvn_lookup("nvnTextureBuilderSetStorage")(OBJ(tb), OBJ(pool), 512, 0, 0, 0, 0, 0);
    CHK(nvn_lookup("nvnTextureInitialize")(OBJ(texture), OBJ(tb), 0, 0, 0, 0, 0, 0) == 1,
        "texture initialization succeeds with valid pool storage");
    const nvn_st_ent *te = nvn_st_get(texture, 12);
    CHK(te && te->v[0] == OBJ(dev) && te->v[10] == 512, "defaults preserves device and texture keeps pool offset");
    nvn_lookup("nvnTextureBuilderSetStorage")(OBJ(tb), OBJ(pool), 4000, 0, 0, 0, 0, 0);
    CHK(nvn_lookup("nvnTextureInitialize")(OBJ(texture), OBJ(tb), 0, 0, 0, 0, 0, 0) == 0,
        "texture cannot exceed pool bounds");
    nvn_st_reset();
    CHK(nvn_st_count() == 0, "清空状态表");
    // Native particle-resource loading exceeded the former 1024-entry limit.
    // Existing pointers must survive growth and unrelated object destruction.
    uint8_t anchor_key;
    nvn_st_ent *anchor = nvn_st_new(&anchor_key, 41);
    anchor->v[15] = UINT64_MAX;
    pthread_t workers[STATE_WORKERS];
    unsigned launched = 0;
    for (; launched < STATE_WORKERS; launched++)
        if (pthread_create(&workers[launched], NULL, create_states, (void *)(uintptr_t)launched)) break;
    CHK(launched == STATE_WORKERS, "launch concurrent state writers");
    int writers_ok = 1;
    for (unsigned w = 0; w < launched; w++) {
        void *result = NULL;
        if (pthread_join(workers[w], &result) || result) writers_ok = 0;
    }
    CHK(writers_ok && nvn_st_count() == 1 + STATE_WORKERS * STATE_OBJECTS,
        "16384 concurrent creations exceed old limit without lost entries");
    int entries_ok = 1;
    for (unsigned w = 0; w < launched; w++) for (unsigned i = 0; i < STATE_OBJECTS; i++) {
        const nvn_st_ent *entry = nvn_st_get(&state_keys[w][i], 42);
        uint64_t expected = w * STATE_OBJECTS + i;
        if (!entry || entry != state_entries[w][i] || entry->v[0] != expected || entry->v[15] != ~expected)
            entries_ok = 0;
    }
    CHK(entries_ok && nvn_st_get(&anchor_key, 41) == anchor && anchor->v[15] == UINT64_MAX,
        "rehashing preserves all state bytes and retained entry pointers");
    for (unsigned w = 0; w < launched; w++) for (unsigned i = 0; i < STATE_OBJECTS; i += 2)
        nvn_st_del(&state_keys[w][i], 42);
    CHK(nvn_st_count() == 1 + STATE_WORKERS * STATE_OBJECTS / 2
        && !nvn_st_get(&state_keys[0][0], 42)
        && nvn_st_get(&state_keys[0][1], 42) == state_entries[0][1]
        && nvn_st_get(&anchor_key, 41) == anchor, "deletion preserves other entries and pointers");
    CHK(nvn_st_new(&anchor_key, 43) == anchor && !anchor->v[15]
        && !nvn_st_get(&anchor_key, 41) && nvn_st_reused() == 1,
        "type reuse clears all 16 state words");
    CHK(nvn_st_iter(nvn_st_count(), NULL) == NULL, "enumeration stops at object count");
    nvn_st_reset();
    CHK(nvn_st_count() == 0 && !nvn_st_iter(0, NULL), "reset releases expanded state table");
    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
