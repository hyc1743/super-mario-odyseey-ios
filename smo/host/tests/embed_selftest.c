// 内嵌装载自检（只在 macOS / iOS 上跑）：验证「三段签名段 → 连续映像」这条路。
//
// 为什么要有它：iOS 真机不许运行时生成/改代码，客体映像必须靠内嵌三段 + mach_vm_remap 拼回来；
// 而 iOS 上连 work/rewrite/*.tsv 都没有（布局与门表也内嵌）。这条路一旦错，真机上表现出来是
// 「跑着跑着跳飞」——所以要在 CI（macOS runner）上先证明：
//   1. 内嵌表能按名字查到模块
//   2. 三段映射出来的映像：布局与原映像一致、代码段 RX、可写段 RW 且真能写
//   3. 装载器**不读任何文件**也能装起来（传一个不存在的目录）—— 这是 iOS 能工作的关键
//   4. 门登记成功（门表来自内嵌，不是 tsv）
//
// 排查约定：每一步都用 LOG（stderr，不缓冲）打进度，CHK 里 flush —— 「无输出崩溃」只能靠这个定位。
#include "dm.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <unistd.h>

#ifndef PAGE
#define PAGE 0x4000          // Apple Silicon 16 KB 页（loader_apple.c 里也有同值）
#endif

static int fails;
// CHK 里 flush：stdout 重定向到文件时是全缓冲的，崩溃会吞掉已打印的内容（本项目踩过两次）
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); fflush(stdout); if (!_ok) fails++; } while (0)

// 查询一段内存的权限。
// 注意缓冲区：用**宏给出的个数**分配 integer_t 数组，而不是自己声明 vm_region_basic_info_data_64_t
// —— 后者一旦与 count 宏的期望尺寸不一致，mach_vm_region 就会越界写栈（CI 上真崩过）。
static int prot_of(void *p, vm_prot_t *out) {
    mach_vm_address_t addr = (mach_vm_address_t)(uintptr_t)p;
    mach_vm_size_t size = 0;
    integer_t info[VM_REGION_BASIC_INFO_COUNT_64];
    mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj = MACH_PORT_NULL;
    kern_return_t kr = mach_vm_region(mach_task_self(), &addr, &size, VM_REGION_BASIC_INFO_64,
                                      (vm_region_info_t)info, &cnt, &obj);
    if (kr != KERN_SUCCESS) return -1;
    *out = ((vm_region_basic_info_data_64_t *)info)->protection;
    return 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("== 1. 内嵌表查找 ==\n");
    const dm_embed_mod *e = dm_embed_find("sdk");
    CHK(e != NULL, "按名字查到 sdk（说明链接进来的是生成的表）");
    CHK(dm_embed_find("nope") == NULL, "不存在的名字返回 NULL");
    if (!e) return 1;
    LOG("内嵌表：sdk x0=%zu W=%zu x1=%zu image=%zu 门 %u",
        e->x0_sz, e->w_sz, e->x1_sz, e->image_size, e->n_gates);

    printf("== 2. 直接映射三段（先不经过装载器）==\n");
    LOG("第 2 步：调 dm_embed_map");
    void *img = dm_embed_map(e);
    LOG("第 2 步：dm_embed_map 返回 %p", img);
    CHK(img != NULL, "dm_embed_map 成功（三段 mach_vm_remap）");
    if (!img) { printf("\n%s\n", "有断言不通过"); return 1; }

    vm_prot_t p0 = 0, p1 = 0;
    CHK(prot_of(img, &p0) == 0, "能查询首地址所在区域");
    LOG("首段权限 = %#x", (unsigned)p0);
    CHK((p0 & VM_PROT_EXECUTE) && (p0 & VM_PROT_READ) && !(p0 & VM_PROT_WRITE),
        "代码段 = RX（来自签名页：可执行、不可写）");
    void *wp = (uint8_t *)img + e->x0_sz + 0x100;
    CHK(prot_of(wp, &p1) == 0, "能查询可写段");
    LOG("可写段权限 = %#x", (unsigned)p1);
    CHK((p1 & VM_PROT_WRITE) && (p1 & VM_PROT_READ), "可写段 = RW");

    printf("== 3. 映射出来的映像与 patched.bin 逐字节相同 ==\n");
    if (argc >= 2) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { printf("  【红】 打不开 %s\n", argv[1]); return 1; }
        uint8_t *raw = malloc(e->image_size);
        size_t got = fread(raw, 1, e->image_size, f);
        fclose(f);
        LOG("读了 %zu 字节（期望 %zu）", got, e->image_size);
        CHK(got == e->image_size, "读到同样多的字节");
        size_t diff = 0, first = 0;
        for (size_t i = 0; i < got; i++)
            if (raw[i] != ((uint8_t *)img)[i]) { if (!diff) first = i; diff++; }
        CHK(diff == 0, "逐字节相同（布局与原映像一致 —— PC 相对寻址才成立）");
        if (diff) LOG("差异 %zu 字节，首个 @ 0x%zx", diff, first);
        free(raw);
    } else {
        printf("  （没给 patched.bin 路径，跳过逐字节比对）\n");
    }

    printf("== 4. 装载器不读文件也能装起来（传一个不存在的目录）==\n");
    LOG("第 4 步：host_thread_keys_create");
    host_thread_keys_create();
    CHK(dm_slot_keys_ready(), "宿主线程键已建");
    LOG("第 4 步：load_modules（内嵌路径）");
    static const char *names[] = { "sdk" };
    load_modules("/nonexistent-dir", names, 1);
    LOG("第 4 步：load_modules 返回");
    CHK(g_nmod == 1, "装载了 1 个模块");
    module_t *m = &g_mod[0];
    CHK(m->embed == e, "模块标记为内嵌映像");
    CHK(m->size == e->image_size, "映像大小来自内嵌表");
    CHK(dm_gate_count() > 0, "门登记成功（门表来自内嵌，不是 tsv）");
    LOG("门 %u 个", dm_gate_count());

    printf("== 5. 键页内容 ==\n");
    const uint64_t *page = (const uint64_t *)(m->base + m->key_page_off);
    unsigned nonzero = 0;
    for (unsigned k = 0; k < KEY_N; k++) if (page[m->key_idx[k]]) nonzero++;
    CHK(nonzero == KEY_N, "8 个键槽都非 0（宿主键号 × 8 已写入）");

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
