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
//      装载器用带 FIXED|OVERWRITE 的 vm_remap 把三段重映射到同一个基址的原相对位置上，
//      于是运行时布局与 patched.bin 逐字节相同（PC 相对寻址、补丁区跳转、宿主「基址+原偏移」全不用改）。
//      出包脚本是 smo/ios/tools/gen_embed.py，CI 上用合成产物验（tests/embed_selftest.c）。
#include "dm.h"
#include <mach/mach.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#if TARGET_OS_IPHONE
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <TargetConditionals.h>
#endif

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
// 内嵌表只在**链接了生成表**的构建里存在（embed 自检、iOS App）。Mach-O 的弱导入对数据符号
// 不生效、对函数也不可靠（ld64 仍报 undefined），所以干脆用显式宏开关 —— 最不折腾、必能链接。
#ifdef SMO_EMBED
const dm_embed_mod *dm_embed_table(unsigned *n);     // 由 gen_embed.py 生成的 .c 提供

const dm_embed_mod *dm_embed_find(const char *name) {
    unsigned n = 0;
    const dm_embed_mod *t = dm_embed_table(&n);
    for (unsigned i = 0; i < n; i++)
        if (!strcmp(t[i].name, name)) return &t[i];
    return NULL;
}
#else
const dm_embed_mod *dm_embed_find(const char *name) { (void)name; return NULL; }   // 本机/开发：从文件装载
#endif

void *dm_embed_map(const dm_embed_mod *e) {
    size_t total = (e->image_size + PAGE - 1) & ~(size_t)(PAGE - 1);
    vm_address_t base = 0;
    kern_return_t kr = vm_allocate(mach_task_self(), &base, total, VM_FLAGS_ANYWHERE);
    if (kr != KERN_SUCCESS) { LOG("%s: vm_allocate(%zu) 失败：%d", e->name, total, (int)kr); return NULL; }

    struct part { const uint8_t *src; size_t sz; size_t off; vm_prot_t prot; boolean_t copy; const char *what; };
    struct part parts[3] = {
        { e->x0, e->x0_sz, 0,                   VM_PROT_READ | VM_PROT_EXECUTE, FALSE, "x0 代码" },
        { e->w,  e->w_sz,  e->x0_sz,            VM_PROT_READ | VM_PROT_WRITE,   TRUE,  "W 数据" },
        { e->x1, e->x1_sz, e->x0_sz + e->w_sz,  VM_PROT_READ | VM_PROT_EXECUTE, FALSE, "x1 补丁" },
    };
    for (int i = 0; i < 3; i++) {
        if (!parts[i].sz) continue;
        vm_address_t dst = (vm_address_t)(base + parts[i].off);
        vm_prot_t cur = 0, maxp = 0;
        // W 段的 max 给 RWX：TEXT_END 不页对齐时，代码尾与缝隙同页（见 gen_embed.py），
        // 那一页既要有 X（代码尾）又要有 W（缝隙里的重定位槽）。
        maxp = (i == 1) ? (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE) : parts[i].prot;
        // vm_remap（旧 API）：macOS 与 iOS 都有；mach_vm_* 是 macOS 专用，
        // iPhoneOS SDK 里 mach/mach_vm.h 直接标 unsupported（CI 上真挂过）。
        kr = vm_remap(mach_task_self(), &dst, parts[i].sz, 0,
                      VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(),
                      (vm_address_t)(uintptr_t)parts[i].src,
                      parts[i].copy, &cur, &maxp, VM_INHERIT_NONE);
        if (kr != KERN_SUCCESS || dst != (vm_address_t)(base + parts[i].off)) {
            LOG("%s: %s 段 vm_remap 失败：%d（dst=%#llx 期望 %#llx sz=%zu）",
                e->name, parts[i].what, (int)kr, (unsigned long long)dst,
                (unsigned long long)(base + parts[i].off), parts[i].sz);
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

// iOS 壳模式：映像取自 Guest.framework/Guest（只装三段的动态库）。
// macOS 上返回 NULL（开发/CI 走文件路径）。与 dm_embed_map 同一个 vm_remap 形状。
void *dm_image_from_dylib(const char *name, size_t *size) {
#if TARGET_OS_IPHONE
    static const struct mach_header_64 *mh;
    if (!mh) {
        char exe[4096];
        uint32_t n = sizeof exe;
        if (_NSGetExecutablePath(exe, &n)) return NULL;
        char *sl = strrchr(exe, '/');
        if (!sl) return NULL;
        strcpy(sl, "/Frameworks/Guest.framework/Guest");
        if (!dlopen(exe, RTLD_NOW)) {
            LOG("壳模式：安装包里没有 Guest.framework/Guest（%s）—— 先用 guest_dylib.py 生成并放进去再签名", dlerror());
            return NULL;
        }
        for (uint32_t i = 0; i < _dyld_image_count(); i++)
            if (strstr(_dyld_get_image_name(i), "/Guest.framework/Guest"))
                mh = (const struct mach_header_64 *)_dyld_get_image_header(i);
        if (!mh) return NULL;
    }
    char sx0[17], sx1[17], dm[17];
    snprintf(sx0, sizeof sx0, "__dm%sx0", name);
    snprintf(sx1, sizeof sx1, "__dm%sx1", name);
    snprintf(dm, sizeof dm, "__DM%sW", name);
    unsigned long z0, zw, z1;
    const uint8_t *x0 = getsectiondata(mh, "__TEXT", sx0, &z0);
    const uint8_t *w  = getsectiondata(mh, dm, "__img", &zw);
    const uint8_t *x1 = getsectiondata(mh, "__TEXT", sx1, &z1);
    if (!x0 || !w || !x1) { LOG("壳模式：%s 的三段节不齐（%s/%s/%s）", name, sx0, dm, sx1); return NULL; }
    size_t total = z0 + zw + z1;
    vm_address_t base = 0;
    if (vm_allocate(mach_task_self(), &base, total, VM_FLAGS_ANYWHERE) != KERN_SUCCESS) return NULL;
    struct part { const uint8_t *s; size_t z; boolean_t cp; vm_prot_t p; } parts[3] = {
        { x0, z0, FALSE, VM_PROT_READ | VM_PROT_EXECUTE },
        { w,  zw, TRUE,  VM_PROT_READ | VM_PROT_WRITE },          // 边界页 RWX 由 dm_code_protect 处理
        { x1, z1, FALSE, VM_PROT_READ | VM_PROT_EXECUTE },
    };
    size_t off = 0;
    for (int i = 0; i < 3; i++) {
        vm_address_t dst = (vm_address_t)(base + off);
        vm_prot_t cur = 0;
        vm_prot_t maxp = (i == 1) ? (VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE) : parts[i].p;
        if (vm_remap(mach_task_self(), &dst, parts[i].z, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE,
                     mach_task_self(), (vm_address_t)(uintptr_t)parts[i].s,
                     parts[i].cp, &cur, &maxp, VM_INHERIT_NONE) != KERN_SUCCESS)
            return NULL;
        off += parts[i].z;
    }
    LOG("%s: 映像取自 Guest 动态库（%zu 字节）", name, total);
    *size = total;
    return (void *)(uintptr_t)base;
#else
    (void)name; (void)size;
    return NULL;
#endif
}

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
