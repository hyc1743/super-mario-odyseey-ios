// 内嵌装载自检（只在 macOS / iOS 上跑）：验证「三段签名段 → 连续映像」这条路。
//
// 为什么要有它：iOS 真机不许运行时生成/改代码，客体映像必须靠内嵌三段 + mach_vm_remap 拼回来；
// 而 iOS 上连 work/rewrite/*.tsv 都没有（布局与门表也内嵌）。这条路一旦错，真机上表现出来是
// 「跑着跑着跳飞」——所以要在 CI（macOS runner）上先证明：
//   1. 内嵌表能按名字查到模块，三段映射成功
//   2. 映射出来的映像与三段自身**逐字节相同**（布局与原映像一致，PC 相对寻址才成立）
//   3. 可写段真能写（写时复制生效）
//   4. 装载器**不读任何文件**也能装起来（传一个不存在的目录）—— 这是 iOS 能工作的关键
//   5. 门与键页都对（两者都来自内嵌表，不是 tsv）
//
// **分段执行**：`embed_selftest <step>` 只跑到第 step 节就退出，`--all`（默认）跑全部。
// 起因：CI 上它曾在第 2 节之后以 SIGBUS(138) 崩溃，而日志停在「映射成功」——分段跑一轮就能定位到节。
//
// 排查约定：每步用 LOG（stderr，不缓冲）打进度，CHK 里 flush。
#include "dm.h"
#include <unistd.h>

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int until = 99;                                   // 默认跑完
    if (argc >= 2 && argv[1][0] != '-') until = atoi(argv[1]);
    LOG("分段执行：跑到第 %d 节", until);

    const dm_embed_mod *e = NULL;
    void *img = NULL;

    printf("== 1. 内嵌表 ==\n");
    e = dm_embed_find("sdk");
    CHK(e != NULL, "按名字查到 sdk（链接进来的是生成的表）");
    CHK(dm_embed_find("nope") == NULL, "不存在的名字返回 NULL");
    if (!e) return 1;
    LOG("内嵌表 sdk：x0=%zu W=%zu x1=%zu image=%zu 门 %u",
        e->x0_sz, e->w_sz, e->x1_sz, e->image_size, e->n_gates);
    if (until <= 1) goto done;

    printf("== 2. 三段映射成一个连续映像 ==\n");
    img = dm_embed_map(e);
    LOG("dm_embed_map 返回 %p", img);
    CHK(img != NULL, "映射成功（两段代码共享签名页 + 一段写时复制）");
    if (!img) goto done;
    if (until <= 2) goto done;

    printf("== 3. 映像与三段自身逐字节相同（不碰文件 I/O）==\n");
    LOG("3.1 比 x0（%zu 字节）", e->x0_sz);
    size_t d = 0;
    for (size_t i = 0; i < e->x0_sz; i++) if (((uint8_t *)img)[i] != e->x0[i]) d++;
    LOG("3.2 x0 差异 %zu", d);
    CHK(d == 0, "代码段（x0）逐字节相同");
    d = 0;
    for (size_t i = 0; i < e->w_sz; i++) if (((uint8_t *)img)[e->x0_sz + i] != e->w[i]) d++;
    LOG("3.3 W 差异 %zu（%zu 字节）", d, e->w_sz);
    CHK(d == 0, "可写段（W）逐字节相同");
    d = 0;
    for (size_t i = 0; i < e->x1_sz; i++)
        if (((uint8_t *)img)[e->x0_sz + e->w_sz + i] != e->x1[i]) d++;
    LOG("3.4 x1 差异 %zu（%zu 字节）", d, e->x1_sz);
    CHK(d == 0, "补丁区（x1）逐字节相同");
    if (until <= 3) goto done;

    printf("== 4. 可写段真能写（写时复制）==\n");
    {
        uint8_t *probe = (uint8_t *)img + e->x0_sz + 0x100;
        LOG("4.1 写 %p", (void *)probe);
        uint8_t save = *probe;
        *probe = 0x5A;
        LOG("4.2 读回 %#x", *probe);
        CHK(*probe == 0x5A, "写进去读回来一致（没崩、也没被签名页拦）");
        *probe = save;
    }
    if (until <= 4) goto done;

    printf("== 5. 装载器不读文件也能装起来 ==\n");
    LOG("5.1 host_thread_keys_create");
    host_thread_keys_create();
    CHK(dm_slot_keys_ready(), "宿主线程键已建");
    LOG("5.2 load_modules（目录故意不存在）");
    static const char *names[] = { "sdk" };
    load_modules("/nonexistent-dir", names, 1);
    LOG("5.3 load_modules 返回");
    CHK(g_nmod == 1, "装载了 1 个模块");
    {
        module_t *m = &g_mod[0];
        CHK(m->embed == e, "模块标记为内嵌映像");
        CHK(m->size == e->image_size, "映像大小来自内嵌表");
        CHK(dm_gate_count() > 0, "门登记成功（门表来自内嵌）");
        LOG("门 %u 个", dm_gate_count());
        if (until <= 5) goto done;

        printf("== 6. 键页 ==\n");
        const uint64_t *page = (const uint64_t *)(m->base + m->key_page_off);
        unsigned nonzero = 0;
        for (unsigned k = 0; k < KEY_N; k++) if (page[m->key_idx[k]]) nonzero++;
        CHK(nonzero == KEY_N, "8 个键槽都非 0（宿主键号 × 8 已写入）");
    }

done:
    printf("\n%s（跑到第 %d 节，%d 条断言未过）\n", fails ? "有断言不通过" : "全部通过", until, fails);
    return fails != 0;
}
