// Metal 重放层第一片的单测（只在 macOS / iOS 上跑；CI 的 macOS runner 离屏验证）。
// 判据：清成纯色 → 回读像素必须逐字节等于该色（8 位无符号量化）。
#include "../nvn_mtl.h"
#include <stdio.h>
#include <string.h>

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

static int draw_and_check(nvn_mtl_ctx *c, float r, float g, float b, uint8_t er, uint8_t eg, uint8_t eb) {
    uint8_t px[8 * 8 * 4];
    int n = nvn_mtl_offscreen_draw(c, 8, 8, r, g, b, px, sizeof px);
    if (n != (int)sizeof px) return 0;
    for (int i = 0; i < 8 * 8; i++) {
        if (px[i * 4] != er || px[i * 4 + 1] != eg || px[i * 4 + 2] != eb || px[i * 4 + 3] != 255)
            return 0;
    }
    return 1;
}

static int clear_and_check(nvn_mtl_ctx *c, float r, float g, float b, uint8_t er, uint8_t eg, uint8_t eb) {
    uint8_t px[8 * 8 * 4];
    int n = nvn_mtl_offscreen_clear(c, 8, 8, r, g, b, 1.0f, px, sizeof px);
    if (n != (int)sizeof px) return 0;
    for (int i = 0; i < 8 * 8; i++) {
        if (px[i * 4] != er || px[i * 4 + 1] != eg || px[i * 4 + 2] != eb || px[i * 4 + 3] != 255)
            return 0;
    }
    return 1;
}

int main(void) {
    printf("== 1. 设备与队列 ==\n");
    nvn_mtl_ctx *c = NULL;
    int r = nvn_mtl_init(&c);
    if (r != 0) {
        printf("  设备不可用（无 Metal 的环境）；CI 的 macOS runner 上应可跑\n");
        printf("\n%s\n", "全部通过（本环境跳过）");
        return 0;
    }
    CHK(c != NULL, "初始化成功（MTLCreateSystemDefaultDevice + 队列）");

    printf("== 2. 清屏 → 回读 ==\n");
    CHK(clear_and_check(c, 1, 0, 0, 255, 0, 0), "红 (1,0,0,1) → 回读 (255,0,0,255) ×64");
    CHK(clear_and_check(c, 0, 1, 0, 0, 255, 0), "绿 (0,1,0,1) → 回读 (0,255,0,255) ×64");
    CHK(clear_and_check(c, 0, 0, 1, 0, 0, 255), "蓝 (0,0,1,1) → 回读 (0,0,255,255) ×64");
    CHK(clear_and_check(c, 0.5f, 0, 0, 128, 0, 0), "半红 0.5 → 回读 128（量化正确）");

    printf("== 3. 参数防御 ==\n");
    CHK(nvn_mtl_offscreen_clear(c, 8, 8, 1, 1, 1, 1, NULL, 0) == -1, "空输出缓冲 → -1");
    CHK(nvn_mtl_offscreen_clear(c, 8, 8, 1, 1, 1, 1, (uint8_t *)&r, 4) == -1, "容量不足 → -1");
    CHK(nvn_mtl_offscreen_clear(NULL, 8, 8, 1, 1, 1, 1, (uint8_t *)&r, 256) == -1, "空上下文 → -1");
    CHK(nvn_mtl_offscreen_draw(NULL, 8, 8, 1, 1, 1, (uint8_t *)&r, 256) == -1, "绘制：空上下文 → -1");
    CHK(nvn_mtl_offscreen_draw(c, 8, 8, 1, 1, 1, (uint8_t *)&r, 4) == -1, "绘制：容量不足 → -1");

    nvn_mtl_term(c);
    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
