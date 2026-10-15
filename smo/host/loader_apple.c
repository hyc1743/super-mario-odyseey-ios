// Apple 平台层：映像分配、段权限、线程键（线程键在 thread.c 里，这里只做映像与保护）。
// 派生自参考项目 d2r-mobile 的 dm/host/loader.c（MIT）。
//
// 两条路线（参考项目两条都有，本项目先落 macOS 这条，iOS 那条留好接口与命名约定）：
//
//   ① **开发/CI（macOS）**：匿名映射 + 立即封代码区。
//      16 KB 页（Apple Silicon）：[0, text_end) 与补丁区设为只读+执行，此后对代码区的任何写入
//      当场崩；中间（ro 与 data 同页，只能一起）可写。
//
//   ② **iOS 真机**：**不许运行时生成/修改代码**，所以映像在出包时就拆成三段内嵌进签名段：
//        __TEXT,__dm<模块名>x0   原始代码（不含补丁区）        → RX
//        __img,__DM<模块名>W     ro + data（可写的那部分）      → RW
//        __TEXT,__dm<模块名>x1   补丁区                        → RX
//      装载器用带 FIXED|OVERWRITE 的 mach_vm_remap 把三段重映射到同一个基址的原相对位置上，
//      于是运行时布局与 patched.bin 逐字节相同（PC 相对寻址、补丁区跳转、宿主「基址+原偏移」全不用改）。
//      出包脚本是 smo/ios/tools/gen_embed.py，CI 上用合成产物验（tests/embed_selftest.c）。
#include "dm.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>

#define PAGE 0x4000                      // Apple Silicon 16 KB 页

// ---------------------------------------------------------------- 内嵌映像（iOS 真机路线）
//
// 真机不许运行时生成/修改代码，所以三段都由 gen_embed.py 在出包时编进签名段：
//   __TEXT,__dm<名>x0（原代码，RX）/ __DM<名>W,__img（ro+data+bss，RW）/ __TEXT,__dm<名>x1（补丁区，RX）
// 这里把它们重映射到**同一块连续地址**的原相对位置上：代码页共享已签名页（可直接执行，不需要 JIT），
// 可写那段用写时复制，于是运行时布局与 patched.bin 逐字节相同 ——
// PC 相对寻址、补丁区的跳转、宿主「基址 + 原偏移」的引用全都不用改。
//
// 弱引用：只有链接了生成的表（gen_embed.py 的 .c）时才存在；macOS 开发/CI 从文件装载，这里返回 NULL。
// Mach-O 的弱符号要用 weak_import（ELF 的 weak 在 ld64 上对未定义数据符号不生效，
// smo_host 不链接内嵌表时会直接链接失败 —— CI 上真的挂过）。
extern const dm_embed_mod g_embed_mods[] __attribute__((weak_import));
extern const unsigned g_embed_n __attribute__((weak_import));

const dm_embed_mod *dm_embed_find(const char *name) {
    if (!&g_embed_mods || !&g_embed_n) return NULL;          // 没链接内嵌表
    for (unsigned i = 0; i < g_embed_n; i++)
        if (!strcmp(g_embed_mods[i].name, name)) return &g_embed_mods[i];
    return NULL;
}

void *dm_embed_map(const dm_embed_mod *e) {
    size_t total = (e->image_size + PAGE - 1) & ~(size_t)(PAGE - 1);
    mach_vm_address_t base = 0;
    kern_return_t kr = mach_vm_allocate(mach_task_self(), &base, total, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) { LOG("%s: mach_vm_allocate(%zu) 失败：%d", e->name, total, (int)kr); return NULL; }

    struct part { const uint8_t *src; size_t sz; size_t off; vm_prot_t prot; boolean_t copy; const char *what; };
    struct part parts[3] = {
        { e->x0, e->x0_sz, 0,                   VM_PROT_READ | VM_PROT_EXECUTE, FALSE, "x0 代码" },
        { e->w,  e->w_sz,  e->x0_sz,            VM_PROT_READ | VM_PROT_WRITE,   TRUE,  "W 数据" },
        { e->x1, e->x1_sz, e->x0_sz + e->w_sz,  VM_PROT_READ | VM_PROT_EXECUTE, FALSE, "x1 补丁" },
    };
    for (int i = 0; i < 3; i++) {
        if (!parts[i].sz) continue;
        mach_vm_address_t dst = base + parts[i].off;
        mach_vm_address_t src = (mach_vm_address_t)(uintptr_t)parts[i].src;
        vm_prot_t cur = parts[i].prot;
        // W 段的 max 给 RWX：TEXT_END 不页对齐时，代码尾与缝隙同页（见 gen_embed.py），
        // 那一页既要有 X（代码尾）又要有 W（缝隙里的重定位槽）。
        vm_prot_t maxp = (i == 1) ? (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE) : parts[i].prot;
        kr = mach_vm_remap(mach_task_self(), &dst, parts[i].sz, 0,
                           VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(), src,
                           parts[i].copy, &cur, &maxp, VM_INHERIT_NONE);
        if (kr != KERN_SUCCESS) {
            LOG("%s: %s 段 mach_vm_remap 失败：%d（dst=%#llx src=%#llx sz=%zu）",
                e->name, parts[i].what, (int)kr, (unsigned long long)dst,
                (unsigned long long)src, parts[i].sz);
            return NULL;
        }
    }
    // 边界页（代码尾 + 缝隙共享的那一页）需要 RWX
    if (e->text_end & (PAGE - 1)) {
        if (mprotect((void *)(base + e->x0_sz), PAGE, PROT_READ | PROT_WRITE | PROT_EXEC))
            LOG("%s: 边界页 RWX 设置失败（mprotect）", e->name);
    }
    LOG("%s: 内嵌映像已就位 基址 %p（三段映射 %zu + %zu + %zu 字节）",
        e->name, (void *)(uintptr_t)base, e->x0_sz, e->w_sz, e->x1_sz);
    return (void *)(uintptr_t)base;
}

// 页大小宏已在文件顶部定义（本块用到它，所以不在这里重复定义）

void *dm_image_alloc(size_t size) {
    size_t n = (size + PAGE - 1) & ~(size_t)(PAGE - 1);
    // MAP_JIT：Apple Silicon 上匿名内存转成可执行需要它（否则 mprotect(RX) 会失败）
    void *p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    if (p == MAP_FAILED) {
        p = mmap(NULL, n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) die("映射 %#zx 字节失败", n);
        LOG("注意：这次没用 MAP_JIT（已回退到普通匿名映射）");
    }
    return p;
}

void dm_code_protect(void *p, size_t n) {
    size_t a = (size_t)n & ~(size_t)(PAGE - 1);          // 只封整页
    if (a && mprotect(p, a, PROT_READ | PROT_EXEC)) die("代码区设 RX 失败（MAP_JIT？）");
    // TEXT_END 不页对齐时，代码尾与「缝隙」（ro/data 的开头）同页：
    // 那一页必须 RWX —— 代码尾要执行，缝隙里可能有重定位槽要写（实测 rtld 就是）。
    if (n & (PAGE - 1))
        if (mprotect(p, PAGE, PROT_READ | PROT_WRITE | PROT_EXEC)) die("边界页设 RWX 失败");
}

void dm_ro_protect(void *p, size_t n) {
    size_t a = (size_t)n & ~(size_t)(PAGE - 1);
    if (!a) return;
    if (mprotect(p, a, PROT_READ)) die("键页设只读失败");
}

// ---- 客体地址空间与宿主栈（内存族 svc 用）----

void *dm_as_reserve(size_t size) {                    // 只占地址不占内存
    void *p = mmap(NULL, size, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}

void dm_as_protect(void *p, size_t n, int rw) {
    if (!n) return;
    if (mprotect(p, n, rw ? (PROT_READ | PROT_WRITE) : PROT_NONE)) die("dm_as_protect(%p, %#zx, %d) 失败", p, n, rw);
}

void *dm_host_stack_top(void) { return pthread_get_stackaddr_np(pthread_self()); }
size_t dm_host_stack_size(void) { return pthread_get_stacksize_np(pthread_self()); }
