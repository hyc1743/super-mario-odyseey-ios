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
//      装载器用 vm_allocate 保留一段地址，再把三段**重映射**到同一个基址上（mach_vm_remap），
//      于是补丁区的相对位置与改写器算出来的 PATCH_OFF 一致。参考项目这样做，出包脚本是
//      dm/ios/tools/gen_embed.py；本项目的对应物还没写（见 STATUS.md 的 TODO）。
#include "dm.h"
#include <sys/mman.h>
#include <unistd.h>

#define PAGE 0x4000                      // Apple Silicon 16 KB 页

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
    size_t a = (size_t)n & ~(size_t)(PAGE - 1);          // 只封整页，跨到可写区的半页不碰
    if (!a) return;
    if (mprotect(p, a, PROT_READ | PROT_EXEC)) die("代码区设 RX 失败（MAP_JIT？）");
}

void dm_ro_protect(void *p, size_t n) {
    size_t a = (size_t)n & ~(size_t)(PAGE - 1);
    if (!a) return;
    if (mprotect(p, a, PROT_READ)) die("键页设只读失败");
}
