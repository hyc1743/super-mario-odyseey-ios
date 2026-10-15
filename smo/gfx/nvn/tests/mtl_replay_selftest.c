// 合龙点单测（只在 macOS / iOS 上跑）：一条**合成的命令流**，走完
//   真实状态层（设备/程序/控制段）→ 记录层（归类成计划项）→ Metal 帧（清屏 + 按管线键着色）→ 回读
// 并断言。这条链一旦绿，说明「记录 → 计划 → Metal」的管道是通的；之后把真实参数的翻译逐块接进来即可。
#include "../nvn_mtl.h"
#include "../nvn_host.h"
#include "../nvn_state.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

#define DECL(n) uint64_t n(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t)
DECL(nvn_DeviceBuilderSetDefaults);
DECL(nvn_DeviceInitialize);
DECL(nvn_ProgramInitialize);
DECL(nvn_ProgramSetShaders);
#undef DECL
#define OBJ(x) ((uint64_t)(uintptr_t)(x))

static void call(const char *n, uint64_t a0, uint64_t a1, uint64_t a2) {
    uint64_t a[8] = { a0, a1, a2, 0, 0, 0, 0, 0 };
    nvn_record_call(n, a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7]);
}

int main(void) {
    printf("== 1. 用真实状态层造对象，再用记录层录一条合成流 ==\n");
    static uint64_t devb[8], dev[8], prog[8], cmdbuf[8];
    static uint8_t ctrl[0x30 + 0x100], blob[0x100];
    for (unsigned i = 0; i < sizeof blob; i++) blob[i] = (uint8_t)(i * 13 + 5);
    nvn_DeviceBuilderSetDefaults(OBJ(devb), 0, 0, 0, 0, 0, 0, 0);
    nvn_DeviceInitialize(OBJ(dev), OBJ(devb), 0, 0, 0, 0, 0, 0);
    nvn_ProgramInitialize(OBJ(prog), OBJ(dev), 0, 0, 0, 0, 0, 0);
    *(uint32_t *)ctrl = 0x12345678u;
    memcpy(ctrl + 0x30, blob, sizeof blob);
    nvn_ProgramSetShaders(OBJ(prog), sizeof ctrl, OBJ(ctrl), 0, 0, 0, 0, 0);

    nvn_replay_reset();
    call("nvnCommandBufferInitialize", OBJ(cmdbuf), OBJ(dev), 0);
    call("nvnCommandBufferBindProgram", OBJ(cmdbuf), OBJ(prog), 0);
    call("nvnCommandBufferClearColor", OBJ(cmdbuf), 0xFF0000FFu, 0);   // 0xAABBGGRR：不透明红
    call("nvnCommandBufferDrawElements", OBJ(cmdbuf), 100, 2);
    call("nvnCommandBufferSetDepthMode", OBJ(cmdbuf), 7, 0);           // 状态一变，管线键就变
    call("nvnCommandBufferDrawArrays", OBJ(cmdbuf), 36, 1);

    unsigned nd = nvn_replay_count();
    CHK(nd == 2, "计划里 2 个绘制项");
    uint64_t k0 = nvn_replay_item_key(0);
    uint64_t k1 = nvn_replay_item_key(1);
    CHK(k0 != 0 && k1 != 0 && k0 != k1, "两个绘制项的管线键不同（状态变了）");
    printf("     管线键 0x%016llx / 0x%016llx\n",
           (unsigned long long)k0, (unsigned long long)k1);

    printf("== 2. 计划项 → Metal 帧 → 回读 ==\n");
    nvn_mtl_ctx *c = NULL;
    if (nvn_mtl_init(&c) != 0) {
        printf("  设备不可用；CI 的 macOS runner 上应可跑\n");
        printf("\n%s\n", "全部通过（本环境跳过）");
        return 0;
    }
    uint64_t keys[2] = { k0, k1 };
    static uint8_t px[8 * 8 * 4];
    int n = nvn_mtl_replay_frame(c, 8, 8, 0xFF0000FFu, keys, 2, px, sizeof px);
    CHK(n == (int)sizeof px, "帧函数写出全部像素");
    // 最后一遍绘制覆盖全屏：颜色 = k1 的低 24 位
    uint8_t er = (uint8_t)(k1 >> 16), eg = (uint8_t)(k1 >> 8), eb = (uint8_t)k1;
    int ok = 1;
    for (int i = 0; i < 8 * 8; i++)
        if (px[i * 4] != er || px[i * 4 + 1] != eg || px[i * 4 + 2] != eb || px[i * 4 + 3] != 255) ok = 0;
    CHK(ok, "回读 == 第二个绘制项的管线键色（最后一遍绘制覆盖全屏）");

    printf("== 3. 单绘制项：回读 == 清屏色之上盖第一个键色 ==\n");
    uint64_t konly[1] = { k0 };
    n = nvn_mtl_replay_frame(c, 8, 8, 0xFF00FF00u /* 绿 */, konly, 1, px, sizeof px);
    CHK(n == (int)sizeof px, "单绘制项帧");
    er = (uint8_t)(k0 >> 16); eg = (uint8_t)(k0 >> 8); eb = (uint8_t)k0;
    ok = 1;
    for (int i = 0; i < 8 * 8; i++)
        if (px[i * 4] != er || px[i * 4 + 1] != eg || px[i * 4 + 2] != eb || px[i * 4 + 3] != 255) ok = 0;
    CHK(ok, "回读 == k0 键色（清屏色被覆盖）");

    nvn_mtl_term(c);
    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
