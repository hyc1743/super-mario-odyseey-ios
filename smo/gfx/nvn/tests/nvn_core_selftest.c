// NVN 核心家族（Device/MemoryPool/Buffer/Sync/Sampler/Program）状态层单测。
// 不需要游戏文件、不需要 Metal：客体对象在测试里就是几个本地数组（宿主侧状态按指针索引，
// 所以对象内容是什么、多大都无所谓 —— 这正是不依赖 NVN 结构布局的好处）。
#include "../nvn_host.h"
#include "../nvn_state.h"

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
DECL(nvn_SyncInitialize);
DECL(nvn_SyncFinalize);
DECL(nvn_SyncWait);
DECL(nvn_SamplerBuilderSetDefaults);
DECL(nvn_SamplerBuilderSetDevice);
DECL(nvn_SamplerBuilderSetMinMagFilter);
DECL(nvn_SamplerBuilderSetWrapMode);
DECL(nvn_SamplerBuilderSetMaxAnisotropy);
DECL(nvn_SamplerInitialize);
DECL(nvn_SamplerFinalize);
DECL(nvn_ProgramInitialize);
DECL(nvn_ProgramSetShaders);
// float 形参：按 ABI 在 s0，不是 x1
uint64_t nvn_SamplerBuilderSetLodBias(uint64_t, float, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
void nvn_sync_signal(void *sync);

#define OBJ(x) ((uint64_t)(uintptr_t)(x))

int main(void) {
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
    CHK(nvn_MemoryPoolMap(OBJ(storage), 0, 0, 0, 0, 0, 0, 0) == 0, "把非池对象当池用 → 返回 0");
    CHK(nvn_MemoryPoolFinalize(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == 0, "池 Finalize 成功");
    CHK(nvn_MemoryPoolMap(OBJ(pool), 0, 0, 0, 0, 0, 0, 0) == 0, "Finalize 之后不能再 Map");

    printf("== 4. 缓冲：地址来自池 ==\n");
    CHK(nvn_BufferBuilderSetDefaults(OBJ(bufb), 0, 0, 0, 0, 0, 0, 0) == 0, "缓冲 builder 默认值");
    CHK(nvn_BufferBuilderSetStorage(OBJ(bufb), OBJ(pool), OBJ(storage) + 64, 256, 0, 0, 0, 0) == 0,
        "缓冲 builder 设存储（+64 偏移，256 字节）");
    CHK(nvn_BufferInitialize(OBJ(buf), OBJ(bufb), 0, 0, 0, 0, 0, 0) == 1, "缓冲 Initialize 成功");
    CHK(nvn_BufferGetAddress(OBJ(buf), 0, 0, 0, 0, 0, 0, 0) == OBJ(storage) + 64, "GetAddress 返回 +64");

    printf("== 5. 同步对象：超时 / 信号 / 等到后自动复位 ==\n");
    CHK(nvn_SyncInitialize(OBJ(sync), OBJ(dev), 0, 0, 0, 0, 0, 0) == 1, "SyncInitialize 成功");
    CHK(nvn_SyncWait(OBJ(sync), 1000000ull, 0, 0, 0, 0, 0, 0) != 0, "没人信号时等待超时（1ms）");
    nvn_sync_signal(sync);
    CHK(nvn_SyncWait(OBJ(sync), 0, 0, 0, 0, 0, 0, 0) == 0, "信号之后等待立即成功");
    CHK(nvn_SyncWait(OBJ(sync), 1000000ull, 0, 0, 0, 0, 0, 0) != 0, "等到之后自动复位（再来要再等）");
    CHK(nvn_SyncFinalize(OBJ(sync), 0, 0, 0, 0, 0, 0, 0) == 0, "SyncFinalize 成功");

    printf("== 6. 采样器：float 参数走 s0（统一 u64 签名对它不成立）==\n");
    CHK(nvn_SamplerBuilderSetDefaults(OBJ(sampb), 0, 0, 0, 0, 0, 0, 0) == 0, "采样器 builder 默认值");
    CHK(nvn_SamplerBuilderSetDevice(OBJ(sampb), OBJ(dev), 0, 0, 0, 0, 0, 0) == 0, "采样器 builder 设设备");
    CHK(nvn_SamplerBuilderSetMinMagFilter(OBJ(sampb), 3, 0, 0, 0, 0, 0, 0) == 0, "设过滤 3");
    CHK(nvn_SamplerBuilderSetWrapMode(OBJ(sampb), 4, 0, 0, 0, 0, 0, 0) == 0, "设环绕 4");
    CHK(nvn_SamplerBuilderSetMaxAnisotropy(OBJ(sampb), 8, 0, 0, 0, 0, 0, 0) == 0, "设各向异性 8");
    CHK(nvn_SamplerBuilderSetLodBias(OBJ(sampb), 1.5f, 0, 0, 0, 0, 0, 0) == 0, "设 LOD 偏移 1.5f");
    CHK(nvn_SamplerInitialize(OBJ(samp), OBJ(sampb), 0, 0, 0, 0, 0, 0) == 1, "采样器 Initialize 成功");
    const nvn_st_ent *se = nvn_st_get(samp, 8 /* NVN_T_SAMPLER */);
    CHK(se != NULL, "采样器对象在状态表里");
    if (se) {
        CHK(se->v[1] == 3 && se->v[2] == 4 && se->v[6] == 8, "过滤/环绕/各向异性都存对了");
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
    CHK(nvn_ProgramSetShaders(OBJ(prog), sizeof ctrl, OBJ(ctrl), 0, 0, 0, 0, 0) == 1, "SetShaders 成功");
    const nvn_st_ent *pe = nvn_st_get(prog, 10 /* NVN_T_PROG */);
    CHK(pe && pe->v[1] == OBJ(ctrl) && pe->v[2] == sizeof ctrl, "控制段指针与大小都记下了");

    printf("== 9. 名字表整体 ==\n");
    unsigned miss = 0;
    for (unsigned i = 0; i < g_nvn_table_n; i++)
        if (!nvn_DeviceGetProcAddress(OBJ(dev), OBJ(g_nvn_table[i].name), 0, 0, 0, 0, 0, 0)) miss++;
    CHK(miss == 0, "表里每个名字都能取到地址（461 个名字由 nvnLoadCProcs 依次索取）");
    printf("     表内 %u：手写实现 38，接记录桩 %u，接未实现入口 %u\n",
           g_nvn_table_n, g_nvn_impl_n - 38, g_nvn_table_n - g_nvn_impl_n);

    nvn_st_reset();
    CHK(nvn_st_count() == 0, "清空状态表");
    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
