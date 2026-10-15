// 重放记账层单测（不需要 Metal、不需要游戏文件）：
// 用真实的状态层 API 造出「设备 + 程序 + 控制段」的对象，再喂一串 NVN 调用记录，
// 检查重建出的绘制计划：条数、程序、着色器阶段提示，以及"绑了没初始化过的对象"能被抓出来。
#include "../nvn_host.h"
#include "../nvn_state.h"
#include "../nvn_shader.h"

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

#define DECL(n) uint64_t n(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
DECL(nvn_DeviceInitialize);
DECL(nvn_DeviceBuilderSetDefaults);
DECL(nvn_ProgramInitialize);
DECL(nvn_ProgramSetShaders);
DECL(nvn_CommandBufferInitialize);

void nvn_replay_stats(unsigned *, unsigned *, unsigned *, unsigned *, unsigned *, unsigned *, unsigned *, unsigned *);

static void call(const char *n, uint64_t a0, uint64_t a1, uint64_t a2) {
    uint64_t a[8] = { a0, a1, a2, 0, 0, 0, 0, 0 };
    nvn_replay_call(n, a);
}

int main(void) {
    uint64_t devb[8] = {0}, dev[8] = {0}, prog[8] = {0}, cmdbuf[8] = {0};
    static uint8_t ctrl[0x30 + 0x100];
    static uint8_t blob[0x100];
    for (unsigned i = 0; i < sizeof blob; i++) blob[i] = (uint8_t)(i * 13 + 5);

    printf("== 0. 造出设备/程序/控制段（真实的状态层 API）==\n");
    nvn_DeviceBuilderSetDefaults((uint64_t)(uintptr_t)devb, 0, 0, 0, 0, 0, 0, 0);
    CHK(nvn_DeviceInitialize((uint64_t)(uintptr_t)dev, (uint64_t)(uintptr_t)devb, 0, 0, 0, 0, 0, 0) == 0,
        "设备初始化");
    CHK(nvn_ProgramInitialize((uint64_t)(uintptr_t)prog, (uint64_t)(uintptr_t)dev, 0, 0, 0, 0, 0, 0) == 0,
        "程序初始化");
    // 控制段按游戏归档槽布局造：marker@+0 / SPH+码@+0x30
    *(uint32_t *)ctrl = 0x12345678u;
    for (unsigned i = 0; i < sizeof blob; i++) ctrl[0x30 + i] = blob[i];
    CHK(nvn_ProgramSetShaders((uint64_t)(uintptr_t)prog, sizeof ctrl, (uint64_t)(uintptr_t)ctrl,
                              0, 0, 0, 0, 0) == 0, "SetShaders（控制段已记下）");

    printf("== 1. 空记录：什么都不该发生 ==\n");
    nvn_replay_reset();
    CHK(nvn_replay_count() == 0 && nvn_replay_draws() == 0, "重置后计划为空");

    printf("== 2. 一串调用：绑定 + 设置 + 两次绘制 + 清屏 + 分派 ==\n");
    call("nvnCommandBufferInitialize", (uint64_t)(uintptr_t)cmdbuf, (uint64_t)(uintptr_t)dev, 0);
    call("nvnCommandBufferBindProgram", (uint64_t)(uintptr_t)cmdbuf, (uint64_t)(uintptr_t)prog, 0);
    call("nvnCommandBufferSetDepthMode", (uint64_t)(uintptr_t)cmdbuf, 1, 0);
    call("nvnCommandBufferDrawElements", (uint64_t)(uintptr_t)cmdbuf, 100, 2);
    call("nvnCommandBufferDrawArrays", (uint64_t)(uintptr_t)cmdbuf, 36, 1);
    call("nvnCommandBufferClearColor", (uint64_t)(uintptr_t)cmdbuf, 0, 0);
    call("nvnCommandBufferDispatchCompute", (uint64_t)(uintptr_t)cmdbuf, 8, 4);
    CHK(nvn_replay_count() == 3, "计划项 3 条（两次绘制 + 一次分派）");
    CHK(nvn_replay_draws() == 2, "绘制 2 次");
    CHK(nvn_replay_unresolved() == 0, "没有绑到未初始化的对象");

    printf("== 3. 计划项内容：带上了当时的 program 与控制段查出的阶段 ==\n");
    const nvn_draw_item *it = nvn_replay_item(0);
    CHK(it && it->program == (uint64_t)(uintptr_t)prog, "第 1 条计划项带着当前 program");
    // 合成本测试里的 blob 不在索引里，所以阶段提示应是"没查到"(0xFFFF) 而不是 0xFFFE（当时已 SetShaders）
    CHK(it && it->stage_hint == 0xFFFF, "着色器未载入索引 → 阶段提示 0xFFFF（与「当时还没 SetShaders」的 0xFFFE 区分开）");
    CHK(it && it->a1 == 100 && it->a2 == 2, "绘制参数原样带过来（count=100, type=2）");

    printf("== 3b. 管线键：状态没变则键不变；状态一变键就变 ==");
    putchar('\n');
    uint64_t k0 = nvn_replay_item_key(0), k1 = nvn_replay_item_key(1);
    CHK(k0 != 0 && k0 == k1, "两次绘制之间没有改状态 → 管线键相同（Metal 层可以复用管线）");
    call("nvnCommandBufferSetDepthMode", (uint64_t)(uintptr_t)cmdbuf, 7, 0);
    call("nvnCommandBufferDrawElements", (uint64_t)(uintptr_t)cmdbuf, 12, 4);
    uint64_t k2 = nvn_replay_item_key(nvn_replay_count() - 1);   // 最后一项 = 刚才那次绘制
    CHK(k2 != 0 && k2 != k0, "改了深度模式再画 → 管线键变了（不会误用旧管线）");
    CHK(nvn_replay_count() == 4, "计划项现在 4 条");

    printf("== 4. 绑了没初始化过的 program：必须被抓住 ==\n");
    nvn_replay_reset();
    static uint64_t bogus[8] = {0};
    call("nvnCommandBufferBindProgram", (uint64_t)(uintptr_t)cmdbuf, (uint64_t)(uintptr_t)bogus, 0);
    call("nvnCommandBufferDrawElements", (uint64_t)(uintptr_t)cmdbuf, 3, 0);
    CHK(nvn_replay_unresolved() == 1, "计数抓到了 1 次未解析绑定");
    CHK(nvn_replay_unresolved_name() && !strcmp(nvn_replay_unresolved_name(), "nvnCommandBufferBindProgram"),
        "记下了是哪个调用出的问题");
    it = nvn_replay_item(0);
    CHK(it && it->program == (uint64_t)(uintptr_t)bogus, "计划项仍然记下它（真机诊断用），但上面已经报红了");

    printf("== 5. 统计口径 ==\n");
    unsigned b2, s2, d2, dp2, c2, cp2, p2, sy2;
    nvn_replay_stats(&b2, &s2, &d2, &dp2, &c2, &cp2, &p2, &sy2);
    CHK(b2 >= 1 && d2 == 1, "未解析那一轮：绑定 >=1、绘制 1");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
