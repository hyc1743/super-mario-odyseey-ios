// 内嵌装载自检（只在 macOS / iOS 上跑）：验证「三段签名段 → 连续映像」这条路。
//
// 为什么要有它：iOS 真机不许运行时生成/改代码，客体映像必须靠内嵌三段 + mach_vm_remap 拼回来；
// 而 iOS 上连 work/rewrite/*.tsv 都没有（布局与门表也内嵌）。这条路一旦错，真机上表现出来是
// 「跑着跑着跳飞」——所以要在 CI（macOS runner）上先证明：
//   1. 内嵌表能按名字查到模块
//   2. 装载器**不读任何文件**也能装起来（传一个不存在的目录）—— 这是 iOS 能工作的关键
//   3. 映射出来的映像与 patched.bin **逐字节相同**（布局与原映像一致，PC 相对寻址才成立）
//   4. 段权限正确：代码段 RX、可写段 RW，且可写段**真的能写**（写时复制）
//   5. 门登记成功（门表来自内嵌，不是 tsv）
//
// 用法：embed_selftest <patched.bin 路径>（CI 传 work/synthetic/sdk.patched.bin）
#include "dm.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <unistd.h>

#ifndef PAGE
#define PAGE 0x4000          // Apple Silicon 16 KB 页（loader_apple.c 里也有同值）
#endif

static int fails;
#define CHK(cond, msg) do { int _ok = (cond); printf("  %s %s\n", _ok ? "✅" : "【红】", msg); if (!_ok) fails++; } while (0)

static int prot_of(void *p, vm_prot_t *out) {
    mach_vm_address_t addr = (mach_vm_address_t)(uintptr_t)p;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj = MACH_PORT_NULL;
    kern_return_t kr = mach_vm_region(mach_task_self(), &addr, &size, VM_REGION_BASIC_INFO_64,
                                      (vm_region_info_t)&info, &cnt, &obj);
    if (kr != KERN_SUCCESS) return -1;
    *out = info.protection;
    return 0;
}

int main(int argc, char **argv) {
    printf("== 1. 内嵌表查找 ==\n");
    const dm_embed_mod *e = dm_embed_find("sdk");
    CHK(e != NULL, "按名字查到 sdk（说明链接进来的是生成的表）");
    CHK(dm_embed_find("nope") == NULL, "不存在的名字返回 NULL");
    if (!e) return 1;
    printf("     sdk：x0=%zu W=%zu x1=%zu image=%zu 门 %u\n",
           e->x0_sz, e->w_sz, e->x1_sz, e->image_size, e->n_gates);

    printf("== 2. 不读文件也能装载（传一个不存在的目录）==\n");
    host_thread_keys_create();      // 与真机（main.c）一样：先建宿主线程键，再装载
    CHK(dm_slot_keys_ready(), "宿主线程键已建");
    static const char *names[] = { "sdk" };
    load_modules("/nonexistent-dir", names, 1);   // 存在内嵌表就走内嵌，目录无关
    CHK(g_nmod == 1, "装载了 1 个模块");
    module_t *m = &g_mod[0];
    CHK(m->embed == e, "模块标记为内嵌映像");
    CHK(m->size == e->image_size, "映像大小来自内嵌表");
    CHK(dm_gate_count() > 0, "门登记成功（门表来自内嵌，不是 tsv）");
    printf("     门 %u 个\n", dm_gate_count());

    printf("== 3. 映射出来的映像与 patched.bin 逐字节相同 ==\n");
    if (argc >= 2) {
        FILE *f = fopen(argv[1], "rb");
        if (!f) { printf("  【红】 打不开 %s\n", argv[1]); return 1; }
        uint8_t *raw = malloc(m->size);
        size_t got = fread(raw, 1, m->size, f);
        fclose(f);
        CHK(got == m->size, "读到同样多的字节");
        size_t diff = 0, first = 0;
        for (size_t i = 0; i < got && i < m->size; i++)
            if (raw[i] != m->base[i]) { if (!diff) first = i; diff++; }
        CHK(diff == 0, "逐字节相同（布局与原映像一致 —— PC 相对寻址才成立）");
        if (diff) printf("     差异 %zu 字节，首个 @ 0x%zx\n", diff, first);
        free(raw);
    } else {
        printf("  （没给 patched.bin 路径，跳过逐字节比对）\n");
    }

    printf("== 4. 段权限 ==\n");
    vm_prot_t p0 = 0, p1 = 0;
    CHK(prot_of(m->base, &p0) == 0, "能查询首地址所在区域");
    CHK((p0 & VM_PROT_EXECUTE) && (p0 & VM_PROT_READ) && !(p0 & VM_PROT_WRITE),
        "代码段 = RX（来自签名页，可执行且不可写）");
    void *wp = m->base + e->x0_sz + 0x100;                 // 可写段内部
    CHK(prot_of(wp, &p1) == 0, "能查询可写段");
    CHK((p1 & VM_PROT_WRITE) && (p1 & VM_PROT_READ), "可写段 = RW");
    uint8_t *probe = (uint8_t *)(m->base + e->image_size - PAGE - 1);   // 尾部空页，写它不会碰到真数据
    uint8_t save = *probe;
    *probe = 0x5A;
    CHK(*probe == 0x5A, "可写段真的能写（写时复制生效，没崩）");
    *probe = save;

    printf("\n%s\n", fails ? "有断言不通过" : "全部通过");
    return fails != 0;
}
