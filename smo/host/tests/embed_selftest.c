// 内嵌装载自检（只在 macOS / iOS 上跑）：验证「三段签名段 → 连续映像」这条路。
//
// 为什么要有它：iOS 真机不许运行时生成/改代码，客体映像必须靠内嵌三段 + mach_vm_remap 拼回来；
// 而 iOS 上连 work/rewrite/*.tsv 都没有（布局与门表也内嵌）。这条路一旦错，真机上表现出来是
// 「跑着跑着跳飞」——所以要在 CI（macOS runner）上先证明：
//   1. 内嵌表能按名字查到模块，三段映射成功
//   2. 映射出来的映像与 patched.bin **逐字节相同**（布局与原映像一致，PC 相对寻址才成立）
//   3. 可写段真能写（写时复制生效）
//   4. 装载器**不读任何文件**也能装起来（传一个不存在的目录）—— 这是 iOS 能工作的关键
//   5. 门与键页都对（两者都来自内嵌表，不是 tsv）
//
// 排查约定：每步都用 LOG（stderr，不缓冲）打进度，CHK 里 flush。
// 不用 mach_vm_region 查权限：实测在 CI runner 上它会让自检自己崩（那是自检的 bug，不是被测代码的），
// 而「可写段真能写」「映像与产物逐字节相同」已经覆盖了真正要验的性质。
#include "dm.h"
#include <unistd.h>

#ifndef PAGE
#define PAGE 0x4000          // Apple Silicon 16 KB 页（loader_apple.c 里也有同值）
#endif

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("== 1. 内嵌表 ==\n");
    const dm_embed_mod *e = dm_embed_find("sdk");
    CHK(e != NULL, "按名字查到 sdk（链接进来的是生成的表）");
    CHK(dm_embed_find("nope") == NULL, "不存在的名字返回 NULL");
    if (!e) return 1;
    LOG("内嵌表 sdk：x0=%zu W=%zu x1=%zu image=%zu 门 %u",
        e->x0_sz, e->w_sz, e->x1_sz, e->image_size, e->n_gates);

    printf("== 2. 三段映射成一个连续映像 ==\n");
    void *img = dm_embed_map(e);
    LOG("dm_embed_map 返回 %p", img);
    CHK(img != NULL, "映射成功（两段代码共享签名页 + 一段写时复制）");
    if (!img) { printf("\n%s\n", "有断言不通过"); return 1; }

    printf("== 3. 映像与 patched.bin 逐字节相同 ==\n");
    if (argc >= 2) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { printf("  【红】 打不开 %s\n", argv[1]); return 1; }
        uint8_t *raw = malloc(e->image_size);
        size_t got = fread(raw, 1, e->image_size, f);
        fclose(f);
        LOG("3.3 读了 %zu 字节", got);
        CHK(got == e->image_size, "读到同样多的字节");
        LOG("3.4 开始逐字节比（%zu 字节）", got);
        size_t diff = 0, first = 0;
        for (size_t i = 0; i < got; i++)
            if (raw[i] != ((uint8_t *)img)[i]) { if (!diff) first = i; diff++; }
        LOG("3.5 比完，差异 %zu", diff);
        CHK(diff == 0, "逐字节相同（运行时布局与原映像一致）");
        if (diff) LOG("差异 %zu 字节，首个 @ 0x%zx", diff, first);
        free(raw);
    } else {
        printf("  （没给 patched.bin 路径，跳过）\n");
    }

    printf("== 4. 可写段真能写（写时复制）==\n");
    uint8_t *probe = (uint8_t *)img + e->x0_sz + 0x100;      // 可写段内部
    uint8_t save = *probe;
    *probe = 0x5A;
    CHK(*probe == 0x5A, "写进去读回来一致（没崩、也没被签名页拦）");
    *probe = save;

    printf("== 5. 装载器不读文件也能装起来 ==\n");
    host_thread_keys_create();
    CHK(dm_slot_keys_ready(), "宿主线程键已建");
    LOG("调 load_modules（目录故意不存在）");
    static const char *names[] = { "sdk" };
    load_modules("/nonexistent-dir", names, 1);
    LOG("load_modules 返回");
    CHK(g_nmod == 1, "装载了 1 个模块");
    module_t *m = &g_mod[0];
    CHK(m->embed == e, "模块标记为内嵌映像");
    CHK(m->size == e->image_size, "映像大小来自内嵌表");
    CHK(dm_gate_count() > 0, "门登记成功（门表来自内嵌）");
    LOG("门 %u 个", dm_gate_count());

    printf("== 6. 键页 ==\n");
    const uint64_t *page = (const uint64_t *)(m->base + m->key_page_off);
    unsigned nonzero = 0;
    for (unsigned k = 0; k < KEY_N; k++) if (page[m->key_idx[k]]) nonzero++;
    CHK(nonzero == KEY_N, "8 个键槽都非 0（宿主键号 × 8 已写入）");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
