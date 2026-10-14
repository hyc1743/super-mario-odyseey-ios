// NVN 接口层单测：不需要游戏文件、不需要 Apple 平台（不碰 Metal）。
// 检查的是「按名取函数」这条最关键的约定：
//   1. 表里 469 个名字（游戏真实索取的）全部能取到非 0 地址
//   2. 不在表里的名字返回 0（NVN 的真实行为：取不到就是 0，游戏自己处理）
//   3. 调用会进命令记录（Metal 重放的接缝），并且可以清空（每帧重放前清）
//   4. 未实现入口会累计次数（真机上「哪些还缺」靠这个数）
#include "../nvn_host.h"

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

unsigned nvn_record_count(void);
void nvn_record_reset(void);

int main(void) {
    printf("== 表 ==\n");
    CHK(g_nvn_table_n > 400, "表里名字数 > 400（游戏真实索取 469 个）");
    unsigned miss = 0, impl = 0;
    for (unsigned i = 0; i < g_nvn_table_n; i++) {
        if (!nvn_BootstrapLoader(g_nvn_table[i].name)) miss++;
        if (g_nvn_table[i].fn != (nvn_fn)nvn_unimplemented) impl++;
    }
    CHK(miss == 0, "每个名字都能按名取到函数地址");
    CHK(impl == g_nvn_impl_n, "已实现数与生成时登记的一致");
    CHK(impl > 0, "核心家族（Device/MemoryPool/Buffer/Sync/Window/Program/Texture/Sampler/Queue）已接");
    printf("     表内 %u：已实现 %u，接未实现入口 %u\n", g_nvn_table_n, impl, g_nvn_table_n - impl);

    printf("== 取不到的名字 ==\n");
    CHK(nvn_BootstrapLoader("nvnThisDoesNotExist") == 0, "不在表里的名字返回 0");
    CHK(nvn_BootstrapLoader("") == 0, "空名字返回 0");

    printf("== 命令记录（Metal 重放的接缝）==\n");
    // 直接测记录函数本身：早先这里拿 nvnDeviceBuilderSetDefaults 当「记录桩」的例子，
    // 结果它后来被写成了真实现，测试就红了 —— 这种耦合要避免（实现清单会一直变）。
    nvn_record_reset();
    nvn_record_call("nvnTestOne", 0x1234, 5, 6, 7, 0, 0, 0, 0);
    nvn_record_call("nvnTestOne", 0x1234, 1, 2, 3, 0, 0, 0, 0);
    CHK(nvn_record_count() == 2, "两次记录调用都进了记录");
    nvn_record_reset();
    CHK(nvn_record_count() == 0, "可以清空（每帧重放前清）");
    nvn_fn some = (nvn_fn)(uintptr_t)nvn_BootstrapLoader("nvnDeviceSetDebugLabel");
    CHK(some != (nvn_fn)0, "家族里还没手写的函数也能取到地址（此例 nvnDeviceSetDebugLabel）");

    printf("== 未实现入口 ==\n");
    unsigned long long before = nvn_unimpl_calls();
    nvn_unimplemented(0, 0, 0, 0, 0, 0, 0, 0);
    CHK(nvn_unimpl_calls() == before + 1, "未实现调用计数 +1");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
